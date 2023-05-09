/*
 * Copyright 2023 Linaro.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#define LOG_MODULE_NAME	ivshmem_test_bed
#define LOG_LEVEL CONFIG_IVSHMEM_LOG_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#include <zephyr/kernel.h>
#include <stdio.h>
#include <stdlib.h>
#include <zephyr/drivers/virtualization/ivshmem.h>
#include <zephyr/shell/shell.h>

/*
 * result of an interrupt
 *
 * Using the ivshmem API, we will receive a signal event + two integers (the
 * interrupt itself is dealt in the internal ivshmem driver api)
 */
struct ivshmem_irq {
	/* k_poll was signaled or not */
	unsigned int signaled;
	/* vector received */
	int result;
};

struct ivshmem_ctx {
	/* dev, received via DTS */
	const struct device *dev;
	/* virtual address to access shared memory of ivshmem */
	void		*mem;
	/* size of shared memory */
	size_t		 size;
	/* my id for ivshmem protocol */
	uint32_t	 id;
	/* number of MSI vectors allocated */
	uint16_t	 vectors;
};

enum ivshmem_send_type {
	IVSHMEM_SEND_STOP,
	IVSHMEM_SEND_ONCE,
	IVSHMEM_SEND_CONTINUOUS,
};

static enum ivshmem_send_type send_type = IVSHMEM_SEND_STOP;
static struct ivshmem_irq irq;
static struct ivshmem_ctx ctx;
static struct k_poll_signal *sig;
static int peer_id;
static int vector;
static char *message;

static void ivshmem_send_loop(void *arg1);
K_THREAD_DEFINE(ivshmem_tid, 4096,
		ivshmem_send_loop, NULL, NULL, NULL,
		8, 0, 0);

/*
 * wait for an interrupt event.
 */
static int wait_for_int(const struct ivshmem_ctx *ctx)
{
	int ret;
	struct k_poll_event events[] = {
		K_POLL_EVENT_INITIALIZER(K_POLL_TYPE_SIGNAL,
					 K_POLL_MODE_NOTIFY_ONLY,
					 sig),
	};

	LOG_DBG("%s: waiting interrupt from client...", __func__);
	ret = k_poll(events, ARRAY_SIZE(events), K_FOREVER);
	if (ret < 0) {
		printf("poll failed\n");
		return ret;
	}

	k_poll_signal_check(sig, &irq.signaled, &irq.result);
	LOG_DBG("%s: sig %p signaled %u result %d", __func__,
		sig, irq.signaled, irq.result);
	/* get ready for next signal */
	k_poll_signal_reset(sig);
	return 0;
}

/*
 * host <-> guest communication loop
 */
static void ivshmem_event_loop(struct ivshmem_ctx *ctx)
{
	int ret;
	char *buf;
	bool truncated;

	buf = malloc(ctx->size);
	if (buf == NULL) {
		printf("Could not allocate message buffer\n");
		return;
	}

	printf("Use ivshmem_send_notify to send a message\n");
	while (1) {
		/* host --> guest */
		ret = wait_for_int(ctx);
		if (ret < 0) {
			break;
		}

		/*
		 * QEMU may fail here if the shared memory has been downsized;
		 * the shared memory object must be protected against rogue
		 * users (check README for details)
		 */
		memcpy(buf, ctx->mem, ctx->size);
		truncated = (buf[ctx->size - 1] != '\0');
		buf[ctx->size - 1] = '\0';

		printf("received IRQ and %s message: %s\n",
		       truncated ? "truncated" : "full", buf);
	}
	free(buf);
}

static void ivshmem_send_loop(void *arg1)
{
	uintptr_t mem;
	const struct device *ivshmem_dev = DEVICE_DT_GET(DT_NODELABEL(ivshmem0));

	if (!IS_ENABLED(CONFIG_IVSHMEM_DOORBELL)) {
		printf("CONFIG_IVSHMEM_DOORBELL is not enabled \n");
		k_panic();
	}

	ivshmem_get_mem(ivshmem_dev, &mem);

	while(1) {

		if(send_type != IVSHMEM_SEND_STOP) {
			memcpy((void *)mem, (const void *)message, strlen(message));
			ivshmem_int_peer(ivshmem_dev, (uint16_t)peer_id, (uint16_t)vector);
		}

		if(send_type == IVSHMEM_SEND_ONCE) {
			send_type = IVSHMEM_SEND_STOP;
		}

		k_sleep(K_MSEC(500));
	}
}

static int setup_ivshmem(bool in_kernel)
{
	int ret;
	size_t i;

	ctx.dev = DEVICE_DT_GET(DT_NODELABEL(ivshmem0));
	if (!device_is_ready(ctx.dev)) {
		printf("Could not get ivshmem device\n");
		return -1;
	}

	ctx.size = ivshmem_get_mem(ctx.dev, (uintptr_t *)&ctx.mem);
	if (ctx.size == 0) {
		printf("Size cannot not be 0");
		return -1;
	}
	if (ctx.mem == NULL) {
		printf("Shared memory cannot be null\n");
		return -1;
	}

	ctx.id = ivshmem_get_id(ctx.dev);
	LOG_DBG("id for doorbell: %u", ctx.id);

	ctx.vectors = ivshmem_get_vectors(ctx.dev);
	if (ctx.vectors == 0) {
		printf("ivshmem-doorbell must have vectors\n");
		return -1;
	}

	if (in_kernel) {
		sig = k_malloc(sizeof(*sig));
		if (sig == NULL) {
			printf("could not allocate sig\n");
			return -1;
		}
		k_poll_signal_init(sig);
	}

	for (i = 0; i < ctx.vectors; i++) {
		ret = ivshmem_register_handler(ctx.dev, sig, i);
		if (ret < 0) {
			printf("registering handlers must be supported\n");
			return -1;
		}
	}
	return 0;
}

int main(void)
{
	int ret;

	ret = setup_ivshmem(true);
	if (ret < 0)
		return ret;

	ivshmem_event_loop(&ctx);
	k_object_free(sig);

	return 0;
}

static int cmd_ivshmem_send_notify(const struct shell *sh,
			   size_t argc, char **argv)
{

	peer_id = strtol(argv[1], NULL, 10);
	vector = strtol(argv[2], NULL, 10);
	message = argv[3];
	send_type = IVSHMEM_SEND_ONCE;

	shell_fprintf(sh, SHELL_NORMAL,
		      "Message %s sent to peer %u on vector %u\n",
		      message,peer_id, vector);

	return 0;
}

static int cmd_ivshmem_send_continuous(const struct shell *sh,
			   size_t argc, char **argv)
{

	peer_id = strtol(argv[1], NULL, 10);
	vector = strtol(argv[2], NULL, 10);
	message = argv[3];
	send_type = IVSHMEM_SEND_CONTINUOUS;

	shell_fprintf(sh, SHELL_NORMAL,
		      "Message %s sent to peer %u on vector %u\n",
		      message,peer_id, vector);

	return 0;
}

static int cmd_ivshmem_send_continuous_stop(const struct shell *sh,
			   size_t argc, char **argv)
{
	send_type = IVSHMEM_SEND_STOP;
	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_ivshmem_msg,
						SHELL_CMD_ARG(ivshmem_send, NULL,
								"Put data on shared mem and notify a peer",
								cmd_ivshmem_send_notify, 3, 0),
						SHELL_CMD_ARG(ivshmem_send_cont, NULL,
								"Put data on shared mem and notify a peer contionus",
								cmd_ivshmem_send_continuous, 3, 0),
						SHELL_CMD_ARG(ivshmem_send_cont_stop, NULL,
								"Stop to send data to a peer",
								cmd_ivshmem_send_continuous_stop, 0, 0),
						SHELL_SUBCMD_SET_END
		);

SHELL_CMD_ARG_REGISTER(ivshmem_send_notify, &sub_ivshmem_msg,
		       "Put data on shared mem and notify a peer",
		       cmd_ivshmem_send_notify, 4 , 0);

SHELL_CMD_ARG_REGISTER(ivshmem_send_continuous, &sub_ivshmem_msg,
		       "Put data on shared mem and notify a peer continuosly",
		       cmd_ivshmem_send_continuous, 4 , 0);

SHELL_CMD_ARG_REGISTER(ivshmem_send_continuous_stop, &sub_ivshmem_msg,
		       "Stops to continuous Put data on shared mem and notify a peer",
		       cmd_ivshmem_send_continuous_stop, 1 , 0);
