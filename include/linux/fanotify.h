#ifndef _LINUX_FANOTIFY_H
#define _LINUX_FANOTIFY_H

#include <linux/types.h>

/* the following events that user-space can register for */
#define FAN_ACCESS		0x00000001	/* File was accessed */
#define FAN_MODIFY		0x00000002	/* File was modified */
#define FAN_CLOSE_WRITE		0x00000008	/* Unwrittable file closed */
#define FAN_CLOSE_NOWRITE	0x00000010	/* Writtable file closed */
#define FAN_OPEN		0x00000020	/* File was opened */

#define FAN_EVENT_ON_CHILD	0x08000000	/* interested in child events */

/* FIXME currently Q's have no limit.... */
#define FAN_Q_OVERFLOW		0x00004000	/* Event queued overflowed */

/* helper events */
#define FAN_CLOSE		(FAN_CLOSE_WRITE | FAN_CLOSE_NOWRITE) /* close */

#define FAN_CLOEXEC		0x00000001
#define FAN_NONBLOCK		0x00000002

#define FAN_ALL_INIT_FLAGS	(FAN_CLOEXEC | FAN_NONBLOCK)
/*
 * All of the events - we build the list by hand so that we can add flags in
 * the future and not break backward compatibility.  Apps will get only the
 * events that they originally wanted.  Be sure to add new events here!
 */
#define FAN_ALL_EVENTS (FAN_ACCESS |\
			FAN_MODIFY |\
			FAN_CLOSE |\
			FAN_OPEN)

/*
 * All legal FAN bits userspace can request (although possibly not all
 * at the same time.
 */
#define FAN_ALL_INCOMING_EVENTS	(FAN_ALL_EVENTS |\
				 FAN_EVENT_ON_CHILD)
#ifdef __KERNEL__

#endif /* __KERNEL__ */
#endif /* _LINUX_FANOTIFY_H */
