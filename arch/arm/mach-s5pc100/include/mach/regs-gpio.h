/* linux/arch/arm/plat-s5pc100/include/plat/regs-gpio.h
 *
 * Copyright 2009 Samsung Electronics Co.
 *      Byungho Min <bhmin@samsung.com>
 *
 * S5PC100 - GPIO register definitions
 */

#ifndef __ASM_MACH_S5PC100_REGS_GPIO_H
#define __ASM_MACH_S5PC100_REGS_GPIO_H __FILE__

#include <mach/map.h>

/* S5PC100 */
#define S5PC100_GPIO_BASE	S5P_VA_GPIO
#define S5PC100_GPA0_BASE	(S5PC100_GPIO_BASE + 0x0000)
#define S5PC100_GPA1_BASE	(S5PC100_GPIO_BASE + 0x0020)
#define S5PC100_GPB_BASE	(S5PC100_GPIO_BASE + 0x0040)
#define S5PC100_GPC_BASE	(S5PC100_GPIO_BASE + 0x0060)
#define S5PC100_GPD_BASE	(S5PC100_GPIO_BASE + 0x0080)
#define S5PC100_GPE0_BASE	(S5PC100_GPIO_BASE + 0x00A0)
#define S5PC100_GPE1_BASE	(S5PC100_GPIO_BASE + 0x00C0)
#define S5PC100_GPF0_BASE	(S5PC100_GPIO_BASE + 0x00E0)
#define S5PC100_GPF1_BASE	(S5PC100_GPIO_BASE + 0x0100)
#define S5PC100_GPF2_BASE	(S5PC100_GPIO_BASE + 0x0120)
#define S5PC100_GPF3_BASE	(S5PC100_GPIO_BASE + 0x0140)
#define S5PC100_GPG0_BASE	(S5PC100_GPIO_BASE + 0x0160)
#define S5PC100_GPG1_BASE	(S5PC100_GPIO_BASE + 0x0180)
#define S5PC100_GPG2_BASE	(S5PC100_GPIO_BASE + 0x01A0)
#define S5PC100_GPG3_BASE	(S5PC100_GPIO_BASE + 0x01C0)
#define S5PC100_GPH0_BASE	(S5PC100_GPIO_BASE + 0x0C00)
#define S5PC100_GPH1_BASE	(S5PC100_GPIO_BASE + 0x0C20)
#define S5PC100_GPH2_BASE	(S5PC100_GPIO_BASE + 0x0C40)
#define S5PC100_GPH3_BASE	(S5PC100_GPIO_BASE + 0x0C60)
#define S5PC100_GPI_BASE	(S5PC100_GPIO_BASE + 0x01E0)
#define S5PC100_GPJ0_BASE	(S5PC100_GPIO_BASE + 0x0200)
#define S5PC100_GPJ1_BASE	(S5PC100_GPIO_BASE + 0x0220)
#define S5PC100_GPJ2_BASE	(S5PC100_GPIO_BASE + 0x0240)
#define S5PC100_GPJ3_BASE	(S5PC100_GPIO_BASE + 0x0260)
#define S5PC100_GPJ4_BASE	(S5PC100_GPIO_BASE + 0x0280)
#define S5PC100_GPK0_BASE	(S5PC100_GPIO_BASE + 0x02A0)
#define S5PC100_GPK1_BASE	(S5PC100_GPIO_BASE + 0x02C0)
#define S5PC100_GPK2_BASE	(S5PC100_GPIO_BASE + 0x02E0)
#define S5PC100_GPK3_BASE	(S5PC100_GPIO_BASE + 0x0300)
#define S5PC100_GPL0_BASE	(S5PC100_GPIO_BASE + 0x0320)
#define S5PC100_GPL1_BASE	(S5PC100_GPIO_BASE + 0x0340)
#define S5PC100_GPL2_BASE	(S5PC100_GPIO_BASE + 0x0360)
#define S5PC100_GPL3_BASE	(S5PC100_GPIO_BASE + 0x0380)
#define S5PC100_GPL4_BASE	(S5PC100_GPIO_BASE + 0x03A0)

#define S5PC100EINT30CON		(S5P_VA_GPIO + 0xE00)
#define S5P_EINT_CON(x)			(S5PC100EINT30CON + ((x) * 0x4))

#define S5PC100EINT30FLTCON0		(S5P_VA_GPIO + 0xE80)
#define S5P_EINT_FLTCON(x)		(S5PC100EINT30FLTCON0 + ((x) * 0x4))

#define S5PC100EINT30MASK		(S5P_VA_GPIO + 0xF00)
#define S5P_EINT_MASK(x)		(S5PC100EINT30MASK + ((x) * 0x4))

#define S5PC100EINT30PEND		(S5P_VA_GPIO + 0xF40)
#define S5P_EINT_PEND(x)		(S5PC100EINT30PEND + ((x) * 0x4))

#define EINT_REG_NR(x)			(EINT_OFFSET(x) >> 3)

#define eint_irq_to_bit(irq)		(1 << (EINT_OFFSET(irq) & 0x7))

#define EINT_MODE		S3C_GPIO_SFN(0x2)

#define EINT_GPIO_0(x)		S5PC100_GPH0(x)
#define EINT_GPIO_1(x)		S5PC100_GPH1(x)
#define EINT_GPIO_2(x)		S5PC100_GPH2(x)
#define EINT_GPIO_3(x)		S5PC100_GPH3(x)

#endif /* __ASM_MACH_S5PC100_REGS_GPIO_H */

