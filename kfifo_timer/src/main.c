/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/atomic.h>

#define FIFO_DEPTH 8
#define FIFO_HALF (FIFO_DEPTH / 2)

struct fifo_item {
	void *fifo_reserved; /* Required by k_fifo */
	uint32_t sequence;
	int64_t uptime_ms;
};

static void publisher_timer_handler(struct k_timer *timer);
static void drain_fifo_batch(void);

K_FIFO_DEFINE(data_fifo);
K_FIFO_DEFINE(free_pool);
K_SEM_DEFINE(batch_ready_sem, 0, 1);
K_TIMER_DEFINE(publisher_timer, publisher_timer_handler, NULL);

static struct fifo_item item_pool[FIFO_DEPTH];
static atomic_t pending_items = ATOMIC_INIT(0);
static atomic_t sequence_counter = ATOMIC_INIT(0);

static void publisher_timer_handler(struct k_timer *timer)
{
	ARG_UNUSED(timer);

	struct fifo_item *item = k_fifo_get(&free_pool, K_NO_WAIT);
	if (item == NULL) {
		printk("producer: no free fifo slots available\n");
		return;
	}

	item->sequence = atomic_add(&sequence_counter, 1) + 1;
	item->uptime_ms = k_uptime_get();

	int previous = atomic_add(&pending_items, 1);

	k_fifo_put(&data_fifo, item);

	if (previous < FIFO_HALF && (previous + 1) >= FIFO_HALF) {
		k_sem_give(&batch_ready_sem);
	}
}

static void drain_fifo_batch(void)
{
	int drained = 0;

	while (1) {
		struct fifo_item *item = k_fifo_get(&data_fifo, K_NO_WAIT);
		if (item == NULL) {
			break;
		}

		atomic_dec(&pending_items);
		drained++;

		printk("consumer: seq=%u uptime=%lld ms\n",
		       item->sequence, (long long)item->uptime_ms);

		k_fifo_put(&free_pool, item);
	}

	if (drained > 0) {
		printk("consumer: drained %d item(s) from fifo\n", drained);
	}
}

void main(void)
{
	printk("k_fifo timer example running on %s\n", CONFIG_BOARD);

	for (int i = 0; i < FIFO_DEPTH; ++i) {
		k_fifo_put(&free_pool, &item_pool[i]);
	}

	k_timer_start(&publisher_timer, K_SECONDS(1), K_SECONDS(1));

	while (1) {
		k_sem_take(&batch_ready_sem, K_FOREVER);
		drain_fifo_batch();
	}
}
