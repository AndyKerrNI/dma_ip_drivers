/*
 * This file is part of the QDMA userspace application
 * to enable the user to execute the QDMA functionality
 *
 * Copyright (c) 2018-2022, Xilinx, Inc. All rights reserved.
 * Copyright (c) 2022-2024, Advanced Micro Devices, Inc. All rights reserved.
 *
 * This source code is licensed under BSD-style license (found in the
 * LICENSE file in the root directory of this source tree)
 */

#define _DEFAULT_SOURCE
#define _XOPEN_SOURCE 500
#include <assert.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "dma_xfer_utils.c"

#define FUNCTION_DEFAULT 0x41000
#define DEVICE_NAME_MAX 64


#define AXI_GPIO_0_BASE 0x20200000000ULL
#define AXI_GPIO_0_WRITE (AXI_GPIO_0_BASE + 0x8)

#define AXI_GPIO_1_BASE 0x20200010000ULL
#define AXI_GPIO_1_REARM (AXI_GPIO_1_BASE + 0x120)
#define MAPPED_MSG_DATA_OFFSET 0x70100000ULL
#define MAPPED_MSG_DATA_LEN 10

#define USER_INTR_DEV_PATH "/dev/qdma_user_intr"
#define USER_INTR_WAIT_TIMEOUT_MS 2000
#define QDMA_USER_INTR_IOCTL_MAGIC 'q'
#define QDMA_USER_INTR_IOCTL_SET_EVENTFD _IOW(QDMA_USER_INTR_IOCTL_MAGIC, 1, int)

static struct option const long_opts[] = {
	{"device", required_argument, NULL, 'd'},
	{"function", required_argument, NULL, 'u'},
	{"write-value", required_argument, NULL, 'w'},
	{"help", no_argument, NULL, 'h'},
	{"verbose", no_argument, NULL, 'v'},
	{0, 0, 0, 0}
};

static int test_dma(char *devname, uint8_t write_seed);

static int wait_for_user_interrupt(int event_fd)
{
	int epfd;
	int rc;
	struct epoll_event ev;
	struct epoll_event out_ev;
	uint64_t event_cnt = 0;

	epfd = epoll_create1(0);
	if (epfd < 0) {
		perror("epoll_create1");
		return -errno;
	}

	memset(&ev, 0, sizeof(ev));
	ev.events = EPOLLIN;
	ev.data.fd = event_fd;

	rc = epoll_ctl(epfd, EPOLL_CTL_ADD, event_fd, &ev);
	if (rc < 0) {
		perror("epoll_ctl");
		rc = -errno;
		goto out;
	}

	rc = epoll_wait(epfd, &out_ev, 1, USER_INTR_WAIT_TIMEOUT_MS);
	if (rc == 0) {
		fprintf(stderr,
			"timeout waiting for user interrupt (%d ms)\n",
			USER_INTR_WAIT_TIMEOUT_MS);
		rc = -ETIMEDOUT;
		goto out;
	}
	if (rc < 0) {
		perror("epoll_wait");
		rc = -errno;
		goto out;
	}

	rc = read(event_fd, &event_cnt, sizeof(event_cnt));
	if (rc < 0) {
		perror("read eventfd counter");
		rc = -errno;
		goto out;
	}
	if (rc != sizeof(event_cnt)) {
		fprintf(stderr, "short read on eventfd\n");
		rc = -EIO;
		goto out;
	}

	if (verbose)
		fprintf(stdout, "user interrupt events = %llu\n",
			(unsigned long long)event_cnt);

	rc = 0;

out:
	close(epfd);
	return rc;
}

static void usage(const char *name)
{
	int i = 0;

	fprintf(stdout, "%s\n\n", name);
	fprintf(stdout, "usage: %s [OPTIONS]\n\n", name);
	fprintf(stdout,
		"Write one 32-bit MM value to a fixed AXI address to trigger an interrupt using the QDMA request-submit path.\n\n");

	fprintf(stdout,
		"  -%c (--%s) function number used to build /dev/qdma<func>-MM-0 (default 0x%x)\n",
		long_opts[i].val, long_opts[i].name, FUNCTION_DEFAULT);
	i++;
	fprintf(stdout, "  -%c (--%s) print usage help and exit\n",
		long_opts[i].val, long_opts[i].name);
	i++;
	fprintf(stdout, "  -%c (--%s) verbose output\n",
		long_opts[i].val, long_opts[i].name);
}

int main(int argc, char *argv[])
{
	int cmd_opt;
	char device_name[DEVICE_NAME_MAX];
	uint64_t function = FUNCTION_DEFAULT;
	uint32_t write_value_arg = 1;

	while ((cmd_opt =
		getopt_long(argc, argv, "vhu:w:", long_opts,
			    NULL)) != -1) {
		switch (cmd_opt) {
		case 0:
			/* long option */
			break;
		case 'u':
			/* Function number (e.g. 0x41000) */
			function = getopt_integer(optarg);
			break;
		case 'w':
			/* First byte value for mapped message write (0-255) */
			write_value_arg = getopt_integer(optarg);
			if (write_value_arg > 0xFF) {
				fprintf(stderr,
					"--write-value must be in range 0-255\n");
				exit(-EINVAL);
			}
			break;
		case 'v':
			verbose = 1;
			break;
		case 'h':
		default:
			usage(argv[0]);
			exit(0);
			break;
		}
	}

	snprintf(device_name, sizeof(device_name), "/dev/qdma%05lx-MM-0",
		 (unsigned long)function);

	if (verbose)
		fprintf(stdout,
		"dev %s, func 0x%lx, static_addr 0x%llx, write_seed 0x%02x\n",
		device_name, function,
		(unsigned long long)AXI_GPIO_0_WRITE,
		(unsigned int)write_value_arg);

	return test_dma(device_name, (uint8_t)write_value_arg);
}

static int test_dma(char *devname, uint8_t write_seed)
{
	ssize_t rc;
	uint32_t write_value = 0;
	uint32_t read_value = 0;
	uint8_t msg_data[MAPPED_MSG_DATA_LEN];
	unsigned int i;
	struct timespec ts_start, ts_end;
	int fpga_fd = open(devname, O_RDWR);
	int intr_fd = -1;
	int event_fd = -1;
	double total_time = 0;


	if (fpga_fd < 0) {
		fprintf(stderr, "unable to open device %s, %d.\n",
			devname, fpga_fd);
		perror("open device");
		return -EINVAL;
	}

	intr_fd = open(USER_INTR_DEV_PATH, O_RDONLY | O_NONBLOCK);
	if (intr_fd < 0) {
		fprintf(stderr, "unable to open %s\n", USER_INTR_DEV_PATH);
		perror("open user interrupt device");
		rc = -errno;
		goto out;
	}

	event_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
	if (event_fd < 0) {
		perror("eventfd");
		rc = -errno;
		goto out;
	}

	rc = ioctl(intr_fd, QDMA_USER_INTR_IOCTL_SET_EVENTFD, &event_fd);
	if (rc < 0) {
		perror("ioctl QDMA_USER_INTR_IOCTL_SET_EVENTFD");
		rc = -errno;
		goto out;
	}

	rc = read_to_buffer(devname, fpga_fd, (char *)&read_value,
		sizeof(uint32_t), AXI_GPIO_0_BASE);
	if (rc < 0)
		goto out;

	write_value = read_value + 1;

	if (verbose)
		fprintf(stdout, "host value = 0x%08x\n", write_value);

	for (i = 0; i < MAPPED_MSG_DATA_LEN; i++)
		msg_data[i] = (uint8_t)(write_seed + i);

	/*
	 * Userspace write on /dev/qdma*-MM-* is converted by the kernel
	 * cdev path into a qdma_request_submit() call.
	 * This single 32-bit write is intended to trigger a PS-side interrupt.
	 */
	clock_gettime(CLOCK_MONOTONIC, &ts_start);
	rc = write_from_buffer(devname, fpga_fd, (char *)msg_data,
			MAPPED_MSG_DATA_LEN, MAPPED_MSG_DATA_OFFSET);
	if (rc < 0)
		goto out;

	rc = write_from_buffer(devname, fpga_fd, (char *)&write_value,
			sizeof(uint32_t), AXI_GPIO_0_WRITE);
	if (rc < 0)
		goto out;

	rc = wait_for_user_interrupt(event_fd);
	if (rc < 0)
		goto out;

	write_value = 1;
	rc = write_from_buffer(devname, fpga_fd, (char *)&write_value,
			sizeof(uint32_t), AXI_GPIO_1_REARM);
	if (rc < 0)
		goto out;

	rc = read_to_buffer(devname, fpga_fd, (char *)&read_value,
			sizeof(uint32_t), AXI_GPIO_0_BASE);
	if (rc < 0)
		goto out;

	rc = clock_gettime(CLOCK_MONOTONIC, &ts_end);
	if (verbose)
		printf("read back value = 0x%08x\n", read_value);

	timespec_sub(&ts_end, &ts_start);
	total_time = (ts_end.tv_sec + ((double)ts_end.tv_nsec / NSEC_DIV));

	if (verbose)
		printf("** device %s, latency = %f sec\n", devname, total_time);

	rc = 0;

out:
	if (event_fd >= 0)
		close(event_fd);
	if (intr_fd >= 0)
		close(intr_fd);
	close(fpga_fd);

	return rc;
}
