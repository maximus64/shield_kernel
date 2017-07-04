/*
 * NVIDIA Tegra Video Input Device Driver Core Helpers
 *
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * Author: Bryan Wu <pengw@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/platform_device.h>

#include "camera/core.h"

static const struct tegra_video_format tegra_video_formats[] = {
	/* RAW 6: TODO */

	/* RAW 7: TODO */

	/* RAW 8 */
	{
		TEGRA_VF_RAW8,
		8,
		MEDIA_BUS_FMT_SRGGB8_1X8,
		1,
		TEGRA_IMAGE_FORMAT_T_L8,
		TEGRA_IMAGE_DT_RAW8,
		V4L2_PIX_FMT_SRGGB8,
		"RGRG.. GBGB..",
	},
	{
		TEGRA_VF_RAW8,
		8,
		MEDIA_BUS_FMT_SGRBG8_1X8,
		1,
		TEGRA_IMAGE_FORMAT_T_L8,
		TEGRA_IMAGE_DT_RAW8,
		V4L2_PIX_FMT_SGRBG8,
		"GRGR.. BGBG..",
	},
	{
		TEGRA_VF_RAW8,
		8,
		MEDIA_BUS_FMT_SGBRG8_1X8,
		1,
		TEGRA_IMAGE_FORMAT_T_L8,
		TEGRA_IMAGE_DT_RAW8,
		V4L2_PIX_FMT_SGBRG8,
		"GBGB.. RGRG..",
	},
	{
		TEGRA_VF_RAW8,
		8,
		MEDIA_BUS_FMT_SBGGR8_1X8,
		1,
		TEGRA_IMAGE_FORMAT_T_L8,
		TEGRA_IMAGE_DT_RAW8,
		V4L2_PIX_FMT_SBGGR8,
		"BGBG.. GRGR..",
	},

	/* RAW 10 */
	{
		TEGRA_VF_RAW10,
		10,
		MEDIA_BUS_FMT_SRGGB10_1X10,
		2,
		TEGRA_IMAGE_FORMAT_T_R16_I,
		TEGRA_IMAGE_DT_RAW10,
		V4L2_PIX_FMT_SRGGB10,
		"RGRG.. GBGB..",
	},
	{
		TEGRA_VF_RAW10,
		10,
		MEDIA_BUS_FMT_SGRBG10_1X10,
		2,
		TEGRA_IMAGE_FORMAT_T_R16_I,
		TEGRA_IMAGE_DT_RAW10,
		V4L2_PIX_FMT_SGRBG10,
		"GRGR.. BGBG..",
	},
	{
		TEGRA_VF_RAW10,
		10,
		MEDIA_BUS_FMT_SGBRG10_1X10,
		2,
		TEGRA_IMAGE_FORMAT_T_R16_I,
		TEGRA_IMAGE_DT_RAW10,
		V4L2_PIX_FMT_SGBRG10,
		"GBGB.. RGRG..",
	},
	{
		TEGRA_VF_RAW10,
		10,
		MEDIA_BUS_FMT_SBGGR10_1X10,
		2,
		TEGRA_IMAGE_FORMAT_T_R16_I,
		TEGRA_IMAGE_DT_RAW10,
		V4L2_PIX_FMT_SBGGR10,
		"BGBG.. GRGR..",
	},

	/* RAW 12 */
	{
		TEGRA_VF_RAW12,
		12,
		MEDIA_BUS_FMT_SRGGB12_1X12,
		2,
		TEGRA_IMAGE_FORMAT_T_R16_I,
		TEGRA_IMAGE_DT_RAW12,
		V4L2_PIX_FMT_SRGGB12,
		"RGRG.. GBGB..",
	},
	{
		TEGRA_VF_RAW12,
		12,
		MEDIA_BUS_FMT_SGRBG12_1X12,
		2,
		TEGRA_IMAGE_FORMAT_T_R16_I,
		TEGRA_IMAGE_DT_RAW12,
		V4L2_PIX_FMT_SGRBG12,
		"GRGR.. BGBG..",
	},
	{
		TEGRA_VF_RAW12,
		12,
		MEDIA_BUS_FMT_SGBRG12_1X12,
		2,
		TEGRA_IMAGE_FORMAT_T_R16_I,
		TEGRA_IMAGE_DT_RAW12,
		V4L2_PIX_FMT_SGBRG12,
		"GBGB.. RGRG..",
	},
	{
		TEGRA_VF_RAW12,
		12,
		MEDIA_BUS_FMT_SBGGR12_1X12,
		2,
		TEGRA_IMAGE_FORMAT_T_R16_I,
		TEGRA_IMAGE_DT_RAW12,
		V4L2_PIX_FMT_SBGGR12,
		"BGBG.. GRGR..",
	},

	/* RGB888 */
	{
		TEGRA_VF_RGB888,
		24,
		MEDIA_BUS_FMT_RGB888_1X24,
		4,
		TEGRA_IMAGE_FORMAT_T_A8R8G8B8,
		TEGRA_IMAGE_DT_RGB888,
		V4L2_PIX_FMT_ABGR32,
		"BGRA-8-8-8-8",
	},
	{
		TEGRA_VF_RGB888,
		24,
		MEDIA_BUS_FMT_RGB888_1X32_PADHI,
		4,
		TEGRA_IMAGE_FORMAT_T_A8B8G8R8,
		TEGRA_IMAGE_DT_RGB888,
		V4L2_PIX_FMT_RGB32,
		"RGB-8-8-8-8",
	},
	/* YUV422 */
	{
		TEGRA_VF_YUV422,
		16,
		MEDIA_BUS_FMT_UYVY8_1X16,
		2,
		TEGRA_IMAGE_FORMAT_T_U8_Y8__V8_Y8,
		TEGRA_IMAGE_DT_YUV422_8,
		V4L2_PIX_FMT_UYVY,
		"YUV 4:2:2",
	},
	{
		TEGRA_VF_YUV422,
		16,
		MEDIA_BUS_FMT_UYVY8_2X8,
		2,
		TEGRA_IMAGE_FORMAT_T_U8_Y8__V8_Y8,
		TEGRA_IMAGE_DT_YUV422_8,
		V4L2_PIX_FMT_UYVY,
		"YUV 4:2:2",
	},
	{
		TEGRA_VF_YUV422,
		16,
		MEDIA_BUS_FMT_VYUY8_2X8,
		2,
		TEGRA_IMAGE_FORMAT_T_T_V8_Y8__U8_Y8,
		TEGRA_IMAGE_DT_YUV422_8,
		V4L2_PIX_FMT_VYUY,
		"YUV 4:2:2",
	},
	{
		TEGRA_VF_YUV422,
		16,
		MEDIA_BUS_FMT_YUYV8_2X8,
		2,
		TEGRA_IMAGE_FORMAT_T_Y8_U8__Y8_V8,
		TEGRA_IMAGE_DT_YUV422_8,
		V4L2_PIX_FMT_YUYV,
		"YUV 4:2:2",
	},
	{
		TEGRA_VF_YUV422,
		16,
		MEDIA_BUS_FMT_YVYU8_2X8,
		2,
		TEGRA_IMAGE_FORMAT_T_Y8_V8__Y8_U8,
		TEGRA_IMAGE_DT_YUV422_8,
		V4L2_PIX_FMT_YVYU,
		"YUV 4:2:2",
	},

	/* YUV422 */
	{
		TEGRA_VF_YUV422,
		16,
		MEDIA_BUS_FMT_UYVY8_1X16,
		2,
		TEGRA_IMAGE_FORMAT_T_U8_Y8__V8_Y8,
		TEGRA_IMAGE_DT_YUV422_8,
		V4L2_PIX_FMT_UYVY,
	},
	{
		TEGRA_VF_YUV422,
		16,
		MEDIA_BUS_FMT_UYVY8_2X8,
		2,
		TEGRA_IMAGE_FORMAT_T_U8_Y8__V8_Y8,
		TEGRA_IMAGE_DT_YUV422_8,
		V4L2_PIX_FMT_UYVY,
	},
	{
		TEGRA_VF_YUV422,
		16,
		MEDIA_BUS_FMT_VYUY8_2X8,
		2,
		TEGRA_IMAGE_FORMAT_T_T_V8_Y8__U8_Y8,
		TEGRA_IMAGE_DT_YUV422_8,
		V4L2_PIX_FMT_VYUY,
	},
	{
		TEGRA_VF_YUV422,
		16,
		MEDIA_BUS_FMT_YUYV8_2X8,
		2,
		TEGRA_IMAGE_FORMAT_T_Y8_U8__Y8_V8,
		TEGRA_IMAGE_DT_YUV422_8,
		V4L2_PIX_FMT_YUYV,
	},
	{
		TEGRA_VF_YUV422,
		16,
		MEDIA_BUS_FMT_YVYU8_2X8,
		2,
		TEGRA_IMAGE_FORMAT_T_Y8_V8__Y8_U8,
		TEGRA_IMAGE_DT_YUV422_8,
		V4L2_PIX_FMT_YVYU,
	},
};

/* -----------------------------------------------------------------------------
 * Helper functions
 */

/**
 * tegra_core_get_fourcc_by_idx - get fourcc of a tegra_video format
 * @index: array index of the tegra_video_formats
 *
 * Return: fourcc code
 */
u32 tegra_core_get_fourcc_by_idx(unsigned int index)
{
	/* return default fourcc format if the index out of bounds */
	if (index > (ARRAY_SIZE(tegra_video_formats) - 1))
		return V4L2_PIX_FMT_SGRBG10;

	return tegra_video_formats[index].fourcc;
}

/**
 * tegra_core_get_description_by_idx - get description of a tegra_video format
 * @index: array index of the tegra_video_formats
 */
void tegra_core_get_description_by_idx(unsigned int index, __u8 *description)
{
	if (index > (ARRAY_SIZE(tegra_video_formats) - 1))
		return;

	if (description)
		strlcpy(description,
			tegra_video_formats[index].description,
			sizeof(tegra_video_formats[index].description));
}

/**
 * tegra_core_get_word_count - Calculate word count
 * @frame_width: number of pixels per line
 * @fmt: Tegra Video format struct which has BPP information
 *
 * Return: word count number
 */
u32 tegra_core_get_word_count(unsigned int frame_width,
			      const struct tegra_video_format *fmt)
{
	return frame_width * fmt->width / 8;
}

/**
 * tegra_core_get_idx_by_code - Retrieve index for a media bus code
 * @code: the format media bus code
 *
 * Return: a index to the format information structure corresponding to the
 * given V4L2 media bus format @code, or -1 if no corresponding format can
 * be found.
 */
int tegra_core_get_idx_by_code(unsigned int code)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(tegra_video_formats); ++i) {
		if (tegra_video_formats[i].code == code)
			return i;
	}

	return -1;
}

/**
 * tegra_core_get_format_by_code - Retrieve format information for a media
 * bus code
 * @code: the format media bus code
 *
 * Return: a pointer to the format information structure corresponding to the
 * given V4L2 media bus format @code, or NULL if no corresponding format can
 * be found.
 */
const struct tegra_video_format *
tegra_core_get_format_by_code(unsigned int code)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(tegra_video_formats); ++i) {
		if (tegra_video_formats[i].code == code)
			return &tegra_video_formats[i];
	}

	return NULL;
}

/**
 * tegra_core_get_format_by_fourcc - Retrieve format information for a 4CC
 * @fourcc: the format 4CC
 *
 * Return: a pointer to the format information structure corresponding to the
 * given V4L2 format @fourcc, or NULL if no corresponding format can be
 * found.
 */
const struct tegra_video_format *tegra_core_get_format_by_fourcc(u32 fourcc)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(tegra_video_formats); ++i) {
		if (tegra_video_formats[i].fourcc == fourcc)
			return &tegra_video_formats[i];
	}

	return NULL;
}

/**
 * tegra_core_bytes_per_line - Calculate bytes per line in one frame
 * @width: frame width
 * @align: number of alignment bytes
 * @fmt: Tegra Video format
 *
 * Simply calcualte the bytes_per_line and if it's not aligned it
 * will be padded to alignment boundary.
 */
u32 tegra_core_bytes_per_line(unsigned int width, unsigned int align,
			      const struct tegra_video_format *fmt)
{
	return roundup(width * fmt->bpp, align);
}
