/*
 *  linux/mm/oom_kill.c
 * 
 *  Copyright (C)  1998,2000  Rik van Riel
 *	Thanks go out to Claus Fischer for some serious inspiration and
 *	for goading me into coding this file...
 *
 *  The routines in this file are used to kill a process when
 *  we're seriously out of memory. This gets called from __alloc_pages()
 *  in mm/page_alloc.c when we really run out of memory.
 *
 *  Since we won't call these routines often (on a well-configured
 *  machine) this file will double as a 'coding guide' and a signpost
 *  for newbie kernel hackers. It features several pointers to major
 *  kernel subsystems and hints as to where to find out what things do.
 */

#include <linux/oom.h>
#include <linux/mm.h>
#include <linux/err.h>
#include <linux/gfp.h>
#include <linux/sched.h>
#include <linux/swap.h>
#include <linux/timex.h>
#include <linux/jiffies.h>
#include <linux/cpuset.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/memcontrol.h>
#include <linux/mempolicy.h>
#include <linux/security.h>

int sysctl_panic_on_oom;
int sysctl_oom_kill_allocating_task;
int sysctl_oom_dump_tasks = 1;
static DEFINE_SPINLOCK(zone_scan_lock);
/* #define DEBUG */

#ifdef CONFIG_NUMA
/**
 * has_intersects_mems_allowed() - check task eligiblity for kill
 * @tsk: task struct of which task to consider
 * @mask: nodemask passed to page allocator for mempolicy ooms
 *
 * Task eligibility is determined by whether or not a candidate task, @tsk,
 * shares the same mempolicy nodes as current if it is bound by such a policy
 * and whether or not it has the same set of allowed cpuset nodes.
 */
static bool has_intersects_mems_allowed(struct task_struct *tsk,
					const nodemask_t *mask)
{
	struct task_struct *start = tsk;

	do {
		if (mask) {
			/*
			 * If this is a mempolicy constrained oom, tsk's
			 * cpuset is irrelevant.  Only return true if its
			 * mempolicy intersects current, otherwise it may be
			 * needlessly killed.
			 */
			if (mempolicy_nodemask_intersects(tsk, mask))
				return true;
		} else {
			/*
			 * This is not a mempolicy constrained oom, so only
			 * check the mems of tsk's cpuset.
			 */
			if (cpuset_mems_allowed_intersects(current, tsk))
				return true;
		}
		tsk = next_thread(tsk);
	} while (tsk != start);
	return false;
}
#else
static bool has_intersects_mems_allowed(struct task_struct *tsk,
					const nodemask_t *mask)
{
	return true;
}
#endif /* CONFIG_NUMA */

/*
 * The process p may have detached its own ->mm while exiting or through
 * use_mm(), but one or more of its subthreads may still have a valid
 * pointer.  Return p, or any of its subthreads with a valid ->mm, with
 * task_lock() held.
 */
static struct task_struct *find_lock_task_mm(struct task_struct *p)
{
	struct task_struct *t = p;

	do {
		task_lock(t);
		if (likely(t->mm))
			return t;
		task_unlock(t);
	} while_each_thread(p, t);

	return NULL;
}

/**
 * badness - calculate a numeric value for how bad this task has been
 * @p: task struct of which task we should calculate
 * @uptime: current uptime in seconds
 *
 * The formula used is relatively simple and documented inline in the
 * function. The main rationale is that we want to select a good task
 * to kill when we run out of memory.
 *
 * Good in this context means that:
 * 1) we lose the minimum amount of work done
 * 2) we recover a large amount of memory
 * 3) we don't kill anything innocent of eating tons of memory
 * 4) we want to kill the minimum amount of processes (one)
 * 5) we try to kill the process the user expects us to kill, this
 *    algorithm has been meticulously tuned to meet the principle
 *    of least surprise ... (be careful when you change it)
 */

unsigned long badness(struct task_struct *p, unsigned long uptime)
{
	unsigned long points, cpu_time, run_time;
	struct task_struct *child;
	struct task_struct *c, *t;
	int oom_adj = p->signal->oom_adj;
	struct task_cputime task_time;
	unsigned long utime;
	unsigned long stime;

	if (oom_adj == OOM_DISABLE)
		return 0;

	p = find_lock_task_mm(p);
	if (!p)
		return 0;

	/*
	 * The memory size of the process is the basis for the badness.
	 */
	points = p->mm->total_vm;
	task_unlock(p);

	/*
	 * swapoff can easily use up all memory, so kill those first.
	 */
	if (p->flags & PF_OOM_ORIGIN)
		return ULONG_MAX;

	/*
	 * Processes which fork a lot of child processes are likely
	 * a good choice. We add half the vmsize of the children if they
	 * have an own mm. This prevents forking servers to flood the
	 * machine with an endless amount of children. In case a single
	 * child is eating the vast majority of memory, adding only half
	 * to the parents will make the child our kill candidate of choice.
	 */
	t = p;
	do {
		list_for_each_entry(c, &t->children, sibling) {
			child = find_lock_task_mm(c);
			if (child) {
				if (child->mm != p->mm)
					points += child->mm->total_vm/2 + 1;
				task_unlock(child);
			}
		}
	} while_each_thread(p, t);

	/*
	 * CPU time is in tens of seconds and run time is in thousands
         * of seconds. There is no particular reason for this other than
         * that it turned out to work very well in practice.
	 */
	thread_group_cputime(p, &task_time);
	utime = cputime_to_jiffies(task_time.utime);
	stime = cputime_to_jiffies(task_time.stime);
	cpu_time = (utime + stime) >> (SHIFT_HZ + 3);


	if (uptime >= p->start_time.tv_sec)
		run_time = (uptime - p->start_time.tv_sec) >> 10;
	else
		run_time = 0;

	if (cpu_time)
		points /= int_sqrt(cpu_time);
	if (run_time)
		points /= int_sqrt(int_sqrt(run_time));

	/*
	 * Niced processes are most likely less important, so double
	 * their badness points.
	 */
	if (task_nice(p) > 0)
		points *= 2;

	/*
	 * Superuser processes are usually more important, so we make it
	 * less likely that we kill those.
	 */
	if (has_capability_noaudit(p, CAP_SYS_ADMIN) ||
	    has_capability_noaudit(p, CAP_SYS_RESOURCE))
		points /= 4;

	/*
	 * We don't want to kill a process with direct hardware access.
	 * Not only could that mess up the hardware, but usually users
	 * tend to only have this flag set on applications they think
	 * of as important.
	 */
	if (has_capability_noaudit(p, CAP_SYS_RAWIO))
		points /= 4;

	/*
	 * Adjust the score by oom_adj.
	 */
	if (oom_adj) {
		if (oom_adj > 0) {
			if (!points)
				points = 1;
			points <<= oom_adj;
		} else
			points >>= -(oom_adj);
	}

#ifdef DEBUG
	printk(KERN_DEBUG "OOMkill: task %d (%s) got %lu points\n",
	p->pid, p->comm, points);
#endif
	return points;
}

/*
 * Determine the type of allocation constraint.
 */
#ifdef CONFIG_NUMA
static enum oom_constraint constrained_alloc(struct zonelist *zonelist,
				    gfp_t gfp_mask, nodemask_t *nodemask)
{
	struct zone *zone;
	struct zoneref *z;
	enum zone_type high_zoneidx = gfp_zone(gfp_mask);

	/*
	 * Reach here only when __GFP_NOFAIL is used. So, we should avoid
	 * to kill current.We have to random task kill in this case.
	 * Hopefully, CONSTRAINT_THISNODE...but no way to handle it, now.
	 */
	if (gfp_mask & __GFP_THISNODE)
		return CONSTRAINT_NONE;

	/*
	 * The nodemask here is a nodemask passed to alloc_pages(). Now,
	 * cpuset doesn't use this nodemask for its hardwall/softwall/hierarchy
	 * feature. mempolicy is an only user of nodemask here.
	 * check mempolicy's nodemask contains all N_HIGH_MEMORY
	 */
	if (nodemask && !nodes_subset(node_states[N_HIGH_MEMORY], *nodemask))
		return CONSTRAINT_MEMORY_POLICY;

	/* Check this allocation failure is caused by cpuset's wall function */
	for_each_zone_zonelist_nodemask(zone, z, zonelist,
			high_zoneidx, nodemask)
		if (!cpuset_zone_allowed_softwall(zone, gfp_mask))
			return CONSTRAINT_CPUSET;

	return CONSTRAINT_NONE;
}
#else
static enum oom_constraint constrained_alloc(struct zonelist *zonelist,
				gfp_t gfp_mask, nodemask_t *nodemask)
{
	return CONSTRAINT_NONE;
}
#endif

/*
 * Simple selection loop. We chose the process with the highest
 * number of 'points'. We expect the caller will lock the tasklist.
 *
 * (not docbooked, we don't want this one cluttering up the manual)
 */
static struct task_struct *select_bad_process(unsigned long *ppoints,
		struct mem_cgroup *mem, const nodemask_t *nodemask)
{
	struct task_struct *p;
	struct task_struct *chosen = NULL;
	struct timespec uptime;
	*ppoints = 0;

	do_posix_clock_monotonic_gettime(&uptime);
	for_each_process(p) {
		unsigned long points;

		/* skip the init task and kthreads */
		if (is_global_init(p) || (p->flags & PF_KTHREAD))
			continue;
		if (mem && !task_in_mem_cgroup(p, mem))
			continue;
		if (!has_intersects_mems_allowed(p, nodemask))
			continue;

		/*
		 * This task already has access to memory reserves and is
		 * being killed. Don't allow any other task access to the
		 * memory reserve.
		 *
		 * Note: this may have a chance of deadlock if it gets
		 * blocked waiting for another task which itself is waiting
		 * for memory. Is there a better alternative?
		 */
		if (test_tsk_thread_flag(p, TIF_MEMDIE))
			return ERR_PTR(-1UL);

		/*
		 * This is in the process of releasing memory so wait for it
		 * to finish before killing some other task by mistake.
		 *
		 * However, if p is the current task, we allow the 'kill' to
		 * go ahead if it is exiting: this will simply set TIF_MEMDIE,
		 * which will allow it to gain access to memory reserves in
		 * the process of exiting and releasing its resources.
		 * Otherwise we could get an easy OOM deadlock.
		 */
		if ((p->flags & PF_EXITING) && p->mm) {
			if (p != current)
				return ERR_PTR(-1UL);

			chosen = p;
			*ppoints = ULONG_MAX;
		}

		if (p->signal->oom_adj == OOM_DISABLE)
			continue;

		points = badness(p, uptime.tv_sec);
		if (points > *ppoints || !chosen) {
			chosen = p;
			*ppoints = points;
		}
	}

	return chosen;
}

/**
 * dump_tasks - dump current memory state of all system tasks
 * @mem: current's memory controller, if constrained
 *
 * Dumps the current memory state of all system tasks, excluding kernel threads.
 * State information includes task's pid, uid, tgid, vm size, rss, cpu, oom_adj
 * score, and name.
 *
 * If the actual is non-NULL, only tasks that are a member of the mem_cgroup are
 * shown.
 *
 * Call with tasklist_lock read-locked.
 */
static void dump_tasks(const struct mem_cgroup *mem)
{
	struct task_struct *p;
	struct task_struct *task;

	printk(KERN_INFO "[ pid ]   uid  tgid total_vm      rss cpu oom_adj "
	       "name\n");
	for_each_process(p) {
		if (p->flags & PF_KTHREAD)
			continue;
		if (mem && !task_in_mem_cgroup(p, mem))
			continue;

		task = find_lock_task_mm(p);
		if (!task) {
			/*
			 * This is a kthread or all of p's threads have already
			 * detached their mm's.  There's no need to report
			 * them; they can't be oom killed anyway.
			 */
			continue;
		}

		printk(KERN_INFO "[%5d] %5d %5d %8lu %8lu %3u     %3d %s\n",
		       task->pid, __task_cred(task)->uid, task->tgid,
		       task->mm->total_vm, get_mm_rss(task->mm),
		       task_cpu(task), task->signal->oom_adj, task->comm);
		task_unlock(task);
	}
}

static void dump_header(struct task_struct *p, gfp_t gfp_mask, int order,
							struct mem_cgroup *mem)
{
	task_lock(current);
	pr_warning("%s invoked oom-killer: gfp_mask=0x%x, order=%d, "
		"oom_adj=%d\n",
		current->comm, gfp_mask, order, current->signal->oom_adj);
	cpuset_print_task_mems_allowed(current);
	task_unlock(current);
	dump_stack();
	mem_cgroup_print_oom_info(mem, p);
	show_mem();
	if (sysctl_oom_dump_tasks)
		dump_tasks(mem);
}

#define K(x) ((x) << (PAGE_SHIFT-10))
static int oom_kill_task(struct task_struct *p)
{
	p = find_lock_task_mm(p);
	if (!p || p->signal->oom_adj == OOM_DISABLE) {
		task_unlock(p);
		return 1;
	}
	pr_err("Killed process %d (%s) total-vm:%lukB, anon-rss:%lukB, file-rss:%lukB\n",
		task_pid_nr(p), p->comm, K(p->mm->total_vm),
		K(get_mm_counter(p->mm, MM_ANONPAGES)),
		K(get_mm_counter(p->mm, MM_FILEPAGES)));
	task_unlock(p);

	p->rt.time_slice = HZ;
	set_tsk_thread_flag(p, TIF_MEMDIE);
	force_sig(SIGKILL, p);
	return 0;
}
#undef K

static int oom_kill_process(struct task_struct *p, gfp_t gfp_mask, int order,
			    unsigned long points, struct mem_cgroup *mem,
			    nodemask_t *nodemask, const char *message)
{
	struct task_struct *victim = p;
	struct task_struct *child;
	struct task_struct *t = p;
	unsigned long victim_points = 0;
	struct timespec uptime;

	if (printk_ratelimit())
		dump_header(p, gfp_mask, order, mem);

	/*
	 * If the task is already exiting, don't alarm the sysadmin or kill
	 * its children or threads, just set TIF_MEMDIE so it can die quickly
	 */
	if (p->flags & PF_EXITING) {
		set_tsk_thread_flag(p, TIF_MEMDIE);
		return 0;
	}

	task_lock(p);
	pr_err("%s: Kill process %d (%s) score %lu or sacrifice child\n",
		message, task_pid_nr(p), p->comm, points);
	task_unlock(p);

	/*
	 * If any of p's children has a different mm and is eligible for kill,
	 * the one with the highest badness() score is sacrificed for its
	 * parent.  This attempts to lose the minimal amount of work done while
	 * still freeing memory.
	 */
	do_posix_clock_monotonic_gettime(&uptime);
	do {
		list_for_each_entry(child, &t->children, sibling) {
			unsigned long child_points;

			if (child->mm == p->mm)
				continue;
			if (mem && !task_in_mem_cgroup(child, mem))
				continue;
			if (!has_intersects_mems_allowed(child, nodemask))
				continue;

			/* badness() returns 0 if the thread is unkillable */
			child_points = badness(child, uptime.tv_sec);
			if (child_points > victim_points) {
				victim = child;
				victim_points = child_points;
			}
		}
	} while_each_thread(p, t);

	return oom_kill_task(victim);
}

/*
 * Determines whether the kernel must panic because of the panic_on_oom sysctl.
 */
static void check_panic_on_oom(enum oom_constraint constraint, gfp_t gfp_mask,
				int order)
{
	if (likely(!sysctl_panic_on_oom))
		return;
	if (sysctl_panic_on_oom != 2) {
		/*
		 * panic_on_oom == 1 only affects CONSTRAINT_NONE, the kernel
		 * does not panic for cpuset, mempolicy, or memcg allocation
		 * failures.
		 */
		if (constraint != CONSTRAINT_NONE)
			return;
	}
	read_lock(&tasklist_lock);
	dump_header(NULL, gfp_mask, order, NULL);
	read_unlock(&tasklist_lock);
	panic("Out of memory: %s panic_on_oom is enabled\n",
		sysctl_panic_on_oom == 2 ? "compulsory" : "system-wide");
}

#ifdef CONFIG_CGROUP_MEM_RES_CTLR
void mem_cgroup_out_of_memory(struct mem_cgroup *mem, gfp_t gfp_mask)
{
	unsigned long points = 0;
	struct task_struct *p;

	check_panic_on_oom(CONSTRAINT_MEMCG, gfp_mask, 0);
	read_lock(&tasklist_lock);
retry:
	p = select_bad_process(&points, mem, NULL);
	if (!p || PTR_ERR(p) == -1UL)
		goto out;

	if (oom_kill_process(p, gfp_mask, 0, points, mem, NULL,
				"Memory cgroup out of memory"))
		goto retry;
out:
	read_unlock(&tasklist_lock);
}
#endif

static BLOCKING_NOTIFIER_HEAD(oom_notify_list);

int register_oom_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&oom_notify_list, nb);
}
EXPORT_SYMBOL_GPL(register_oom_notifier);

int unregister_oom_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&oom_notify_list, nb);
}
EXPORT_SYMBOL_GPL(unregister_oom_notifier);

/*
 * Try to acquire the OOM killer lock for the zones in zonelist.  Returns zero
 * if a parallel OOM killing is already taking place that includes a zone in
 * the zonelist.  Otherwise, locks all zones in the zonelist and returns 1.
 */
int try_set_zonelist_oom(struct zonelist *zonelist, gfp_t gfp_mask)
{
	struct zoneref *z;
	struct zone *zone;
	int ret = 1;

	spin_lock(&zone_scan_lock);
	for_each_zone_zonelist(zone, z, zonelist, gfp_zone(gfp_mask)) {
		if (zone_is_oom_locked(zone)) {
			ret = 0;
			goto out;
		}
	}

	for_each_zone_zonelist(zone, z, zonelist, gfp_zone(gfp_mask)) {
		/*
		 * Lock each zone in the zonelist under zone_scan_lock so a
		 * parallel invocation of try_set_zonelist_oom() doesn't succeed
		 * when it shouldn't.
		 */
		zone_set_flag(zone, ZONE_OOM_LOCKED);
	}

out:
	spin_unlock(&zone_scan_lock);
	return ret;
}

/*
 * Clears the ZONE_OOM_LOCKED flag for all zones in the zonelist so that failed
 * allocation attempts with zonelists containing them may now recall the OOM
 * killer, if necessary.
 */
void clear_zonelist_oom(struct zonelist *zonelist, gfp_t gfp_mask)
{
	struct zoneref *z;
	struct zone *zone;

	spin_lock(&zone_scan_lock);
	for_each_zone_zonelist(zone, z, zonelist, gfp_zone(gfp_mask)) {
		zone_clear_flag(zone, ZONE_OOM_LOCKED);
	}
	spin_unlock(&zone_scan_lock);
}

/*
 * Try to acquire the oom killer lock for all system zones.  Returns zero if a
 * parallel oom killing is taking place, otherwise locks all zones and returns
 * non-zero.
 */
static int try_set_system_oom(void)
{
	struct zone *zone;
	int ret = 1;

	spin_lock(&zone_scan_lock);
	for_each_populated_zone(zone)
		if (zone_is_oom_locked(zone)) {
			ret = 0;
			goto out;
		}
	for_each_populated_zone(zone)
		zone_set_flag(zone, ZONE_OOM_LOCKED);
out:
	spin_unlock(&zone_scan_lock);
	return ret;
}

/*
 * Clears ZONE_OOM_LOCKED for all system zones so that failed allocation
 * attempts or page faults may now recall the oom killer, if necessary.
 */
static void clear_system_oom(void)
{
	struct zone *zone;

	spin_lock(&zone_scan_lock);
	for_each_populated_zone(zone)
		zone_clear_flag(zone, ZONE_OOM_LOCKED);
	spin_unlock(&zone_scan_lock);
}

/**
 * out_of_memory - kill the "best" process when we run out of memory
 * @zonelist: zonelist pointer
 * @gfp_mask: memory allocation flags
 * @order: amount of memory being requested as a power of 2
 * @nodemask: nodemask passed to page allocator
 *
 * If we run out of memory, we have the choice between either
 * killing a random task (bad), letting the system crash (worse)
 * OR try to be smart about which process to kill. Note that we
 * don't have to be perfect here, we just have to be good.
 */
void out_of_memory(struct zonelist *zonelist, gfp_t gfp_mask,
		int order, nodemask_t *nodemask)
{
	struct task_struct *p;
	unsigned long freed = 0;
	unsigned long points;
	enum oom_constraint constraint = CONSTRAINT_NONE;

	blocking_notifier_call_chain(&oom_notify_list, 0, &freed);
	if (freed > 0)
		/* Got some memory back in the last second. */
		return;

	/*
	 * If current has a pending SIGKILL, then automatically select it.  The
	 * goal is to allow it to allocate so that it may quickly exit and free
	 * its memory.
	 */
	if (fatal_signal_pending(current)) {
		set_thread_flag(TIF_MEMDIE);
		return;
	}

	/*
	 * Check if there were limitations on the allocation (only relevant for
	 * NUMA) that may require different handling.
	 */
	if (zonelist)
		constraint = constrained_alloc(zonelist, gfp_mask, nodemask);
	check_panic_on_oom(constraint, gfp_mask, order);

	read_lock(&tasklist_lock);
	if (sysctl_oom_kill_allocating_task) {
		/*
		 * oom_kill_process() needs tasklist_lock held.  If it returns
		 * non-zero, current could not be killed so we must fallback to
		 * the tasklist scan.
		 */
		if (!oom_kill_process(current, gfp_mask, order, 0, NULL,
				nodemask,
				"Out of memory (oom_kill_allocating_task)"))
			return;
	}

retry:
	p = select_bad_process(&points, NULL,
			constraint == CONSTRAINT_MEMORY_POLICY ? nodemask :
								 NULL);
	if (PTR_ERR(p) == -1UL)
		return;

	/* Found nothing?!?! Either we hang forever, or we panic. */
	if (!p) {
		dump_header(NULL, gfp_mask, order, NULL);
		read_unlock(&tasklist_lock);
		panic("Out of memory and no killable processes...\n");
	}

	if (oom_kill_process(p, gfp_mask, order, points, NULL, nodemask,
			     "Out of memory"))
		goto retry;
	read_unlock(&tasklist_lock);

	/*
	 * Give "p" a good chance of killing itself before we
	 * retry to allocate memory unless "p" is current
	 */
	if (!test_thread_flag(TIF_MEMDIE))
		schedule_timeout_uninterruptible(1);
}

/*
 * The pagefault handler calls here because it is out of memory, so kill a
 * memory-hogging task.  If a populated zone has ZONE_OOM_LOCKED set, a parallel
 * oom killing is already in progress so do nothing.  If a task is found with
 * TIF_MEMDIE set, it has been killed so do nothing and allow it to exit.
 */
void pagefault_out_of_memory(void)
{
	if (try_set_system_oom()) {
		out_of_memory(NULL, 0, 0, NULL);
		clear_system_oom();
	}
	if (!test_thread_flag(TIF_MEMDIE))
		schedule_timeout_uninterruptible(1);
}
