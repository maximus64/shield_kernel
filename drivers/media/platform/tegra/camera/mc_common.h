/*
 * drivers/media/platform/tegra/camera/mc_common.h
 *
 * Tegra Media controller common APIs
 *
 * Copyright (c) 2012-2016, NVIDIA CORPORATION. All rights reserved.
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

#ifndef __MC_COMMON_H__
#define __MC_COMMON_H__

#include <media/media-device.h>
#include <media/media-entity.h>
#include <media/v4l2-async.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-dev.h>
#include <media/videobuf2-core.h>

#include "camera/core.h"
#include "csi/csi.h"

#define MAX_FORMAT_NUM	64
#define	MAX_SUBDEVICES	4
#define	QUEUED_BUFFERS	4
#define	ENABLE		1
#define	DISABLE		0

enum channel_capture_state {
	CAPTURE_IDLE = 0,
	CAPTURE_GOOD,
	CAPTURE_TIMEOUT,
	CAPTURE_ERROR,
};
/**
 * struct tegra_channel_buffer - video channel buffer
 * @buf: vb2 buffer base object
 * @queue: buffer list entry in the channel queued buffers list
 * @chan: channel that uses the buffer
 * @addr: Tegra IOVA buffer address for VI output
 */
struct tegra_channel_buffer {
	struct vb2_buffer buf;
	struct list_head queue;
	struct tegra_channel *chan;

	dma_addr_t addr;
};

#define to_tegra_channel_buffer(vb) \
	container_of(vb, struct tegra_channel_buffer, buf)

/**
 * struct tegra_vi_graph_entity - Entity in the video graph
 * @list: list entry in a graph entities list
 * @node: the entity's DT node
 * @entity: media entity, from the corresponding V4L2 subdev
 * @asd: subdev asynchronous registration information
 * @subdev: V4L2 subdev
 */
struct tegra_vi_graph_entity {
	struct list_head list;
	struct device_node *node;
	struct media_entity *entity;

	struct v4l2_async_subdev asd;
	struct v4l2_subdev *subdev;
};

/**
 * struct tegra_channel - Tegra video channel
 * @list: list entry in a composite device dmas list
 * @video: V4L2 video device associated with the video channel
 * @video_lock:
 * @pad: media pad for the video device entity
 * @pipe: pipeline belonging to the channel
 *
 * @vi: composite device DT node port number for the channel
 *
 * @kthread_capture: kernel thread task structure of this video channel
 * @wait: wait queue structure for kernel thread
 *
 * @format: active V4L2 pixel format
 * @fmtinfo: format information corresponding to the active @format
 *
 * @queue: vb2 buffers queue
 * @alloc_ctx: allocation context for the vb2 @queue
 * @sequence: V4L2 buffers sequence number
 *
 * @capture: list of queued buffers for capture
 * @queued_lock: protects the buf_queued list
 *
 * @csi: CSI register bases
 * @stride_align: channel buffer stride alignment, default is 64
 * @width_align: image width alignment, default is 4
 * @port: CSI port of this video channel
 * @io_id: Tegra IO rail ID of this video channel
 *
 * @fmts_bitmap: a bitmap for formats supported
 * @bypass: bypass flag for VI bypass mode
 */
struct tegra_channel {
	struct list_head list;
	struct video_device video;
	struct media_pad pad;
	struct media_pipeline pipe;
	struct mutex video_lock;

	struct tegra_mc_vi *vi;
	struct v4l2_subdev *subdev[MAX_SUBDEVICES];
	struct v4l2_subdev *subdev_on_csi;

	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_pix_format format;
	const struct tegra_video_format *fmtinfo;
	struct mutex stop_kthread_lock;

	unsigned char port[TEGRA_CSI_BLOCKS];
	unsigned int syncpt[TEGRA_CSI_BLOCKS];
	unsigned int syncpoint_fifo[TEGRA_CSI_BLOCKS];
	unsigned int buffer_offset[TEGRA_CSI_BLOCKS];
	unsigned int buffer_state[QUEUED_BUFFERS];
	struct vb2_buffer *buffers[QUEUED_BUFFERS];
	unsigned int timeout;
	unsigned int save_index;
	unsigned int free_index;
	unsigned int num_buffers;
	unsigned int released_bufs;

	struct task_struct *kthread_capture_start;
	wait_queue_head_t start_wait;
	struct vb2_queue queue;
	void *alloc_ctx;
	struct list_head capture;
	spinlock_t start_lock;
	struct completion capture_comp;

	void __iomem *csibase[TEGRA_CSI_BLOCKS];
	unsigned int stride_align;
	unsigned int width_align;
	unsigned int valid_ports;
	unsigned int total_ports;
	unsigned int numlanes;
	unsigned int io_id;
	unsigned int num_subdevs;
	unsigned int sequence;
	unsigned int saved_ctx_bypass;
	unsigned int saved_ctx_pgmode;
	unsigned int gang_mode;
	unsigned int gang_width;
	unsigned int gang_height;
	unsigned int gang_bytesperline;
	unsigned int gang_sizeimage;

	DECLARE_BITMAP(fmts_bitmap, MAX_FORMAT_NUM);
	atomic_t power_on_refcnt;
	struct v4l2_fh *fh;
	bool bypass;
	bool bfirst_fstart;
	enum channel_capture_state capture_state;
	atomic_t is_streaming;
	int requested_kbyteps;
	unsigned long requested_hz;
	int grp_id;
};

#define to_tegra_channel(vdev) \
	container_of(vdev, struct tegra_channel, video)

enum tegra_vi_pg_mode {
	TEGRA_VI_PG_DISABLED = 0,
	TEGRA_VI_PG_DIRECT,
	TEGRA_VI_PG_PATCH,
};

/**
 * struct tegra_mc_vi - NVIDIA Tegra Media controller structure
 * @v4l2_dev: V4L2 device
 * @media_dev: media device
 * @dev: device struct
 * @tegra_camera: tegra camera structure
 * @nvhost_device_data: NvHost VI device information
 *
 * @notifier: V4L2 asynchronous subdevs notifier
 * @entities: entities in the graph as a list of tegra_vi_graph_entity
 * @num_subdevs: number of subdevs in the pipeline
 *
 * @channels: list of channels at the pipeline output and input
 *
 * @ctrl_handler: V4L2 control handler
 * @pattern: test pattern generator V4L2 control
 * @pg_mode: test pattern generator mode (disabled/direct/patch)
 * @tpg_fmts_bitmap: a bitmap for formats in test pattern generator mode
 *
 * @has_sensors: a flag to indicate whether is a real sensor connecting
 */
struct tegra_mc_vi {
	struct vi *vi;
	struct platform_device *ndev;
	struct v4l2_device v4l2_dev;
	struct media_device media_dev;
	struct device *dev;
	struct nvhost_device_data *ndata;

	struct regulator *reg;
	struct clk *clk;
	struct clk *parent_clk;

	struct v4l2_async_notifier notifier;
	struct list_head entities;
	unsigned int num_channels;
	unsigned int num_subdevs;

	struct tegra_csi_device *csi;
	struct tegra_channel *chans;
	void __iomem *iomem;

	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_ctrl *pattern;
	struct v4l2_ctrl *bypass;
	enum tegra_vi_pg_mode pg_mode;
	DECLARE_BITMAP(tpg_fmts_bitmap, MAX_FORMAT_NUM);

	bool has_sensors;
	atomic_t power_on_refcnt;
	struct mutex bw_update_lock;
	unsigned long aggregated_kbyteps;
	unsigned long max_requested_hz;
	struct mutex mipical_lock;
	unsigned int link_status;
	unsigned int subdevs_bound;
};

int tegra_vi_get_port_info(struct tegra_channel *chan,
			struct device_node *node, unsigned int index);
void tegra_vi_v4l2_cleanup(struct tegra_mc_vi *vi);
int tegra_vi_v4l2_init(struct tegra_mc_vi *vi);
int tegra_vi_tpg_graph_init(struct tegra_mc_vi *vi);
int tegra_vi_graph_init(struct tegra_mc_vi *vi);
void tegra_vi_graph_cleanup(struct tegra_mc_vi *vi);
int tegra_vi_channels_init(struct tegra_mc_vi *vi);
int tegra_vi_channels_cleanup(struct tegra_mc_vi *vi);
int tegra_channel_init_subdevices(struct tegra_channel *chan);
int tegra_vi_power_on(struct tegra_mc_vi *vi);
void tegra_vi_power_off(struct tegra_mc_vi *vi);
int tegra_clean_unlinked_channels(struct tegra_mc_vi *vi);
int tegra_vi_media_controller_init(struct tegra_mc_vi *mc_vi,
			struct platform_device *pdev);
void tegra_vi_media_controller_cleanup(struct tegra_mc_vi *mc_vi);
void tegra_channel_query_hdmiin_unplug(struct tegra_channel *chan,
		struct v4l2_event *event);
#endif
