;--------------------------------------------------------
; File Created by SDCC : free open source ISO C Compiler 
; Version 4.2.2 #13448 (MINGW32)
;--------------------------------------------------------
	.file	"main.c"
;	.optsdcc -mmcs51
	; --model-large

	
;--------------------------------------------------------
; Public variables in this module
;--------------------------------------------------------
	.globl	_main
	.globl	_WCAN_Init
;--------------------------------------------------------
; special function registers
;--------------------------------------------------------
	.section .bdata.i51,"aw" ;reg_name ;area
_SCR_IO_PAGE	=	0x008f
_SCR_IO_P00_IOCR0	=	0x0090
_SCR_IO_P00_IOCR1	=	0x0091
_SCR_IO_P00_IOCR2	=	0x0092
_SCR_IO_P00_IOCR3	=	0x0093
_SCR_IO_P00_IOCR4	=	0x0094
_SCR_IO_P00_IOCR5	=	0x0095
_SCR_IO_P00_IOCR6	=	0x0096
_SCR_IO_P00_IOCR7	=	0x0097
_SCR_IO_P00_PDR0	=	0x0090
_SCR_IO_P00_PDR2	=	0x0091
_SCR_IO_P00_PDR4	=	0x0092
_SCR_IO_P00_PDR6	=	0x0093
_SCR_IO_P00_PDISC	=	0x0095
_SCR_IO_P00_OUT	=	0x0090
_SCR_IO_P00_OMSR	=	0x0092
_SCR_IO_P00_OMCR	=	0x0093
_SCR_IO_P00_OMTR	=	0x0094
_SCR_IO_P00_IN	=	0x0091
_SCR_IO_P01_IOCR0	=	0x0098
_SCR_IO_P01_IOCR1	=	0x0099
_SCR_IO_P01_IOCR2	=	0x009a
_SCR_IO_P01_IOCR3	=	0x009b
_SCR_IO_P01_IOCR4	=	0x009c
_SCR_IO_P01_IOCR5	=	0x009d
_SCR_IO_P01_IOCR6	=	0x009e
_SCR_IO_P01_IOCR7	=	0x009f
_SCR_IO_P01_PDR0	=	0x0098
_SCR_IO_P01_PDR2	=	0x0099
_SCR_IO_P01_PDR4	=	0x009a
_SCR_IO_P01_PDR6	=	0x009b
_SCR_IO_P01_PDISC	=	0x009d
_SCR_IO_P01_OUT	=	0x0098
_SCR_IO_P01_OMSR	=	0x009a
_SCR_IO_P01_OMCR	=	0x009b
_SCR_IO_P01_OMTR	=	0x009c
_SCR_IO_P01_IN	=	0x0099
_SCR_SCU_MODPISEL0	=	0x00f2
_SCR_SCU_MODPISEL1	=	0x00f3
_SCR_SCU_MODPISEL2	=	0x00f4
_SCR_SCU_MODPISEL3	=	0x00f5
_SCR_SCU_MODPISEL4	=	0x00f6
_SCR_SCU_MODPISEL5	=	0x00f7
_SCR_SCU_PAGE	=	0x00f1
_SCR_SCU_CMCON	=	0x00fa
_SCR_SCU_DBG_MODSUSP	=	0x00f9
_SCR_SCU_RSTST	=	0x00f7
_SCR_SCU_MRSTST	=	0x00f8
_SCR_SCU_RSTCON	=	0x00f9
_SCR_SCU_STDBYWKP	=	0x00fc
_SCR_SCU_SR	=	0x00f9
_SCR_SCU_PMCON1	=	0x00fb
_SCR_WCAN_PAGE	=	0x00b8
_SCR_WCAN_CFG	=	0x00b0
_SCR_WCAN_INTMRSLT	=	0x00b1
_SCR_WCAN_INTESTAT0	=	0x00b0
_SCR_WCAN_INTESTAT1	=	0x00b1
_SCR_WCAN_FRMERRCNT	=	0x00b2
_SCR_WCAN_INTESCLR0	=	0x00b3
_SCR_WCAN_INTESCLR1	=	0x00b4
_SCR_WCAN_BTL1_CTRL	=	0x00b5
_SCR_WCAN_BTL2_CTRL	=	0x00b6
_SCR_WCAN_CDR_CTRL	=	0x00b2
_SCR_WCAN_CDR_UPPER_CTRL	=	0x00b3
_SCR_WCAN_CDR_LOWER_CTRL	=	0x00b4
_SCR_WCAN_CDR_MEAS_HIGH	=	0x00b5
_SCR_WCAN_CDR_MEAS_LOW	=	0x00b6
_SCR_WCAN_FD_CTRL	=	0x00b7
_SCR_WCAN_DLC_CTRL	=	0x00b7
_SCR_WCAN_ID0_CTRL	=	0x00b0
_SCR_WCAN_ID1_CTRL	=	0x00b1
_SCR_WCAN_ID2_CTRL	=	0x00b2
_SCR_WCAN_ID3_CTRL	=	0x00b3
_SCR_WCAN_MASK_ID0_CTRL	=	0x00b4
_SCR_WCAN_MASK_ID1_CTRL	=	0x00b5
_SCR_WCAN_MASK_ID2_CTRL	=	0x00b6
_SCR_WCAN_MASK_ID3_CTRL	=	0x00b7
_SCR_WCAN_DATA0_CTRL	=	0x00b0
_SCR_WCAN_DATA1_CTRL	=	0x00b1
_SCR_WCAN_DATA2_CTRL	=	0x00b2
_SCR_WCAN_DATA3_CTRL	=	0x00b3
_SCR_WCAN_DATA4_CTRL	=	0x00b4
_SCR_WCAN_DATA5_CTRL	=	0x00b5
_SCR_WCAN_DATA6_CTRL	=	0x00b6
_SCR_WCAN_DATA7_CTRL	=	0x00b7
_SCR_IEN0	=	0x00d8
_SCR_IEN1	=	0x00d1
_SCR_SCU_NMICON	=	0x00f3
_SCR_SCU_NMISR	=	0x00f2
_SCR_SCU_EXICON0	=	0x00f4
_SCR_SCU_EXICON1	=	0x00f5
_SCR_SCU_EXICON2	=	0x00f6
_SCR_SCU_EXICON3	=	0x00f7
_SCR_SCU_IRCON0	=	0x00f2
_SCR_SCU_IRCON1	=	0x00f3
_SCR_SCU_IRCON2	=	0x00f4
_SCR_IP	=	0x00dc
_SCR_IPH	=	0x00d2
_SCR_IP1	=	0x00db
_SCR_IPH1	=	0x00d3
_SCR_SCU_MODIEN	=	0x00f8
_SCR_SCU_SCRINTEXCHG	=	0x00f5
_SCR_SCU_TCINTEXCHG	=	0x00f6
_SCR_SP	=	0x00d4
_SCR_DPL	=	0x00d5
_SCR_DPH	=	0x00d6
_SCR_ACC	=	0x00e0
_SCR_B	=	0x00da
_SCR_PSW	=	0x00d0
_SCR_EO	=	0x00d7
_SCR_PCON	=	0x00d9
_SCR_XADDRH	=	0x0087
_SCR_SYSCON0	=	0x0088
_SCR_PASSWD	=	0x0086
;--------------------------------------------------------
; special function bits
;--------------------------------------------------------
	.section .bdata.i51,"aw" ;reg_name ;area
_SCR_IO_P00_OUT_0	=	0x0090
_SCR_IO_P00_OUT_1	=	0x0091
_SCR_IO_P00_OUT_2	=	0x0092
_SCR_IO_P00_OUT_3	=	0x0093
_SCR_IO_P00_OUT_4	=	0x0094
_SCR_IO_P00_OUT_5	=	0x0095
_SCR_IO_P00_OUT_6	=	0x0096
_SCR_IO_P00_OUT_7	=	0x0097
_SCR_IO_P01_OUT_0	=	0x0098
_SCR_IO_P01_OUT_1	=	0x0099
_SCR_IO_P01_OUT_2	=	0x009a
_SCR_IO_P01_OUT_3	=	0x009b
_SCR_IO_P01_OUT_4	=	0x009c
_SCR_IO_P01_OUT_5	=	0x009d
_SCR_IO_P01_OUT_6	=	0x009e
_SCR_IO_P01_OUT_7	=	0x009f
_SCR_SCU_MRSTST_SMURST	=	0x00f8
_SCR_SCU_MRSTST_RST	=	0x00f9
_SCR_IEN0_EX0	=	0x00d8
_SCR_IEN0_ET0	=	0x00d9
_SCR_IEN0_EX1	=	0x00da
_SCR_IEN0_ET1	=	0x00db
_SCR_IEN0_ES	=	0x00dc
_SCR_IEN0_ET2	=	0x00dd
_SCR_IEN0_EA	=	0x00df
_SCR_SCU_MODIEN_EIREN	=	0x00f8
_SCR_SCU_MODIEN_TIREN	=	0x00f9
_SCR_SCU_MODIEN_RIREN	=	0x00fa
_SCR_SCU_MODIEN_FEEN	=	0x00fb
_SCR_SCU_MODIEN_FFEN	=	0x00fc
_SCR_PSW_P	=	0x00d0
_SCR_PSW_F1	=	0x00d1
_SCR_PSW_OV	=	0x00d2
_SCR_PSW_F0	=	0x00d5
_SCR_PSW_AC	=	0x00d6
_SCR_PSW_CY	=	0x00d7
_SCR_SYSCON0_RMAP	=	0x0088
_SCR_SYSCON0_AMSEL	=	0x008a
;--------------------------------------------------------
; uninitialized external ram data
;--------------------------------------------------------
	.section .xdata.i51,"aw" ;xdata_name ;area
_Scr_set_fsys_div_65536_1:
	.ds.b	1
_Scr_set_fsys_70kHz_cmcon_65537_5:
	.ds.b	1
_WCAN_Init_TempVar_65536_11:
	.ds.b	1
_main_cmcon_327680_19:
	.ds.b	1
;--------------------------------------------------------
; code
;--------------------------------------------------------
;------------------------------------------------------------
;Allocation info for local variables in function 'Scr_set_fsys'
;------------------------------------------------------------
;div                       Allocated with name '_Scr_set_fsys_div_65536_1'
;------------------------------------------------------------
;	../SCR/scr_common.h:61: SCR_INLINE void Scr_set_fsys(ScrClkDiv div)
;	-----------------------------------------
;	 function Scr_set_fsys
;	-----------------------------------------
	.section .text.code.Scr_set_fsys,"ax" ;code for function Scr_set_fsys
	.type   Scr_set_fsys, @function
_Scr_set_fsys:
	.using 0
	mov	a,dpl
	mov	dptr,#_Scr_set_fsys_div_65536_1
	movx	@dptr,a
;	../SCR/scr_common.h:63: SCR_UNLOCK_PROTECTED_BITS();          /* Open access to write protected bits  */
	mov	_SCR_PASSWD,#0x98
;	../SCR/scr_common.h:65: SCR_SET_SCU_PAGE(MOD_PAGE_1);
	mov	_SCR_SCU_PAGE,#0x01
;	../SCR/scr_common.h:70: SCR_SCU_CMCON = ((div & DIV_MASK) | OSCWAKE_MASK);
	movx	a,@dptr
	anl	a,#0x0F
	orl	a,#0x10
	mov	_SCR_SCU_CMCON,a
;	../SCR/scr_common.h:72: SCR_LOCK_PROTECTED_BITS();            /* Close access to write protected bits */
	mov	_SCR_PASSWD,#0xA8
.00101:
;	../SCR/scr_common.h:73: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'Scr_set_fsys_70kHz'
;------------------------------------------------------------
;cmcon                     Allocated with name '_Scr_set_fsys_70kHz_cmcon_65537_5'
;------------------------------------------------------------
;	../SCR/scr_common.h:75: SCR_INLINE void Scr_set_fsys_70kHz(void)
;	-----------------------------------------
;	 function Scr_set_fsys_70kHz
;	-----------------------------------------
	.section .text.code.Scr_set_fsys_70kHz,"ax" ;code for function Scr_set_fsys_70kHz
	.type   Scr_set_fsys_70kHz, @function
_Scr_set_fsys_70kHz:
	.using 0
;	../SCR/scr_common.h:77: SCR_UNLOCK_PROTECTED_BITS();          /* Open access to write protected bits  */
	mov	_SCR_PASSWD,#0x98
;	../SCR/scr_common.h:79: SCR_SET_SCU_PAGE(MOD_PAGE_1);
	mov	_SCR_SCU_PAGE,#0x01
;	../SCR/scr_common.h:80: uint8 cmcon = SCR_SCU_CMCON;
	mov	dptr,#_Scr_set_fsys_70kHz_cmcon_65537_5
	mov	a,_SCR_SCU_CMCON
	movx	@dptr,a
;	../SCR/scr_common.h:85: SCR_SCU_CMCON = (OSCPD_MASK | (cmcon & DIV_MASK));
	movx	a,@dptr
	anl	a,#0x0F
	orl	a,#0x20
	mov	_SCR_SCU_CMCON,a
;	../SCR/scr_common.h:87: SCR_LOCK_PROTECTED_BITS();            /* Close access to write protected bits */
	mov	_SCR_PASSWD,#0xA8
.00103:
;	../SCR/scr_common.h:88: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'Scr_is_in_standby'
;------------------------------------------------------------
;	../SCR/scr_common.h:90: SCR_INLINE boolean Scr_is_in_standby(void)
;	-----------------------------------------
;	 function Scr_is_in_standby
;	-----------------------------------------
	.section .text.code.Scr_is_in_standby,"ax" ;code for function Scr_is_in_standby
	.type   Scr_is_in_standby, @function
_Scr_is_in_standby:
	.using 0
;	../SCR/scr_common.h:92: return (SCR_SCU_SR == STBY_MASK);
	mov	a,#0x08
	cjne	a,_SCR_SCU_SR,.00107
	mov	a,#0x01
	sjmp	.00108
.00107:
	clr	a
.00108:
;	../SCR/scr_common.h:93: }
	mov	dpl,a
.00105:
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'Scr_request_idle'
;------------------------------------------------------------
;	../SCR/scr_common.h:95: SCR_INLINE void Scr_request_idle(void)
;	-----------------------------------------
;	 function Scr_request_idle
;	-----------------------------------------
	.section .text.code.Scr_request_idle,"ax" ;code for function Scr_request_idle
	.type   Scr_request_idle, @function
_Scr_request_idle:
	.using 0
;	../SCR/scr_common.h:97: SCR_PCON |= IDLE_MASK;
	orl	_SCR_PCON,#0x01
.00109:
;	../SCR/scr_common.h:98: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'WCAN_Init'
;------------------------------------------------------------
;result                    Allocated with name '_WCAN_Init_result_65536_11'
;TempVar                   Allocated with name '_WCAN_Init_TempVar_65536_11'
;------------------------------------------------------------
;	../SCR/main.c:41: char WCAN_Init(void)
;	-----------------------------------------
;	 function WCAN_Init
;	-----------------------------------------
	.section .text.code.WCAN_Init,"ax" ;code for function WCAN_Init
	.type   WCAN_Init, @function
_WCAN_Init:
	.using 0
;	../SCR/main.c:44: unsigned char volatile TempVar = 0;
	mov	dptr,#_WCAN_Init_TempVar_65536_11
	clr	a
	movx	@dptr,a
;	../SCR/main.c:52: SCR_IO_PAGE = 0x1;
	mov	_SCR_IO_PAGE,#0x01
;	../SCR/main.c:54: SCR_IO_P01_IOCR4 = 0x20 ; // WCANRXDG(P33.12)
	mov	_SCR_IO_P01_IOCR4,#0x20
;	../SCR/main.c:56: SCR_IO_PAGE = 0x2;
	mov	_SCR_IO_PAGE,#0x02
;	../SCR/main.c:57: SCR_IO_P01_PDISC = 0xFB ; // WCANRXDG(P33.12)
	mov	_SCR_IO_P01_PDISC,#0xFB
;	../SCR/main.c:59: SCR_SCU_PAGE = 0x1;
	mov	_SCR_SCU_PAGE,#0x01
;	../SCR/main.c:60: SCR_SCU_RSTCON = 0x0 ; // Disable WDT and ECC reset
;	1-genFromRTrack replaced	mov	_SCR_SCU_RSTCON,#0x00
	mov	_SCR_SCU_RSTCON,a
;	../SCR/main.c:61: SCR_SCU_STDBYWKP = 0x4; // Select WCAN as wakeup source
	mov	_SCR_SCU_STDBYWKP,#0x04
;	../SCR/main.c:79: SCR_SCU_PAGE = 1;
	mov	_SCR_SCU_PAGE,#0x01
;	../SCR/main.c:80: SCR_SCU_PMCON1 &= ~(1<< 3); // enable WCAN clock (bit_3), default: WCAN is disabled
	anl	_SCR_SCU_PMCON1,#0xF7
;	../SCR/main.c:82: SCR_SCU_PAGE = 0x2;
	mov	_SCR_SCU_PAGE,#0x02
;	../SCR/main.c:83: TempVar = SCR_SCU_MODPISEL0 & MaskWCANRXDIS; // Mask WCAN bits
	mov	a,_SCR_SCU_MODPISEL0
	mov	dptr,#_WCAN_Init_TempVar_65536_11
	anl	a,#0x0F
	movx	@dptr,a
;	../SCR/main.c:84: SCR_SCU_MODPISEL0 = TempVar | (SetWCANRXDG<<4); // Enable CAN on P33.12
	movx	a,@dptr
	orl	a,#0x60
	mov	_SCR_SCU_MODPISEL0,a
;	../SCR/main.c:86: SCR_WCAN_PAGE = 0x0 ;
	mov	_SCR_WCAN_PAGE,#0x00
;	../SCR/main.c:87: SCR_WCAN_CFG = (1<<3)|(0<<2)|(1<<0) ; // CCE=1, SELWK_EN=0, WCAN_EN=1 --> according to UM
	mov	_SCR_WCAN_CFG,#0x09
;	../SCR/main.c:88: SCR_WCAN_INTMRSLT = (0<<3)|(1<<2)|(0<<1)|(0<<0); // WUP=0, WUF=1, ERR=0, CANTO=0 --> enable WUF interrupt
	mov	_SCR_WCAN_INTMRSLT,#0x04
;	../SCR/main.c:93: SCR_WCAN_PAGE = 0x1;
	mov	_SCR_WCAN_PAGE,#0x01
;	../SCR/main.c:94: SCR_WCAN_DLC_CTRL = 0x8 ; // 8 bytes of data
	mov	_SCR_WCAN_DLC_CTRL,#0x08
;	../SCR/main.c:95: SCR_WCAN_BTL1_CTRL = 0x64 ; // Configure Baud Rate of 500 kbits/s (see table 747 and register WCAN_BTL2_CTRL)
	mov	_SCR_WCAN_BTL1_CTRL,#0x64
;	../SCR/main.c:96: SCR_WCAN_BTL2_CTRL = (1<<6) | (0x33<<0) ; // BRP=01(Divide by 2) and SP=0x33 represents ~80%SP
	mov	_SCR_WCAN_BTL2_CTRL,#0x73
;	../SCR/main.c:102: SCR_WCAN_PAGE = 0x0 ;
	mov	_SCR_WCAN_PAGE,#0x00
;	../SCR/main.c:103: SCR_WCAN_CFG &= ~((1<<3)|(1<<2)|(0<<0)); // reset CCE=1, SELWK_EN=1, WCAN_EN=0 according to UM
	anl	_SCR_WCAN_CFG,#0xF3
;	../SCR/main.c:108: SCR_WCAN_PAGE = 0x2 ;
	mov	_SCR_WCAN_PAGE,#0x02
;	../SCR/main.c:109: SCR_WCAN_ID0_CTRL = 0x00 ; // ID = 0x555 (11-bit)
	mov	_SCR_WCAN_ID0_CTRL,#0x00
;	../SCR/main.c:110: SCR_WCAN_ID1_CTRL = 0x00 ;
	mov	_SCR_WCAN_ID1_CTRL,#0x00
;	../SCR/main.c:111: SCR_WCAN_ID2_CTRL = 0x04 ;
	mov	_SCR_WCAN_ID2_CTRL,#0x04
;	../SCR/main.c:112: SCR_WCAN_ID3_CTRL = 0x54 ; // RTR=0 ; IDE = 0
	mov	_SCR_WCAN_ID3_CTRL,#0x54
;	../SCR/main.c:114: SCR_WCAN_MASK_ID0_CTRL = 0xFF ;
	mov	_SCR_WCAN_MASK_ID0_CTRL,#0xFF
;	../SCR/main.c:115: SCR_WCAN_MASK_ID1_CTRL = 0xFF ;
	mov	_SCR_WCAN_MASK_ID1_CTRL,#0xFF
;	../SCR/main.c:116: SCR_WCAN_MASK_ID2_CTRL = 0xFF ;
	mov	_SCR_WCAN_MASK_ID2_CTRL,#0xFF
;	../SCR/main.c:117: SCR_WCAN_MASK_ID3_CTRL = 0xFF ;
	mov	_SCR_WCAN_MASK_ID3_CTRL,#0xFF
;	../SCR/main.c:119: SCR_WCAN_PAGE = 0x3 ;
	mov	_SCR_WCAN_PAGE,#0x03
;	../SCR/main.c:120: SCR_WCAN_DATA7_CTRL = 0x01 ; // Data Frame = 0x01 23 45 67 89 ab cd ef (byte8...byte1)
	mov	_SCR_WCAN_DATA7_CTRL,#0x01
;	../SCR/main.c:121: SCR_WCAN_DATA6_CTRL = 0x23 ;
	mov	_SCR_WCAN_DATA6_CTRL,#0x23
;	../SCR/main.c:122: SCR_WCAN_DATA5_CTRL = 0x45 ;
	mov	_SCR_WCAN_DATA5_CTRL,#0x45
;	../SCR/main.c:123: SCR_WCAN_DATA4_CTRL = 0x67 ;
	mov	_SCR_WCAN_DATA4_CTRL,#0x67
;	../SCR/main.c:124: SCR_WCAN_DATA3_CTRL = 0x89 ;
	mov	_SCR_WCAN_DATA3_CTRL,#0x89
;	../SCR/main.c:125: SCR_WCAN_DATA2_CTRL = 0xab ;
	mov	_SCR_WCAN_DATA2_CTRL,#0xAB
;	../SCR/main.c:126: SCR_WCAN_DATA1_CTRL = 0xcd ;
	mov	_SCR_WCAN_DATA1_CTRL,#0xCD
;	../SCR/main.c:127: SCR_WCAN_DATA0_CTRL = 0xef ;
	mov	_SCR_WCAN_DATA0_CTRL,#0xEF
;	../SCR/main.c:132: SCR_WCAN_PAGE = 0x0 ;
	mov	_SCR_WCAN_PAGE,#0x00
;	../SCR/main.c:133: SCR_WCAN_CFG |= ((0<<3)|(1<<2)|(0<<0)); //set CCE=0, SELWK_EN=1, WCAN_EN=0
	orl	_SCR_WCAN_CFG,#0x04
;	../SCR/main.c:138: SCR_WCAN_PAGE = 0x1 ;
	mov	_SCR_WCAN_PAGE,#0x01
;	../SCR/main.c:139: while ((SCR_WCAN_INTESTAT0 & 0x1) != 0x1) { } ; // selective wake up enable protocol handle is activated
.00111:
	mov	r6,_SCR_WCAN_INTESTAT0
	anl	ar6,#0x01
	mov	r7,#0x00
	cjne	r6,#0x01,.00124
	cjne	r7,#0x00,.00124
	sjmp	.00125
.00124:
	sjmp	.00111
.00125:
;	../SCR/main.c:141: return result;
	mov	dpl,#0x00
.00114:
;	../SCR/main.c:142: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'main'
;------------------------------------------------------------
;cmcon                     Allocated with name '_main_cmcon_327680_19'
;------------------------------------------------------------
;	../SCR/main.c:144: void main(void)
;	-----------------------------------------
;	 function main
;	-----------------------------------------
	.section .text.code.main,"ax" ;code for function main
	.type   main, @function
_main:
	.using 0
;	../SCR/main.c:146: *G_COUNTER_ADDR = 0u;
	mov	dptr,#0x1760
	clr	a
	movx	@dptr,a
;	../SCR/scr_common.h:77: SCR_UNLOCK_PROTECTED_BITS();          /* Open access to write protected bits  */
	mov	_SCR_PASSWD,#0x98
;	../SCR/scr_common.h:79: SCR_SET_SCU_PAGE(MOD_PAGE_1);
	mov	_SCR_SCU_PAGE,#0x01
;	../SCR/scr_common.h:80: uint8 cmcon = SCR_SCU_CMCON;
	mov	dptr,#_main_cmcon_327680_19
	mov	a,_SCR_SCU_CMCON
	movx	@dptr,a
;	../SCR/scr_common.h:85: SCR_SCU_CMCON = (OSCPD_MASK | (cmcon & DIV_MASK));
	movx	a,@dptr
	anl	a,#0x0F
	orl	a,#0x20
	mov	_SCR_SCU_CMCON,a
;	../SCR/scr_common.h:87: SCR_LOCK_PROTECTED_BITS();            /* Close access to write protected bits */
	mov	_SCR_PASSWD,#0xA8
;	../SCR/main.c:148: WCAN_Init();
	lcall	_WCAN_Init
;	../SCR/main.c:150: while(1)
.00127:
;	../SCR/main.c:153: (*G_COUNTER_ADDR)++;
	mov	dptr,#0x1760
	movx	a,@dptr
	mov	r7,a
	inc	r7
	mov	dptr,#0x1760
	mov	a,r7
	movx	@dptr,a
	sjmp	.00127
.00130:
;	../SCR/main.c:155: }
	ret
