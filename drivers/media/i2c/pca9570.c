/*
 * pca9570.c - pca9570 IO Expander driver
 *
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <media/camera_common.h>
#include <linux/module.h>


#define IMX185_PCA9570_I2C_ADDR (0x24)
#define PCA9570_MODE_STEPS	5
#define PCA9570_FORWARD 0
#define PCA9570_REVERSE 1

struct pca9570 {
	struct i2c_client *i2c_client;
	struct regmap *regmap;
	const char *channel;
};

static int pca9570_write_reg(struct pca9570 *priv,
				u8 addr, u8 val)
{
	int err;

	err = regmap_write(priv->regmap, addr, val);
	if (err)
		pr_err("%s:i2c write failed, 0x%x = %x\n",
			__func__, addr, val);

	return err;
}

static int pca9570_icr_move(struct pca9570 *priv, u32 direction, u32 val)
{
	struct i2c_client *i2c_client = priv->i2c_client;
	int i;
	int steps = 0;
	int err = 0;
	u8 reg = 0;

	steps = val;
	if (steps < 1)
		steps = PCA9570_MODE_STEPS;

	if (direction == PCA9570_FORWARD) {
		dev_info(&i2c_client->dev, "%s, forward val=%d\n",
			 __func__, steps);
		reg = 0x02;
	} else if (direction == PCA9570_REVERSE) {
		dev_info(&i2c_client->dev, "%s, reverse val=%d\n",
			 __func__, steps);
		reg = 0x01;
	} else
		return -ENOMEM;

	for (i = 0; i < steps; i++) {
		usleep_range(1000*100, 1000*110);
		err |= pca9570_write_reg(priv, IMX185_PCA9570_I2C_ADDR, 0x48);
		err |= pca9570_write_reg(priv, IMX185_PCA9570_I2C_ADDR, reg);
		usleep_range(1000*100, 1000*110);
		err |= pca9570_write_reg(priv, IMX185_PCA9570_I2C_ADDR, 0x48);
		err |= pca9570_write_reg(priv, IMX185_PCA9570_I2C_ADDR, 0x03);
	}
	if (err)
		pr_err("%s:i2c write failed\n", __func__);

	return err;
}


static int pca9570_icr_daymode(struct pca9570 *priv)
{
	int err = 0;

	err = pca9570_icr_move(priv, PCA9570_FORWARD, PCA9570_MODE_STEPS);
	return err;
}


static int pca9570_icr_nightmode(struct pca9570 *priv)
{
	int err = 0;

	err = pca9570_icr_move(priv, PCA9570_REVERSE, PCA9570_MODE_STEPS);
	return err;
}

static int pca9570_stats_show(struct seq_file *s, void *data)
{
	return 0;
}

static int pca9570_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, pca9570_stats_show, inode->i_private);
}

static ssize_t pca9570_debugfs_write(struct file *s,
				const char __user *user_buf,
				size_t count, loff_t *ppos)
{
	struct pca9570 *priv =
		((struct seq_file *)s->private_data)->private;
	struct i2c_client *i2c_client = priv->i2c_client;

	char buf[255];
	int buf_size;
	u32 val = 0;
	int err = 0;

	if (!user_buf || count <= 1)
		return -EFAULT;

	memset(buf, 0, sizeof(buf));
	buf_size = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;

	if (buf[0] == 'd') {
		dev_info(&i2c_client->dev, "%s, set daymode\n", __func__);
		err = pca9570_icr_daymode(priv);
		if (err)
			dev_info(&i2c_client->dev, "%s, set daymode fail\n",
				 __func__);
		return count;
	}

	if (buf[0] == 'n') {
		dev_info(&i2c_client->dev, "%s, set nightmode\n", __func__);
		err = pca9570_icr_nightmode(priv);
		if (err)
			dev_info(&i2c_client->dev, "%s, set nightmode fail\n",
				 __func__);
		return count;
	}

	if (sscanf(buf + 1, "0x%x", &val) == 1)
		goto set_attr;
	if (sscanf(buf + 1, "0X%x", &val) == 1)
		goto set_attr;
	if (sscanf(buf + 1, "%d", &val) == 1)
		goto set_attr;

	pr_err("SYNTAX ERROR: %s\n", buf);
	return -EFAULT;

set_attr:
	dev_info(&i2c_client->dev, "%s, val=%c%d\n", __func__, buf[0], val);
	switch (buf[0]) {
	case 'f':
		err = pca9570_icr_move(priv, PCA9570_FORWARD, val);
		if (err)
			dev_info(&i2c_client->dev, "%s, move forward fail\n",
				 __func__);
		break;
	case 'r':
		err = pca9570_icr_move(priv, PCA9570_REVERSE, val);
		if (err)
			dev_info(&i2c_client->dev, "%s, move reverse fail\n",
				 __func__);
		break;
	}

	return count;
}


static const struct file_operations pca9570_debugfs_fops = {
	.open = pca9570_debugfs_open,
	.read = seq_read,
	.write = pca9570_debugfs_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static int pca9570_debugfs_init(const char *dir_name,
				struct dentry **d_entry,
				struct dentry **f_entry,
				struct pca9570 *priv)
{
	struct dentry  *dp, *fp;
	char *dev_name[20];
	struct i2c_client *i2c_client = priv->i2c_client;
	struct device_node *np = i2c_client->dev.of_node;
	int err = 0;

	if (np) {
		err = of_property_read_string(np, "channel", &priv->channel);
		if (err)
			dev_err(&i2c_client->dev, "channel not found\n");
		snprintf(dev_name, sizeof(dev_name), "pca9570_%s",
			priv->channel);
	}

	dp = debugfs_create_dir(dev_name, NULL);
	if (dp == NULL) {
		pr_info("%s: debugfs create dir failed\n", __func__);
		return -ENOMEM;
	}

	fp = debugfs_create_file("pca9570", S_IRUGO|S_IWUSR,
		dp, priv, &pca9570_debugfs_fops);
	if (!fp) {
		pr_info("%s: debugfs create file failed\n", __func__);
		debugfs_remove_recursive(dp);
		return -ENOMEM;
	}

	if (d_entry)
		*d_entry = dp;
	if (f_entry)
		*f_entry = fp;
	return 0;
}

static  struct regmap_config pca9570_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.cache_type = REGCACHE_RBTREE,
};

static int pca9570_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct pca9570 *priv;
	int err = 0;

	priv = devm_kzalloc(&client->dev, sizeof(*priv), GFP_KERNEL);
	if (priv == NULL) {
		dev_err(&client->dev, "%s: kzalloc error\n", __func__);
		return -ENOMEM;
	}

	priv->i2c_client = client;
	priv->regmap = devm_regmap_init_i2c(priv->i2c_client,
				&pca9570_regmap_config);
	if (IS_ERR(priv->regmap)) {
		dev_err(&client->dev,
			"regmap init failed: %ld\n", PTR_ERR(priv->regmap));
		return -ENODEV;
	}


	err = pca9570_debugfs_init(NULL, NULL, NULL, priv);
	if (err)
		return err;
	/*set daymode by fault*/
	err = pca9570_icr_daymode(priv);
	if (err)
		return err;
	dev_info(&client->dev, "%s:  success\n", __func__);

	return err;
}


static int
pca9570_remove(struct i2c_client *client)
{

	if (client != NULL) {
		i2c_unregister_device(client);
		client = NULL;
	}

	return 0;
}

static const struct i2c_device_id pca9570_id[] = {
	{ "pca9570", 0 },
	{ },
};

static struct of_device_id pca9570_of_match[] = {
	{ .compatible = "nvidia,pca9570", },
	{ },
};
MODULE_DEVICE_TABLE(of, imx185_of_match);
MODULE_DEVICE_TABLE(i2c, pca9570_id);

static struct i2c_driver pca9570_i2c_driver = {
	.driver = {
		.name = "pca9570",
		.owner = THIS_MODULE,
	},
	.probe = pca9570_probe,
	.remove = pca9570_remove,
	.id_table = pca9570_id,
};

static int __init pca9570_init(void)
{
	return i2c_add_driver(&pca9570_i2c_driver);
}

static void __exit pca9570_exit(void)
{
	i2c_del_driver(&pca9570_i2c_driver);
}

module_init(pca9570_init);
module_exit(pca9570_exit);

MODULE_DESCRIPTION("IO Expander driver pca9570");
MODULE_AUTHOR("NVIDIA Corporation");
MODULE_LICENSE("GPL v2");
