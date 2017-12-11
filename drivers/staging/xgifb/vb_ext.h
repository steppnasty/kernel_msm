#ifndef  _VBEXT_
#define  _VBEXT_

struct DWORDREGS {
    unsigned long    Eax, Ebx, Ecx, Edx, Esi, Edi, Ebp;
};

struct WORDREGS {
    unsigned short ax, hi_ax, bx, hi_bx, cx, hi_cx, dx, hi_dx, si,
	    hi_si, di, hi_di, bp, hi_bp;
};

struct BYTEREGS {
    UCHAR   al, ah, hi_al, hi_ah, bl, bh, hi_bl, hi_bh, cl, ch, hi_cl, hi_ch, dl, dh, hi_dl, hi_dh;
};

typedef union   _X86_REGS    {
    struct  DWORDREGS e;
    struct  WORDREGS x;
    struct  BYTEREGS h;
} X86_REGS, *PX86_REGS;

extern   void     XGI_XG21Fun14( PXGI_HW_DEVICE_INFO pXGIHWDE, PX86_REGS pBiosArguments);
extern void XGISetDPMS(PXGI_HW_DEVICE_INFO pXGIHWDE,
		       unsigned long VESA_POWER_STATE);
extern   void     XGI_GetSenseStatus( PXGI_HW_DEVICE_INFO HwDeviceExtension , PVB_DEVICE_INFO pVBInfo );
extern   void     XGINew_SetModeScratch ( PXGI_HW_DEVICE_INFO HwDeviceExtension , PVB_DEVICE_INFO pVBInfo ) ;
extern   void 	  ReadVBIOSTablData( UCHAR ChipType , PVB_DEVICE_INFO pVBInfo);
extern unsigned short XGINew_SenseLCD(PXGI_HW_DEVICE_INFO,
				      PVB_DEVICE_INFO pVBInfo);

#endif
