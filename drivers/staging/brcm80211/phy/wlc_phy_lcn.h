/*
 * Copyright (c) 2010 Broadcom Corporation
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _wlc_phy_lcn_h_
#define _wlc_phy_lcn_h_

#include <typedefs.h>

struct phy_info_lcnphy {
	int lcnphy_txrf_sp_9_override;
	uint8 lcnphy_full_cal_channel;
	uint8 lcnphy_cal_counter;
	uint16 lcnphy_cal_temper;
	bool lcnphy_recal;

	uint8 lcnphy_rc_cap;
	uint32 lcnphy_mcs20_po;

	uint8 lcnphy_tr_isolation_mid;
	uint8 lcnphy_tr_isolation_low;
	uint8 lcnphy_tr_isolation_hi;

	uint8 lcnphy_bx_arch;
	uint8 lcnphy_rx_power_offset;
	uint8 lcnphy_rssi_vf;
	uint8 lcnphy_rssi_vc;
	uint8 lcnphy_rssi_gs;
	uint8 lcnphy_tssi_val;
	uint8 lcnphy_rssi_vf_lowtemp;
	uint8 lcnphy_rssi_vc_lowtemp;
	uint8 lcnphy_rssi_gs_lowtemp;

	uint8 lcnphy_rssi_vf_hightemp;
	uint8 lcnphy_rssi_vc_hightemp;
	uint8 lcnphy_rssi_gs_hightemp;

	int16 lcnphy_pa0b0;
	int16 lcnphy_pa0b1;
	int16 lcnphy_pa0b2;

	uint16 lcnphy_rawtempsense;
	uint8 lcnphy_measPower;
	uint8 lcnphy_tempsense_slope;
	uint8 lcnphy_freqoffset_corr;
	uint8 lcnphy_tempsense_option;
	uint8 lcnphy_tempcorrx;
	bool lcnphy_iqcal_swp_dis;
	bool lcnphy_hw_iqcal_en;
	uint lcnphy_bandedge_corr;
	bool lcnphy_spurmod;
	uint16 lcnphy_tssi_tx_cnt;
	uint16 lcnphy_tssi_idx;
	uint16 lcnphy_tssi_npt;

	uint16 lcnphy_target_tx_freq;
	int8 lcnphy_tx_power_idx_override;
	uint16 lcnphy_noise_samples;

	uint32 lcnphy_papdRxGnIdx;
	uint32 lcnphy_papd_rxGnCtrl_init;

	uint32 lcnphy_gain_idx_14_lowword;
	uint32 lcnphy_gain_idx_14_hiword;
	uint32 lcnphy_gain_idx_27_lowword;
	uint32 lcnphy_gain_idx_27_hiword;
	int16 lcnphy_ofdmgainidxtableoffset;
	int16 lcnphy_dsssgainidxtableoffset;
	uint32 lcnphy_tr_R_gain_val;
	uint32 lcnphy_tr_T_gain_val;
	int8 lcnphy_input_pwr_offset_db;
	uint16 lcnphy_Med_Low_Gain_db;
	uint16 lcnphy_Very_Low_Gain_db;
	int8 lcnphy_lastsensed_temperature;
	int8 lcnphy_pkteng_rssi_slope;
	uint8 lcnphy_saved_tx_user_target[TXP_NUM_RATES];
	uint8 lcnphy_volt_winner;
	uint8 lcnphy_volt_low;
	uint8 lcnphy_54_48_36_24mbps_backoff;
	uint8 lcnphy_11n_backoff;
	uint8 lcnphy_lowerofdm;
	uint8 lcnphy_cck;
	uint8 lcnphy_psat_2pt3_detected;
	int32 lcnphy_lowest_Re_div_Im;
	int8 lcnphy_final_papd_cal_idx;
	uint16 lcnphy_extstxctrl4;
	uint16 lcnphy_extstxctrl0;
	uint16 lcnphy_extstxctrl1;
	int16 lcnphy_cck_dig_filt_type;
	int16 lcnphy_ofdm_dig_filt_type;
	lcnphy_cal_results_t lcnphy_cal_results;

	uint8 lcnphy_psat_pwr;
	uint8 lcnphy_psat_indx;
	int32 lcnphy_min_phase;
	uint8 lcnphy_final_idx;
	uint8 lcnphy_start_idx;
	uint8 lcnphy_current_index;
	uint16 lcnphy_logen_buf_1;
	uint16 lcnphy_local_ovr_2;
	uint16 lcnphy_local_oval_6;
	uint16 lcnphy_local_oval_5;
	uint16 lcnphy_logen_mixer_1;

	uint8 lcnphy_aci_stat;
	uint lcnphy_aci_start_time;
	int8 lcnphy_tx_power_offset[TXP_NUM_RATES];
};
#endif				/* _wlc_phy_lcn_h_ */
