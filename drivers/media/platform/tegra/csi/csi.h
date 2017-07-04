/*
 * NVIDIA Tegra CSI Device Header
 *
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * Author: Bryan Wu <pengw@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __CSI_H_
#define __CSI_H_

#include <media/media-entity.h>
#include <media/v4l2-async.h>
#include <media/v4l2-ctrls.h>

#include <media/camera_common.h>
#include "camera/registers.h"

enum tegra_csi_port_num {
	PORT_A = 0,
	PORT_B = 1,
	PORT_C = 2,
	PORT_D = 3,
	PORT_E = 4,
	PORT_F = 5,
};

#define csi_port_is_valid(port) \
	(port < PORT_A ? 0 : (port > PORT_F ? 0 : 1))

enum camera_gang_mode {
	CAMERA_NO_GANG_MODE = 0,
	CAMERA_GANG_L_R = 1,
	CAMERA_GANG_T_B,
	CAMERA_GANG_R_L,
	CAMERA_GANG_B_T
};

struct tegra_csi_port {
	void __iomem *pixel_parser;
	void __iomem *cil;
	void __iomem *tpg;

	/* One pair of sink/source pad has one format */
	struct v4l2_mbus_framefmt format;
	const struct tegra_video_format *core_format;
	unsigned int lanes;

	enum tegra_csi_port_num num;
};

struct tegra_csi_device {
	struct v4l2_subdev subdev;
	struct device *dev;
	void __iomem *iomem[3];
	struct clk *clk;
	struct clk *tpg_clk;
	struct clk *cil[3];

	struct camera_common_data s_data[6];
	struct tegra_csi_port *ports;
	struct media_pad *pads;

	unsigned int clk_freq;
	int num_ports;
	int pg_mode;
};

static inline struct tegra_csi_device *to_csi(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct tegra_csi_device, subdev);
}

void set_csi_portinfo(struct tegra_csi_device *csi,
	unsigned int port, unsigned int numlanes);
void tegra_csi_status(struct tegra_csi_device *csi,
			enum tegra_csi_port_num port_num);
int tegra_csi_error(struct tegra_csi_device *csi,
			enum tegra_csi_port_num port_num);
void tegra_csi_tpg_start_streaming(struct tegra_csi_device *csi,
				enum tegra_csi_port_num port_num);
void tegra_csi_start_streaming(struct tegra_csi_device *csi,
				enum tegra_csi_port_num port_num);
void tegra_csi_stop_streaming(struct tegra_csi_device *csi,
				enum tegra_csi_port_num port_num);
void tegra_csi_error_recover(struct tegra_csi_device *csi,
				enum tegra_csi_port_num port_num);
void tegra_csi_pad_control(struct tegra_csi_device *csi,
				unsigned char *port_num, int enable);
int tegra_csi_channel_power(struct tegra_csi_device *csi,
				unsigned char *port, int enable);
#define tegra_csi_channel_power_on(csi, port) \
	tegra_csi_channel_power(csi, port, 1)
#define tegra_csi_channel_power_off(csi, port) \
	tegra_csi_channel_power(csi, port, 0)
int tegra_csi_power(struct tegra_csi_device *csi, int enable);
#define tegra_csi_power_on(csi) tegra_csi_power(csi, 1)
#define tegra_csi_power_off(csi) tegra_csi_power(csi, 0)
int tegra_csi_init(struct tegra_csi_device *csi,
		struct platform_device *pdev);
int tegra_csi_media_controller_init(struct tegra_csi_device *csi,
				struct platform_device *pdev);
int tegra_csi_media_controller_remove(struct tegra_csi_device *csi);
#endif
