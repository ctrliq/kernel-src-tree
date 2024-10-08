#ifndef INT_BLK_MQ_H
#define INT_BLK_MQ_H

struct blk_mq_ctx {
	struct {
		spinlock_t		lock;
		struct list_head	rq_list;
	}  ____cacheline_aligned_in_smp;

	unsigned int		cpu;
	unsigned int		index_hw;

	/* incremented at dispatch time */
	unsigned long		rq_dispatched[2];
	unsigned long		rq_merged;

	/* incremented at completion time */
	unsigned long		____cacheline_aligned_in_smp rq_completed[2];

	struct request_queue	*queue;
	struct kobject		kobj;
};

void __blk_mq_complete_request(struct request *rq);
void blk_mq_run_hw_queue(struct blk_mq_hw_ctx *hctx, bool async);
void blk_mq_init_flush(struct request_queue *q);
void blk_mq_drain_queue(struct request_queue *q);
void blk_mq_free_queue(struct request_queue *q);
void blk_mq_rq_init(struct blk_mq_hw_ctx *hctx, struct request *rq);

/*
 * CPU hotplug helpers
 */
struct blk_mq_cpu_notifier;
void blk_mq_init_cpu_notifier(struct blk_mq_cpu_notifier *notifier,
			      int (*fn)(void *, unsigned long, unsigned int),
			      void *data);
void blk_mq_register_cpu_notifier(struct blk_mq_cpu_notifier *notifier);
void blk_mq_unregister_cpu_notifier(struct blk_mq_cpu_notifier *notifier);
void blk_mq_cpu_init(void);
void blk_mq_enable_hotplug(void);
void blk_mq_disable_hotplug(void);

/*
 * CPU -> queue mappings
 */
struct blk_mq_reg;
extern unsigned int *blk_mq_make_queue_map(struct blk_mq_reg *reg);
extern int blk_mq_update_queue_map(unsigned int *map, unsigned int nr_queues);

void blk_mq_add_timer(struct request *rq);

static inline struct blk_mq_ctx *__blk_mq_get_ctx(struct request_queue *q,
					   unsigned int cpu)
{
	return per_cpu_ptr(q->queue_ctx, cpu);
}

/*
 * This assumes per-cpu software queueing queues. They could be per-node
 * as well, for instance. For now this is hardcoded as-is. Note that we don't
 * care about preemption, since we know the ctx's are persistent. This does
 * mean that we can't rely on ctx always matching the currently running CPU.
 */
static inline struct blk_mq_ctx *blk_mq_get_ctx(struct request_queue *q)
{
	return __blk_mq_get_ctx(q, get_cpu());
}

static inline void blk_mq_put_ctx(struct blk_mq_ctx *ctx)
{
	put_cpu();
}

#endif
