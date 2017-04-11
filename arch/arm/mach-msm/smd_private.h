/* arch/arm/mach-msm/smd_private.h
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2007 QUALCOMM Incorporated
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _ARCH_ARM_MACH_MSM_MSM_SMD_PRIVATE_H_
#define _ARCH_ARM_MACH_MSM_MSM_SMD_PRIVATE_H_

#include <mach/msm_smsm.h>

struct smem_heap_info {
	unsigned initialized;
	unsigned free_offset;
	unsigned heap_remaining;
	unsigned reserved;
};

struct smem_heap_entry {
	unsigned allocated;
	unsigned offset;
	unsigned size;
	unsigned reserved;
};

struct smem_proc_comm {
	unsigned command;
	unsigned status;
	unsigned data1;
	unsigned data2;
};

#define PC_APPS  0
#define PC_MODEM 1

#define VERSION_SMD       0
#define VERSION_QDSP6     4
#define VERSION_APPS_SBL  6
#define VERSION_MODEM_SBL 7
#define VERSION_APPS      8
#define VERSION_MODEM     9

struct smem_shared {
	struct smem_proc_comm proc_comm[4];
	unsigned version[32];
	struct smem_heap_info heap_info;
	struct smem_heap_entry heap_toc[512];
};

#define SMSM_V1_SIZE		(sizeof(unsigned) * 8)
#define SMSM_V2_SIZE		(sizeof(unsigned) * 4)

#if defined(CONFIG_MSM_N_WAY_SMD)
#define DEM_MAX_PORT_NAME_LEN (20)
struct msm_dem_slave_data {
	uint32_t sleep_time;
	uint32_t interrupt_mask;
	uint32_t resources_used;
	uint32_t reserved1;

	uint32_t wakeup_reason;
	uint32_t pending_interrupts;
	uint32_t rpc_prog;
	uint32_t rpc_proc;
	char     smd_port_name[DEM_MAX_PORT_NAME_LEN];
	uint32_t reserved2;
};
#else
#define SMSM_MAX_PORT_NAME_LEN    20
struct smsm_interrupt_info {
	uint32_t interrupt_mask;
	uint32_t pending_interrupts;
	uint32_t wakeup_reason;
	uint32_t aArm_rpc_prog;
	uint32_t aArm_rpc_proc;
	char aArm_smd_port_name[SMSM_MAX_PORT_NAME_LEN];
	/* If the wakeup reason is GPIO then send the gpio info */
	uint32_t aArm_gpio_info;
	/*uint32_t interrupt_mask;
	uint32_t pending_interrupts;
	uint32_t wakeup_reason;*/
};
#endif

#define SZ_DIAG_ERR_MSG 0xC8
#define ID_DIAG_ERR_MSG SMEM_DIAG_ERR_MESSAGE
#define ID_SMD_CHANNELS SMEM_SMD_BASE_ID
#define ID_SHARED_STATE SMEM_SMSM_SHARED_STATE
#define ID_CH_ALLOC_TBL SMEM_CHANNEL_ALLOC_TBL

#define SMSM_INIT		0x00000001
#define SMSM_OSENTERED		0x00000002
#define SMSM_SMDWAIT		0x00000004
#define SMSM_SMDINIT		0x00000008
#define SMSM_RPCWAIT		0x00000010
#define SMSM_RPCINIT		0x00000020
#define SMSM_RESET		0x00000040
#define SMSM_RSA		0x00000080
#define SMSM_RUN		0x00000100
#define SMSM_PWRC		0x00000200
#define SMSM_TIMEWAIT		0x00000400
#define SMSM_TIMEINIT		0x00000800
#define SMSM_PWRC_EARLY_EXIT	0x00001000
#define SMSM_WFPI		0x00002000
#define SMSM_SLEEP		0x00004000
#define SMSM_SLEEPEXIT		0x00008000
#define SMSM_OEMSBL_RELEASE	0x00010000
#define SMSM_APPS_REBOOT	0x00020000
#define SMSM_SYSTEM_POWER_DOWN	0x00040000
#define SMSM_SYSTEM_REBOOT	0x00080000
#define SMSM_SYSTEM_DOWNLOAD	0x00100000
#define SMSM_PWRC_SUSPEND	0x00200000
#define SMSM_APPS_SHUTDOWN	0x00400000
#define SMSM_SMD_LOOPBACK	0x00800000
#define SMSM_RUN_QUIET		0x01000000
#define SMSM_MODEM_WAIT		0x02000000
#define SMSM_MODEM_BREAK	0x04000000
#define SMSM_MODEM_CONTINUE	0x08000000
#define SMSM_UNKNOWN		0x80000000

#define SMSM_WKUP_REASON_RPC	0x00000001
#define SMSM_WKUP_REASON_INT	0x00000002
#define SMSM_WKUP_REASON_GPIO	0x00000004
#define SMSM_WKUP_REASON_TIMER	0x00000008
#define SMSM_WKUP_REASON_ALARM	0x00000010
#define SMSM_WKUP_REASON_RESET	0x00000020

int smsm_set_sleep_duration(uint32_t delay);
int smsm_set_sleep_limit(uint32_t sleep_limit);

#define SMD_SS_CLOSED		0x00000000
#define SMD_SS_OPENING		0x00000001
#define SMD_SS_OPENED		0x00000002
#define SMD_SS_FLUSHING		0x00000003
#define SMD_SS_CLOSING		0x00000004
#define SMD_SS_RESET		0x00000005
#define SMD_SS_RESET_OPENING	0x00000006

#define SMD_BUF_SIZE		8192
#define SMD_CHANNELS		64

#define SMD_HEADER_SIZE		20

#define SMD_TYPE_MASK		0x0FF
#define SMD_TYPE_APPS_MODEM	0x000
#define SMD_TYPE_APPS_DSP	0x001
#define SMD_TYPE_MODEM_DSP	0x002

#define SMD_KIND_MASK		0xF00
#define SMD_KIND_UNKNOWN	0x000
#define SMD_KIND_STREAM		0x100
#define SMD_KIND_PACKET		0x200

uint32_t raw_smsm_get_state(uint32_t smsm_entry);

#endif
