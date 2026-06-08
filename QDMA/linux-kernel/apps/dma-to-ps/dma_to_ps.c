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
#include <math.h>
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

#define USER_INTR_DEV_PATH "/dev/qdma_user_intr"
#define USER_INTR_WAIT_TIMEOUT_MS 2000
#define QDMA_USER_INTR_IOCTL_MAGIC 'q'
#define QDMA_USER_INTR_IOCTL_SET_EVENTFD _IOW(QDMA_USER_INTR_IOCTL_MAGIC, 1, int)

static struct option const long_opts[] = {
	{"count", required_argument, NULL, 'n'},
	{"function", required_argument, NULL, 'u'},
	{"payload-size", required_argument, NULL, 'p'},
	{"help", no_argument, NULL, 'h'},
	{"verbose", no_argument, NULL, 'v'},
	{0, 0, 0, 0}
};

struct dma_message {
	uint32_t size;
	uint8_t payload[];
};

static int test_dma(char *devname, uint32_t payload_size,
	uint32_t transaction_count);

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
	fprintf(stdout, "%s\n\n", name);
	fprintf(stdout, "usage: %s [OPTIONS]\n\n", name);
	fprintf(stdout,
		"Write a DMA message to a fixed MM address and then trigger a PS interrupt using the QDMA request-submit path.\n\n");

	fprintf(stdout,
		"  -n (--count) number of DMA transactions to run for statistics (default 1)\n");
	fprintf(stdout,
		"  -u (--function) function number used to build /dev/qdma<func>-MM-0 (default 0x%x)\n",
		FUNCTION_DEFAULT);
	fprintf(stdout,
		"  -p (--payload-size) payload size in bytes for dma_message payload (required)\n");
	fprintf(stdout, "  -h (--help) print usage help and exit\n");
	fprintf(stdout, "  -v (--verbose) verbose output\n");
}

int main(int argc, char *argv[])
{
	int cmd_opt;
	char device_name[DEVICE_NAME_MAX];
	uint64_t function = FUNCTION_DEFAULT;
	uint64_t transaction_count_arg = 1;
	uint64_t payload_size_arg = 0;
	int payload_size_set = 0;

	while ((cmd_opt =
		getopt_long(argc, argv, "vhn:p:u:", long_opts,
			    NULL)) != -1) {
		switch (cmd_opt) {
		case 0:
			/* long option */
			break;
		case 'n':
			transaction_count_arg = getopt_integer(optarg);
			if (!transaction_count_arg ||
			    transaction_count_arg > UINT32_MAX) {
				fprintf(stderr,
					"--count must be in range 1-%u\n",
					UINT32_MAX);
				exit(-EINVAL);
			}
			break;
		case 'u':
			/* Function number (e.g. 0x41000) */
			function = getopt_integer(optarg);
			break;
		case 'p':
			payload_size_arg = getopt_integer(optarg);
			payload_size_set = 1;
			if (payload_size_arg > UINT32_MAX) {
				fprintf(stderr,
					"--payload-size must be in range 0-%u\n",
					UINT32_MAX);
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

	if (!payload_size_set) {
		fprintf(stderr, "--payload-size is required\n");
		usage(argv[0]);
		exit(-EINVAL);
	}

	snprintf(device_name, sizeof(device_name), "/dev/qdma%05lx-MM-0",
		 (unsigned long)function);

	if (verbose)
		fprintf(stdout,
		"dev %s, func 0x%lx, static_addr 0x%llx, payload_size %llu, count %llu\n",
		device_name, function,
		(unsigned long long)AXI_GPIO_0_WRITE,
		(unsigned long long)payload_size_arg,
		(unsigned long long)transaction_count_arg);

	return test_dma(device_name, (uint32_t)payload_size_arg,
		(uint32_t)transaction_count_arg);
}

static int test_dma(char *devname, uint32_t payload_size,
	uint32_t transaction_count)
{
	ssize_t rc;
	uint32_t write_value = 0;
	uint32_t read_value = 0;
	struct dma_message *msg_data = NULL;
	size_t msg_data_bytes = 0;
	unsigned int i;
	uint32_t txn;
	struct timespec ts_start, ts_end;
	int fpga_fd = open(devname, O_RDWR);
	int intr_fd = -1;
	int event_fd = -1;
	double sample_time = 0;
	double latency_mean = 0;
	double latency_m2 = 0;
	double latency_stddev = 0;
	double latency_min = 0;
	double latency_max = 0;
	double latency_jitter = 0;

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

	msg_data_bytes = sizeof(*msg_data) + payload_size;
	msg_data = malloc(msg_data_bytes);
	if (!msg_data) {
		perror("malloc dma_message");
		rc = -ENOMEM;
		goto out;
	}

	msg_data->size = payload_size;
	for (i = 0; i < payload_size; i++)
		msg_data->payload[i] = 0xAA;

	/*
	 * Userspace write on /dev/qdma*-MM-* is converted by the kernel
	 * cdev path into a qdma_request_submit() call.
	 * This single 32-bit write is intended to trigger a PS-side interrupt.
	 */
	for (txn = 0; txn < transaction_count; txn++) {
		rc = read_to_buffer(devname, fpga_fd, (char *)&read_value,
			sizeof(uint32_t), AXI_GPIO_0_BASE);
		if (rc < 0)
			goto out;

		write_value = read_value + 1;

		if (verbose)
			fprintf(stdout,
				"transaction %u host value = 0x%08x\n",
				txn + 1, write_value);

		clock_gettime(CLOCK_MONOTONIC, &ts_start);
		rc = write_from_buffer(devname, fpga_fd, (char *)msg_data,
				msg_data_bytes, MAPPED_MSG_DATA_OFFSET);
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
		if (rc < 0) {
			perror("clock_gettime");
			rc = -errno;
			goto out;
		}

		if (verbose)
			printf("transaction %u read back value = 0x%08x\n",
				txn + 1, read_value);

		timespec_sub(&ts_end, &ts_start);
		sample_time =
			(ts_end.tv_sec + ((double)ts_end.tv_nsec / NSEC_DIV));
		if (txn == 0) {
			latency_min = sample_time;
			latency_max = sample_time;
		} else {
			if (sample_time < latency_min)
				latency_min = sample_time;
			if (sample_time > latency_max)
				latency_max = sample_time;
		}
		{
			double delta = sample_time - latency_mean;

			latency_mean += delta / (txn + 1);
			latency_m2 += delta * (sample_time - latency_mean);
		}

		if (verbose)
			printf("** device %s, transaction %u latency = %.3f usec\n",
				devname, txn + 1, sample_time * 1000000.0);

		//  usleep(1000);
	}

	if (transaction_count > 1)
		latency_stddev = sqrt(latency_m2 / transaction_count);

	latency_jitter = latency_max - latency_min;

	fprintf(stdout, "iterations = %u\n", transaction_count);
	fprintf(stdout, "latency min = %.3f usec\n",
		latency_min * 1000000.0);
	fprintf(stdout, "latency max = %.3f usec\n",
		latency_max * 1000000.0);
	fprintf(stdout, "latency mean = %.3f usec\n",
		latency_mean * 1000000.0);
	fprintf(stdout, "latency stddev = %.3f usec\n",
		latency_stddev * 1000000.0);
	fprintf(stdout, "latency jitter (pk-pk) = %.3f usec\n",
		latency_jitter * 1000000.0);

	rc = 0;

out:
	free(msg_data);
	if (event_fd >= 0)
		close(event_fd);
	if (intr_fd >= 0)
		close(intr_fd);
	close(fpga_fd);

	return rc;
}
