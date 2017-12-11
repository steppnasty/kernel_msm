#include <linux/fcntl.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/anon_inodes.h>
#include <linux/fsnotify_backend.h>
#include <linux/init.h>
#include <linux/namei.h>
#include <linux/security.h>
#include <linux/syscalls.h>
#include <linux/types.h>

#include "fanotify.h"

static struct kmem_cache *fanotify_mark_cache __read_mostly;

static int fanotify_release(struct inode *ignored, struct file *file)
{
	struct fsnotify_group *group = file->private_data;

	pr_debug("%s: file=%p group=%p\n", __func__, file, group);

	/* matches the fanotify_init->fsnotify_alloc_group */
	fsnotify_put_group(group);

	return 0;
}

static const struct file_operations fanotify_fops = {
	.poll		= NULL,
	.read		= NULL,
	.fasync		= NULL,
	.release	= fanotify_release,
	.unlocked_ioctl	= NULL,
	.compat_ioctl	= NULL,
};

static void fanotify_free_mark(struct fsnotify_mark *fsn_mark)
{
	kmem_cache_free(fanotify_mark_cache, fsn_mark);
}

static int fanotify_find_path(int dfd, const char __user *filename,
			      struct path *path, unsigned int flags)
{
	int ret;

	pr_debug("%s: dfd=%d filename=%p flags=%x\n", __func__,
		 dfd, filename, flags);

	if (filename == NULL) {
		struct file *file;
		int fput_needed;

		ret = -EBADF;
		file = fget_light(dfd, &fput_needed);
		if (!file)
			goto out;

		ret = -ENOTDIR;
		if ((flags & FAN_MARK_ONLYDIR) &&
		    !(S_ISDIR(file->f_path.dentry->d_inode->i_mode))) {
			fput_light(file, fput_needed);
			goto out;
		}

		*path = file->f_path;
		path_get(path);
		fput_light(file, fput_needed);
	} else {
		unsigned int lookup_flags = 0;

		if (!(flags & FAN_MARK_DONT_FOLLOW))
			lookup_flags |= LOOKUP_FOLLOW;
		if (flags & FAN_MARK_ONLYDIR)
			lookup_flags |= LOOKUP_DIRECTORY;

		ret = user_path_at(dfd, filename, lookup_flags, path);
		if (ret)
			goto out;
	}

	/* you can only watch an inode if you have read permissions on it */
	ret = inode_permission(path->dentry->d_inode, MAY_READ);
	if (ret)
		path_put(path);
out:
	return ret;
}

static int fanotify_remove_mark(struct fsnotify_group *group,
				struct inode *inode,
				__u32 mask)
{
	struct fsnotify_mark *fsn_mark;
	__u32 new_mask;

	pr_debug("%s: group=%p inode=%p mask=%x\n", __func__,
		 group, inode, mask);

	fsn_mark = fsnotify_find_mark(group, inode);
	if (!fsn_mark)
		return -ENOENT;

	spin_lock(&fsn_mark->lock);
	fsn_mark->mask &= ~mask;
	new_mask = fsn_mark->mask;
	spin_unlock(&fsn_mark->lock);

	if (!new_mask)
		fsnotify_destroy_mark(fsn_mark);
	else
		fsnotify_recalc_inode_mask(inode);

	fsnotify_recalc_group_mask(group);

	/* matches the fsnotify_find_mark() */
	fsnotify_put_mark(fsn_mark);

	return 0;
}

static int fanotify_add_mark(struct fsnotify_group *group,
			     struct inode *inode,
			     __u32 mask)
{
	struct fsnotify_mark *fsn_mark;
	__u32 old_mask, new_mask;
	int ret;

	pr_debug("%s: group=%p inode=%p mask=%x\n", __func__,
		 group, inode, mask);

	fsn_mark = fsnotify_find_mark(group, inode);
	if (!fsn_mark) {
		struct fsnotify_mark *new_fsn_mark;

		ret = -ENOMEM;
		new_fsn_mark = kmem_cache_alloc(fanotify_mark_cache, GFP_KERNEL);
		if (!new_fsn_mark)
			goto out;

		fsnotify_init_mark(new_fsn_mark, fanotify_free_mark);
		ret = fsnotify_add_mark(new_fsn_mark, group, inode, 0);
		if (ret) {
			fanotify_free_mark(new_fsn_mark);
			goto out;
		}

		fsn_mark = new_fsn_mark;
	}

	ret = 0;

	spin_lock(&fsn_mark->lock);
	old_mask = fsn_mark->mask;
	fsn_mark->mask |= mask;
	new_mask = fsn_mark->mask;
	spin_unlock(&fsn_mark->lock);

	/* we made changes to a mask, update the group mask and the inode mask
	 * so things happen quickly. */
	if (old_mask != new_mask) {
		/* more bits in old than in new? */
		int dropped = (old_mask & ~new_mask);
		/* more bits in this mark than the inode's mask? */
		int do_inode = (new_mask & ~inode->i_fsnotify_mask);
		/* more bits in this mark than the group? */
		int do_group = (new_mask & ~group->mask);

		/* update the inode with this new mark */
		if (dropped || do_inode)
			fsnotify_recalc_inode_mask(inode);

		/* update the group mask with the new mask */
		if (dropped || do_group)
			fsnotify_recalc_group_mask(group);
	}

	/* match the init or the find.... */
	fsnotify_put_mark(fsn_mark);
out:
	return ret;
}

static int fanotify_update_mark(struct fsnotify_group *group,
				struct inode *inode, int flags,
				__u32 mask)
{
	pr_debug("%s: group=%p inode=%p flags=%x mask=%x\n", __func__,
		 group, inode, flags, mask);

	if (flags & FAN_MARK_ADD)
		fanotify_add_mark(group, inode, mask);
	else if (flags & FAN_MARK_REMOVE)
		fanotify_remove_mark(group, inode, mask);
	else
		BUG();

	return 0;
}

static bool fanotify_mark_validate_input(int flags,
					 __u32 mask)
{
	pr_debug("%s: flags=%x mask=%x\n", __func__, flags, mask);

	/* are flags valid of this operation? */
	if (!fanotify_mark_flags_valid(flags))
		return false;
	/* is the mask valid? */
	if (!fanotify_mask_valid(mask))
		return false;
	return true;
}

/* fanotify syscalls */
SYSCALL_DEFINE3(fanotify_init, unsigned int, flags, unsigned int, event_f_flags,
		unsigned int, priority)
{
	struct fsnotify_group *group;
	int f_flags, fd;

	pr_debug("%s: flags=%d event_f_flags=%d priority=%d\n",
		__func__, flags, event_f_flags, priority);

	if (event_f_flags)
		return -EINVAL;
	if (priority)
		return -EINVAL;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	if (flags & ~FAN_ALL_INIT_FLAGS)
		return -EINVAL;

	f_flags = (O_RDONLY | FMODE_NONOTIFY);
	if (flags & FAN_CLOEXEC)
		f_flags |= O_CLOEXEC;
	if (flags & FAN_NONBLOCK)
		f_flags |= O_NONBLOCK;

	/* fsnotify_alloc_group takes a ref.  Dropped in fanotify_release */
	group = fsnotify_alloc_group(&fanotify_fsnotify_ops);
	if (IS_ERR(group))
		return PTR_ERR(group);

	fd = anon_inode_getfd("[fanotify]", &fanotify_fops, group, f_flags);
	if (fd < 0)
		goto out_put_group;

	return fd;

out_put_group:
	fsnotify_put_group(group);
	return fd;
}

SYSCALL_DEFINE5(fanotify_mark, int, fanotify_fd, unsigned int, flags,
		__u64, mask, int, dfd, const char  __user *, pathname)
{
	struct inode *inode;
	struct fsnotify_group *group;
	struct file *filp;
	struct path path;
	int ret, fput_needed;

	pr_debug("%s: fanotify_fd=%d flags=%x dfd=%d pathname=%p mask=%llx\n",
		 __func__, fanotify_fd, flags, dfd, pathname, mask);

	/* we only use the lower 32 bits as of right now. */
	if (mask & ((__u64)0xffffffff << 32))
		return -EINVAL;

	if (!fanotify_mark_validate_input(flags, mask))
		return -EINVAL;

	filp = fget_light(fanotify_fd, &fput_needed);
	if (unlikely(!filp))
		return -EBADF;

	/* verify that this is indeed an fanotify instance */
	ret = -EINVAL;
	if (unlikely(filp->f_op != &fanotify_fops))
		goto fput_and_out;

	ret = fanotify_find_path(dfd, pathname, &path, flags);
	if (ret)
		goto fput_and_out;

	/* inode held in place by reference to path; group by fget on fd */
	inode = path.dentry->d_inode;
	group = filp->private_data;

	/* create/update an inode mark */
	ret = fanotify_update_mark(group, inode, flags, mask);

	path_put(&path);
fput_and_out:
	fput_light(filp, fput_needed);
	return ret;
}

/*
 * fanotify_user_setup - Our initialization function.  Note that we cannnot return
 * error because we have compiled-in VFS hooks.  So an (unlikely) failure here
 * must result in panic().
 */
static int __init fanotify_user_setup(void)
{
	fanotify_mark_cache = KMEM_CACHE(fsnotify_mark, SLAB_PANIC);

	return 0;
}
device_initcall(fanotify_user_setup);
