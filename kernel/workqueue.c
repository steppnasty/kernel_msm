/*
 * linux/kernel/workqueue.c
 *
 * Generic mechanism for defining kernel helper threads for running
 * arbitrary tasks in process context.
 *
 * Started by Ingo Molnar, Copyright (C) 2002
 *
 * Derived from the taskqueue/keventd code by:
 *
 *   David Woodhouse <dwmw2@infradead.org>
 *   Andrew Morton
 *   Kai Petzke <wpp@marie.physik.tu-berlin.de>
 *   Theodore Ts'o <tytso@mit.edu>
 *
 * Made to use alloc_percpu by Christoph Lameter.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/signal.h>
#include <linux/completion.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/cpu.h>
#include <linux/notifier.h>
#include <linux/kthread.h>
#include <linux/hardirq.h>
#include <linux/mempolicy.h>
#include <linux/freezer.h>
#include <linux/kallsyms.h>
#include <linux/debug_locks.h>
#include <linux/lockdep.h>
#include <linux/idr.h>

#include <mach/board.h>

#define WQ_NAME			"events"
#define WQ_HIST_LEN			(20)

static unsigned int workqueue_debug_level = 0;

static int wq_pos = 0;
static unsigned long wq_hist[WQ_HIST_LEN];

static int workqueue_boot_config(char *str)
{
	unsigned kernel_flag;

	if (!str)
		return -EINVAL;

	kernel_flag = simple_strtoul(str, NULL, 16);
	if (kernel_flag & BIT5)
		workqueue_debug_level = 1;
	else
		workqueue_debug_level = 0;
	return 0;
}
early_param("kernelflag", workqueue_boot_config);

static int store_workqueue(const char *wq_name, unsigned long f_addr)
{
	char func_sym[KSYM_SYMBOL_LEN];

	if (strcmp(wq_name, WQ_NAME) == 0) {
		if (wq_hist[0] && (++wq_pos >= WQ_HIST_LEN))
			wq_pos = 0;

		wq_hist[wq_pos] = f_addr;

		if (workqueue_debug_level) {
			sprint_symbol(func_sym, f_addr);
			printk(KERN_INFO "[wq] %s: %s\n", wq_name, func_sym);
		}
	}

	return 0;
}

int print_workqueue(void)
{
	char func_sym[KSYM_SYMBOL_LEN];
	int i = wq_pos, count = 0;

	do {
		sprint_symbol(func_sym, wq_hist[i]);
		printk(KERN_INFO "[wq_list] %s[%d]: %s\n", WQ_NAME, count--, func_sym);

		if (--i < 0)
			i = WQ_HIST_LEN - 1;
	} while (wq_pos != i);

	return 0;
}

/*
 * Structure fields follow one of the following exclusion rules.
 *
 * I: Set during initialization and read-only afterwards.
 *
 * L: cwq->lock protected.  Access with cwq->lock held.
 *
 * F: wq->flush_mutex protected.
 *
 * W: workqueue_lock protected.
 */

struct cpu_workqueue_struct;

struct worker {
	struct work_struct	*current_work;	/* L: work being processed */
	struct task_struct	*task;		/* I: worker task */
	struct cpu_workqueue_struct *cwq;	/* I: the associated cwq */
	int			id;		/* I: worker id */
};

/*
 * The per-CPU workqueue (if single thread, we always use the first
 * possible cpu).  The lower WORK_STRUCT_FLAG_BITS of
 * work_struct->data are used for flags and thus cwqs need to be
 * aligned at two's power of the number of flag bits.
 */
struct cpu_workqueue_struct {

	spinlock_t lock;

	struct list_head worklist;
	wait_queue_head_t more_work;
	unsigned int		cpu;
	struct worker		*worker;

	struct workqueue_struct *wq;		/* I: the owning workqueue */
	int			work_color;	/* L: current color */
	int			flush_color;	/* L: flushing color */
	int			nr_in_flight[WORK_NR_COLORS];
						/* L: nr of in_flight works */
};

/*
 * Structure used to wait for workqueue flush.
 */
struct wq_flusher {
	struct list_head	list;		/* F: list of flushers */
	int			flush_color;	/* F: flush color waiting for */
	struct completion	done;		/* flush completion */
};

/*
 * The externally visible workqueue abstraction is an array of
 * per-CPU workqueues:
 */
struct workqueue_struct {
	unsigned int		flags;		/* I: WQ_* flags */
	struct cpu_workqueue_struct *cpu_wq;	/* I: cwq's */
	struct list_head	list;		/* W: list of all workqueues */

	struct mutex		flush_mutex;	/* protects wq flushing */
	int			work_color;	/* F: current work color */
	int			flush_color;	/* F: current flush color */
	atomic_t		nr_cwqs_to_flush; /* flush in progress */
	struct wq_flusher	*first_flusher;	/* F: first flusher */
	struct list_head	flusher_queue;	/* F: flush waiters */
	struct list_head	flusher_overflow; /* F: flush overflow list */

	const char		*name;		/* I: workqueue name */
#ifdef CONFIG_LOCKDEP
	struct lockdep_map	lockdep_map;
#endif
};

#ifdef CONFIG_LOCKDEP
/**
 * in_workqueue_context() - in context of specified workqueue?
 * @wq: the workqueue of interest
 *
 * Checks lockdep state to see if the current task is executing from
 * within a workqueue item.  This function exists only if lockdep is
 * enabled.
 */
int in_workqueue_context(struct workqueue_struct *wq)
{
	return lock_is_held(&wq->lockdep_map);
}
#endif

#ifdef CONFIG_DEBUG_OBJECTS_WORK

static struct debug_obj_descr work_debug_descr;

/*
 * fixup_init is called when:
 * - an active object is initialized
 */
static int work_fixup_init(void *addr, enum debug_obj_state state)
{
	struct work_struct *work = addr;

	switch (state) {
	case ODEBUG_STATE_ACTIVE:
		cancel_work_sync(work);
		debug_object_init(work, &work_debug_descr);
		return 1;
	default:
		return 0;
	}
}

/*
 * fixup_activate is called when:
 * - an active object is activated
 * - an unknown object is activated (might be a statically initialized object)
 */
static int work_fixup_activate(void *addr, enum debug_obj_state state)
{
	struct work_struct *work = addr;

	switch (state) {

	case ODEBUG_STATE_NOTAVAILABLE:
		/*
		 * This is not really a fixup. The work struct was
		 * statically initialized. We just make sure that it
		 * is tracked in the object tracker.
		 */
		if (test_bit(WORK_STRUCT_STATIC_BIT, work_data_bits(work))) {
			debug_object_init(work, &work_debug_descr);
			debug_object_activate(work, &work_debug_descr);
			return 0;
		}
		WARN_ON_ONCE(1);
		return 0;

	case ODEBUG_STATE_ACTIVE:
		WARN_ON(1);

	default:
		return 0;
	}
}

/*
 * fixup_free is called when:
 * - an active object is freed
 */
static int work_fixup_free(void *addr, enum debug_obj_state state)
{
	struct work_struct *work = addr;

	switch (state) {
	case ODEBUG_STATE_ACTIVE:
		cancel_work_sync(work);
		debug_object_free(work, &work_debug_descr);
		return 1;
	default:
		return 0;
	}
}

static struct debug_obj_descr work_debug_descr = {
	.name		= "work_struct",
	.fixup_init	= work_fixup_init,
	.fixup_activate	= work_fixup_activate,
	.fixup_free	= work_fixup_free,
};

static inline void debug_work_activate(struct work_struct *work)
{
	debug_object_activate(work, &work_debug_descr);
}

static inline void debug_work_deactivate(struct work_struct *work)
{
	debug_object_deactivate(work, &work_debug_descr);
}

void __init_work(struct work_struct *work, int onstack)
{
	if (onstack)
		debug_object_init_on_stack(work, &work_debug_descr);
	else
		debug_object_init(work, &work_debug_descr);
}
EXPORT_SYMBOL_GPL(__init_work);

void destroy_work_on_stack(struct work_struct *work)
{
	debug_object_free(work, &work_debug_descr);
}
EXPORT_SYMBOL_GPL(destroy_work_on_stack);

#else
static inline void debug_work_activate(struct work_struct *work) { }
static inline void debug_work_deactivate(struct work_struct *work) { }
#endif

/* Serializes the accesses to the list of workqueues. */
static DEFINE_SPINLOCK(workqueue_lock);
static LIST_HEAD(workqueues);
static DEFINE_PER_CPU(struct ida, worker_ida);

static int worker_thread(void *__worker);

static int singlethread_cpu __read_mostly;

static struct cpu_workqueue_struct *get_cwq(unsigned int cpu,
					    struct workqueue_struct *wq)
{
	return per_cpu_ptr(wq->cpu_wq, cpu);
}

static struct cpu_workqueue_struct *target_cwq(unsigned int cpu,
					       struct workqueue_struct *wq)
{
	if (unlikely(wq->flags & WQ_SINGLE_THREAD))
		cpu = singlethread_cpu;
	return get_cwq(cpu, wq);
}

static unsigned int work_color_to_flags(int color)
{
	return color << WORK_STRUCT_COLOR_SHIFT;
}

static int get_work_color(struct work_struct *work)
{
	return (*work_data_bits(work) >> WORK_STRUCT_COLOR_SHIFT) &
		((1 << WORK_STRUCT_COLOR_BITS) - 1);
}

static int work_next_color(int color)
{
	return (color + 1) % WORK_NR_COLORS;
}

/*
 * Set the workqueue on which a work item is to be run
 * - Must *only* be called if the pending flag is set
 */
static inline void set_wq_data(struct work_struct *work,
			       struct cpu_workqueue_struct *cwq,
			       unsigned long extra_flags)
{
	BUG_ON(!work_pending(work));

	atomic_long_set(&work->data, (unsigned long)cwq | work_static(work) |
			WORK_STRUCT_PENDING | extra_flags);
}

/*
 * Clear WORK_STRUCT_PENDING and the workqueue on which it was queued.
 */
static inline void clear_wq_data(struct work_struct *work)
{
	atomic_long_set(&work->data, work_static(work));
}

static inline struct cpu_workqueue_struct *get_wq_data(struct work_struct *work)
{
	return (void *)(atomic_long_read(&work->data) &
			WORK_STRUCT_WQ_DATA_MASK);
}

/**
 * insert_work - insert a work into cwq
 * @cwq: cwq @work belongs to
 * @work: work to insert
 * @head: insertion point
 * @extra_flags: extra WORK_STRUCT_* flags to set
 *
 * Insert @work into @cwq after @head.
 *
 * CONTEXT:
 * spin_lock_irq(cwq->lock).
 */
static void insert_work(struct cpu_workqueue_struct *cwq,
			struct work_struct *work, struct list_head *head,
			unsigned int extra_flags)
{
	/* we own @work, set data and link */
	set_wq_data(work, cwq, extra_flags);

	/*
	 * Ensure that we get the right work->data if we see the
	 * result of list_add() below, see try_to_grab_pending().
	 */
	smp_wmb();

	list_add_tail(&work->entry, head);
	wake_up(&cwq->more_work);
}

static void __queue_work(unsigned int cpu, struct workqueue_struct *wq,
			 struct work_struct *work)
{
	struct cpu_workqueue_struct *cwq = target_cwq(cpu, wq);
	unsigned long flags;

	debug_work_activate(work);
	spin_lock_irqsave(&cwq->lock, flags);
	BUG_ON(!list_empty(&work->entry));
	cwq->nr_in_flight[cwq->work_color]++;
	insert_work(cwq, work, &cwq->worklist,
		    work_color_to_flags(cwq->work_color));
	spin_unlock_irqrestore(&cwq->lock, flags);
}

/**
 * queue_work - queue work on a workqueue
 * @wq: workqueue to use
 * @work: work to queue
 *
 * Returns 0 if @work was already on a queue, non-zero otherwise.
 *
 * We queue the work to the CPU on which it was submitted, but if the CPU dies
 * it can be processed by another CPU.
 */
int queue_work(struct workqueue_struct *wq, struct work_struct *work)
{
	int ret;

	ret = queue_work_on(get_cpu(), wq, work);
	put_cpu();

	return ret;
}
EXPORT_SYMBOL_GPL(queue_work);

/**
 * queue_work_on - queue work on specific cpu
 * @cpu: CPU number to execute work on
 * @wq: workqueue to use
 * @work: work to queue
 *
 * Returns 0 if @work was already on a queue, non-zero otherwise.
 *
 * We queue the work to a specific CPU, the caller must ensure it
 * can't go away.
 */
int
queue_work_on(int cpu, struct workqueue_struct *wq, struct work_struct *work)
{
	int ret = 0;

	if (!test_and_set_bit(WORK_STRUCT_PENDING_BIT, work_data_bits(work))) {
		__queue_work(cpu, wq, work);
		ret = 1;
	}
	return ret;
}
EXPORT_SYMBOL_GPL(queue_work_on);

static void delayed_work_timer_fn(unsigned long __data)
{
	struct delayed_work *dwork = (struct delayed_work *)__data;
	struct cpu_workqueue_struct *cwq = get_wq_data(&dwork->work);

	__queue_work(smp_processor_id(), cwq->wq, &dwork->work);
}

/**
 * queue_delayed_work - queue work on a workqueue after delay
 * @wq: workqueue to use
 * @dwork: delayable work to queue
 * @delay: number of jiffies to wait before queueing
 *
 * Returns 0 if @work was already on a queue, non-zero otherwise.
 */
int queue_delayed_work(struct workqueue_struct *wq,
			struct delayed_work *dwork, unsigned long delay)
{
	if (delay == 0)
		return queue_work(wq, &dwork->work);

	return queue_delayed_work_on(-1, wq, dwork, delay);
}
EXPORT_SYMBOL_GPL(queue_delayed_work);

/**
 * queue_delayed_work_on - queue work on specific CPU after delay
 * @cpu: CPU number to execute work on
 * @wq: workqueue to use
 * @dwork: work to queue
 * @delay: number of jiffies to wait before queueing
 *
 * Returns 0 if @work was already on a queue, non-zero otherwise.
 */
int queue_delayed_work_on(int cpu, struct workqueue_struct *wq,
			struct delayed_work *dwork, unsigned long delay)
{
	int ret = 0;
	struct timer_list *timer = &dwork->timer;
	struct work_struct *work = &dwork->work;

	if (!test_and_set_bit(WORK_STRUCT_PENDING_BIT, work_data_bits(work))) {
		BUG_ON(timer_pending(timer));
		BUG_ON(!list_empty(&work->entry));

		timer_stats_timer_set_start_info(&dwork->timer);

		/* This stores cwq for the moment, for the timer_fn */
		set_wq_data(work, target_cwq(raw_smp_processor_id(), wq), 0);
		timer->expires = jiffies + delay;
		timer->data = (unsigned long)dwork;
		timer->function = delayed_work_timer_fn;

		if (unlikely(cpu >= 0))
			add_timer_on(timer, cpu);
		else
			add_timer(timer);
		ret = 1;
	}
	return ret;
}
EXPORT_SYMBOL_GPL(queue_delayed_work_on);

static struct worker *alloc_worker(void)
{
	struct worker *worker;

	worker = kzalloc(sizeof(*worker), GFP_KERNEL);
	return worker;
}

/**
 * create_worker - create a new workqueue worker
 * @cwq: cwq the new worker will belong to
 * @bind: whether to set affinity to @cpu or not
 *
 * Create a new worker which is bound to @cwq.  The returned worker
 * can be started by calling start_worker() or destroyed using
 * destroy_worker().
 *
 * CONTEXT:
 * Might sleep.  Does GFP_KERNEL allocations.
 *
 * RETURNS:
 * Pointer to the newly created worker.
 */
static struct worker *create_worker(struct cpu_workqueue_struct *cwq, bool bind)
{
	int id = -1;
	struct worker *worker = NULL;

	spin_lock(&workqueue_lock);
	while (ida_get_new(&per_cpu(worker_ida, cwq->cpu), &id)) {
		spin_unlock(&workqueue_lock);
		if (!ida_pre_get(&per_cpu(worker_ida, cwq->cpu), GFP_KERNEL))
			goto fail;
		spin_lock(&workqueue_lock);
	}
	spin_unlock(&workqueue_lock);

	worker = alloc_worker();
	if (!worker)
		goto fail;

	worker->cwq = cwq;
	worker->id = id;

	worker->task = kthread_create(worker_thread, worker, "kworker/%u:%d",
				      cwq->cpu, id);
	if (IS_ERR(worker->task))
		goto fail;

	if (bind)
		kthread_bind(worker->task, cwq->cpu);

	return worker;
fail:
	if (id >= 0) {
		spin_lock(&workqueue_lock);
		ida_remove(&per_cpu(worker_ida, cwq->cpu), id);
		spin_unlock(&workqueue_lock);
	}
	kfree(worker);
	return NULL;
}

/**
 * start_worker - start a newly created worker
 * @worker: worker to start
 *
 * Start @worker.
 *
 * CONTEXT:
 * spin_lock_irq(cwq->lock).
 */
static void start_worker(struct worker *worker)
{
	wake_up_process(worker->task);
}

/**
 * destroy_worker - destroy a workqueue worker
 * @worker: worker to be destroyed
 *
 * Destroy @worker.
 */
static void destroy_worker(struct worker *worker)
{
	int cpu = worker->cwq->cpu;
	int id = worker->id;

	/* sanity check frenzy */
	BUG_ON(worker->current_work);

	kthread_stop(worker->task);
	kfree(worker);

	spin_lock(&workqueue_lock);
	ida_remove(&per_cpu(worker_ida, cpu), id);
	spin_unlock(&workqueue_lock);
}

/**
 * cwq_dec_nr_in_flight - decrement cwq's nr_in_flight
 * @cwq: cwq of interest
 * @color: color of work which left the queue
 *
 * A work either has completed or is removed from pending queue,
 * decrement nr_in_flight of its cwq and handle workqueue flushing.
 *
 * CONTEXT:
 * spin_lock_irq(cwq->lock).
 */
static void cwq_dec_nr_in_flight(struct cpu_workqueue_struct *cwq, int color)
{
	/* ignore uncolored works */
	if (color == WORK_NO_COLOR)
		return;

	cwq->nr_in_flight[color]--;

	/* is flush in progress and are we at the flushing tip? */
	if (likely(cwq->flush_color != color))
		return;

	/* are there still in-flight works? */
	if (cwq->nr_in_flight[color])
		return;

	/* this cwq is done, clear flush_color */
	cwq->flush_color = -1;

	/*
	 * If this was the last cwq, wake up the first flusher.  It
	 * will handle the rest.
	 */
	if (atomic_dec_and_test(&cwq->wq->nr_cwqs_to_flush))
		complete(&cwq->wq->first_flusher->done);
}

/**
 * process_one_work - process single work
 * @worker: self
 * @work: work to process
 *
 * Process @work.  This function contains all the logics necessary to
 * process a single work including synchronization against and
 * interaction with other workers on the same cpu, queueing and
 * flushing.  As long as context requirement is met, any worker can
 * call this function to process a work.
 *
 * CONTEXT:
 * spin_lock_irq(cwq->lock) which is released and regrabbed.
 */
static void process_one_work(struct worker *worker, struct work_struct *work)
{
	struct cpu_workqueue_struct *cwq = worker->cwq;
	work_func_t f = work->func;
	int work_color;
#ifdef CONFIG_LOCKDEP
	/*
	 * It is permissible to free the struct work_struct from
	 * inside the function that is called from it, this we need to
	 * take into account for lockdep too.  To avoid bogus "held
	 * lock freed" warnings as well as problems when looking into
	 * work->lockdep_map, make a copy and use that here.
	 */
	struct lockdep_map lockdep_map = work->lockdep_map;
#endif
	/* claim and process */
	debug_work_deactivate(work);
	worker->current_work = work;
	work_color = get_work_color(work);
	list_del_init(&work->entry);

	spin_unlock_irq(&cwq->lock);

	BUG_ON(get_wq_data(work) != cwq);
	work_clear_pending(work);
	lock_map_acquire(&cwq->wq->lockdep_map);
	lock_map_acquire(&lockdep_map);
	f(work);
	lock_map_release(&lockdep_map);
	lock_map_release(&cwq->wq->lockdep_map);

	if (unlikely(in_atomic() || lockdep_depth(current) > 0)) {
		printk(KERN_ERR "BUG: workqueue leaked lock or atomic: "
		       "%s/0x%08x/%d\n",
		       current->comm, preempt_count(), task_pid_nr(current));
		printk(KERN_ERR "    last function: ");
		print_symbol("%s\n", (unsigned long)f);
		debug_show_held_locks(current);
		dump_stack();
	}

	spin_lock_irq(&cwq->lock);

	/* we're done with it, release */
	worker->current_work = NULL;
	cwq_dec_nr_in_flight(cwq, work_color);
}

static void run_workqueue(struct worker *worker)
{
	struct cpu_workqueue_struct *cwq = worker->cwq;

	spin_lock_irq(&cwq->lock);
	while (!list_empty(&cwq->worklist)) {
		struct work_struct *work = list_entry(cwq->worklist.next,
						struct work_struct, entry);
		process_one_work(worker, work);
	}
	spin_unlock_irq(&cwq->lock);
}

/**
 * worker_thread - the worker thread function
 * @__worker: self
 *
 * The cwq worker thread function.
 */
static int worker_thread(void *__worker)
{
	struct worker *worker = __worker;
	struct cpu_workqueue_struct *cwq = worker->cwq;
	DEFINE_WAIT(wait);

	if (cwq->wq->flags & WQ_FREEZEABLE)
		set_freezable();

	for (;;) {
		prepare_to_wait(&cwq->more_work, &wait, TASK_INTERRUPTIBLE);
		if (!freezing(current) &&
		    !kthread_should_stop() &&
		    list_empty(&cwq->worklist))
			schedule();
		finish_wait(&cwq->more_work, &wait);

		try_to_freeze();

		if (kthread_should_stop())
			break;

		if (unlikely(!cpumask_equal(&worker->task->cpus_allowed,
					    get_cpu_mask(cwq->cpu))))
			set_cpus_allowed_ptr(worker->task,
					     get_cpu_mask(cwq->cpu));
		run_workqueue(worker);
	}

	return 0;
}

struct wq_barrier {
	struct work_struct	work;
	struct completion	done;
};

static void wq_barrier_func(struct work_struct *work)
{
	struct wq_barrier *barr = container_of(work, struct wq_barrier, work);
	complete(&barr->done);
}

/**
 * insert_wq_barrier - insert a barrier work
 * @cwq: cwq to insert barrier into
 * @barr: wq_barrier to insert
 * @head: insertion point
 *
 * Insert barrier @barr into @cwq before @head.
 *
 * CONTEXT:
 * spin_lock_irq(cwq->lock).
 */
static void insert_wq_barrier(struct cpu_workqueue_struct *cwq,
			struct wq_barrier *barr, struct list_head *head)
{
	/*
	 * debugobject calls are safe here even with cwq->lock locked
	 * as we know for sure that this will not trigger any of the
	 * checks and call back into the fixup functions where we
	 * might deadlock.
	 */
	INIT_WORK_ON_STACK(&barr->work, wq_barrier_func);
	__set_bit(WORK_STRUCT_PENDING_BIT, work_data_bits(&barr->work));
	init_completion(&barr->done);

	debug_work_activate(&barr->work);
	insert_work(cwq, &barr->work, head, work_color_to_flags(WORK_NO_COLOR));
}

/**
 * flush_workqueue_prep_cwqs - prepare cwqs for workqueue flushing
 * @wq: workqueue being flushed
 * @flush_color: new flush color, < 0 for no-op
 * @work_color: new work color, < 0 for no-op
 *
 * Prepare cwqs for workqueue flushing.
 *
 * If @flush_color is non-negative, flush_color on all cwqs should be
 * -1.  If no cwq has in-flight commands at the specified color, all
 * cwq->flush_color's stay at -1 and %false is returned.  If any cwq
 * has in flight commands, its cwq->flush_color is set to
 * @flush_color, @wq->nr_cwqs_to_flush is updated accordingly, cwq
 * wakeup logic is armed and %true is returned.
 *
 * The caller should have initialized @wq->first_flusher prior to
 * calling this function with non-negative @flush_color.  If
 * @flush_color is negative, no flush color update is done and %false
 * is returned.
 *
 * If @work_color is non-negative, all cwqs should have the same
 * work_color which is previous to @work_color and all will be
 * advanced to @work_color.
 *
 * CONTEXT:
 * mutex_lock(wq->flush_mutex).
 *
 * RETURNS:
 * %true if @flush_color >= 0 and there's something to flush.  %false
 * otherwise.
 */
static bool flush_workqueue_prep_cwqs(struct workqueue_struct *wq,
				      int flush_color, int work_color)
{
	bool wait = false;
	unsigned int cpu;

	if (flush_color >= 0) {
		BUG_ON(atomic_read(&wq->nr_cwqs_to_flush));
		atomic_set(&wq->nr_cwqs_to_flush, 1);
	}

	for_each_possible_cpu(cpu) {
		struct cpu_workqueue_struct *cwq = get_cwq(cpu, wq);

		spin_lock_irq(&cwq->lock);

		if (flush_color >= 0) {
			BUG_ON(cwq->flush_color != -1);

			if (cwq->nr_in_flight[flush_color]) {
				cwq->flush_color = flush_color;
				atomic_inc(&wq->nr_cwqs_to_flush);
				wait = true;
			}
		}

		if (work_color >= 0) {
			BUG_ON(work_color != work_next_color(cwq->work_color));
			cwq->work_color = work_color;
		}

		spin_unlock_irq(&cwq->lock);
	}

	if (flush_color >= 0 && atomic_dec_and_test(&wq->nr_cwqs_to_flush))
		complete(&wq->first_flusher->done);

	return wait;
}

/**
 * flush_workqueue - ensure that any scheduled work has run to completion.
 * @wq: workqueue to flush
 *
 * Forces execution of the workqueue and blocks until its completion.
 * This is typically used in driver shutdown handlers.
 *
 * We sleep until all works which were queued on entry have been handled,
 * but we are not livelocked by new incoming ones.
 */
void flush_workqueue(struct workqueue_struct *wq)
{
	struct wq_flusher this_flusher = {
		.list = LIST_HEAD_INIT(this_flusher.list),
		.flush_color = -1,
		.done = COMPLETION_INITIALIZER_ONSTACK(this_flusher.done),
	};
	int next_color;

	lock_map_acquire(&wq->lockdep_map);
	lock_map_release(&wq->lockdep_map);

	mutex_lock(&wq->flush_mutex);

	/*
	 * Start-to-wait phase
	 */
	next_color = work_next_color(wq->work_color);

	if (next_color != wq->flush_color) {
		/*
		 * Color space is not full.  The current work_color
		 * becomes our flush_color and work_color is advanced
		 * by one.
		 */
		BUG_ON(!list_empty(&wq->flusher_overflow));
		this_flusher.flush_color = wq->work_color;
		wq->work_color = next_color;

		if (!wq->first_flusher) {
			/* no flush in progress, become the first flusher */
			BUG_ON(wq->flush_color != this_flusher.flush_color);

			wq->first_flusher = &this_flusher;

			if (!flush_workqueue_prep_cwqs(wq, wq->flush_color,
						       wq->work_color)) {
				/* nothing to flush, done */
				wq->flush_color = next_color;
				wq->first_flusher = NULL;
				goto out_unlock;
			}
		} else {
			/* wait in queue */
			BUG_ON(wq->flush_color == this_flusher.flush_color);
			list_add_tail(&this_flusher.list, &wq->flusher_queue);
			flush_workqueue_prep_cwqs(wq, -1, wq->work_color);
		}
	} else {
		/*
		 * Oops, color space is full, wait on overflow queue.
		 * The next flush completion will assign us
		 * flush_color and transfer to flusher_queue.
		 */
		list_add_tail(&this_flusher.list, &wq->flusher_overflow);
	}

	mutex_unlock(&wq->flush_mutex);

	wait_for_completion(&this_flusher.done);

	/*
	 * Wake-up-and-cascade phase
	 *
	 * First flushers are responsible for cascading flushes and
	 * handling overflow.  Non-first flushers can simply return.
	 */
	if (wq->first_flusher != &this_flusher)
		return;

	mutex_lock(&wq->flush_mutex);

	wq->first_flusher = NULL;

	BUG_ON(!list_empty(&this_flusher.list));
	BUG_ON(wq->flush_color != this_flusher.flush_color);

	while (true) {
		struct wq_flusher *next, *tmp;

		/* complete all the flushers sharing the current flush color */
		list_for_each_entry_safe(next, tmp, &wq->flusher_queue, list) {
			if (next->flush_color != wq->flush_color)
				break;
			list_del_init(&next->list);
			complete(&next->done);
		}

		BUG_ON(!list_empty(&wq->flusher_overflow) &&
		       wq->flush_color != work_next_color(wq->work_color));

		/* this flush_color is finished, advance by one */
		wq->flush_color = work_next_color(wq->flush_color);

		/* one color has been freed, handle overflow queue */
		if (!list_empty(&wq->flusher_overflow)) {
			/*
			 * Assign the same color to all overflowed
			 * flushers, advance work_color and append to
			 * flusher_queue.  This is the start-to-wait
			 * phase for these overflowed flushers.
			 */
			list_for_each_entry(tmp, &wq->flusher_overflow, list)
				tmp->flush_color = wq->work_color;

			wq->work_color = work_next_color(wq->work_color);

			list_splice_tail_init(&wq->flusher_overflow,
					      &wq->flusher_queue);
			flush_workqueue_prep_cwqs(wq, -1, wq->work_color);
		}

		if (list_empty(&wq->flusher_queue)) {
			BUG_ON(wq->flush_color != wq->work_color);
			break;
		}

		/*
		 * Need to flush more colors.  Make the next flusher
		 * the new first flusher and arm cwqs.
		 */
		BUG_ON(wq->flush_color == wq->work_color);
		BUG_ON(wq->flush_color != next->flush_color);

		list_del_init(&next->list);
		wq->first_flusher = next;

		if (flush_workqueue_prep_cwqs(wq, wq->flush_color, -1))
			break;

		/*
		 * Meh... this color is already done, clear first
		 * flusher and repeat cascading.
		 */
		wq->first_flusher = NULL;
	}

out_unlock:
	mutex_unlock(&wq->flush_mutex);
}
EXPORT_SYMBOL_GPL(flush_workqueue);

/**
 * flush_work - block until a work_struct's callback has terminated
 * @work: the work which is to be flushed
 *
 * Returns false if @work has already terminated.
 *
 * It is expected that, prior to calling flush_work(), the caller has
 * arranged for the work to not be requeued, otherwise it doesn't make
 * sense to use this function.
 */
int flush_work(struct work_struct *work)
{
	struct cpu_workqueue_struct *cwq;
	struct list_head *prev;
	struct wq_barrier barr;

	might_sleep();
	cwq = get_wq_data(work);
	if (!cwq)
		return 0;

	lock_map_acquire(&cwq->wq->lockdep_map);
	lock_map_release(&cwq->wq->lockdep_map);

	spin_lock_irq(&cwq->lock);
	if (!list_empty(&work->entry)) {
		/*
		 * See the comment near try_to_grab_pending()->smp_rmb().
		 * If it was re-queued under us we are not going to wait.
		 */
		smp_rmb();
		if (unlikely(cwq != get_wq_data(work)))
			goto already_gone;
		prev = &work->entry;
	} else {
		if (!cwq->worker || cwq->worker->current_work != work)
			goto already_gone;
		prev = &cwq->worklist;
	}
	insert_wq_barrier(cwq, &barr, prev->next);

	spin_unlock_irq(&cwq->lock);
	wait_for_completion(&barr.done);
	destroy_work_on_stack(&barr.work);
	return 1;
already_gone:
	spin_unlock_irq(&cwq->lock);
	return 0;
}
EXPORT_SYMBOL_GPL(flush_work);

/*
 * Upon a successful return (>= 0), the caller "owns" WORK_STRUCT_PENDING bit,
 * so this work can't be re-armed in any way.
 */
static int try_to_grab_pending(struct work_struct *work)
{
	struct cpu_workqueue_struct *cwq;
	int ret = -1;

	if (!test_and_set_bit(WORK_STRUCT_PENDING_BIT, work_data_bits(work)))
		return 0;

	/*
	 * The queueing is in progress, or it is already queued. Try to
	 * steal it from ->worklist without clearing WORK_STRUCT_PENDING.
	 */

	cwq = get_wq_data(work);
	if (!cwq)
		return ret;

	spin_lock_irq(&cwq->lock);
	if (!list_empty(&work->entry)) {
		/*
		 * This work is queued, but perhaps we locked the wrong cwq.
		 * In that case we must see the new value after rmb(), see
		 * insert_work()->wmb().
		 */
		smp_rmb();
		if (cwq == get_wq_data(work)) {
			debug_work_deactivate(work);
			list_del_init(&work->entry);
			cwq_dec_nr_in_flight(cwq, get_work_color(work));
			ret = 1;
		}
	}
	spin_unlock_irq(&cwq->lock);

	return ret;
}

static void wait_on_cpu_work(struct cpu_workqueue_struct *cwq,
				struct work_struct *work)
{
	struct wq_barrier barr;
	int running = 0;

	spin_lock_irq(&cwq->lock);
	if (unlikely(cwq->worker && cwq->worker->current_work == work)) {
		insert_wq_barrier(cwq, &barr, cwq->worklist.next);
		running = 1;
	}
	spin_unlock_irq(&cwq->lock);

	if (unlikely(running)) {
		wait_for_completion(&barr.done);
		destroy_work_on_stack(&barr.work);
	}
}

static void wait_on_work(struct work_struct *work)
{
	struct cpu_workqueue_struct *cwq;
	struct workqueue_struct *wq;
	int cpu;

	might_sleep();

	lock_map_acquire(&work->lockdep_map);
	lock_map_release(&work->lockdep_map);

	cwq = get_wq_data(work);
	if (!cwq)
		return;

	wq = cwq->wq;

	for_each_possible_cpu(cpu)
		wait_on_cpu_work(get_cwq(cpu, wq), work);
}

static int __cancel_work_timer(struct work_struct *work,
				struct timer_list* timer)
{
	int ret;

	do {
		ret = (timer && likely(del_timer(timer)));
		if (!ret)
			ret = try_to_grab_pending(work);
		wait_on_work(work);
	} while (unlikely(ret < 0));

	clear_wq_data(work);
	return ret;
}

/**
 * cancel_work_sync - block until a work_struct's callback has terminated
 * @work: the work which is to be flushed
 *
 * Returns true if @work was pending.
 *
 * cancel_work_sync() will cancel the work if it is queued. If the work's
 * callback appears to be running, cancel_work_sync() will block until it
 * has completed.
 *
 * It is possible to use this function if the work re-queues itself. It can
 * cancel the work even if it migrates to another workqueue, however in that
 * case it only guarantees that work->func() has completed on the last queued
 * workqueue.
 *
 * cancel_work_sync(&delayed_work->work) should be used only if ->timer is not
 * pending, otherwise it goes into a busy-wait loop until the timer expires.
 *
 * The caller must ensure that workqueue_struct on which this work was last
 * queued can't be destroyed before this function returns.
 */
int cancel_work_sync(struct work_struct *work)
{
	return __cancel_work_timer(work, NULL);
}
EXPORT_SYMBOL_GPL(cancel_work_sync);

/**
 * cancel_delayed_work_sync - reliably kill off a delayed work.
 * @dwork: the delayed work struct
 *
 * Returns true if @dwork was pending.
 *
 * It is possible to use this function if @dwork rearms itself via queue_work()
 * or queue_delayed_work(). See also the comment for cancel_work_sync().
 */
int cancel_delayed_work_sync(struct delayed_work *dwork)
{
	return __cancel_work_timer(&dwork->work, &dwork->timer);
}
EXPORT_SYMBOL(cancel_delayed_work_sync);

static struct workqueue_struct *keventd_wq __read_mostly;

/**
 * schedule_work - put work task in global workqueue
 * @work: job to be done
 *
 * Returns zero if @work was already on the kernel-global workqueue and
 * non-zero otherwise.
 *
 * This puts a job in the kernel-global workqueue if it was not already
 * queued and leaves it in the same position on the kernel-global
 * workqueue otherwise.
 */
int schedule_work(struct work_struct *work)
{
	return queue_work(keventd_wq, work);
}
EXPORT_SYMBOL(schedule_work);

/*
 * schedule_work_on - put work task on a specific cpu
 * @cpu: cpu to put the work task on
 * @work: job to be done
 *
 * This puts a job on a specific cpu
 */
int schedule_work_on(int cpu, struct work_struct *work)
{
	return queue_work_on(cpu, keventd_wq, work);
}
EXPORT_SYMBOL(schedule_work_on);

/**
 * schedule_delayed_work - put work task in global workqueue after delay
 * @dwork: job to be done
 * @delay: number of jiffies to wait or 0 for immediate execution
 *
 * After waiting for a given time this puts a job in the kernel-global
 * workqueue.
 */
int schedule_delayed_work(struct delayed_work *dwork,
					unsigned long delay)
{
	return queue_delayed_work(keventd_wq, dwork, delay);
}
EXPORT_SYMBOL(schedule_delayed_work);

/**
 * flush_delayed_work - block until a dwork_struct's callback has terminated
 * @dwork: the delayed work which is to be flushed
 *
 * Any timeout is cancelled, and any pending work is run immediately.
 */
void flush_delayed_work(struct delayed_work *dwork)
{
	if (del_timer_sync(&dwork->timer)) {
		__queue_work(get_cpu(), get_wq_data(&dwork->work)->wq,
			     &dwork->work);
		put_cpu();
	}
	flush_work(&dwork->work);
}
EXPORT_SYMBOL(flush_delayed_work);

/**
 * schedule_delayed_work_on - queue work in global workqueue on CPU after delay
 * @cpu: cpu to use
 * @dwork: job to be done
 * @delay: number of jiffies to wait
 *
 * After waiting for a given time this puts a job in the kernel-global
 * workqueue on the specified CPU.
 */
int schedule_delayed_work_on(int cpu,
			struct delayed_work *dwork, unsigned long delay)
{
	return queue_delayed_work_on(cpu, keventd_wq, dwork, delay);
}
EXPORT_SYMBOL(schedule_delayed_work_on);

/**
 * schedule_on_each_cpu - call a function on each online CPU from keventd
 * @func: the function to call
 *
 * Returns zero on success.
 * Returns -ve errno on failure.
 *
 * schedule_on_each_cpu() is very slow.
 */
int schedule_on_each_cpu(work_func_t func)
{
	int cpu;
	int orig = -1;
	struct work_struct *works;

	works = alloc_percpu(struct work_struct);
	if (!works)
		return -ENOMEM;

	get_online_cpus();

	/*
	 * When running in keventd don't schedule a work item on
	 * itself.  Can just call directly because the work queue is
	 * already bound.  This also is faster.
	 */
	if (current_is_keventd())
		orig = raw_smp_processor_id();

	for_each_online_cpu(cpu) {
		struct work_struct *work = per_cpu_ptr(works, cpu);

		INIT_WORK(work, func);
		if (cpu != orig)
			schedule_work_on(cpu, work);
	}
	if (orig >= 0)
		func(per_cpu_ptr(works, orig));

	for_each_online_cpu(cpu)
		flush_work(per_cpu_ptr(works, cpu));

	put_online_cpus();
	free_percpu(works);
	return 0;
}

/**
 * flush_scheduled_work - ensure that any scheduled work has run to completion.
 *
 * Forces execution of the kernel-global workqueue and blocks until its
 * completion.
 *
 * Think twice before calling this function!  It's very easy to get into
 * trouble if you don't take great care.  Either of the following situations
 * will lead to deadlock:
 *
 *	One of the work items currently on the workqueue needs to acquire
 *	a lock held by your code or its caller.
 *
 *	Your code is running in the context of a work routine.
 *
 * They will be detected by lockdep when they occur, but the first might not
 * occur very often.  It depends on what work items are on the workqueue and
 * what locks they need, which you have no control over.
 *
 * In most situations flushing the entire workqueue is overkill; you merely
 * need to know that a particular work item isn't queued and isn't running.
 * In such cases you should use cancel_delayed_work_sync() or
 * cancel_work_sync() instead.
 */
void flush_scheduled_work(void)
{
	flush_workqueue(keventd_wq);
}
EXPORT_SYMBOL(flush_scheduled_work);

/**
 * execute_in_process_context - reliably execute the routine with user context
 * @fn:		the function to execute
 * @ew:		guaranteed storage for the execute work structure (must
 *		be available when the work executes)
 *
 * Executes the function immediately if process context is available,
 * otherwise schedules the function for delayed execution.
 *
 * Returns:	0 - function was executed
 *		1 - function was scheduled for execution
 */
int execute_in_process_context(work_func_t fn, struct execute_work *ew)
{
	if (!in_interrupt()) {
		fn(&ew->work);
		return 0;
	}

	INIT_WORK(&ew->work, fn);
	schedule_work(&ew->work);

	return 1;
}
EXPORT_SYMBOL_GPL(execute_in_process_context);

int keventd_up(void)
{
	return keventd_wq != NULL;
}

int current_is_keventd(void)
{
	struct cpu_workqueue_struct *cwq;
	int cpu = raw_smp_processor_id(); /* preempt-safe: keventd is per-cpu */
	int ret = 0;

	BUG_ON(!keventd_wq);

	cwq = get_cwq(cpu, keventd_wq);
	if (current == cwq->worker->task)
		ret = 1;

	return ret;

}

static struct cpu_workqueue_struct *alloc_cwqs(void)
{
	/*
	 * cwqs are forced aligned according to WORK_STRUCT_FLAG_BITS.
	 * Make sure that the alignment isn't lower than that of
	 * unsigned long long.
	 */
	const size_t size = sizeof(struct cpu_workqueue_struct);
	const size_t align = max_t(size_t, 1 << WORK_STRUCT_FLAG_BITS,
				   __alignof__(unsigned long long));
	struct cpu_workqueue_struct *cwqs;
#ifndef CONFIG_SMP
	void *ptr;

	/*
	 * On UP, percpu allocator doesn't honor alignment parameter
	 * and simply uses arch-dependent default.  Allocate enough
	 * room to align cwq and put an extra pointer at the end
	 * pointing back to the originally allocated pointer which
	 * will be used for free.
	 *
	 * FIXME: This really belongs to UP percpu code.  Update UP
	 * percpu code to honor alignment and remove this ugliness.
	 */
	ptr = __alloc_percpu(size + align + sizeof(void *), 1);
	cwqs = PTR_ALIGN(ptr, align);
	*(void **)per_cpu_ptr(cwqs + 1, 0) = ptr;
#else
	/* On SMP, percpu allocator can do it itself */
	cwqs = __alloc_percpu(size, align);
#endif
	/* just in case, make sure it's actually aligned */
	BUG_ON(!IS_ALIGNED((unsigned long)cwqs, align));
	return cwqs;
}

static void free_cwqs(struct cpu_workqueue_struct *cwqs)
{
#ifndef CONFIG_SMP
	/* on UP, the pointer to free is stored right after the cwq */
	if (cwqs)
		free_percpu(*(void **)per_cpu_ptr(cwqs + 1, 0));
#else
	free_percpu(cwqs);
#endif
}

struct workqueue_struct *__create_workqueue_key(const char *name,
						unsigned int flags,
						struct lock_class_key *key,
						const char *lock_name)
{
	bool singlethread = flags & WQ_SINGLE_THREAD;
	struct workqueue_struct *wq;
	bool failed = false;
	unsigned int cpu;

	wq = kzalloc(sizeof(*wq), GFP_KERNEL);
	if (!wq)
		goto err;

	wq->cpu_wq = alloc_cwqs();
	if (!wq->cpu_wq)
		goto err;

	wq->flags = flags;
	mutex_init(&wq->flush_mutex);
	atomic_set(&wq->nr_cwqs_to_flush, 0);
	INIT_LIST_HEAD(&wq->flusher_queue);
	INIT_LIST_HEAD(&wq->flusher_overflow);
	wq->name = name;
	lockdep_init_map(&wq->lockdep_map, lock_name, key, 0);
	INIT_LIST_HEAD(&wq->list);

	cpu_maps_update_begin();
	/*
	 * We must initialize cwqs for each possible cpu even if we
	 * are going to call destroy_workqueue() finally. Otherwise
	 * cpu_up() can hit the uninitialized cwq once we drop the
	 * lock.
	 */
	for_each_possible_cpu(cpu) {
		struct cpu_workqueue_struct *cwq = get_cwq(cpu, wq);

		BUG_ON((unsigned long)cwq & WORK_STRUCT_FLAG_MASK);
		cwq->cpu = cpu;
		cwq->wq = wq;
		cwq->flush_color = -1;
		spin_lock_init(&cwq->lock);
		INIT_LIST_HEAD(&cwq->worklist);
		init_waitqueue_head(&cwq->more_work);

		if (failed)
			continue;
		cwq->worker = create_worker(cwq,
					    cpu_online(cpu) && !singlethread);
		if (cwq->worker)
			start_worker(cwq->worker);
		else
			failed = true;
	}

	spin_lock(&workqueue_lock);
	list_add(&wq->list, &workqueues);
	spin_unlock(&workqueue_lock);

	cpu_maps_update_done();

	if (failed) {
		destroy_workqueue(wq);
		wq = NULL;
	}
	return wq;
err:
	if (wq) {
		free_cwqs(wq->cpu_wq);
		kfree(wq);
	}
	return NULL;
}
EXPORT_SYMBOL_GPL(__create_workqueue_key);

/**
 * destroy_workqueue - safely terminate a workqueue
 * @wq: target workqueue
 *
 * Safely destroy a workqueue. All work currently pending will be done first.
 */
void destroy_workqueue(struct workqueue_struct *wq)
{
	int cpu;

	cpu_maps_update_begin();
	spin_lock(&workqueue_lock);
	list_del(&wq->list);
	spin_unlock(&workqueue_lock);
	cpu_maps_update_done();

	flush_workqueue(wq);

	for_each_possible_cpu(cpu) {
		struct cpu_workqueue_struct *cwq = get_cwq(cpu, wq);
		int i;

		if (cwq->worker) {
			destroy_worker(cwq->worker);
			cwq->worker = NULL;
		}

		for (i = 0; i < WORK_NR_COLORS; i++)
			BUG_ON(cwq->nr_in_flight[i]);
	}

	free_cwqs(wq->cpu_wq);
	kfree(wq);
}
EXPORT_SYMBOL_GPL(destroy_workqueue);

static int __devinit workqueue_cpu_callback(struct notifier_block *nfb,
						unsigned long action,
						void *hcpu)
{
	unsigned int cpu = (unsigned long)hcpu;
	struct cpu_workqueue_struct *cwq;
	struct workqueue_struct *wq;

	action &= ~CPU_TASKS_FROZEN;

	list_for_each_entry(wq, &workqueues, list) {
		if (wq->flags & WQ_SINGLE_THREAD)
			continue;

		cwq = get_cwq(cpu, wq);

		switch (action) {
		case CPU_POST_DEAD:
			flush_workqueue(wq);
			break;
		}
	}

	return notifier_from_errno(0);
}

#ifdef CONFIG_SMP

struct work_for_cpu {
	struct completion completion;
	long (*fn)(void *);
	void *arg;
	long ret;
};

static int do_work_for_cpu(void *_wfc)
{
	struct work_for_cpu *wfc = _wfc;
	wfc->ret = wfc->fn(wfc->arg);
	complete(&wfc->completion);
	return 0;
}

/**
 * work_on_cpu - run a function in user context on a particular cpu
 * @cpu: the cpu to run on
 * @fn: the function to run
 * @arg: the function arg
 *
 * This will return the value @fn returns.
 * It is up to the caller to ensure that the cpu doesn't go offline.
 * The caller must not hold any locks which would prevent @fn from completing.
 */
long work_on_cpu(unsigned int cpu, long (*fn)(void *), void *arg)
{
	struct task_struct *sub_thread;
	struct work_for_cpu wfc = {
		.completion = COMPLETION_INITIALIZER_ONSTACK(wfc.completion),
		.fn = fn,
		.arg = arg,
	};

	sub_thread = kthread_create(do_work_for_cpu, &wfc, "work_for_cpu");
	if (IS_ERR(sub_thread))
		return PTR_ERR(sub_thread);
	kthread_bind(sub_thread, cpu);
	wake_up_process(sub_thread);
	wait_for_completion(&wfc.completion);
	return wfc.ret;
}
EXPORT_SYMBOL_GPL(work_on_cpu);
#endif /* CONFIG_SMP */

void __init init_workqueues(void)
{
	unsigned int cpu;

	for_each_possible_cpu(cpu)
		ida_init(&per_cpu(worker_ida, cpu));

	singlethread_cpu = cpumask_first(cpu_possible_mask);
	hotcpu_notifier(workqueue_cpu_callback, 0);
	keventd_wq = create_workqueue("events");
	BUG_ON(!keventd_wq);
}
