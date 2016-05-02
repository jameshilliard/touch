/*
 * SiW touch bus core driver
 *
 * Copyright (C) 2016 Silicon Works - http://www.siliconworks.co.kr
 * Author: Hyunho Kim <kimhh@siliconworks.co.kr>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 */

#ifndef __SIW_TOUCH_BUS_H
#define __SIW_TOUCH_BUS_H

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/irq.h>


#define SIW_TOUCH_MAX_BUF_SIZE			(64<<10)
#define SIW_TOUCH_MAX_XFER_BUF_SIZE		(100<<10)
#define SIW_TOUCH_MAX_XFER_COUNT		10

enum {
	I2C_BUS_TX_HDR_SZ = 2,
	I2C_BUS_RX_HDR_SZ = 0,
	I2C_BUS_TX_DUMMY_SZ = 0,
	I2C_BUS_RX_DUMMY_SZ = 0,
};

enum {
	SPI_BUS_TX_HDR_SZ = 2,
	SPI_BUS_RX_HDR_SZ = 4,
	SPI_BUS_TX_DUMMY_SZ = 0,
	SPI_BUS_RX_DUMMY_SZ = 2,
};

struct touch_bus_msg {
	u8 *tx_buf;
	int tx_size;
	u8 *rx_buf;
	int rx_size;
	u8 bits_per_word;
	int priv;
};

struct touch_xfer_data_t {
	u16 addr;
	u16 size;
	u8 *buf;
	u8 data[SIW_TOUCH_MAX_XFER_BUF_SIZE];
};

struct touch_xfer_data {
	struct touch_xfer_data_t tx;
	struct touch_xfer_data_t rx;
};

struct touch_xfer_msg {
	struct touch_xfer_data data[SIW_TOUCH_MAX_XFER_COUNT];
	u8 bits_per_word;
	u8 msg_count;
};

struct siw_touch_bus_drv {
	union {
		struct i2c_driver i2c_drv;
		struct spi_driver spi_drv;
	} bus;
	struct siw_touch_pdata *pdata;
};


#define t_dev_info_bus_parent(_dev)	\
	do {	\
		t_dev_info(_dev, "dev bus probe : %s/%s/%s\n",	\
				dev_name(_dev->parent->parent),		\
				dev_name(_dev->parent),		\
				dev_name(_dev));	\
	} while(0)


extern void siw_hal_bus_xfer_init(struct device *dev,
							struct touch_xfer_msg *xfer);
extern void siw_hal_bus_xfer_add_rx(struct touch_xfer_msg *xfer,
						u32 reg, void *buf, u32 size);
extern void siw_hal_bus_xfer_add_rx_seq(struct touch_xfer_msg *xfer,
						u32 reg, u32 *data, int cnt);
extern void siw_hal_bus_xfer_add_tx(struct touch_xfer_msg *xfer,
						u32 reg, void *buf, u32 size);
extern void siw_hal_bus_xfer_add_tx_seq(struct touch_xfer_msg *xfer,
						u32 reg, u32 *data, int cnt);


extern int siw_touch_bus_pin_get(struct siw_ts *ts);
extern int siw_touch_bus_pin_put(struct siw_ts *ts);

extern void *siw_touch_bus_create_bus_drv(int bus_type);
extern void *siw_touch_bus_create_bus_pdata(int bus_type);

extern int siw_touch_bus_alloc_buffer(struct siw_ts *ts);
extern int siw_touch_bus_free_buffer(struct siw_ts *ts);

extern int siw_touch_bus_read(struct device *dev, struct touch_bus_msg *msg);
extern int siw_touch_bus_write(struct device *dev, struct touch_bus_msg *msg);
extern int siw_touch_bus_xfer(struct device *dev, struct touch_xfer_msg *xfer);

extern void siw_touch_bus_err_dump_data(struct device *dev,
							u8 *buf, int len,
							int idx, char *name);

extern int siw_touch_bus_add_driver(struct siw_touch_chip_data *chip_data);
extern int siw_touch_bus_del_driver(struct siw_touch_chip_data *chip_data);

#endif	/* __SIW_TOUCH_BUS_H */

