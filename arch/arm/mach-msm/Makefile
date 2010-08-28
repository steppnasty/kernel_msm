CFLAGS_scm.o :=$(call as-instr,.arch_extension sec,-DREQUIRES_SEC=1)

obj-y += io.o dma.o memory.o timer.o
obj-y += clock.o clock-voter.o clock-dummy.o
obj-y += modem_notifier.o subsystem_map.o
obj-$(CONFIG_CPU_FREQ) += cpufreq.o
obj-$(CONFIG_DEBUG_FS) += clock-debug.o

obj-$(CONFIG_MSM_PROC_COMM) += proc_comm.o
ifndef CONFIG_ARCH_MSM8X60
	obj-$(CONFIG_MSM_PROC_COMM) += clock-pcom.o
	obj-$(CONFIG_MSM_PROC_COMM) += vreg.o
	obj-y += pmic.o
ifdef CONFIG_MSM_PROC_COMM
		obj-$(CONFIG_MSM_PROC_COMM) += footswitch-pcom.o
endif
endif

obj-y += acpuclock.o

obj-$(CONFIG_ARCH_MSM_SCORPION) += msm_fault_handlers.o

obj-$(CONFIG_MSM_VIC) += irq-vic.o

obj-$(CONFIG_MSM_CPU_AVS) += avs.o avs_hw.o
obj-$(CONFIG_CPU_V7) += idle-v7.o
obj-$(CONFIG_MSM_JTAG_V7) += jtag-v7.o

quiet_cmd_mkrpcsym = MKCAP   $@
      cmd_mkrpcsym = $(PERL) $(srctree)/$(src)/mkrpcsym.pl $< $@

target += smd_rpc_sym.c
$(obj)/smd_rpc_sym.c: $(src)/smd_rpc_sym $(src)/mkrpcsym.pl
	$(call if_changed,mkrpcsym)

obj-$(CONFIG_MSM_SCM) += scm.o scm-boot.o htc_simlock-8x60.o scm-log.o
obj-$(CONFIG_MSM_SECURE_IO) += scm-io.o
obj-$(CONFIG_MSM_PIL) += peripheral-loader.o
obj-$(CONFIG_MSM_PIL) += peripheral-reset.o
obj-$(CONFIG_ARCH_QSD8X50) += sirc.o
obj-$(CONFIG_MSM_FIQ_SUPPORT) += fiq_glue.o
obj-$(CONFIG_MACH_TROUT) += board-trout-rfkill.o
obj-$(CONFIG_ARCH_MSM_ARM11) += acpuclock-arm11.o idle.o
obj-$(CONFIG_ARCH_QSD8X50) += arch-init-scorpion.o acpuclock-scorpion.o
obj-$(CONFIG_ACPUCLOCK_DEBUG) += acpuclock_debug.o
obj-$(CONFIG_ARCH_MSM7X30) += clock-local.o clock-7x30.o acpuclock-7x30.o arch-init-7x30.o
obj-$(CONFIG_ARCH_MSM7X30) += spm.o

obj-y += nand_partitions.o
obj-y += drv_callback.o
obj-y += radio_feedback.o
obj-$(CONFIG_ARCH_QSD8X50) += pmic.o htc_wifi_nvs.o htc_bluetooth.o

obj-$(CONFIG_SMP) += headsmp.o platsmp.o
obj-$(CONFIG_ARCH_MSM8X60) += devices-msm8x60.o clock-local.o clock-8x60.o acpuclock-8x60.o
obj-$(CONFIG_ARCH_MSM8X60) += spm.o restart.o mpm.o mpp.o
obj-$(CONFIG_ARCH_MSM8X60) += subsystem-fatal-8x60.o
obj-$(CONFIG_ARCH_MSM8X60) += clock-rpm.o
obj-$(CONFIG_ARCH_MSM8X60) += saw-regulator.o
obj-$(CONFIG_ARCH_MSM8X60) += rpm-regulator.o rpm-regulator-8660.o
obj-$(CONFIG_ARCH_MSM8X60) += footswitch-8x60.o
obj-$(CONFIG_ARCH_MSM8X60) += sysinfo-8x60.o
obj-$(CONFIG_MSM_SUBSYSTEM_RESTART) += subsystem_notif.o
obj-$(CONFIG_MSM_SUBSYSTEM_RESTART) += subsystem_restart.o

obj-$(CONFIG_MSM_SMD) += htc_port_list.o
obj-$(CONFIG_MSM_SMD_LOGGING) += smem_log.o
obj-$(CONFIG_MSM_SMD) += smd.o smd_debug.o remote_spinlock.o
        obj-y += socinfo.o
ifndef CONFIG_ARCH_MSM9615
ifndef CONFIG_ARCH_MSM8960
ifndef CONFIG_ARCH_MSM8X60
	obj-$(CONFIG_MSM_ONCRPCROUTER) += rpc_hsusb.o rpc_pmapp.o rpc_fsusb.o
endif
endif
endif
obj-$(CONFIG_MSM_SMD_TTY) += smd_tty.o
obj-$(CONFIG_MSM_SMD_QMI) += smd_qmi.o
obj-$(CONFIG_MSM_SMD_PKT) += smd_pkt.o
obj-$(CONFIG_MSM_DSPS) += msm_dsps.o
obj-$(CONFIG_MSM_ONCRPCROUTER) += smd_rpcrouter.o
obj-$(CONFIG_MSM_ONCRPCROUTER) += smd_rpcrouter_device.o
obj-$(CONFIG_DEBUG_FS) += smd_rpc_sym.o
obj-$(CONFIG_MSM_ONCRPCROUTER) += smd_rpcrouter_servers.o
obj-$(CONFIG_MSM_ONCRPCROUTER) += smd_rpcrouter_clients.o
obj-$(CONFIG_MSM_ONCRPCROUTER) += smd_rpcrouter_xdr.o
obj-$(CONFIG_MSM_ONCRPCROUTER) += rpcrouter_smd_xprt.o
obj-$(CONFIG_MSM_RPC_SDIO_XPRT) += rpcrouter_sdio_xprt.o
obj-$(CONFIG_MSM_RPC_PING) += ping_mdm_rpc_client.o ping_apps_server.o
obj-$(CONFIG_MSM_RPC_OEM_RAPI) += oem_rapi_client.o
obj-$(CONFIG_MSM_RPCSERVER_WATCHDOG) += rpc_server_dog_keepalive.o
obj-$(CONFIG_MSM_RPCSERVER_TIME_REMOTE) += rpc_server_time_remote.o
obj-$(CONFIG_MSM_DALRPC) += dal.o
obj-$(CONFIG_MSM_DALRPC_TEST) += dal_remotetest.o
obj-$(CONFIG_ARCH_MSM7X30) += dal_axi.o
obj-$(CONFIG_MSM_ADSP) += qdsp5/
obj-$(CONFIG_MSM_ADSP_COMP) += qdsp5_comp/
obj-$(CONFIG_MSM7KV2_AUDIO) += qdsp5v2/
obj-$(CONFIG_MSM7KV2_AUDIO) += htc_acoustic.o htc_acdb_7x30.o
obj-$(CONFIG_MSM7KV2_1X_AUDIO) += qdsp5v2_1x/
obj-$(CONFIG_MSM7KV2_1X_AUDIO) += htc_acoustic.o htc_acdb_7x30.o
obj-$(CONFIG_HTC_ACOUSTIC_QSD) += htc_acoustic_qsd.o
obj-$(CONFIG_MSM8X60_AUDIO) += qdsp6v2/
obj-$(CONFIG_MSM8X60_AUDIO) += htc_acoustic.o
obj-$(CONFIG_MSM_HW3D) += hw3d.o

ifdef CONFIG_PM
	obj-$(CONFIG_ARCH_MSM8960) += pm-8x60.o
	obj-$(CONFIG_ARCH_MSM8X60) += pm-8x60.o
	obj-$(CONFIG_ARCH_MSM9615) += pm-8x60.o
	obj-$(CONFIG_ARCH_QSD8X50) += pm2.o
	obj-$(CONFIG_ARCH_MSM7X30) += pm2.o
	obj-$(CONFIG_ARCH_MSM7X27) += pm2.o
	obj-$(CONFIG_ARCH_MSM7X27A) += pm2.o
	obj-$(CONFIG_ARCH_MSM7X25) += pm.o
	obj-$(CONFIG_ARCH_MSM7X01A) += pm.o
endif

ifdef CONFIG_ARCH_MSM8X60
	obj-$(CONFIG_CPU_IDLE) += cpuidle.o
	obj-$(CONFIG_MSM_WATCHDOG) += msm_watchdog.o
	obj-$(CONFIG_MSM_WATCHDOG) += htc_watchdog_monitor.o
endif

obj-$(CONFIG_SENSORS_AKM8976) += htc_akm_cal.o

obj-$(CONFIG_MACH_MAHIMAHI) += board-mahimahi.o board-mahimahi-panel.o
obj-$(CONFIG_MACH_MAHIMAHI) += board-mahimahi-keypad.o board-mahimahi-mmc.o
obj-$(CONFIG_MACH_MAHIMAHI) += board-mahimahi-rfkill.o htc_wifi_nvs.o
obj-$(CONFIG_MACH_MAHIMAHI) += board-mahimahi-wifi.o board-mahimahi-audio.o
obj-$(CONFIG_MACH_MAHIMAHI) += msm_vibrator.o
obj-$(CONFIG_MACH_MAHIMAHI) += board-mahimahi-microp.o
obj-$(CONFIG_MACH_MAHIMAHI) += htc_acoustic_qsd.o
obj-$(CONFIG_MACH_MAHIMAHI) += board-mahimahi-flashlight.o

obj-$(CONFIG_MACH_SUPERSONIC) += board-supersonic.o board-supersonic-panel.o
obj-$(CONFIG_MACH_SUPERSONIC) += board-supersonic-keypad.o board-supersonic-mmc.o
obj-$(CONFIG_MACH_SUPERSONIC) += board-supersonic-rfkill.o board-supersonic-audio.o
obj-$(CONFIG_MACH_SUPERSONIC) += board-supersonic-wifi.o htc_awb_cal.o
obj-$(CONFIG_MACH_SUPERSONIC) += board-supersonic-microp.o

# MSM7x30 boards
obj-$(CONFIG_ARCH_MSM7X30) += htc_wifi_nvs.o htc_awb_cal.o

obj-$(CONFIG_MACH_GLACIER) += board-glacier.o
obj-$(CONFIG_MACH_GLACIER) += board-glacier-audio.o
obj-$(CONFIG_MACH_GLACIER) += board-glacier-camera.o
obj-$(CONFIG_MACH_GLACIER) += board-glacier-keypad.o
obj-$(CONFIG_MACH_GLACIER) += board-glacier-microp.o
obj-$(CONFIG_MACH_GLACIER) += board-glacier-mmc.o board-glacier-wifi.o
obj-$(CONFIG_MACH_GLACIER) += board-glacier-panel.o
obj-$(CONFIG_MACH_GLACIER) += board-glacier-rfkill.o
obj-$(CONFIG_MACH_GLACIER) += htc_bluetooth.o htc_fmtx_rfkill.o

# MSM7200A boards
obj-$(CONFIG_MACH_HERO) += board-hero.o board-hero-panel.o board-hero-keypad.o
obj-$(CONFIG_MACH_HERO) += board-hero-rfkill.o
obj-$(CONFIG_MACH_HERO) += board-hero-mmc.o board-hero-wifi.o htc_wifi_nvs.o htc_awb_cal.o

# MSM7225 boards
obj-$(CONFIG_MACH_BUZZ) += board-buzz.o board-buzz-panel.o board-buzz-keypad.o board-buzz-microp.o
obj-$(CONFIG_MACH_BUZZ) += board-buzz-rfkill.o board-buzz-mmc.o board-buzz-wifi.o
obj-$(CONFIG_MACH_BEE) += board-bee.o board-bee-panel.o board-bee-keypad.o board-bee-microp.o
obj-$(CONFIG_MACH_BEE) += board-bee-rfkill.o board-bee-mmc.o board-bee-wifi.o

# MSM7227 boards
obj-$(CONFIG_MACH_LEGEND) += board-legend-rfkill.o
obj-$(CONFIG_MACH_LEGEND) += board-legend.o board-legend-keypad.o board-legend-panel.o
obj-$(CONFIG_MACH_LEGEND) += board-legend-mmc.o board-legend-wifi.o board-legend-microp.o
obj-$(CONFIG_MACH_LATTE) += board-latte-rfkill.o
obj-$(CONFIG_MACH_LATTE) += board-latte.o board-latte-keypad.o board-latte-panel.o
obj-$(CONFIG_MACH_LATTE) += board-latte-mmc.o board-latte-wifi.o board-latte-microp.o
obj-$(CONFIG_MACH_LIBERTY) += board-liberty-rfkill.o htc_bluetooth.o
obj-$(CONFIG_MACH_LIBERTY) += board-liberty.o board-liberty-keypad.o board-liberty-panel.o
obj-$(CONFIG_MACH_LIBERTY) += board-liberty-mmc.o board-liberty-wifi.o board-liberty-microp.o
obj-$(CONFIG_MACH_MARVEL) += board-marvel-rfkill.o htc_bluetooth.o
obj-$(CONFIG_MACH_MARVEL) += board-marvel.o board-marvel-keypad.o board-marvel-panel.o
obj-$(CONFIG_MACH_MARVEL) += board-marvel-mmc.o board-marvel-wifi.o board-marvel-microp.o
obj-$(CONFIG_MACH_MARVELC) += board-marvelc-rfkill.o htc_bluetooth.o
obj-$(CONFIG_MACH_MARVELC) += board-marvelc.o board-marvelc-keypad.o board-marvelc-panel.o
obj-$(CONFIG_MACH_MARVELC) += board-marvelc-mmc.o board-marvelc-wifi.o board-marvelc-microp.o
obj-$(CONFIG_MACH_ICONG) += board-icong-rfkill.o htc_bluetooth.o
obj-$(CONFIG_MACH_ICONG) += board-icong.o board-icong-keypad.o board-icong-panel.o
obj-$(CONFIG_MACH_ICONG) += board-icong-mmc.o board-icong-wifi.o board-icong-microp.o
obj-$(CONFIG_MACH_CHACHA) += board-chacha-rfkill.o htc_bluetooth.o
obj-$(CONFIG_MACH_CHACHA) += board-chacha.o board-chacha-keypad.o board-chacha-panel.o
obj-$(CONFIG_MACH_CHACHA) += board-chacha-mmc.o board-chacha-wifi.o board-chacha-microp.o

# MSM7600 boards
obj-$(CONFIG_MACH_DESIREC) += board-desirec.o board-desirec-panel.o board-desirec-keypad.o
obj-$(CONFIG_MACH_DESIREC) += board-desirec-rfkill.o
obj-$(CONFIG_MACH_DESIREC) += board-desirec-mmc.o board-desirec-wifi.o htc_wifi_nvs.o
obj-$(CONFIG_MACH_HEROC) += board-heroc.o board-heroc-panel.o board-heroc-keypad.o
obj-$(CONFIG_MACH_HEROC) += board-heroc-rfkill.o
obj-$(CONFIG_MACH_HEROC) += board-heroc-mmc.o board-heroc-wifi.o htc_wifi_nvs.o

# MSM8x60 boards
obj-$(CONFIG_MACH_PYRAMID) += board-pyramid.o
obj-$(CONFIG_MACH_PYRAMID) += board-pyramid-keypad.o
obj-$(CONFIG_MACH_PYRAMID) += board-pyramid-audio.o
obj-$(CONFIG_MACH_PYRAMID) += htc_bluetooth.o htc_sleep_clk.o board-pyramid-rfkill.o
obj-$(CONFIG_MACH_PYRAMID) += board-pyramid-mmc.o board-pyramid-wifi.o
obj-$(CONFIG_MACH_SHOOTER) += board-shooter.o board-shooter-audio.o
obj-$(CONFIG_MACH_SHOOTER) += htc_bluetooth.o htc_sleep_clk.o board-shooter-rfkill.o
obj-$(CONFIG_MACH_SHOOTER) += board-shooter-mmc.o board-shooter-wifi.o
obj-$(CONFIG_MACH_SHOOTER) += board-shooter-keypad.o
obj-$(CONFIG_MACH_SHOOTER) += board-shooter-3Dpanel.o
obj-$(CONFIG_MACH_DOUBLESHOT) += board-doubleshot.o
obj-$(CONFIG_MACH_DOUBLESHOT) += board-doubleshot-audio.o
obj-$(CONFIG_MACH_DOUBLESHOT) += board-doubleshot-camera.o
obj-$(CONFIG_MACH_DOUBLESHOT) += board-doubleshot-keypad.o
obj-$(CONFIG_MACH_DOUBLESHOT) += board-doubleshot-microp.o
obj-$(CONFIG_MACH_DOUBLESHOT) += board-doubleshot-mmc.o
obj-$(CONFIG_MACH_DOUBLESHOT) += board-doubleshot-panel.o
obj-$(CONFIG_MACH_DOUBLESHOT) += board-doubleshot-wifi.o
obj-$(CONFIG_MACH_DOUBLESHOT) += htc_bluetooth.o htc_sleep_clk.o board-doubleshot-rfkill.o htc_fmtx_rfkill.o
obj-$(CONFIG_MACH_LEAD) += board-lead.o
obj-$(CONFIG_MACH_LEAD) += board-lead-audio.o
obj-$(CONFIG_MACH_LEAD) += board-lead-keypad.o
obj-$(CONFIG_MACH_LEAD) += htc_bluetooth.o htc_sleep_clk.o board-lead-rfkill.o htc_fmtx_rfkill.o
obj-$(CONFIG_MACH_LEAD) += board-lead-mmc.o board-lead-wifi.o
obj-$(CONFIG_MACH_VERDI) += board-verdi.o
obj-$(CONFIG_MACH_VERDI) += board-verdi-keypad.o
obj-$(CONFIG_MACH_VERDI) += board-verdi-audio.o
obj-$(CONFIG_MACH_VERDI) += htc_bluetooth.o htc_sleep_clk.o board-verdi-rfkill.o
obj-$(CONFIG_MACH_VERDI) += board-verdi-mmc.o board-verdi-wifi.o
obj-$(CONFIG_MACH_VERDI_LTE) += board-verdi-lte.o
obj-$(CONFIG_MACH_VERDI_LTE) += board-verdi-lte-keypad.o
obj-$(CONFIG_MACH_VERDI_LTE) += board-verdi-lte-audio.o
obj-$(CONFIG_MACH_VERDI_LTE) += htc_bluetooth.o htc_sleep_clk.o board-verdi-lte-rfkill.o
obj-$(CONFIG_MACH_VERDI_LTE) += board-verdi-lte-mmc.o board-verdi-lte-wifi.o
obj-$(CONFIG_MACH_VIGOR) += board-vigor.o
obj-$(CONFIG_MACH_VIGOR) += board-vigor-audio.o
obj-$(CONFIG_MACH_VIGOR) += board-vigor-keypad.o
obj-$(CONFIG_MACH_VIGOR) += htc_bluetooth.o htc_sleep_clk.o board-vigor-rfkill.o htc_fmtx_rfkill.o
obj-$(CONFIG_MACH_VIGOR) += board-vigor-mmc.o board-vigor-wifi.o

obj-$(CONFIG_ARCH_MSM7X00A) += msm_vibrator.o devices_htc.o
obj-$(CONFIG_ARCH_MSM7225) += msm_vibrator.o devices_htc.o htc_wifi_nvs.o htc_awb_cal.o htc_bluetooth.o
obj-$(CONFIG_ARCH_MSM7227) += msm_vibrator.o devices_htc.o htc_wifi_nvs.o htc_awb_cal.o
obj-$(CONFIG_ARCH_QSD8X50) += msm_vibrator.o devices_htc.o
obj-$(CONFIG_ARCH_MSM7X30) += msm_vibrator.o devices_htc.o devices-msm7x30.o
obj-$(CONFIG_ARCH_MSM8X60) += devices_htc.o htc_wifi_nvs.o htc_awb_cal.o
obj-$(CONFIG_ARCH_MSM_FLASHLIGHT) += msm_flashlight.o

obj-$(CONFIG_TROUT_BATTCHG_DOCK) += htc_one_wire.o
obj-$(CONFIG_TROUT_BATTCHG) += htc_battery_trout.o
ifdef CONFIG_ARCH_MSM8X60
obj-$(CONFIG_HTC_BATTCHG) += htc_battery_8x60.o
else
obj-$(CONFIG_HTC_BATTCHG) += htc_battery.o
endif
obj-$(CONFIG_HTC_BATT_CORE) += htc_battery_core.o
obj-$(CONFIG_HTC_PWRSINK) += htc_pwrsink.o
obj-$(CONFIG_HTC_HEADSET_MGR) += htc_headset_mgr.o
obj-$(CONFIG_HTC_HEADSET_H2W) += htc_headset_h2w.o
obj-$(CONFIG_HTC_HEADSET_GPIO) += htc_headset_gpio.o
obj-$(CONFIG_HTC_HEADSET_MICROP) += htc_headset_microp.o
obj-$(CONFIG_HTC_HEADSET_PMIC) += htc_headset_pmic.o
obj-$(CONFIG_HTC_HEADSET_MISC) += htc_headset_misc.o
obj-$(CONFIG_HTC_HEADSET_8X60) += htc_headset_8x60.o
obj-$(CONFIG_MSM_RMT_STORAGE_SERVER) += rmt_storage_server.o
obj-$(CONFIG_MSM_RMT_STORAGE_CLIENT) += rmt_storage_client.o
ifeq ($(CONFIG_MSM_SSBI),y)
obj-$(CONFIG_PMIC8058) += pmic8058-mpp.o
else
ifeq ($(CONFIG_ARCH_MSM8X60),y)
obj-$(CONFIG_PMIC8058) += pmic8058-gpio-8x60.o pmic8058-mpp-8x60.o
else
obj-$(CONFIG_PMIC8058) += pmic8058-gpio.o pmic8058-mpp.o
endif
endif
obj-$(CONFIG_MSM_NPA) += npa.o npa_resources.o msm_pm_qos.o msm_reqs.o
obj-$(CONFIG_MSM_NPA_REMOTE) += npa_remote.o
obj-$(CONFIG_MSM_NPA_REMOTE_ONCRPC) += npa_remote_rpc_client.o

obj-$(CONFIG_PERFLOCK) += perflock.o
obj-$(CONFIG_PERFLOCK) += htc_set_perflock.o
obj-$(CONFIG_MICROP_COMMON) += atmega_microp_common.o
obj-$(CONFIG_TOUCHSCREEN_TSSC_MANAGER) += htc_tp_cal.o

obj-$(CONFIG_MSM_RPM) += rpm.o rpm_resources.o
obj-$(CONFIG_MSM_RPM_STATS_LOG) += rpm_stats.o
obj-$(CONFIG_MSM_RPM_LOG) += rpm_log.o
obj-$(CONFIG_MSM_XO) += msm_xo.o
obj-$(CONFIG_MSM_BUS_SCALING) += msm_bus/

obj-$(CONFIG_SMMU) += iommu.o iommu_dev.o
obj-$(CONFIG_SMMU_8X60) += devices-msm8x60-iommu.o

obj-$(CONFIG_MSM_EVENT_TIMER) += event_timer.o

ifdef CONFIG_VCM
obj-$(CONFIG_ARCH_MSM8X60) += board-msm8x60-vcm.o
endif

obj-$(CONFIG_ARCH_MSM7X30) += gpiomux-7x30.o gpiomux-v1.o gpiomux.o
obj-$(CONFIG_ARCH_QSD8X50) += gpiomux-8x50.o gpiomux-v1.o gpiomux.o
obj-$(CONFIG_ARCH_MSM8X60) += gpiomux-8x60.o gpiomux-v2.o gpiomux.o

ifdef CONFIG_MSM_V2_TLMM
obj-y	+= gpio-v2.o
else
obj-y += generic_gpio.o
endif
obj-$(CONFIG_MSM_SLEEP_STATS) += msm_sleep_stats.o
obj-$(CONFIG_MSM_SHOW_RESUME_IRQ) += msm_show_resume_irq.o
obj-$(CONFIG_MACH_MAHIMAHI) += board-mahimahi-tpa2018d1.o
obj-$(CONFIG_MACH_MAHIMAHI) += board-mahimahi-smb329.o

obj-$(CONFIG_ARCH_MSM_SCORPION) += pmu.o
obj-$(CONFIG_ARCH_MSM_SCORPIONMP) += pmu.o

ifeq ($(CONFIG_ARCH_MSM8X60),y)
obj-$(CONFIG_MSM8X60_SSBI) += ssbi_8x60.o
else
obj-$(CONFIG_MSM_SSBI) += ssbi.o
endif

obj-$(CONFIG_MSM_PROC_COMM_REGULATOR) += proccomm-regulator.o
ifdef CONFIG_MSM_PROC_COMM_REGULATOR
obj-$(CONFIG_MACH_GLACIER) += board-glacier-regulator.o
endif
