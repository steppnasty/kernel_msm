
# core framebuffer
#
obj-y := msm_fb.o logo.o
ifeq ($(CONFIG_FB_MSM_LOGO),y)
obj-y += logo.o
endif

# MDP DMA/PPP engine
#
obj-y += mdp.o
obj-$(CONFIG_FB_MSM_OVERLAY) += mdp4_util.o
obj-$(CONFIG_FB_MSM_OVERLAY) += mdp4_overlay.o
obj-$(CONFIG_FB_MSM_MDDI) += mdp4_overlay_mddi.o
obj-$(CONFIG_FB_MSM_LCDC) += mdp4_overlay_lcdc.o

obj-$(CONFIG_DEBUG_FS) += mdp_debugfs.o

obj-$(CONFIG_FB_MSM_LEGACY_MDP) += mdp_hw_legacy.o

obj-$(CONFIG_FB_MSM_MDP_PPP) += mdp_ppp.o
obj-$(CONFIG_MSM_MDP22) += mdp_ppp22.o
obj-$(CONFIG_MSM_MDP30) += mdp_ppp22.o
obj-$(CONFIG_MSM_MDP302)+= mdp_ppp22.o
obj-$(CONFIG_MSM_MDP31) += mdp_ppp31.o

obj-y += mdp_dma.o
obj-y += mdp_dma_s.o
obj-y += mdp_vsync.o
obj-y += mdp_cursor.o
obj-y += mdp_dma_tv.o

# LCDC
obj-$(CONFIG_FB_MSM_LCDC) += lcdc.o

# MDDI
msm_mddi-objs := mddi.o mddihost.o mddihosti.o
obj-$(CONFIG_FB_MSM_MDDI) += msm_mddi.o

# MDDI client/panel drivers
#
ifdef CONFIG_ARCH_MSM7X30
obj-$(CONFIG_MACH_GLACIER) += mddi_nt21856_wvga.o
endif

# MDP LCD controller driver
obj-$(CONFIG_FB_MSM_LCDC) += mdp_lcdc.o
obj-$(CONFIG_FB_MSM_DTV) += mdp_dtv.o
obj-$(CONFIG_FB_MSM_TVOUT) += tvenc.o tvfb.o

# MSM FB Panel
obj-y += msm_fb_panel.o

obj-$(CONFIG_MSM_VIDC_720P) += vidc/

