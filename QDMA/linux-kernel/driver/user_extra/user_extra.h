/*
 * This file is part of the Xilinx DMA IP Core driver for Linux
 *
 * Copyright (c) 2017-2022, Xilinx, Inc. All rights reserved.
 * Copyright (c) 2022-2026, Advanced Micro Devices, Inc. All rights reserved.
 *
 * This source code is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * The full GNU General Public License is included in this distribution in
 * the file called "COPYING".
 */


#ifndef __USER_EXTRA_CDEV_H__
#define __USER_EXTRA_CDEV_H__

/* Forward declaration to avoid exposing full structure */
struct qdma_cdev;

/**
 * struct user_extra_cdev_cb - Extra callback structure
 * @open: Optional callback invoked during cdev open
 * @close: Optional callback invoked during cdev close
 * @ioctl: Optional callback invoked during cdev ioctl
 *
 * Users can define these callbacks in qdma_cdev_extra.c
 * to extend cdev functionality without modifying core driver.
 */
struct user_extra_cdev_cb {
	int (*open)(struct qdma_cdev *xcdev);
	int (*close)(struct qdma_cdev *xcdev);
	long (*ioctl)(struct qdma_cdev *xcdev, unsigned int cmd,
	unsigned long arg);
};


/**
 * user_extra_cdev_register_cb() - Register callbacks with a cdev
 * @xcdev: Target character device
 *
 * This is called internally during cdev creation to attach callbacks.
 */
void user_extra_cdev_register_cb(struct qdma_cdev *xcdev);

#endif /* __USER_EXTRA_CDEV_H__ */
