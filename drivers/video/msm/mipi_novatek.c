/* Copyright (c) 2010-2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifdef CONFIG_SPI_QUP
#include <linux/spi/spi.h>
#endif
#include <linux/leds.h>
#include <mach/panel_id.h>
#include "msm_fb.h"
#include "mipi_dsi.h"
#include "mipi_novatek.h"
#include "mdp4.h"

#define DEFAULT_BRIGHTNESS 83
static int bl_level_prevset = 1;
static struct dsi_cmd_desc *mipi_power_on_cmd;
static int mipi_power_on_cmd_size;

static struct mipi_dsi_panel_platform_data *mipi_novatek_pdata;

static struct dsi_buf novatek_tx_buf;
static struct dsi_buf novatek_rx_buf;
static int mipi_novatek_lcd_init(void);

static int wled_trigger_initialized;

#define MIPI_DSI_NOVATEK_SPI_DEVICE_NAME	"dsi_novatek_3d_panel_spi"
#define HPCI_FPGA_READ_CMD	0x84
#define HPCI_FPGA_WRITE_CMD	0x04

#ifdef CONFIG_SPI_QUP
static struct spi_device *panel_3d_spi_client;

static void novatek_fpga_write(uint8 addr, uint16 value)
{
	char tx_buf[32];
	int  rc;
	struct spi_message  m;
	struct spi_transfer t;
	u8 data[4] = {0x0, 0x0, 0x0, 0x0};

	if (!panel_3d_spi_client) {
		pr_err("%s panel_3d_spi_client is NULL\n", __func__);
		return;
	}
	data[0] = HPCI_FPGA_WRITE_CMD;
	data[1] = addr;
	data[2] = ((value >> 8) & 0xFF);
	data[3] = (value & 0xFF);

	memset(&t, 0, sizeof t);
	memset(tx_buf, 0, sizeof tx_buf);
	t.tx_buf = data;
	t.len = 4;
	spi_setup(panel_3d_spi_client);
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	rc = spi_sync(panel_3d_spi_client, &m);
	if (rc)
		pr_err("%s: SPI transfer failed\n", __func__);

	return;
}

static void novatek_fpga_read(uint8 addr)
{
	char tx_buf[32];
	int  rc;
	struct spi_message  m;
	struct spi_transfer t;
	struct spi_transfer rx;
	char rx_value[2];
	u8 data[4] = {0x0, 0x0};

	if (!panel_3d_spi_client) {
		pr_err("%s panel_3d_spi_client is NULL\n", __func__);
		return;
	}

	data[0] = HPCI_FPGA_READ_CMD;
	data[1] = addr;

	memset(&t, 0, sizeof t);
	memset(tx_buf, 0, sizeof tx_buf);
	memset(&rx, 0, sizeof rx);
	memset(rx_value, 0, sizeof rx_value);
	t.tx_buf = data;
	t.len = 2;
	rx.rx_buf = rx_value;
	rx.len = 2;
	spi_setup(panel_3d_spi_client);
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	spi_message_add_tail(&rx, &m);

	rc = spi_sync(panel_3d_spi_client, &m);
	if (rc)
		pr_err("%s: SPI transfer failed\n", __func__);
	else
		pr_info("%s: rx_value = 0x%x, 0x%x\n", __func__,
						rx_value[0], rx_value[1]);

	return;
}

static int __devinit panel_3d_spi_probe(struct spi_device *spi)
{
	panel_3d_spi_client = spi;
	return 0;
}
static int __devexit panel_3d_spi_remove(struct spi_device *spi)
{
	panel_3d_spi_client = NULL;
	return 0;
}
static struct spi_driver panel_3d_spi_driver = {
	.probe         = panel_3d_spi_probe,
	.remove        = __devexit_p(panel_3d_spi_remove),
	.driver		   = {
		.name = "dsi_novatek_3d_panel_spi",
		.owner  = THIS_MODULE,
	}
};

#else

static void novatek_fpga_write(uint8 addr, uint16 value)
{
	return;
}

static void novatek_fpga_read(uint8 addr)
{
	return;
}

#endif

static unsigned char bkl_enable_cmds[] = {0x53, 0x24};/* DTYPE_DCS_WRITE1 *///bkl on and no dim
static unsigned char bkl_disable_cmds[] = {0x53, 0x00};/* DTYPE_DCS_WRITE1 *///bkl off

#ifdef NOVETAK_COMMANDS_UNUSED
static char display_config_cmd_mode1[] = {
	/* TYPE_DCS_LWRITE */
	0x2A, 0x00, 0x00, 0x01,
	0x3F, 0xFF, 0xFF, 0xFF
};

static char display_config_cmd_mode2[] = {
	/* DTYPE_DCS_LWRITE */
	0x2B, 0x00, 0x00, 0x01,
	0xDF, 0xFF, 0xFF, 0xFF
};

static char display_config_cmd_mode3_666[] = {
	/* DTYPE_DCS_WRITE1 */
	0x3A, 0x66, 0x15, 0x80 /* 666 Packed (18-bits) */
};

static char display_config_cmd_mode3_565[] = {
	/* DTYPE_DCS_WRITE1 */
	0x3A, 0x55, 0x15, 0x80 /* 565 mode */
};

static char display_config_321[] = {
	/* DTYPE_DCS_WRITE1 */
	0x66, 0x2e, 0x15, 0x00 /* Reg 0x66 : 2E */
};

static char display_config_323[] = {
	/* DTYPE_DCS_WRITE */
	0x13, 0x00, 0x05, 0x00 /* Reg 0x13 < Set for Normal Mode> */
};

static char display_config_2lan[] = {
	/* DTYPE_DCS_WRITE */
	0x61, 0x01, 0x02, 0xff /* Reg 0x61 : 01,02 < Set for 2 Data Lane > */
};

static char display_config_exit_sleep[] = {
	/* DTYPE_DCS_WRITE */
	0x11, 0x00, 0x05, 0x80 /* Reg 0x11 < exit sleep mode> */
};

static char display_config_TE_ON[] = {
	/* DTYPE_DCS_WRITE1 */
	0x35, 0x00, 0x15, 0x80
};

static char display_config_39H[] = {
	/* DTYPE_DCS_WRITE */
	0x39, 0x00, 0x05, 0x80
};

static char display_config_set_tear_scanline[] = {
	/* DTYPE_DCS_LWRITE */
	0x44, 0x00, 0x00, 0xff
};

static char display_config_set_twolane[] = {
	/* DTYPE_DCS_WRITE1 */
	0xae, 0x03, 0x15, 0x80
};

static char display_config_set_threelane[] = {
	/* DTYPE_DCS_WRITE1 */
	0xae, 0x05, 0x15, 0x80
};

#else

static char sw_reset[2] = {0x01, 0x00}; /* DTYPE_DCS_WRITE */
static char enter_sleep[2] = {0x10, 0x00}; /* DTYPE_DCS_WRITE */
static char exit_sleep[2] = {0x11, 0x00}; /* DTYPE_DCS_WRITE */
static char display_off[2] = {0x28, 0x00}; /* DTYPE_DCS_WRITE */
static char display_on[2] = {0x29, 0x00}; /* DTYPE_DCS_WRITE */
static char enable_te[2] = {0x35, 0x00};/* DTYPE_DCS_WRITE1 */
static char test_reg[3] = {0x44, 0x02, 0xCF};/* DTYPE_DCS_WRITE1 */

static char rgb_888[2] = {0x3A, 0x77}; /* DTYPE_DCS_WRITE1 */

static char max_pktsize[2] = {MIPI_DSI_MRPS, 0x00}; /* LSB tx first, 16 bytes */

#if defined(NOVATEK_TWO_LANE)
static char set_num_of_lanes[2] = {0xae, 0x03}; /* DTYPE_DCS_WRITE1 */
#else  /* 1 lane */
static char set_num_of_lanes[2] = {0xae, 0x01}; /* DTYPE_DCS_WRITE1 */
#endif
/* commands by Novatek */
static char novatek_f4[2] = {0xf4, 0x55}; /* DTYPE_DCS_WRITE1 */
static char novatek_8c[16] = { /* DTYPE_DCS_LWRITE */
	0x8C, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x08, 0x08, 0x00, 0x30, 0xC0, 0xB7, 0x37};
static char novatek_ff[2] = {0xff, 0x55 }; /* DTYPE_DCS_WRITE1 */

static char set_width[5] = { /* DTYPE_DCS_LWRITE */
	0x2A, 0x00, 0x00, 0x02, 0x1B}; /* 540 - 1 */
static char set_height[5] = { /* DTYPE_DCS_LWRITE */
	0x2B, 0x00, 0x00, 0x03, 0xBF}; /* 960 - 1 */
static char novatek_pwm_f3[2] = {0xF3, 0xAA }; /* DTYPE_DCS_WRITE1 */
static char novatek_pwm_00[2] = {0x00, 0x01 }; /* DTYPE_DCS_WRITE1 */
static char novatek_pwm_21[2] = {0x21, 0x20 }; /* DTYPE_DCS_WRITE1 */
static char novatek_pwm_22[2] = {0x22, 0x03 }; /* DTYPE_DCS_WRITE1 */
static char novatek_pwm_7d[2] = {0x7D, 0x01 }; /* DTYPE_DCS_WRITE1 */
static char novatek_pwm_7f[2] = {0x7F, 0xAA }; /* DTYPE_DCS_WRITE1 */

static char novatek_pwm_cp[2] = {0x09, 0x34 }; /* DTYPE_DCS_WRITE1 */
static char novatek_pwm_cp2[2] = {0xc9, 0x01 }; /* DTYPE_DCS_WRITE1 */
static char novatek_pwm_cp3[2] = {0xff, 0xaa }; /* DTYPE_DCS_WRITE1 */
static char lv3[5] = {0xFF, 0xAA, 0x55, 0x25, 0x01}; /* DTYPE_DCS_LWRITE */
static char maucctr_0[6] = {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00};/* DTYPE_DCS_LWRITE */
static char maucctr_1[6] = {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x01}; /* DTYPE_DCS_LWRITE */
static char novatek_b5x[4] = {0xB5, 0x05, 0x05, 0x05}; /* DTYPE_DCS_LWRITE */
static char novatek_ce[8] = {0xCE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; /* DTYPE_DCS_LWRITE */
static char novatek_e0[3] = {0xE0, 0x01, 0x03}; /* DTYPE_DCS_LWRITE */
static char novatek_f8[39] = { /* DTYPE_DCS_LWRITE */
	0xF8, 0x01, 0x02, 0x00, 0x20, 0x33, 0x13,
	0x00, 0x40, 0x00, 0x00, 0x23, 0x02, 0x99,
	0xC8, 0x00, 0x00, 0x11, 0x00, 0x00, 0x23,
	0x0F, 0x22, 0x22, 0x22, 0x22, 0x00, 0x00,
	0x06, 0x04, 0x00, 0x37, 0x13, 0x06, 0x55,
	0x00, 0x18, 0x00, 0x30};
#endif

static char led_pwm1[2] = {0x51, 0x0};	/* DTYPE_DCS_WRITE1 */
static char led_pwm2[2] = {0x53, 0x24}; /* DTYPE_DCS_WRITE1 */
static char led_pwm3[2] = {0x55, 0x00}; /* DTYPE_DCS_WRITE1 */

static struct dsi_cmd_desc novatek_video_on_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 50,
		sizeof(sw_reset), sw_reset},
	{DTYPE_DCS_WRITE, 1, 0, 0, 10,
		sizeof(exit_sleep), exit_sleep},
	{DTYPE_DCS_WRITE, 1, 0, 0, 10,
		sizeof(display_on), display_on},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 10,
		sizeof(set_num_of_lanes), set_num_of_lanes},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 10,
		sizeof(rgb_888), rgb_888},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 10,
		sizeof(led_pwm2), led_pwm2},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 10,
		sizeof(led_pwm3), led_pwm3},
};

static struct dsi_cmd_desc novatek_wvga_c3_cmd_on_cmds[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, 100,
		sizeof(lv3), lv3},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 1,
		sizeof(novatek_f8), novatek_f8},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(maucctr_0), maucctr_0},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(novatek_e0), novatek_e0},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(maucctr_1), maucctr_1},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(novatek_b5x), novatek_b5x},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(novatek_ce), novatek_ce},
	{DTYPE_DCS_WRITE, 1, 0, 0, 120,
		sizeof(exit_sleep), exit_sleep},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(enable_te), enable_te},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(test_reg), test_reg},
};

/* Gamma table of PYD, sharp panel */
static char pyd_sharp_gm[] = {
	/* into gamma page */
	0xf3, 0xaa,
	/* R positive */
	0x24, 0x00, 0x25, 0x04, 0x26, 0x11, 0x27, 0x1c, 0x28, 0x1a, 0x29, 0x2e,
	0x2a, 0x5e, 0x2b, 0x21, 0x2d, 0x1f, 0x2f, 0x27, 0x30, 0x60, 0x31, 0x15,
	0x32, 0x3e, 0x33, 0x5f, 0x34, 0x7c, 0x35, 0x86, 0x36, 0x87, 0x37, 0x08,
	/* R negative */
	0x38, 0x01, 0x39, 0x06, 0x3a, 0x14, 0x3b, 0x21, 0x3d, 0x1a, 0x3f, 0x2d,
	0x40, 0x5f, 0x41, 0x33, 0x42, 0x20, 0x43, 0x27, 0x44, 0x7b, 0x45, 0x15,
	0x46, 0x3e, 0x47, 0x5f, 0x48, 0xa7, 0x49, 0xb3, 0x4a, 0xb4, 0x4b, 0x35,
	/* G positive */
	0x4c, 0x2a, 0x4d, 0x2d, 0x4e, 0x36, 0x4f, 0x3e, 0x50, 0x18, 0x51, 0x2a,
	0x52, 0x5c, 0x53, 0x2c, 0x54, 0x1d, 0x55, 0x25, 0x56, 0x65, 0x57, 0x12,
	0x58, 0x3a, 0x59, 0x57, 0x5a, 0x93, 0x5b, 0xb2, 0x5c, 0xb6, 0x5d, 0x37,
	/* G negative */
	0x5e, 0x30, 0x5f, 0x34, 0x60, 0x3e, 0x61, 0x46, 0x62, 0x19, 0x63, 0x2b,
	0x64, 0x5c, 0x65, 0x3f, 0x66, 0x1f, 0x67, 0x26, 0x68, 0x80, 0x69, 0x13,
	0x6a, 0x3c, 0x6b, 0x57, 0x6c, 0xc0, 0x6d, 0xe2, 0x6e, 0xe7, 0x6f, 0x68,
	/* B positive */
	0x70, 0x00, 0x71, 0x0a, 0x72, 0x26, 0x73, 0x37, 0x74, 0x1e, 0x75, 0x32,
	0x76, 0x60, 0x77, 0x32, 0x78, 0x1f, 0x79, 0x26, 0x7a, 0x68, 0x7b, 0x14,
	0x7c, 0x39, 0x7d, 0x59, 0x7e, 0x85, 0x7f, 0x86, 0x80, 0x87, 0x81, 0x08,
	/* B negative */
	0x82, 0x01, 0x83, 0x0c, 0x84, 0x2b, 0x85, 0x3e, 0x86, 0x1f, 0x87, 0x33,
	0x88, 0x61, 0x89, 0x45, 0x8a, 0x1f, 0x8b, 0x26, 0x8c, 0x84, 0x8d, 0x14,
	0x8e, 0x3a, 0x8f, 0x59, 0x90, 0xb1, 0x91, 0xb3, 0x92, 0xb4, 0x93, 0x35,
	0xc9, 0x01,
	/* out of gamma page */
	0xff, 0xaa,
};

static struct dsi_cmd_desc pyd_sharp_cmd_on_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 10,
		sizeof(sw_reset), sw_reset},

	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +0},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +2},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +4},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +6},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +8},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +10},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +12},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +14},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +16},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +18},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +20},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +22},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +24},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +26},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +28},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +30},/* don't change this line */
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +32},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +34},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +36},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +38},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +40},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +42},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +44},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +46},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +48},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +50},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +52},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +54},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +56},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +58},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +60},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +62},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +64},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +66},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +68},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +70},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +72},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +74},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +76},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +78},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +80},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +82},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +84},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +86},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +88},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +90},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +92},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +94},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +96},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +98},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +100},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +102},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +104},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +106},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +108},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +110},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +112},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +114},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +116},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +118},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +120},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +122},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +124},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +126},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +128},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +130},/* don't change this line */
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +132},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +134},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +136},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +138},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +140},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +142},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +144},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +146},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +148},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +150},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +152},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +154},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +156},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +158},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +160},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +162},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +164},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +166},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +168},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +170},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +172},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +174},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +176},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +178},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +180},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +182},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +184},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +186},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +188},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +190},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +192},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +194},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +196},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +198},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +200},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +202},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +204},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +206},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +208},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +210},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +212},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +214},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +216},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +218},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_sharp_gm +220},

	{DTYPE_DCS_WRITE, 1, 0, 0, 120,
		sizeof(exit_sleep), exit_sleep},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(novatek_pwm_f3), novatek_pwm_f3},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(novatek_pwm_00), novatek_pwm_00},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(novatek_pwm_21), novatek_pwm_21},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(novatek_pwm_22), novatek_pwm_22},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(novatek_pwm_7d), novatek_pwm_7d},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(novatek_pwm_7f), novatek_pwm_7f},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(novatek_pwm_f3), novatek_pwm_f3},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(novatek_pwm_cp), novatek_pwm_cp},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(novatek_pwm_cp2), novatek_pwm_cp2},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(novatek_pwm_cp3), novatek_pwm_cp3},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(enable_te), enable_te},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(test_reg), test_reg},
	{DTYPE_MAX_PKTSIZE, 1, 0, 0, 0,
		sizeof(max_pktsize), max_pktsize},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(novatek_f4), novatek_f4},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(novatek_8c), novatek_8c},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(novatek_ff), novatek_ff},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(set_num_of_lanes), set_num_of_lanes},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(set_width), set_width},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(set_height), set_height},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(rgb_888), rgb_888},
};

/* Gamma table of PYD, sharp panel */
static char pyd_auo_gm[] = {
	/* into gamma page */
	0xf3, 0xaa,
	/* R positive */
	0x24,0X63,0x25,0X6B,0x26,0X78,0x27,0X7E,0x28,0X19,0x29,0X2E,
	0x2A,0X61,0x2B,0X61,0x2D,0X1b,0x2F,0X22,0x30,0X84,0x31,0X1B,
	0x32,0X4F,0x33,0X63,0x34,0x28,0x35,0XDF,0x36,0XC9,0x37,0X69,
	/* R negative */
	0x38,0X63,0x39,0X6B,0x3A,0X78,0x3B,0X7E,0x3D,0X19,0x3F,0X2E,
	0x40,0X61,0x41,0X61,0x42,0X1b,0x43,0X22,0x44,0X84,0x45,0X1B,
	0x46,0X4F,0x47,0X63,0x48,0XC7,0x49,0XDF,0x4A,0XC9,0x4B,0X69,
	/* G positive */
	0x4C,0X45,0x4D,0X54,0x4E,0X64,0x4F,0X75,0x50,0X18,0x51,0X2E,
	0x52,0X62,0x53,0X61,0x54,0X1D,0x55,0X26,0x56,0X9D,0x57,0X10,
	0x58,0X39,0x59,0X55,0x5A,0XC3,0x5B,0XD7,0x5C,0XFF,0x5D,0X6B,
	/* G negative */
	0x5E,0X45,0x5F,0X54,0x60,0X64,0x61,0X75,0x62,0X18,0x63,0X2E,
	0x64,0X62,0x65,0X61,0x66,0X1D,0x67,0X26,0x68,0x65,0x69,0X10,
	0x6A,0X39,0x6B,0X55,0x6C,0XC3,0x6D,0XD7,0x6E,0XFF,0x6F,0X6B,
	/* B positive */
	0x70,0X7D,0x71,0X82,0x72,0X89,0x73,0X97,0x74,0X19,0x75,0X2E,
	0x76,0X61,0x77,0X6E,0x78,0X1A,0x79,0X1E,0x7A,0X8E,0x7B,0X0C,
	0x7C,0X27,0x7D,0X58,0x7E,0XCF,0x7F,0XD9,0x80,0XFc,0x81,0X68,
	/* B negative */
	0x82,0X7D,0x83,0X82,0x84,0X89,0x85,0X97,0x86,0X19,0x87,0X2E,
	0x88,0X61,0x89,0X6E,0x8A,0X1A,0x8B,0X1E,0x8C,0X8E,0x8D,0X0C,
	0x8E,0X27,0x8F,0X58,0x90,0XCF,0x91,0XD9,0x92,0XFc,0x93,0X68,
	/* out of gamma page */
	0xC9,0x01,
	0xff, 0xaa,
};

/* Gamma table of RIR, sharp panel */
static char rir_shp_gm[] = {
	0xf3, 0xaa,
	// 2.5 gamma
	/* R + */
	0x24, 0x00, 0x25, 0x03, 0x26, 0x0e, 0x27, 0x19, 0x28, 0x18, 0x29, 0x2c,
	0x2A, 0x5d, 0x2B, 0x18, 0x2D, 0x1f, 0x2F, 0x26, 0x30, 0x58, 0x31, 0x16,
	0x32, 0x3a, 0x33, 0x53, 0x34, 0x69, 0x35, 0x6f, 0x36, 0x71, 0x37, 0x08,
	/* R - */
	0x38, 0x01, 0x39, 0x05, 0x3A, 0x11, 0x3B, 0x1d, 0x3D, 0x18, 0x3F, 0x2c,
	0x40, 0x5d, 0x41, 0x29, 0x42, 0x1f, 0x43, 0x26, 0x44, 0x72, 0x45, 0x15,
	0x46, 0x3a, 0x47, 0x53, 0x48, 0x93, 0x49, 0x9a, 0x4A, 0x9c, 0x4B, 0x35,
	/* G + */
	0x4C, 0x17, 0x4D, 0x1A, 0x4E, 0x24, 0x4F, 0x2d, 0x50, 0x19, 0x51, 0x2c,
	0x52, 0x5d, 0x53, 0x21, 0x54, 0x1E, 0x55, 0x25, 0x56, 0x5d, 0x57, 0x13,
	0x58, 0x37, 0x59, 0x4f, 0x5A, 0x78, 0x5B, 0xaa, 0x5C, 0xb6, 0x5D, 0x37,
	/* G - */
	0x5E, 0x1A, 0x5F, 0x1e, 0x60, 0x29, 0x61, 0x33, 0x62, 0x19, 0x63, 0x2c,
	0x64, 0x5d, 0x65, 0x33, 0x66, 0x1f, 0x67, 0x26, 0x68, 0x77, 0x69, 0x14,
	0x6A, 0x38, 0x6B, 0x4f, 0x6C, 0xa3, 0x6D, 0xdb, 0x6E, 0xe8, 0x6F, 0x68,
	/* B + */
	0x70, 0x4B, 0x71, 0x4d, 0x72, 0x55, 0x73, 0x5a, 0x74, 0x17, 0x75, 0x2a,
	0x76, 0x5b, 0x77, 0x32, 0x78, 0x1E, 0x79, 0x25, 0x7A, 0x64, 0x7B, 0x19,
	0x7C, 0x4c, 0x7D, 0x62, 0x7E, 0x53, 0x7F, 0x67, 0x80, 0x71, 0x81, 0x08,
	/* B - */
	0x82, 0x54, 0x83, 0x57, 0x84, 0x5f, 0x85, 0x65, 0x86, 0x17, 0x87, 0x29,
	0x88, 0x5a, 0x89, 0x47, 0x8A, 0x1d, 0x8B, 0x25, 0x8C, 0x7f, 0x8D, 0x1a,
	0x8E, 0x4c, 0x8F, 0x62, 0x90, 0x7a, 0x91, 0x90, 0x92, 0x9B, 0x93, 0x35,

	// 2.x gamma

	0xff, 0xaa,
};

static char rir_auo_gm[] = {
	0xf3, 0xaa,
	// 2.5 gamma
	/* R + */
	0x24, 0x00, 0x25, 0x0C, 0x26, 0x27, 0x27, 0x39, 0x28, 0x1B, 0x29, 0x2E,
	0x2A, 0x5D, 0x2B, 0x43, 0x2D, 0x20, 0x2F, 0x28, 0x30, 0x80, 0x31, 0x0F,
	0x32, 0x24, 0x33, 0x46, 0x34, 0xB8, 0x35, 0xDE, 0x36, 0xEF, 0x37, 0x6B,
	/* R - */
	0x38, 0x00, 0x39, 0x0C, 0x3A, 0x27, 0x3B, 0x39, 0x3D, 0x1B, 0x3F, 0x2E,
	0x40, 0x5D, 0x41, 0x43, 0x42, 0x20, 0x43, 0x28, 0x44, 0x80, 0x45, 0x0F,
	0x46, 0x24, 0x47, 0x46, 0x48, 0xB8, 0x49, 0xDE, 0x4A, 0xEF, 0x4B, 0x6B,
	/* G + */
	0x4C, 0x00, 0x4D, 0x0C, 0x4E, 0x20, 0x4F, 0x35, 0x50, 0x21, 0x51, 0x35,
	0x52, 0x64, 0x53, 0x53, 0x54, 0x20, 0x55, 0x28, 0x56, 0x88, 0x57, 0x0F,
	0x58, 0x27, 0x59, 0x46, 0x5A, 0xB8, 0x5B, 0xDE, 0x5C, 0xEF, 0x5D, 0x6B,
	/* G - */
	0x5E, 0x00, 0x5F, 0x0C, 0x60, 0x20, 0x61, 0x35, 0x62, 0x21, 0x63, 0x35,
	0x64, 0x64, 0x65, 0x53, 0x66, 0x20, 0x67, 0x28, 0x68, 0x88, 0x69, 0x0F,
	0x6A, 0x27, 0x6B, 0x46, 0x6C, 0xB8, 0x6D, 0xDE, 0x6E, 0xEF, 0x6F, 0x6B,
	/* B + */
	0x70, 0x00, 0x71, 0x0C, 0x72, 0x95, 0x73, 0x99, 0x74, 0x1B, 0x75, 0x2F,
	0x76, 0x5F, 0x77, 0x71, 0x78, 0x20, 0x79, 0x28, 0x7A, 0x96, 0x7B, 0x0F,
	0x7C, 0x29, 0x7D, 0x46, 0x7E, 0xB8, 0x7F, 0xDE, 0x80, 0xEF, 0x81, 0x6B,
	/* B - */
	0x82, 0x00, 0x83, 0x0C, 0x84, 0x95, 0x85, 0x99, 0x86, 0x1B, 0x87, 0x2F,
	0x88, 0x5F, 0x89, 0x71, 0x8A, 0x20, 0x8B, 0x28, 0x8C, 0x96, 0x8D, 0x0F,
	0x8E, 0x29, 0x8F, 0x46, 0x90, 0xB8, 0x91, 0xDE, 0x92, 0xEF, 0x93, 0x6B,

	// 2.x gamma
	0xC9, 0x01,
	0xff, 0xaa,
};

static struct dsi_cmd_desc pyd_auo_cmd_on_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 10,
		sizeof(sw_reset), sw_reset},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, (char[]){0xf3, 0xaa}},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, (char[]){0xA3, 0xFF}},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, (char[]){0xA4, 0xFF}},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, (char[]){0xA5, 0xFF}},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, (char[]){0xA6, 0x01}},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, (char[]){0xFF, 0xAA}},

	/* Gamma table start */
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +0},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +2},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +4},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +6},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +8},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +10},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +12},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +14},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +16},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +18},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +20},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +22},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +24},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +26},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +28},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +30},/* don't change this line! */
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +32},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +34},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +36},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +38},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +40},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +42},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +44},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +46},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +48},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +50},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +52},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +54},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +56},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +58},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +60},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +62},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +64},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +66},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +68},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +70},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +72},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +74},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +76},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +78},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +80},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +82},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +84},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +86},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +88},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +90},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +92},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +94},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +96},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +98},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +100},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +102},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +104},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +106},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +108},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +110},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +112},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +114},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +116},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +118},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +120},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +122},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +124},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +126},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +128},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +130},/* don't change this line! */
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +132},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +134},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +136},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +138},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +140},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +142},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +144},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +146},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +148},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +150},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +152},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +154},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +156},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +158},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +160},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +162},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +164},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +166},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +168},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +170},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +172},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +174},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +176},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +178},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +180},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +182},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +184},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +186},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +188},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +190},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +192},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +194},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +196},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +198},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +200},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +202},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +204},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +206},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +208},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +210},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +212},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +214},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +216},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +218},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, pyd_auo_gm +220},
	/* Gamma table end */
	{DTYPE_DCS_WRITE, 1, 0, 0, 120,
		sizeof(exit_sleep), exit_sleep},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(novatek_pwm_f3), novatek_pwm_f3},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(novatek_pwm_00), novatek_pwm_00},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(novatek_pwm_21), novatek_pwm_21},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(novatek_pwm_22), novatek_pwm_22},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(novatek_pwm_7d), novatek_pwm_7d},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(novatek_pwm_7f), novatek_pwm_7f},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(novatek_pwm_f3), novatek_pwm_f3},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(novatek_pwm_cp), novatek_pwm_cp},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(novatek_pwm_cp2), novatek_pwm_cp2},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(novatek_pwm_cp3), novatek_pwm_cp3},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(enable_te), enable_te},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(test_reg), test_reg},
	{DTYPE_MAX_PKTSIZE, 1, 0, 0, 0,
		sizeof(max_pktsize), max_pktsize},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(set_width), set_width},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(set_height), set_height},
};

static struct dsi_cmd_desc novatek_sony_mipi_init[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, 100, 5, (char[]) {
		0xFF, 0xAA, 0x55, 0x25, 0x01}},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 39, (char[]) {
		0xF8, 0x02, 0x02, 0x00, 0x20, 0x33, 0x00,
		0x00, 0x40, 0x00, 0x00, 0x23, 0x02, 0x99,
		0xC8, 0x02, 0x00, 0x00, 0x00, 0x00, 0x23,
		0x0F, 0x22, 0x22, 0x22, 0x22, 0x00, 0x00,
		0x05, 0x05, 0x00, 0x34, 0x00, 0x04, 0x55,
		0x00, 0x04, 0x11, 0x38}},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 9, (char[]) {
		0xF3, 0x00, 0x32, 0x00, 0x38, 0x31, 0x08,
		0x11, 0x00}},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 6, (char[]) {
		0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00}},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 3, (char[]) {
		0xB1, 0xEC, 0x0A}},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, (char[]) {
		0xB6, 0x07}},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 3, (char[]) {
		0xB7, 0x00, 0x73}},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 5, (char[]) {
		0xB8, 0x01, 0x04, 0x06, 0x05}},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 3, (char[]) {
		0xB9, 0x00, 0x00}},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, (char[]) {
		0xBA, 0x02}},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 4, (char[]) {
		0xBB, 0x88, 0x08, 0x88}},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 4, (char[]) {
		0xBC, 0x01, 0x01, 0x01}},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 6, (char[]) {
		0xBD, 0x01, 0x84, 0x1D, 0x1C, 0x00}},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 6, (char[]) {
		0xBE, 0x01, 0x84, 0x1D, 0x1C, 0x00}},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 6, (char[]) {
		0xBF, 0x01, 0x84, 0x1D, 0x1C, 0x00}},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 13, (char[]) {
		0xCA, 0x01, 0xE4, 0xE4, 0xE4, 0xE4, 0xE4,
		0xE4, 0xE4, 0x08, 0x08, 0x00, 0x01}},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 3, (char[]) {
		0xE0, 0x01, 0x03}},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 6, (char[]) {
		0xF0, 0x55, 0xAA, 0x52, 0x08, 0x01}},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 4, (char[]) {
		0xB0, 0x0A, 0x0A, 0x0A}},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 4, (char[]) {
		0xB1, 0x0A, 0x0A, 0x0A}},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 4, (char[]) {
		0xB2, 0x02, 0x02, 0x02}},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 4, (char[]) {
		0xB3, 0x08, 0x08, 0x08}},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 4, (char[]) {
		0xB5, 0x05, 0x05, 0x05}},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 4, (char[]) {
		0xB6, 0x45, 0x45, 0x45}},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 4, (char[]) {
		0xB7, 0x25, 0x25, 0x25}},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 4, (char[]) {
		0xB8, 0x36, 0x36, 0x36}},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 4, (char[]) {
		0xB9, 0x36, 0x36, 0x36}},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 4, (char[]) {
		0xBA, 0x15, 0x15, 0x15}},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, (char[]) {
		0xBF, 0x01}},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, (char[]) {
		0xC2, 0x01}},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 8, (char[]) {
		0xCE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00}},
	{DTYPE_DCS_WRITE, 1, 0, 0, 120, 2, (char[]) {
		0x11, 0x00}},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, (char[]) {
		0x2C, 0x00}},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, (char[]) {
		0x35, 0x00}},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 3, (char[]) {
		0x44, 0x02, 0xCF}},
};

static struct dsi_cmd_desc shp_novatek_cmd_on_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 10,
		sizeof(sw_reset), sw_reset},
	{DTYPE_DCS_WRITE, 1, 0, 0, 150,
		sizeof(exit_sleep), exit_sleep},

	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm },
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +2},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +4},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +6},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +8},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +10},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +12},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +14},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +16},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +18},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +20},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +22},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +24},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +26},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +28},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +20},/* don't change this line! */
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +32},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +34},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +36},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +38},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +40},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +42},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +44},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +46},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +48},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +50},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +52},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +54},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +56},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +58},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +60},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +62},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +64},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +66},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +68},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +70},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +72},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +74},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +76},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +78},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +80},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +82},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +84},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +86},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +88},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +90},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +92},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +94},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +96},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +98},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +100},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +102},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +104},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +106},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +108},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +110},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +112},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +114},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +116},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +118},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +120},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +122},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +124},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +126},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +128},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +120},/* don't change this line! */
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +132},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +134},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +136},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +138},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +140},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +142},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +144},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +146},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +148},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +150},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +152},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +154},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +156},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +158},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +160},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +162},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +164},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +166},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +168},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +170},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +172},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +174},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +176},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +178},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +180},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +182},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +184},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +186},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +188},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +190},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +192},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +194},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +196},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +198},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +200},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +202},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +204},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +206},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +208},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +210},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +212},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +214},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_shp_gm +216},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 50, 2, rir_shp_gm +218},

	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(novatek_pwm_f3), novatek_pwm_f3},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(novatek_pwm_00), novatek_pwm_00},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(novatek_pwm_21), novatek_pwm_21},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(novatek_pwm_22), novatek_pwm_22},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(novatek_pwm_7d), novatek_pwm_7d},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(novatek_pwm_7f), novatek_pwm_7f},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(novatek_pwm_f3), novatek_pwm_f3},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(novatek_pwm_cp), novatek_pwm_cp},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(novatek_pwm_cp2), novatek_pwm_cp2},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(novatek_pwm_cp3), novatek_pwm_cp3},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(enable_te), enable_te},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(test_reg), test_reg},
	{DTYPE_MAX_PKTSIZE, 1, 0, 0, 0,
		sizeof(max_pktsize), max_pktsize},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(novatek_f4), novatek_f4},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(novatek_8c), novatek_8c},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(novatek_ff), novatek_ff},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(set_num_of_lanes), set_num_of_lanes},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(set_width), set_width},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(set_height), set_height},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(rgb_888), rgb_888},
};

static struct dsi_cmd_desc auo_nov_cmd_on_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 10, sizeof(sw_reset), sw_reset},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, (char[]){0xf3, 0xaa}},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, (char[]){0xA3, 0xFF}},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, (char[]){0xA4, 0xFF}},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, (char[]){0xA5, 0xFF}},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, (char[]){0xA6, 0x01}},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, (char[]){0xFF, 0xAA}},

	/* Gamma table start */
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +0},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +2},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +4},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +6},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +8},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +10},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +12},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +14},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +16},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +18},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +20},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +22},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +24},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +26},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +28},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +30},/* don't change this line! */
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +32},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +34},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +36},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +38},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +40},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +42},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +44},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +46},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +48},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +50},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +52},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +54},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +56},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +58},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +60},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +62},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +64},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +66},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +68},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +70},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +72},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +74},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +76},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +78},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +80},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +82},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +84},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +86},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +88},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +90},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +92},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +94},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +96},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +98},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +100},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +102},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +104},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +106},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +108},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +110},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +112},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +114},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +116},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +118},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +120},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +122},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +124},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +126},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +128},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +130},/* don't change this line! */
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +132},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +134},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +136},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +138},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +140},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +142},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +144},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +146},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +148},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +150},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +152},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +154},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +156},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +158},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +160},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +162},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +164},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +166},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +168},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +170},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +172},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +174},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +176},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +178},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +180},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +182},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +184},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +186},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +188},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +190},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +192},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +194},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +196},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +198},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +200},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +202},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +204},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +206},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +208},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +210},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +212},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +214},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +216},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +218},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, rir_auo_gm +220},
	/* Gamma table end */
	{DTYPE_DCS_WRITE, 1, 0, 0, 120,
		sizeof(exit_sleep), exit_sleep},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(novatek_pwm_f3), novatek_pwm_f3},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(novatek_pwm_00), novatek_pwm_00},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(novatek_pwm_21), novatek_pwm_21},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(novatek_pwm_22), novatek_pwm_22},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(novatek_pwm_7d), novatek_pwm_7d},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(novatek_pwm_7f), novatek_pwm_7f},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(novatek_pwm_f3), novatek_pwm_f3},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(novatek_pwm_cp), novatek_pwm_cp},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(novatek_pwm_cp2), novatek_pwm_cp2},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(novatek_pwm_cp3), novatek_pwm_cp3},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(enable_te), enable_te},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(test_reg), test_reg},
	{DTYPE_MAX_PKTSIZE, 1, 0, 0, 0,
		sizeof(max_pktsize), max_pktsize},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(set_width), set_width},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(set_height), set_height},
};

static struct dsi_cmd_desc shr_sharp_cmd_on_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 10,
		sizeof(sw_reset), sw_reset},
	{DTYPE_DCS_WRITE, 1, 0, 0, 120,
		sizeof(exit_sleep), exit_sleep},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(novatek_pwm_f3), novatek_pwm_f3},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(novatek_pwm_00), novatek_pwm_00},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(novatek_pwm_21), novatek_pwm_21},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(novatek_pwm_22), novatek_pwm_22},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(novatek_pwm_7d), novatek_pwm_7d},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(novatek_pwm_7f), novatek_pwm_7f},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(novatek_pwm_f3), novatek_pwm_f3},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(novatek_pwm_cp), novatek_pwm_cp},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(novatek_pwm_cp2), novatek_pwm_cp2},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(novatek_pwm_cp3), novatek_pwm_cp3},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(enable_te), enable_te},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(test_reg), test_reg},
	{DTYPE_MAX_PKTSIZE, 1, 0, 0, 0,
		sizeof(max_pktsize), max_pktsize},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(novatek_f4), novatek_f4},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(novatek_8c), novatek_8c},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(novatek_ff), novatek_ff},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(set_num_of_lanes), set_num_of_lanes},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(set_width), set_width},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(set_height), set_height},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(rgb_888), rgb_888},
};

static struct dsi_cmd_desc novatek_cmd_on_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 50,
		sizeof(sw_reset), sw_reset},
	{DTYPE_DCS_WRITE, 1, 0, 0, 10,
		sizeof(exit_sleep), exit_sleep},
	{DTYPE_DCS_WRITE, 1, 0, 0, 10,
		sizeof(display_on), display_on},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 50,
		sizeof(novatek_f4), novatek_f4},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 50,
		sizeof(novatek_8c), novatek_8c},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 50,
		sizeof(novatek_ff), novatek_ff},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 10,
		sizeof(set_num_of_lanes), set_num_of_lanes},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 50,
		sizeof(set_width), set_width},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 50,
		sizeof(set_height), set_height},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 10,
		sizeof(rgb_888), rgb_888},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 1,
		sizeof(led_pwm2), led_pwm2},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 1,
		sizeof(led_pwm3), led_pwm3},
};

static struct dsi_cmd_desc novatek_display_off_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 10,
		sizeof(display_off), display_off},
	{DTYPE_DCS_WRITE, 1, 0, 0, 120,
		sizeof(enter_sleep), enter_sleep}
};

static char manufacture_id[2] = {0x04, 0x00}; /* DTYPE_DCS_READ */

static struct dsi_cmd_desc novatek_manufacture_id_cmd = {
	DTYPE_DCS_READ, 1, 0, 1, 5, sizeof(manufacture_id), manufacture_id};

static u32 manu_id;

static void mipi_novatek_manufacture_cb(u32 data)
{
	manu_id = data;
	pr_info("%s: manufacture_id=%x\n", __func__, manu_id);
}

static uint32 mipi_novatek_manufacture_id(struct msm_fb_data_type *mfd)
{
	struct dcs_cmd_req cmdreq;

	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = &novatek_manufacture_id_cmd;
	cmdreq.cmds_cnt = 1;
	cmdreq.flags = CMD_REQ_RX | CMD_REQ_COMMIT;
	cmdreq.rlen = 3;
	cmdreq.rbuf = NULL;
	cmdreq.cb = mipi_novatek_manufacture_cb; /* call back */
	mipi_dsi_cmdlist_put(&cmdreq);
	/*
	 * blocked here, untill call back called
	 */

	return manu_id;
}

static int fpga_addr;
static int fpga_access_mode;
static bool support_3d;

static void mipi_novatek_3d_init(int addr, int mode)
{
	fpga_addr = addr;
	fpga_access_mode = mode;
}

static void mipi_dsi_enable_3d_barrier(int mode)
{
	void __iomem *fpga_ptr;
	uint32_t ptr_value = 0;

	if (!fpga_addr && support_3d) {
		pr_err("%s: fpga_addr not set. Failed to enable 3D barrier\n",
					__func__);
		return;
	}

	if (fpga_access_mode == FPGA_SPI_INTF) {
		if (mode == LANDSCAPE)
			novatek_fpga_write(fpga_addr, 1);
		else if (mode == PORTRAIT)
			novatek_fpga_write(fpga_addr, 3);
		else
			novatek_fpga_write(fpga_addr, 0);

		mb();
		novatek_fpga_read(fpga_addr);
	} else if (fpga_access_mode == FPGA_EBI2_INTF) {
		fpga_ptr = ioremap_nocache(fpga_addr, sizeof(uint32_t));
		if (!fpga_ptr) {
			pr_err("%s: FPGA ioremap failed."
				"Failed to enable 3D barrier\n",
						__func__);
			return;
		}

		ptr_value = readl_relaxed(fpga_ptr);
		if (mode == LANDSCAPE)
			writel_relaxed(((0xFFFF0000 & ptr_value) | 1),
								fpga_ptr);
		else if (mode == PORTRAIT)
			writel_relaxed(((0xFFFF0000 & ptr_value) | 3),
								fpga_ptr);
		else
			writel_relaxed((0xFFFF0000 & ptr_value),
								fpga_ptr);

		mb();
		iounmap(fpga_ptr);
	} else
		pr_err("%s: 3D barrier not configured correctly\n",
					__func__);
}

static struct dsi_cmd_desc novatek_cmd_backlight_cmds[] = {
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(led_pwm1), led_pwm1},
};

static struct dsi_cmd_desc novatek_display_on_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 0,
		sizeof(display_on), display_on},
};

static struct dsi_cmd_desc novatek_bkl_enable_cmds[] = {
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(bkl_enable_cmds), bkl_enable_cmds},
};

static struct dsi_cmd_desc novatek_bkl_disable_cmds[] = {
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(bkl_disable_cmds), bkl_disable_cmds},
};

static void mipi_novatek_panel_type_detect(void) {
	if (panel_type == PANEL_ID_PYD_SHARP) {
		pr_info("%s: panel_type=PANEL_ID_PYD_SHARP\n", __func__);
		mipi_power_on_cmd = pyd_sharp_cmd_on_cmds;
		mipi_power_on_cmd_size = ARRAY_SIZE(pyd_sharp_cmd_on_cmds);
	} else if (panel_type == PANEL_ID_PYD_AUO_NT) {
		pr_info("%s: panel_type=PANEL_ID_PYD_AUO_NT\n", __func__);
		mipi_power_on_cmd = pyd_auo_cmd_on_cmds;
		mipi_power_on_cmd_size = ARRAY_SIZE(pyd_auo_cmd_on_cmds);
	} else if (panel_type == PANEL_ID_DOT_SONY) {
		pr_info("%s: panel_type=PANEL_ID_DOT_SONY\n", __func__);
		mipi_power_on_cmd = novatek_sony_mipi_init;;
		mipi_power_on_cmd_size = ARRAY_SIZE(novatek_sony_mipi_init);
	} else if (panel_type == PANEL_ID_DOT_SONY_C3) {
		pr_info("%s: panel_type=PANEL_ID_DOT_SONY_C3\n", __func__);
		mipi_power_on_cmd = novatek_wvga_c3_cmd_on_cmds;
		mipi_power_on_cmd_size = ARRAY_SIZE(novatek_wvga_c3_cmd_on_cmds);
	} else if (panel_type == PANEL_ID_RIR_SHARP_NT) {
		pr_info("%s: panel_type=PANEL_ID_RIR_SHARP_NT\n", __func__);
		mipi_power_on_cmd = shp_novatek_cmd_on_cmds;
		mipi_power_on_cmd_size = ARRAY_SIZE(shp_novatek_cmd_on_cmds);
	} else if (panel_type == PANEL_ID_RIR_AUO_NT) {
		pr_info("%s: panel_type=PANEL_ID_RIR_AUO_NT\n", __func__);
		mipi_power_on_cmd = auo_nov_cmd_on_cmds;
		mipi_power_on_cmd_size = ARRAY_SIZE(auo_nov_cmd_on_cmds);
	} else if (panel_type == PANEL_ID_SHR_SHARP_NT) {
		pr_info("%s: panel_type=PANEL_ID_SHR_SHARP_NT\n", __func__);
		mipi_power_on_cmd = shr_sharp_cmd_on_cmds;
		mipi_power_on_cmd_size = ARRAY_SIZE(shr_sharp_cmd_on_cmds);
	} else {
		pr_err("%s: unknown panel_type=0x%x\n", __func__, panel_type);
		mipi_power_on_cmd = novatek_cmd_on_cmds;
		mipi_power_on_cmd_size = ARRAY_SIZE(novatek_cmd_on_cmds);
	}
	return;
}

static int mipi_novatek_lcd_on(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	struct mipi_panel_info *mipi;
	struct msm_panel_info *pinfo;
	struct dcs_cmd_req cmdreq;
	static int init;

	mfd = platform_get_drvdata(pdev);
	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL;

	pinfo = &mfd->panel_info;
	if (pinfo->is_3d_panel)
		support_3d = TRUE;

	mipi  = &mfd->panel_info.mipi;

	if (init == 0) {
		mipi_novatek_panel_type_detect();
		init = 1;
		return 0;
	} else {
		if (mipi->mode == DSI_VIDEO_MODE) {
			cmdreq.cmds = novatek_video_on_cmds;
			cmdreq.cmds_cnt = ARRAY_SIZE(novatek_video_on_cmds);
			cmdreq.flags = CMD_REQ_COMMIT;
			cmdreq.rlen = 0;
			cmdreq.cb = NULL;
			mipi_dsi_cmdlist_put(&cmdreq);
		} else {
			cmdreq.cmds = mipi_power_on_cmd;
			cmdreq.cmds_cnt = mipi_power_on_cmd_size;
			cmdreq.flags = CMD_REQ_COMMIT;
			cmdreq.rlen = 0;
			cmdreq.cb = NULL;
			mipi_dsi_cmdlist_put(&cmdreq);

			/* clean up ack_err_status */
			mipi_dsi_cmd_bta_sw_trigger();
			mipi_novatek_manufacture_id(mfd);
		}
	}
	return 0;
}

static int mipi_novatek_lcd_off(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	struct dcs_cmd_req cmdreq;

	mfd = platform_get_drvdata(pdev);

	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL;

	cmdreq.cmds = novatek_display_off_cmds;
	cmdreq.cmds_cnt = ARRAY_SIZE(novatek_display_off_cmds);
	cmdreq.flags = CMD_REQ_COMMIT;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	mipi_dsi_cmdlist_put(&cmdreq);

	return 0;
}

static int mipi_novatek_lcd_late_init(struct platform_device *pdev)
{
	return 0;
}

DEFINE_LED_TRIGGER(bkl_led_trigger);

static void mipi_novatek_set_backlight(struct msm_fb_data_type *mfd)
{
	struct dcs_cmd_req cmdreq;

	if (mipi_novatek_pdata &&
	    mipi_novatek_pdata->gpio_set_backlight) {
		mipi_novatek_pdata->gpio_set_backlight(mfd->bl_level);
		return;
	}

	if ((mipi_novatek_pdata->enable_wled_bl_ctrl)
	    && (wled_trigger_initialized)) {
		led_trigger_event(bkl_led_trigger, mfd->bl_level);
		return;
	}

	if (mipi_novatek_pdata && mipi_novatek_pdata->shrink_pwm)
		led_pwm1[1] = mipi_novatek_pdata->shrink_pwm(mfd->bl_level);
	else
		led_pwm1[1] = (unsigned char)mfd->bl_level;

	cmdreq.cmds = novatek_cmd_backlight_cmds;
	cmdreq.cmds_cnt = ARRAY_SIZE(novatek_cmd_backlight_cmds);
	cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	mipi_dsi_cmdlist_put(&cmdreq);
	bl_level_prevset = mfd->bl_level;
}

static void mipi_novatek_display_on(struct msm_fb_data_type *mfd)
{
	struct dcs_cmd_req cmdreq;

	cmdreq.cmds = novatek_display_on_cmds,
	cmdreq.cmds_cnt = ARRAY_SIZE(novatek_display_on_cmds);
	cmdreq.flags = CMD_REQ_COMMIT;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	mipi_dsi_cmdlist_put(&cmdreq);
}

static void mipi_novatek_bkl_switch(struct msm_fb_data_type *mfd, bool on)
{
	if (on) {
		if (mfd->bl_level == 0) {
			if (bl_level_prevset != 1)
				mfd->bl_level = bl_level_prevset;
			else
				mfd->bl_level = DEFAULT_BRIGHTNESS;
		}
		mipi_novatek_set_backlight(mfd);
	} else
		mfd->bl_level = 0;
}

static void mipi_novatek_bkl_ctrl(bool on)
{
	struct dcs_cmd_req cmdreq;

	if (on) {
		cmdreq.cmds = novatek_bkl_enable_cmds;
		cmdreq.cmds_cnt = ARRAY_SIZE(novatek_bkl_enable_cmds);
		cmdreq.flags = CMD_REQ_COMMIT;
		cmdreq.rlen = 0;
		cmdreq.cb = NULL;
	} else {
		cmdreq.cmds = novatek_bkl_disable_cmds,
		cmdreq.cmds_cnt = ARRAY_SIZE(novatek_bkl_disable_cmds);
		cmdreq.flags = CMD_REQ_COMMIT;
		cmdreq.rlen = 0;
		cmdreq.cb = NULL;
	}
	mipi_dsi_cmdlist_put(&cmdreq);
}

static int mipi_dsi_3d_barrier_sysfs_register(struct device *dev);
static int barrier_mode;

static int __devinit mipi_novatek_lcd_probe(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	struct mipi_panel_info *mipi;
	struct platform_device *current_pdev;
	static struct mipi_dsi_phy_ctrl *phy_settings;
	static char dlane_swap;

	if (pdev->id == 0) {
		mipi_novatek_pdata = pdev->dev.platform_data;

		if (mipi_novatek_pdata
			&& mipi_novatek_pdata->phy_ctrl_settings) {
			phy_settings = (mipi_novatek_pdata->phy_ctrl_settings);
		}

		if (mipi_novatek_pdata
			&& mipi_novatek_pdata->dlane_swap) {
			dlane_swap = (mipi_novatek_pdata->dlane_swap);
		}

		if (mipi_novatek_pdata
			 && mipi_novatek_pdata->fpga_3d_config_addr)
			mipi_novatek_3d_init(mipi_novatek_pdata
	->fpga_3d_config_addr, mipi_novatek_pdata->fpga_ctrl_mode);

		/* create sysfs to control 3D barrier for the Sharp panel */
		if (mipi_dsi_3d_barrier_sysfs_register(&pdev->dev)) {
			pr_err("%s: Failed to register 3d Barrier sysfs\n",
						__func__);
			return -ENODEV;
		}
		barrier_mode = 0;

		return 0;
	}

	current_pdev = msm_fb_add_device(pdev);

	if (current_pdev) {
		mfd = platform_get_drvdata(current_pdev);
		if (!mfd)
			return -ENODEV;
		if (mfd->key != MFD_KEY)
			return -EINVAL;

		mipi  = &mfd->panel_info.mipi;

		if (phy_settings != NULL)
			mipi->dsi_phy_db = phy_settings;

		if (dlane_swap)
			mipi->dlane_swap = dlane_swap;
	}
	return 0;
}

static struct platform_driver this_driver = {
	.probe  = mipi_novatek_lcd_probe,
	.driver = {
		.name   = "mipi_novatek",
	},
};

static struct msm_fb_panel_data novatek_panel_data = {
	.on		= mipi_novatek_lcd_on,
	.off		= mipi_novatek_lcd_off,
	.late_init	= mipi_novatek_lcd_late_init,
	.set_backlight	= mipi_novatek_set_backlight,
	.display_on	= mipi_novatek_display_on,
	.bklswitch	= mipi_novatek_bkl_switch,
	.bklctrl	= mipi_novatek_bkl_ctrl,
};

static ssize_t mipi_dsi_3d_barrier_read(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	return snprintf((char *)buf, sizeof(buf), "%u\n", barrier_mode);
}

static ssize_t mipi_dsi_3d_barrier_write(struct device *dev,
				struct device_attribute *attr,
				const char *buf,
				size_t count)
{
	int ret = -1;
	u32 data = 0;

	if (sscanf((char *)buf, "%u", &data) != 1) {
		dev_err(dev, "%s\n", __func__);
		ret = -EINVAL;
	} else {
		barrier_mode = data;
		if (data == 1)
			mipi_dsi_enable_3d_barrier(LANDSCAPE);
		else if (data == 2)
			mipi_dsi_enable_3d_barrier(PORTRAIT);
		else
			mipi_dsi_enable_3d_barrier(0);
	}

	return count;
}

static struct device_attribute mipi_dsi_3d_barrier_attributes[] = {
	__ATTR(enable_3d_barrier, 0664, mipi_dsi_3d_barrier_read,
					 mipi_dsi_3d_barrier_write),
};

static int mipi_dsi_3d_barrier_sysfs_register(struct device *dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mipi_dsi_3d_barrier_attributes); i++)
		if (device_create_file(dev, mipi_dsi_3d_barrier_attributes + i))
			goto error;

	return 0;

error:
	for (; i >= 0 ; i--)
		device_remove_file(dev, mipi_dsi_3d_barrier_attributes + i);
	pr_err("%s: Unable to create interface\n", __func__);

	return -ENODEV;
}

static int ch_used[3];

int mipi_novatek_device_register(struct msm_panel_info *pinfo,
					u32 channel, u32 panel)
{
	struct platform_device *pdev = NULL;
	int ret;

	if ((channel >= 3) || ch_used[channel])
		return -ENODEV;

	ch_used[channel] = TRUE;

	ret = mipi_novatek_lcd_init();
	if (ret) {
		pr_err("mipi_novatek_lcd_init() failed with ret %u\n", ret);
		return ret;
	}

	pdev = platform_device_alloc("mipi_novatek", (panel << 8)|channel);
	if (!pdev)
		return -ENOMEM;

	novatek_panel_data.panel_info = *pinfo;

	ret = platform_device_add_data(pdev, &novatek_panel_data,
		sizeof(novatek_panel_data));
	if (ret) {
		printk(KERN_ERR
		  "%s: platform_device_add_data failed!\n", __func__);
		goto err_device_put;
	}

	ret = platform_device_add(pdev);
	if (ret) {
		printk(KERN_ERR
		  "%s: platform_device_register failed!\n", __func__);
		goto err_device_put;
	}

	return 0;

err_device_put:
	platform_device_put(pdev);
	return ret;
}

static int mipi_novatek_lcd_init(void)
{
#ifdef CONFIG_SPI_QUP
	int ret;
	ret = spi_register_driver(&panel_3d_spi_driver);

	if (ret) {
		pr_err("%s: spi register failed: rc=%d\n", __func__, ret);
		platform_driver_unregister(&this_driver);
	} else
		pr_info("%s: SUCCESS (SPI)\n", __func__);
#endif

	led_trigger_register_simple("bkl_trigger", &bkl_led_trigger);
	pr_info("%s: SUCCESS (WLED TRIGGER)\n", __func__);
	wled_trigger_initialized = 1;

	mipi_dsi_buf_alloc(&novatek_tx_buf, DSI_BUF_SIZE);
	mipi_dsi_buf_alloc(&novatek_rx_buf, DSI_BUF_SIZE);

	return platform_driver_register(&this_driver);
}
