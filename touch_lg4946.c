/*
 * touch_lg4946.c - SiW touch driver glue for LG4946
 *
 * Copyright (C) 2016 Silicon Works - http://www.siliconworks.co.kr
 * Author: Hyunho Kim <kimhh@siliconworks.co.kr>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/async.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/irqreturn.h>
#include <linux/slab.h>
#include <linux/pm.h>
#include <linux/gpio.h>
#include <linux/string.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/firmware.h>
#include <linux/spi/spi.h>
#include <asm/page.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/memory.h>

#include "siw_touch.h"
#include "siw_touch_hal.h"
#include "siw_touch_bus.h"

#define CHIP_ID						"4946"
#define CHIP_DEVICE_NAME			"LG4946"
#define CHIP_COMPATIBLE_NAME		"siw,lg4946"
#define CHIP_DEVICE_DESC			"SiW Touch LG4946 Driver"

#define CHIP_TYPE					CHIP_LG4946

#define CHIP_MODE_ALLOWED			(0 |	\
									LCD_MODE_BIT_U0 |	\
									LCD_MODE_BIT_U2 |	\
									LCD_MODE_BIT_U3 |	\
									LCD_MODE_BIT_U3_PARTIAL |	\
									LCD_MODE_BIT_STOP |	\
									0)

#define CHIP_FW_SIZE				(72<<10)


#define CHIP_INPUT_ID_BUSTYPE		BUS_SPI
#define CHIP_INPUT_ID_VENDOR		0xABCD
#define CHIP_INPUT_ID_PRODUCT		0x9876
#define CHIP_INPUT_ID_VERSION		0x1234

#if defined(CONFIG_ARCH_EXYNOS5)
#define __CHIP_QUIRK_ADD			CHIP_QUIRK_NOT_SUPPORT_XFER
#else
#define __CHIP_QUIRK_ADD			0
#endif

#define CHIP_QUIRKS					(0 |	\
									CHIP_QUIRK_NOT_SUPPORT_IME |	\
									__CHIP_QUIRK_ADD |	\
									0)


#define CHIP_BUS_TYPE				BUS_IF_SPI
#define	CHIP_BUF_SIZE				0
#define CHIP_SPI_MODE				SPI_MODE_0
#define CHIP_BPW					8
#define CHIP_MAX_FREQ				(5 * 1000* 1000)
#define CHIP_TX_HDR_SZ				SPI_BUS_TX_HDR_SZ
#define CHIP_RX_HDR_SZ				SPI_BUS_RX_HDR_SZ
#define CHIP_TX_DUMMY_SZ			SPI_BUS_TX_DUMMY_SZ
#define CHIP_RX_DUMMY_SZ			SPI_BUS_RX_DUMMY_SZ


#if !defined(__SIW_CONFIG_OF)	//See siw_touch_cfg.h
enum CHIP_CAPABILITY {
	CHIP_MAX_X			= 720,
	CHIP_MAX_Y			= 1280,
	CHIP_MAX_PRESSURE	= 255,
	CHIP_MAX_WIDTH		= 15,
	CHIP_MAX_ORI		= 1,
	CHIP_MAX_ID			= 10,
	/* */
	CHIP_HW_RST_DELAY	= 210,
	CHIP_SW_RST_DELAY	= 90,
};
#endif

static const struct tci_info chip_tci_info[2] = {
	[TCI_1] = {
		.tap_count		= 2,
		.min_intertap	= 0,
		.max_intertap	= 70,
		.touch_slop		= 100,
		.tap_distance	= 10,
		.intr_delay		= 0,
	},
	[TCI_2] = {
		.tap_count		= 0,
		.min_intertap	= 0,
		.max_intertap	= 70,
		.touch_slop		= 100,
		.tap_distance	= 255,
		.intr_delay		= 20,
	},
};

static const struct reset_area chip_tci_reset_area = {
	.x1	= ((35<<16) | 35),
	.y1 = ((684<<16) | 684),
	.x2 = ((35<<16) | 35),
	.y2 = ((1324<<16) | 1324),
};

static const struct siw_hal_swipe_ctrl chip_swipe_info = {
	.mode	= SWIPE_LEFT_BIT | SWIPE_RIGHT_BIT,
	.info = {
		[SWIPE_R] = {
			.distance		= 5,
			.ratio_thres	= 100,
			.ratio_distance	= 2,
			.ratio_period	= 5,
			.min_time		= 0,
			.max_time		= 150,
			.area.x1		= 201,
			.area.y1		= 0,
			.area.x2		= 719,
			.area.y2		= 79,
		},
		[SWIPE_L] = {
			.distance		= 5,
			.ratio_thres	= 100,
			.ratio_distance	= 2,
			.ratio_period	= 5,
			.min_time		= 0,
			.max_time		= 150,
			.area.x1		= 201,
			.area.y1		= 0,
			.area.x2		= 719,
			.area.y2		= 79,
		},
	},
};

static const struct reset_area chip_watch_win_range = {
	.x1	= 520,
	.y1 = 0,
	.x2 = 720,
	.y2 = 80,
};


/*
 * of_device_is_compatible(dev->of_node, CHIP_COMPATIBLE_NAME)
 */
static const struct of_device_id chip_match_ids[] = {
	{ .compatible = CHIP_COMPATIBLE_NAME },
	{ },
};

#if !defined(__SIW_CONFIG_OF)
#define CHIP_PIN_RESET			0
#define CHIP_PIN_IRQ			0
#define CHIP_PIN_MAKER			-1
#define CHIP_PIN_VDD			-1
#define CHIP_PIN_VIO			-1

#if (CHIP_PIN_RESET == 0) || (CHIP_PIN_IRQ == 0)
	#error Assign external pin & flag first!!!
#endif
#endif	/* __SIW_CONFIG_OF */

/* use eg. cname=arc1 to change name */
static char chip_name[32] = CHIP_DEVICE_NAME;
module_param_string(cname, chip_name, sizeof(chip_name), 0);

/* use eg. dname=arc1 to change name */
static char chip_drv_name[32] = SIW_TOUCH_NAME;
module_param_string(dname, chip_drv_name, sizeof(chip_drv_name), 0);

/* use eg. iname=arc1 to change input name */
static char chip_idrv_name[32] = SIW_TOUCH_INPUT;
module_param_string(iname, chip_idrv_name, sizeof(chip_idrv_name), 0);

static const struct siw_touch_pdata chip_pdata = {
	/* Configuration */
	.chip_id			= CHIP_ID,
	.chip_name			= chip_name,
	.drv_name			= chip_drv_name,
	.idrv_name			= chip_idrv_name,
	.owner				= THIS_MODULE,
	.of_match_table		= of_match_ptr(chip_match_ids),
	.chip_type			= CHIP_TYPE,
	.mode_allowed		= CHIP_MODE_ALLOWED,
	.fw_size			= CHIP_FW_SIZE,
	.flags				= 0,	/* Caution : MSB(bit31) unavailable */
	.irqflags			= 0,
	.quirks				= CHIP_QUIRKS,
	/* */
	.bus_info			= {
		.bus_type			= CHIP_BUS_TYPE,
		.buf_size			= CHIP_BUF_SIZE,
		.spi_mode			= CHIP_SPI_MODE,
		.bits_per_word		= CHIP_BPW,
		.max_freq			= CHIP_MAX_FREQ,
		.bus_tx_hdr_size	= CHIP_TX_HDR_SZ,
		.bus_rx_hdr_size	= CHIP_RX_HDR_SZ,
		.bus_tx_dummy_size	= CHIP_TX_DUMMY_SZ,
		.bus_rx_dummy_size	= CHIP_RX_DUMMY_SZ,
	},
#if !defined(__SIW_CONFIG_OF)
	.pins				= {
		.reset_pin		= CHIP_PIN_RESET,
		.reset_pin_pol	= OF_GPIO_ACTIVE_LOW,
		.irq_pin		= CHIP_PIN_IRQ,
		.maker_id_pin	= CHIP_PIN_MAKER,
		.vdd_pin		= CHIP_PIN_VDD,
		.vio_pin		= CHIP_PIN_VIO,
	},
	.caps				= {
		.max_x			= CHIP_MAX_X,
		.max_y			= CHIP_MAX_Y,
		.max_pressure	= CHIP_MAX_PRESSURE,
		.max_width		= CHIP_MAX_WIDTH,
		.max_orientation = CHIP_MAX_ORI;
		.max_id			= CHIP_MAX_ID,
		.hw_reset_delay	= CHIP_HW_RST_DELAY,
		.sw_reset_delay	= CHIP_SW_RST_DELAY,
	},
#endif
	/* Input Device ID */
	.i_id				= {
		.bustype		= CHIP_INPUT_ID_BUSTYPE,
		.vendor 		= CHIP_INPUT_ID_VENDOR,
		.product 		= CHIP_INPUT_ID_PRODUCT,
		.version 		= CHIP_INPUT_ID_VERSION,
	},
	/* */
	//See 'siw_hal_get_default_ops' [siw_touch_hal.c]
	.ops				= NULL,
	/* */
	//See 'siw_hal_get_tci_info' [siw_touch_hal.c]
	.tci_info			= (void *)chip_tci_info,
	.tci_reset_area		= (void *)&chip_tci_reset_area,
	.tci_qcover_open	= NULL,
	.tci_qcover_close	= NULL,
	//See 'siw_hal_get_swipe_info' [siw_touch_hal.c]
	.swipe_ctrl			= (void *)&chip_swipe_info,
	//See 'store_ext_watch_config_font_position' [siw_touch_hal_watch.c]
	.watch_win			= (void *)&chip_watch_win_range,
	//See 'siw_setup_operations'
	.reg_quirks			= NULL,
};

static struct siw_touch_chip_data chip_data = {
	.pdata = &chip_pdata,
	.bus_drv = NULL,
};

siw_chip_module_init(CHIP_DEVICE_NAME,
				chip_data,
				CHIP_DEVICE_DESC,
				"kimhh@siliconworks.co.kr");


__siw_setup_str("siw_chip_name=", siw_setup_chip_name, chip_name);
__siw_setup_str("siw_drv_name=", siw_setup_drv_name, chip_drv_name);
__siw_setup_str("siw_idrv_name=", siw_setup_idrv_name, chip_idrv_name);



