/*
 * Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * Author: Laxman Dewangan<ldewangan@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/proc_fs.h>
#include <linux/of.h>
#include <linux/of_fdt.h>

enum plugin_manager_match_type {
	PLUGIN_MANAGER_MATCH_EXACT,
	PLUGIN_MANAGER_MATCH_PARTIAL,
	PLUGIN_MANAGER_MATCH_GE,
};

static struct property *__of_copy_property(const struct property *prop,
		gfp_t flags)
{
	struct property *propn;

	propn = kzalloc(sizeof(*prop), flags);
	if (propn == NULL)
		return NULL;

	propn->name = kstrdup(prop->name, flags);
	if (propn->name == NULL)
		goto err_fail_name;

	if (prop->length > 0) {
		propn->value = kmalloc(prop->length, flags);
		if (propn->value == NULL)
			goto err_fail_value;
		memcpy(propn->value, prop->value, prop->length);
		propn->length = prop->length;
	}
	return propn;

err_fail_value:
	kfree(propn->name);
err_fail_name:
	kfree(propn);
	return NULL;
}


static int __init update_target_node_from_overlay(
		struct device_node *target, struct device_node *overlay)
{
	struct property *prop;
	struct property *tprop;
	struct property *new_prop;
	const char *pval;
	int lenp = 0;
	int ret;

	for_each_property_of_node(overlay, prop) {
		/* Skip those we do not want to proceed */
		if (!strcmp(prop->name, "name") ||
			!strcmp(prop->name, "phandle") ||
			!strcmp(prop->name, "linux,phandle"))
				continue;
		if (!strcmp(prop->name, "delete-target-property")) {
			if (prop->length <= 0)
				continue;
			pval = (const char *)prop->value;
			pr_info("Removing Prop %s from target %s\n",
				pval, target->full_name);
			tprop = of_find_property(target, pval, &lenp);
			if (tprop)
				of_remove_property(target, tprop);
			continue;
		}

		new_prop = __of_copy_property(prop, GFP_KERNEL);
		if (!new_prop) {
			pr_err("Prop %s can not be duplicated\n",
				prop->name);
			return -EINVAL;
		}

		tprop = of_find_property(target, prop->name, &lenp);
		if (!tprop) {
			ret = of_add_property(target, new_prop);
			if (ret < 0) {
				pr_err("Prop %s can not be added on node %s\n",
					new_prop->name, target->full_name);
				return ret;
			}
		} else {
			ret = of_update_property(target, new_prop);
			if (ret < 0) {
				pr_err("Prop %s can not be updated on node %s\n",
					new_prop->name, target->full_name);
				return ret;
			}
		}
	}
	return 0;
}

static int plugin_manager_get_fabid(const char *id_str)
{
	int fabid = 0;
	int i;

	if (strlen(id_str) < 13)
		return -EINVAL;

	for (i = 0; i < 3; ++i) {
		if ((id_str[10 + i] >= '0') && (id_str[10 + i] <= '9'))
			fabid = fabid * 10 + id_str[10 + i] - '0';
		else
			return -EINVAL;
	}

	return fabid;
}

static bool plugin_manager_match_id(struct device_node *np, const char *id_name)
{
	struct property *prop;
	const char *in_str = id_name;
	int match_type = PLUGIN_MANAGER_MATCH_EXACT;
	int valid_str_len = strlen(id_name);
	int fabid = 0, prop_fabid;
	int i;

	if ((valid_str_len > 2) && (in_str[0] == '>') && (in_str[1] == '=')) {
		in_str += 2;
		valid_str_len -= 2;
		match_type = PLUGIN_MANAGER_MATCH_GE;
		goto match_type_done;
	}

	if ((valid_str_len > 1) && (in_str[0] == '^')) {
		in_str += 1;
		valid_str_len -= 1;
		match_type = PLUGIN_MANAGER_MATCH_PARTIAL;
		goto match_type_done;
	}

	for (i = 0; i < valid_str_len; ++i) {
		if (in_str[i] == '*') {
			valid_str_len = i;
			match_type = PLUGIN_MANAGER_MATCH_PARTIAL;
			break;
		}
	}

match_type_done:
	if (match_type == PLUGIN_MANAGER_MATCH_GE) {
		fabid = plugin_manager_get_fabid(in_str);
		if (fabid < 0)
			return false;
	}

	for_each_property_of_node(np, prop) {
		/* Skip those we do not want to proceed */
		if (!strcmp(prop->name, "name") ||
			!strcmp(prop->name, "phandle") ||
			!strcmp(prop->name, "linux,phandle"))
				continue;
		switch (match_type) {
		case PLUGIN_MANAGER_MATCH_EXACT:
			if (strlen(prop->name) != valid_str_len)
				break;
			if (!memcmp(in_str, prop->name, valid_str_len))
				return true;
			break;

		case PLUGIN_MANAGER_MATCH_PARTIAL:
			if (strlen(prop->name) < valid_str_len)
				break;
			if (!memcmp(in_str, prop->name, valid_str_len))
				return true;
			break;

		case PLUGIN_MANAGER_MATCH_GE:
			if (strlen(prop->name) < 13)
				break;
			if (memcmp(in_str, prop->name, 10))
				break;
			prop_fabid = plugin_manager_get_fabid(prop->name);
			if (prop_fabid < 0)
				break;
			if (prop_fabid >= fabid)
				return true;
			break;
		default:
			break;
		}
	}

	return false;
}

static int __init update_target_node(struct device_node *target,
	struct device_node *overlay)
{
	struct device_node *tchild, *ochild;
	int ret;

	ret = update_target_node_from_overlay(target, overlay);
	if (ret < 0) {
		pr_err("Target %s update with overlay %s failed: %d\n",
			target->name, overlay->name, ret);
		return ret;
	}

	for_each_child_of_node(overlay, ochild) {
		tchild = of_get_child_by_name(target, ochild->name);
		if (!tchild) {
			pr_err("Overlay child %s not found on target %s\n",
				ochild->full_name, tchild->full_name);
			continue;
		}

		ret = update_target_node(tchild, ochild);
		if (ret < 0) {
			pr_err("Target %s update with overlay %s failed: %d\n",
				tchild->name, ochild->name, ret);
			return ret;
		}
	}
	return 0;
}

static int __init parse_fragment(struct device_node *np)
{
	struct device_node *board_np, *odm_np, *overlay, *target, *cnp;
	const char *bname;
	struct property *prop;
	int board_count;
	int odm_count;
	int nchild;
	bool found = false;
	int ret;

	board_count = of_property_count_strings(np, "ids");
	odm_count = of_property_count_strings(np, "odm-data");
	if ((board_count <= 0) && (odm_count <= 0)) {
		pr_err("Node %s does not have property ids and odm data\n",
			np->name);
		return -EINVAL;
	}

	nchild = of_get_child_count(np);
	if (!nchild) {
		pr_err("Node %s does not have Overlay child\n", np->name);
		return -EINVAL;
	}

	/* Match the IDs or odm data */
	board_np = of_find_node_by_path("/chosen/plugin-manager/ids");
	odm_np = of_find_node_by_path("/chosen/plugin-manager/odm-data");
	if (!board_np && !odm_np) {
		pr_err("chosen/plugin-manager does'nt have ids and odm-data\n");
		return -EINVAL;
	}

	if ((board_count > 0) && board_np) {
		of_property_for_each_string(np, "ids", prop, bname) {
			found = plugin_manager_match_id(board_np, bname);
			if (found) {
				pr_info("node %s match with board %s\n",
					np->full_name, bname);
				break;
			}
		}
	}

	if (!found && (odm_count > 0) && odm_np) {
		of_property_for_each_string(np, "odm-data", prop, bname) {
			found = of_property_read_bool(odm_np, bname);
			if (found) {
				pr_info("node %s match with odm-data %s\n",
					np->full_name, bname);
				break;
			}
		}
	}

	if (!found)
		return 0;

	for_each_child_of_node(np, cnp) {
		target = of_parse_phandle(cnp, "target", 0);
		if (!target) {
			pr_err("Node %s does not have targer node\n",
				cnp->name);
			continue;
		}

		overlay = of_get_child_by_name(cnp, "_overlay_");
		if (!overlay) {
			pr_err("Node %s does not have Overlay\n", cnp->name);
			continue;
		}

		ret = update_target_node(target, overlay);
		if (ret < 0) {
			pr_err("Target %s update with overlay %s failed: %d\n",
				target->name, overlay->name, ret);
			continue;
		}
	}
	return 0;
}

static int __init plugin_manager_init(void)
{
	struct device_node *pm_node;
	struct device_node *child;
	int ret;

	pr_info("Initializing plugin-manager\n");

	pm_node = of_find_node_by_path("/plugin-manager");
	if (!pm_node) {
		pr_info("Plugin-manager not available\n");
		return 0;
	}

	if (!of_device_is_available(pm_node)) {
		pr_info("Plugin-manager status disabled\n");
		return 0;
	}

	for_each_child_of_node(pm_node, child) {
		if (!of_device_is_available(child)) {
			pr_info("Plugin-manager child %s status disabled\n",
				child->name);
			continue;
		}
		ret = parse_fragment(child);
		if (ret < 0)
			pr_err("Error in parsing node %s: %d\n",
				child->full_name, ret);
	}
	return 0;
}
core_initcall(plugin_manager_init);
