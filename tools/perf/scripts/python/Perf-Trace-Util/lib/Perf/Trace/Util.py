# Util.py - Python extension for perf trace, miscellaneous utility code
#
# Copyright (C) 2010 by Tom Zanussi <tzanussi@gmail.com>
#
# This software may be distributed under the terms of the GNU General
# Public License ("GPL") version 2 as published by the Free Software
# Foundation.

import errno, os

NSECS_PER_SEC    = 1000000000

def avg(total, n):
    return total / n

def nsecs(secs, nsecs):
    return secs * NSECS_PER_SEC + nsecs

def nsecs_secs(nsecs):
    return nsecs / NSECS_PER_SEC

def nsecs_nsecs(nsecs):
    return nsecs % NSECS_PER_SEC

def nsecs_str(nsecs):
    str = "%5u.%09u" % (nsecs_secs(nsecs), nsecs_nsecs(nsecs)),
    return str

def clear_term():
    print("\x1b[H\x1b[2J")

audit_package_warned = False

try:
	import audit
	machine_to_id = {
		'x86_64': audit.MACH_86_64,
		'alpha'	: audit.MACH_ALPHA,
		'armeb'	: audit.MACH_ARMEB,
		'ia64'	: audit.MACH_IA64,
		'ppc'	: audit.MACH_PPC,
		'ppc64'	: audit.MACH_PPC64,
		's390'	: audit.MACH_S390,
		's390x'	: audit.MACH_S390X,
		'i386'	: audit.MACH_X86,
		'i586'	: audit.MACH_X86,
		'i686'	: audit.MACH_X86,
	}
	machine_id = machine_to_id[os.uname()[4]]
except:
	if not audit_package_warned:
		audit_package_warned = True
		print "Install the audit-libs-python package to get syscall names"

def syscall_name(id):
	try:
		return audit.audit_syscall_to_name(id, machine_id)
	except:
		return str(id)

def strerror(nr):
	try:
		return errno.errorcode[abs(nr)]
	except:
		return "Unknown %d errno" % nr
