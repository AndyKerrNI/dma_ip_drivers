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

/*
 * QDMA Character Device Extension Interface
 *
 * This file provides a placeholder for users to implement custom
 * open, close, and ioctl handlers for QDMA character devices.
 *
 * By default, all callbacks are NULL. Users can assign their own
 * functions based on their specific use case (e.g., HSDP, custom
 * protocols, etc.).
 *
 * To use:
 * 1. Modify the static extra_cb structure below with your callbacks
 * 2. Implement your callback functions in this file
 * 3. Recompile the driver
 */

#include "user_extra.h"
#include "../src/cdev.h"
/**
 * Extra callback structure - Users modify this to add custom handlers
 * Default: all callbacks are NULL (no custom behavior)
 */
static const struct user_extra_cdev_cb extra_cb = {
	.open =  NULL,   /* Replace with user open method */
	.close = NULL,  /* Replace with user close method */
	.ioctl = NULL,  /* Replace with user ioctl method */
};


void user_extra_cdev_register_cb(struct qdma_cdev *xcdev) {

	const struct user_extra_cdev_cb *cb;
	if (!xcdev)
		return;

	cb = &extra_cb;
	xcdev->fp_open_extra = extra_cb.open;
	xcdev->fp_close_extra = extra_cb.close;
	xcdev->fp_ioctl_extra = extra_cb.ioctl;
	if (cb->open || cb->close || cb->ioctl) {
		pr_info("Extra callbacks registered for device: %s\n",
			 xcdev->name);
	}
}
