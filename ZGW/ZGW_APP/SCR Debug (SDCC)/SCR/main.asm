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
	.globl	_SCR_RtcCompareIsr
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
_SCR_RTC_CON	=	0x00e1
_SCR_RTC_CNT0	=	0x00e2
_SCR_RTC_CNT1	=	0x00e3
_SCR_RTC_CNT2	=	0x00e4
_SCR_RTC_CNT3	=	0x00e5
_SCR_RTC_CR0	=	0x00e6
_SCR_RTC_CR1	=	0x00e7
_SCR_RTC_CR2	=	0x00e8
_SCR_RTC_CR3	=	0x00e9
_SCR_WDT_CON	=	0x0081
_SCR_WDT_REL	=	0x0082
_SCR_WDT_L	=	0x0084
_SCR_WDT_H	=	0x0085
_SCR_WDT_WINB	=	0x0083
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
; internal ram data
;--------------------------------------------------------
	.section .ddata.i51,"aw" ;data_name ;area
_SCR_TimeLoadU32_sloc0_1_0:
	.ds.b	4
_SCR_TimeStoreBasePlusElapsedNs_sloc0_1_0:
	.ds.b	4
_SCR_TimeStoreBasePlusElapsedNs_sloc1_1_0:
	.ds.b	4
_SCR_RtcReadCounter_sloc0_1_0:
	.ds.b	4
;--------------------------------------------------------
; uninitialized external ram data
;--------------------------------------------------------
	.section .xdata.i51,"aw" ;xdata_name ;area
_Scr_set_count_registers_value_65536_7:
	.ds.b	4
_Scr_set_fsys_div_65536_13:
	.ds.b	1
_Scr_set_fsys_70kHz_cmcon_65537_17:
	.ds.b	1
_SCR_TimeBaseUtcHigh:
	.ds.b	4
_SCR_TimeBaseUtcLow:
	.ds.b	4
_SCR_TimeBaseVehicleHigh:
	.ds.b	4
_SCR_TimeBaseVehicleLow:
	.ds.b	4
_SCR_TimeProductLow:
	.ds.b	4
_SCR_TimeProductHigh:
	.ds.b	4
_SCR_TimeElapsedLow:
	.ds.b	4
_SCR_TimeElapsedHigh:
	.ds.b	4
_SCR_TimeCurrentLow:
	.ds.b	4
_SCR_TimeCurrentHigh:
	.ds.b	4
_SCR_TotalElapsedTicksLow:
	.ds.b	4
_SCR_TotalElapsedTicksHigh:
	.ds.b	4
_SCR_TimeAddr_offset_65536_22:
	.ds.b	1
_SCR_TimeLoadU8_offset_65536_24:
	.ds.b	1
_SCR_TimeStoreU8_PARM_2:
	.ds.b	1
_SCR_TimeStoreU8_offset_65536_26:
	.ds.b	1
_SCR_TimeLoadU32_offset_65536_28:
	.ds.b	1
_SCR_TimeStoreU32_PARM_2:
	.ds.b	4
_SCR_TimeStoreU32_offset_65536_30:
	.ds.b	1
_SCR_TimeStoreBasePlusElapsedNs_PARM_2:
	.ds.b	4
_SCR_TimeStoreBasePlusElapsedNs_PARM_3:
	.ds.b	4
_SCR_TimeStoreBasePlusElapsedNs_PARM_4:
	.ds.b	4
_SCR_TimeStoreBasePlusElapsedNs_offset_65536_32:
	.ds.b	1
_SCR_RtcReadCounter_cnt3Before_65536_37:
	.ds.b	1
_SCR_RtcReadCounter_cnt3After_65536_37:
	.ds.b	1
_SCR_RtcReadCounter_cnt2Before_65536_37:
	.ds.b	1
_SCR_RtcReadCounter_cnt2After_65536_37:
	.ds.b	1
_SCR_RtcReadCounter_cnt1Before_65536_37:
	.ds.b	1
_SCR_RtcReadCounter_cnt1After_65536_37:
	.ds.b	1
_SCR_RtcReadCounter_cnt0_65536_37:
	.ds.b	1
_SCR_RtcCompareIsr_previousLow_65536_40:
	.ds.b	4
_WCAN_WaitForSelectiveWakeAck_expected_65536_63:
	.ds.b	1
_SCR_RequestStandbyWake_wakeReason_65536_68:
	.ds.b	1
_SCR_CheckFaultWake_nmiStatus_65536_75:
	.ds.b	1
_SCR_CheckFaultWake_rstStatus_65536_75:
	.ds.b	1
_SCR_CheckFaultWake_faultStatus_65536_75:
	.ds.b	1
_SCR_CheckFaultWake_wakeReason_65536_75:
	.ds.b	1
_WCAN_CheckWake_wcanStatus0_65536_82:
	.ds.b	1
_WCAN_CheckWake_wcanStatus1_65536_82:
	.ds.b	1
_WCAN_CheckWake_p00In_65536_82:
	.ds.b	1
_WCAN_CheckWake_wakeReason_65536_82:
	.ds.b	1
_WCAN_Init_result_65536_89:
	.ds.b	1
_WCAN_Init_TempVar_65536_89:
	.ds.b	1
_main_div_196608_96:
	.ds.b	1
;--------------------------------------------------------
; initialized external ram data
;--------------------------------------------------------
	.section .xdata.init,"aw" ;xidata_name ;area
_SCR_TimeBaseActive:
	.ds.b	1
;--------------------------------------------------------
; interrupt vector
;--------------------------------------------------------
	.globl _SCR_RtcCompareIsr 
	.section .isr13, "ax"
	ljmp	_SCR_RtcCompareIsr
;--------------------------------------------------------
; code
;--------------------------------------------------------
;------------------------------------------------------------
;Allocation info for local variables in function 'Scr_is_rtc_running'
;------------------------------------------------------------
;	../SCR/scr_rtc_sdcc.h:184: SCR_INLINE boolean Scr_is_rtc_running(void)
;	-----------------------------------------
;	 function Scr_is_rtc_running
;	-----------------------------------------
	.section .text.code.Scr_is_rtc_running,"ax" ;code for function Scr_is_rtc_running
	.type   Scr_is_rtc_running, @function
_Scr_is_rtc_running:
	.using 0
;	../SCR/scr_rtc_sdcc.h:186: return (SCR_RTC_CON & RTCC_MASK);
	mov	a,_SCR_RTC_CON
	anl	a,#0x01
;	../SCR/scr_rtc_sdcc.h:187: }
	mov	dpl,a
.00101:
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'Scr_start_rtc'
;------------------------------------------------------------
;	../SCR/scr_rtc_sdcc.h:189: SCR_INLINE void Scr_start_rtc(void)
;	-----------------------------------------
;	 function Scr_start_rtc
;	-----------------------------------------
	.section .text.code.Scr_start_rtc,"ax" ;code for function Scr_start_rtc
	.type   Scr_start_rtc, @function
_Scr_start_rtc:
	.using 0
;	../SCR/scr_rtc_sdcc.h:191: SCR_RTC_CON |= RTCC_MASK;
	orl	_SCR_RTC_CON,#0x01
.00103:
;	../SCR/scr_rtc_sdcc.h:192: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'Scr_stop_rtc'
;------------------------------------------------------------
;	../SCR/scr_rtc_sdcc.h:194: SCR_INLINE void Scr_stop_rtc(void)
;	-----------------------------------------
;	 function Scr_stop_rtc
;	-----------------------------------------
	.section .text.code.Scr_stop_rtc,"ax" ;code for function Scr_stop_rtc
	.type   Scr_stop_rtc, @function
_Scr_stop_rtc:
	.using 0
;	../SCR/scr_rtc_sdcc.h:196: SCR_RTC_CON &= (~RTCC_MASK);
	anl	_SCR_RTC_CON,#0xFE
.00105:
;	../SCR/scr_rtc_sdcc.h:197: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'Scr_set_count_registers'
;------------------------------------------------------------
;value                     Allocated with name '_Scr_set_count_registers_value_65536_7'
;__1310720199              Allocated with name '_Scr_set_count_registers___1310720199_131072_8'
;------------------------------------------------------------
;	../SCR/scr_rtc_sdcc.h:199: SCR_INLINE void Scr_set_count_registers(uint32 value)
;	-----------------------------------------
;	 function Scr_set_count_registers
;	-----------------------------------------
	.section .text.code.Scr_set_count_registers,"ax" ;code for function Scr_set_count_registers
	.type   Scr_set_count_registers, @function
_Scr_set_count_registers:
	.using 0
	mov	r7,dpl
	mov	r6,dph
	mov	r5,b
	mov	r4,a
	mov	dptr,#_Scr_set_count_registers_value_65536_7
	mov	a,r7
	movx	@dptr,a
	mov	a,r6
	inc	dptr
	movx	@dptr,a
	mov	a,r5
	inc	dptr
	movx	@dptr,a
	mov	a,r4
	inc	dptr
	movx	@dptr,a
;	../SCR/scr_rtc_sdcc.h:186: return (SCR_RTC_CON & RTCC_MASK);
	mov	a,_SCR_RTC_CON
	jb	acc.0,.00110
.00116:
;	../SCR/scr_rtc_sdcc.h:201: if(!Scr_is_rtc_running())
;	../SCR/scr_rtc_sdcc.h:203: SCR_UNLOCK_PROTECTED_BITS();
	mov	_SCR_PASSWD,#0x98
;	../SCR/scr_rtc_sdcc.h:204: SCR_RTC_CNT3 = (uint8)(value >> 24);
	mov	dptr,#_Scr_set_count_registers_value_65536_7
	movx	a,@dptr
	mov	r4,a
	inc	dptr
	movx	a,@dptr
	mov	r5,a
	inc	dptr
	movx	a,@dptr
	mov	r6,a
	inc	dptr
	movx	a,@dptr
	mov	r7,a
	mov	_SCR_RTC_CNT3,r7
;	../SCR/scr_rtc_sdcc.h:205: SCR_RTC_CNT2 = (uint8)(value >> 16);
	mov	_SCR_RTC_CNT2,r6
;	../SCR/scr_rtc_sdcc.h:206: SCR_RTC_CNT1 = (uint8)(value >> 8);
	mov	_SCR_RTC_CNT1,r5
;	../SCR/scr_rtc_sdcc.h:207: SCR_RTC_CNT0 = (uint8)(value);
	mov	_SCR_RTC_CNT0,r4
;	../SCR/scr_rtc_sdcc.h:208: SCR_LOCK_PROTECTED_BITS();
	mov	_SCR_PASSWD,#0xA8
.00110:
;	../SCR/scr_rtc_sdcc.h:210: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'Scr_set_fsys'
;------------------------------------------------------------
;div                       Allocated with name '_Scr_set_fsys_div_65536_13'
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
	mov	dptr,#_Scr_set_fsys_div_65536_13
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
.00117:
;	../SCR/scr_common.h:73: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'Scr_set_fsys_70kHz'
;------------------------------------------------------------
;cmcon                     Allocated with name '_Scr_set_fsys_70kHz_cmcon_65537_17'
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
	mov	dptr,#_Scr_set_fsys_70kHz_cmcon_65537_17
	mov	a,_SCR_SCU_CMCON
	movx	@dptr,a
;	../SCR/scr_common.h:85: SCR_SCU_CMCON = (OSCPD_MASK | (cmcon & DIV_MASK));
	movx	a,@dptr
	anl	a,#0x0F
	orl	a,#0x20
	mov	_SCR_SCU_CMCON,a
;	../SCR/scr_common.h:87: SCR_LOCK_PROTECTED_BITS();            /* Close access to write protected bits */
	mov	_SCR_PASSWD,#0xA8
.00119:
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
	cjne	a,_SCR_SCU_SR,.00123
	mov	a,#0x01
	sjmp	.00124
.00123:
	clr	a
.00124:
;	../SCR/scr_common.h:93: }
	mov	dpl,a
.00121:
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
.00125:
;	../SCR/scr_common.h:98: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'SCR_TimeAddr'
;------------------------------------------------------------
;offset                    Allocated with name '_SCR_TimeAddr_offset_65536_22'
;------------------------------------------------------------
;	../SCR/main.c:163: static volatile __xdata uint8 *SCR_TimeAddr(uint8 offset)
;	-----------------------------------------
;	 function SCR_TimeAddr
;	-----------------------------------------
	.section .text.code.SCR_TimeAddr,"ax" ;code for function SCR_TimeAddr
	.type   SCR_TimeAddr, @function
_SCR_TimeAddr:
	.using 0
	mov	a,dpl
	mov	dptr,#_SCR_TimeAddr_offset_65536_22
	movx	@dptr,a
;	../SCR/main.c:165: return (volatile __xdata uint8 *)(SCR_TIME_XRAM_BASE + offset);
	movx	a,@dptr
	mov	r7,a
	mov	r6,#0x00
	mov	a,#0x90
	add	a,r7
	mov	r7,a
	mov	a,#0x17
	addc	a,r6
;	../SCR/main.c:166: }
	mov	dpl,r7
	mov	dph,a
.00127:
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'SCR_TimeLoadU8'
;------------------------------------------------------------
;offset                    Allocated with name '_SCR_TimeLoadU8_offset_65536_24'
;------------------------------------------------------------
;	../SCR/main.c:168: static uint8 SCR_TimeLoadU8(uint8 offset)
;	-----------------------------------------
;	 function SCR_TimeLoadU8
;	-----------------------------------------
	.section .text.code.SCR_TimeLoadU8,"ax" ;code for function SCR_TimeLoadU8
	.type   SCR_TimeLoadU8, @function
_SCR_TimeLoadU8:
	.using 0
	mov	a,dpl
	mov	dptr,#_SCR_TimeLoadU8_offset_65536_24
	movx	@dptr,a
;	../SCR/main.c:170: return *SCR_TimeAddr(offset);
	movx	a,@dptr
	mov	dpl,a
	lcall	_SCR_TimeAddr
	movx	a,@dptr
;	../SCR/main.c:171: }
	mov	dpl,a
.00129:
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'SCR_TimeStoreU8'
;------------------------------------------------------------
;value                     Allocated with name '_SCR_TimeStoreU8_PARM_2'
;offset                    Allocated with name '_SCR_TimeStoreU8_offset_65536_26'
;------------------------------------------------------------
;	../SCR/main.c:173: static void SCR_TimeStoreU8(uint8 offset, uint8 value)
;	-----------------------------------------
;	 function SCR_TimeStoreU8
;	-----------------------------------------
	.section .text.code.SCR_TimeStoreU8,"ax" ;code for function SCR_TimeStoreU8
	.type   SCR_TimeStoreU8, @function
_SCR_TimeStoreU8:
	.using 0
	mov	a,dpl
	mov	dptr,#_SCR_TimeStoreU8_offset_65536_26
	movx	@dptr,a
;	../SCR/main.c:175: *SCR_TimeAddr(offset) = value;
	movx	a,@dptr
	mov	dpl,a
	lcall	_SCR_TimeAddr
	mov	r6,dpl
	mov	r7,dph
	mov	dptr,#_SCR_TimeStoreU8_PARM_2
	movx	a,@dptr
	mov	r5,a
	mov	dpl,r6
	mov	dph,r7
	movx	@dptr,a
.00131:
;	../SCR/main.c:176: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'SCR_TimeLoadU32'
;------------------------------------------------------------
;sloc0                     Allocated with name '_SCR_TimeLoadU32_sloc0_1_0'
;offset                    Allocated with name '_SCR_TimeLoadU32_offset_65536_28'
;value                     Allocated with name '_SCR_TimeLoadU32_value_65536_29'
;------------------------------------------------------------
;	../SCR/main.c:178: static uint32 SCR_TimeLoadU32(uint8 offset)
;	-----------------------------------------
;	 function SCR_TimeLoadU32
;	-----------------------------------------
	.section .text.code.SCR_TimeLoadU32,"ax" ;code for function SCR_TimeLoadU32
	.type   SCR_TimeLoadU32, @function
_SCR_TimeLoadU32:
	.using 0
	mov	a,dpl
	mov	dptr,#_SCR_TimeLoadU32_offset_65536_28
	movx	@dptr,a
;	../SCR/main.c:182: value = ((uint32)SCR_TimeLoadU8(offset) << 24);
	movx	a,@dptr
	mov	r7,a
	mov	dpl,a
	push	ar7
	lcall	_SCR_TimeLoadU8
	mov	r6,dpl
	pop	ar7
	mov	r5,#0x00
	mov	(_SCR_TimeLoadU32_sloc0_1_0 + 3),r6
;	1-genFromRTrack replaced	mov	_SCR_TimeLoadU32_sloc0_1_0,#0x00
	mov	_SCR_TimeLoadU32_sloc0_1_0,r5
;	1-genFromRTrack replaced	mov	(_SCR_TimeLoadU32_sloc0_1_0 + 1),#0x00
	mov	(_SCR_TimeLoadU32_sloc0_1_0 + 1),r5
;	1-genFromRTrack replaced	mov	(_SCR_TimeLoadU32_sloc0_1_0 + 2),#0x00
	mov	(_SCR_TimeLoadU32_sloc0_1_0 + 2),r5
;	../SCR/main.c:183: value |= ((uint32)SCR_TimeLoadU8((uint8)(offset + 1u)) << 16);
	mov	a,r7
	inc	a
	mov	dpl,a
	push	ar7
	lcall	_SCR_TimeLoadU8
	mov	r2,dpl
	pop	ar7
	mov	ar0,r2
	mov	r1,#0x00
	mov	ar6,r1
	mov	ar2,r0
	clr	a
	mov	r1,a
	orl	_SCR_TimeLoadU32_sloc0_1_0,a
	mov	a,r1
	orl	(_SCR_TimeLoadU32_sloc0_1_0 + 1),a
	mov	a,r2
	orl	(_SCR_TimeLoadU32_sloc0_1_0 + 2),a
	mov	a,r6
	orl	(_SCR_TimeLoadU32_sloc0_1_0 + 3),a
;	../SCR/main.c:184: value |= ((uint32)SCR_TimeLoadU8((uint8)(offset + 2u)) << 8);
	mov	a,#0x02
	add	a,r7
	mov	dpl,a
	push	ar7
	lcall	_SCR_TimeLoadU8
	mov	r5,dpl
	pop	ar7
	mov	ar3,r5
	mov	r4,#0x00
	mov	r5,#0x00
	mov	ar6,r5
	mov	ar5,r4
	mov	ar4,r3
	mov	r3,#0x00
	mov	a,_SCR_TimeLoadU32_sloc0_1_0
	orl	ar3,a
	mov	a,(_SCR_TimeLoadU32_sloc0_1_0 + 1)
	orl	ar4,a
	mov	a,(_SCR_TimeLoadU32_sloc0_1_0 + 2)
	orl	ar5,a
	mov	a,(_SCR_TimeLoadU32_sloc0_1_0 + 3)
	orl	ar6,a
;	../SCR/main.c:185: value |= (uint32)SCR_TimeLoadU8((uint8)(offset + 3u));
	inc	r7
	inc	r7
	inc	r7
	mov	dpl,r7
	push	ar6
	push	ar5
	push	ar4
	push	ar3
	lcall	_SCR_TimeLoadU8
	mov	r7,dpl
	pop	ar3
	pop	ar4
	pop	ar5
	pop	ar6
	mov	ar0,r7
	clr	a
	mov	r1,a
	mov	r2,a
	mov	r7,a
	mov	a,r0
	orl	ar3,a
	mov	a,r1
	orl	ar4,a
	mov	a,r2
	orl	ar5,a
	mov	a,r7
	orl	ar6,a
;	../SCR/main.c:187: return value;
	mov	dpl,r3
	mov	dph,r4
	mov	b,r5
	mov	a,r6
.00133:
;	../SCR/main.c:188: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'SCR_TimeStoreU32'
;------------------------------------------------------------
;value                     Allocated with name '_SCR_TimeStoreU32_PARM_2'
;offset                    Allocated with name '_SCR_TimeStoreU32_offset_65536_30'
;------------------------------------------------------------
;	../SCR/main.c:190: static void SCR_TimeStoreU32(uint8 offset, uint32 value)
;	-----------------------------------------
;	 function SCR_TimeStoreU32
;	-----------------------------------------
	.section .text.code.SCR_TimeStoreU32,"ax" ;code for function SCR_TimeStoreU32
	.type   SCR_TimeStoreU32, @function
_SCR_TimeStoreU32:
	.using 0
	mov	a,dpl
	mov	dptr,#_SCR_TimeStoreU32_offset_65536_30
	movx	@dptr,a
;	../SCR/main.c:192: SCR_TimeStoreU8(offset, (uint8)(value >> 24));
	movx	a,@dptr
	mov	r7,a
	mov	dptr,#_SCR_TimeStoreU32_PARM_2
	movx	a,@dptr
	mov	r3,a
	inc	dptr
	movx	a,@dptr
	mov	r4,a
	inc	dptr
	movx	a,@dptr
	mov	r5,a
	inc	dptr
	movx	a,@dptr
	mov	r6,a
	mov	dptr,#_SCR_TimeStoreU8_PARM_2
	movx	@dptr,a
	mov	dpl,r7
	push	ar7
	push	ar6
	push	ar5
	push	ar4
	push	ar3
	lcall	_SCR_TimeStoreU8
	pop	ar3
	pop	ar4
	pop	ar5
	pop	ar6
	pop	ar7
;	../SCR/main.c:193: SCR_TimeStoreU8((uint8)(offset + 1u), (uint8)(value >> 16));
	mov	a,r7
	inc	a
	mov	r2,a
	mov	dptr,#_SCR_TimeStoreU8_PARM_2
	mov	a,r5
	movx	@dptr,a
	mov	dpl,r2
	push	ar7
	push	ar6
	push	ar5
	push	ar4
	push	ar3
	lcall	_SCR_TimeStoreU8
	pop	ar3
	pop	ar4
	pop	ar5
	pop	ar6
	pop	ar7
;	../SCR/main.c:194: SCR_TimeStoreU8((uint8)(offset + 2u), (uint8)(value >> 8));
	mov	a,#0x02
	add	a,r7
	mov	r2,a
	mov	dptr,#_SCR_TimeStoreU8_PARM_2
	mov	a,r4
	movx	@dptr,a
	mov	dpl,r2
	push	ar7
	push	ar6
	push	ar5
	push	ar4
	push	ar3
	lcall	_SCR_TimeStoreU8
	pop	ar3
	pop	ar4
	pop	ar5
	pop	ar6
	pop	ar7
;	../SCR/main.c:195: SCR_TimeStoreU8((uint8)(offset + 3u), (uint8)value);
	inc	r7
	inc	r7
	inc	r7
	mov	dptr,#_SCR_TimeStoreU8_PARM_2
	mov	a,r3
	movx	@dptr,a
	mov	dpl,r7
	lcall	_SCR_TimeStoreU8
.00135:
;	../SCR/main.c:196: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'SCR_TimeStoreBasePlusElapsedNs'
;------------------------------------------------------------
;sloc0                     Allocated with name '_SCR_TimeStoreBasePlusElapsedNs_sloc0_1_0'
;sloc1                     Allocated with name '_SCR_TimeStoreBasePlusElapsedNs_sloc1_1_0'
;baseHigh                  Allocated with name '_SCR_TimeStoreBasePlusElapsedNs_PARM_2'
;baseLow                   Allocated with name '_SCR_TimeStoreBasePlusElapsedNs_PARM_3'
;elapsedTicks              Allocated with name '_SCR_TimeStoreBasePlusElapsedNs_PARM_4'
;offset                    Allocated with name '_SCR_TimeStoreBasePlusElapsedNs_offset_65536_32'
;------------------------------------------------------------
;	../SCR/main.c:198: static void SCR_TimeStoreBasePlusElapsedNs(
;	-----------------------------------------
;	 function SCR_TimeStoreBasePlusElapsedNs
;	-----------------------------------------
	.section .text.code.SCR_TimeStoreBasePlusElapsedNs,"ax" ;code for function SCR_TimeStoreBasePlusElapsedNs
	.type   SCR_TimeStoreBasePlusElapsedNs, @function
_SCR_TimeStoreBasePlusElapsedNs:
	.using 0
	mov	a,dpl
	mov	dptr,#_SCR_TimeStoreBasePlusElapsedNs_offset_65536_32
	movx	@dptr,a
;	../SCR/main.c:204: SCR_TimeProductLow = (elapsedTicks & 0xFFFFu) * (uint32)SCR_TIME_RTC_TICK_NS;
	mov	dptr,#_SCR_TimeStoreBasePlusElapsedNs_PARM_4
	movx	a,@dptr
	mov	r4,a
	inc	dptr
	movx	a,@dptr
	mov	r5,a
	inc	dptr
	movx	a,@dptr
	mov	r6,a
	inc	dptr
	movx	a,@dptr
	mov	r7,a
	mov	dptr,#__mullong_PARM_2
	mov	a,r4
	movx	@dptr,a
	mov	a,r5
	inc	dptr
	movx	@dptr,a
	clr	a
	inc	dptr
	movx	@dptr,a
	inc	dptr
	movx	@dptr,a
	mov	dptr,#(0x32&0x00ff)
	clr	a
	mov	b,a
	push	ar7
	push	ar6
	push	ar5
	push	ar4
	lcall	__mullong
	mov	r0,dpl
	mov	r1,dph
	mov	r2,b
	mov	r3,a
	pop	ar4
	pop	ar5
	pop	ar6
	pop	ar7
	mov	dptr,#_SCR_TimeProductLow
	mov	a,r0
	movx	@dptr,a
	mov	a,r1
	inc	dptr
	movx	@dptr,a
	mov	a,r2
	inc	dptr
	movx	@dptr,a
	mov	a,r3
	inc	dptr
	movx	@dptr,a
;	../SCR/main.c:205: SCR_TimeProductHigh = (elapsedTicks >> 16u) * (uint32)SCR_TIME_RTC_TICK_NS;
	mov	ar4,r6
	mov	ar5,r7
	mov	r6,#0x00
	mov	r7,#0x00
	mov	dptr,#__mullong_PARM_2
	mov	a,r4
	movx	@dptr,a
	mov	a,r5
	inc	dptr
	movx	@dptr,a
	mov	a,r6
	inc	dptr
	movx	@dptr,a
	mov	a,r7
	inc	dptr
	movx	@dptr,a
	mov	dptr,#(0x32&0x00ff)
	clr	a
	mov	b,a
	push	ar3
	push	ar2
	push	ar1
	push	ar0
	lcall	__mullong
	mov	_SCR_TimeStoreBasePlusElapsedNs_sloc0_1_0,dpl
	mov	(_SCR_TimeStoreBasePlusElapsedNs_sloc0_1_0 + 1),dph
	mov	(_SCR_TimeStoreBasePlusElapsedNs_sloc0_1_0 + 2),b
	mov	(_SCR_TimeStoreBasePlusElapsedNs_sloc0_1_0 + 3),a
	pop	ar0
	pop	ar1
	pop	ar2
	pop	ar3
	mov	dptr,#_SCR_TimeProductHigh
	mov	a,_SCR_TimeStoreBasePlusElapsedNs_sloc0_1_0
	movx	@dptr,a
	mov	a,(_SCR_TimeStoreBasePlusElapsedNs_sloc0_1_0 + 1)
	inc	dptr
	movx	@dptr,a
	mov	a,(_SCR_TimeStoreBasePlusElapsedNs_sloc0_1_0 + 2)
	inc	dptr
	movx	@dptr,a
	mov	a,(_SCR_TimeStoreBasePlusElapsedNs_sloc0_1_0 + 3)
	inc	dptr
	movx	@dptr,a
;	../SCR/main.c:206: SCR_TimeElapsedLow = SCR_TimeProductLow + (SCR_TimeProductHigh << 16u);
	mov	r7,(_SCR_TimeStoreBasePlusElapsedNs_sloc0_1_0 + 1)
	mov	r6,_SCR_TimeStoreBasePlusElapsedNs_sloc0_1_0
	clr	a
	mov	r5,a
	add	a,r0
	mov	r4,a
	mov	a,r5
	addc	a,r1
	mov	r5,a
	mov	a,r6
	addc	a,r2
	mov	r6,a
	mov	a,r7
	addc	a,r3
	mov	r7,a
	mov	dptr,#_SCR_TimeElapsedLow
	mov	a,r4
	movx	@dptr,a
	mov	a,r5
	inc	dptr
	movx	@dptr,a
	mov	a,r6
	inc	dptr
	movx	@dptr,a
	mov	a,r7
	inc	dptr
	movx	@dptr,a
;	../SCR/main.c:207: SCR_TimeElapsedHigh = (SCR_TimeProductHigh >> 16u);
	mov	_SCR_TimeStoreBasePlusElapsedNs_sloc0_1_0,(_SCR_TimeStoreBasePlusElapsedNs_sloc0_1_0 + 2)
	mov	(_SCR_TimeStoreBasePlusElapsedNs_sloc0_1_0 + 1),(_SCR_TimeStoreBasePlusElapsedNs_sloc0_1_0 + 3)
	mov	(_SCR_TimeStoreBasePlusElapsedNs_sloc0_1_0 + 2),#0x00
	mov	(_SCR_TimeStoreBasePlusElapsedNs_sloc0_1_0 + 3),#0x00
	mov	dptr,#_SCR_TimeElapsedHigh
	mov	a,_SCR_TimeStoreBasePlusElapsedNs_sloc0_1_0
	movx	@dptr,a
	mov	a,(_SCR_TimeStoreBasePlusElapsedNs_sloc0_1_0 + 1)
	inc	dptr
	movx	@dptr,a
	mov	a,(_SCR_TimeStoreBasePlusElapsedNs_sloc0_1_0 + 2)
	inc	dptr
	movx	@dptr,a
	mov	a,(_SCR_TimeStoreBasePlusElapsedNs_sloc0_1_0 + 3)
	inc	dptr
	movx	@dptr,a
;	../SCR/main.c:208: if (SCR_TimeElapsedLow < SCR_TimeProductLow)
	clr	c
	mov	a,r4
	subb	a,r0
	mov	a,r5
	subb	a,r1
	mov	a,r6
	subb	a,r2
	mov	a,r7
	subb	a,r3
	jnc	.00138
.00151:
;	../SCR/main.c:210: SCR_TimeElapsedHigh++;
	mov	dptr,#_SCR_TimeElapsedHigh
	mov	a,#0x01
	add	a,_SCR_TimeStoreBasePlusElapsedNs_sloc0_1_0
	movx	@dptr,a
	clr	a
	addc	a,(_SCR_TimeStoreBasePlusElapsedNs_sloc0_1_0 + 1)
	inc	dptr
	movx	@dptr,a
	clr	a
	addc	a,(_SCR_TimeStoreBasePlusElapsedNs_sloc0_1_0 + 2)
	inc	dptr
	movx	@dptr,a
	clr	a
	addc	a,(_SCR_TimeStoreBasePlusElapsedNs_sloc0_1_0 + 3)
	inc	dptr
	movx	@dptr,a
.00138:
;	../SCR/main.c:213: SCR_TimeCurrentLow = baseLow + SCR_TimeElapsedLow;
	mov	dptr,#_SCR_TimeStoreBasePlusElapsedNs_PARM_3
	movx	a,@dptr
	mov	r0,a
	inc	dptr
	movx	a,@dptr
	mov	r1,a
	inc	dptr
	movx	a,@dptr
	mov	r2,a
	inc	dptr
	movx	a,@dptr
	mov	r3,a
	mov	a,r4
	add	a,r0
	mov	_SCR_TimeStoreBasePlusElapsedNs_sloc0_1_0,a
	mov	a,r5
	addc	a,r1
	mov	(_SCR_TimeStoreBasePlusElapsedNs_sloc0_1_0 + 1),a
	mov	a,r6
	addc	a,r2
	mov	(_SCR_TimeStoreBasePlusElapsedNs_sloc0_1_0 + 2),a
	mov	a,r7
	addc	a,r3
	mov	(_SCR_TimeStoreBasePlusElapsedNs_sloc0_1_0 + 3),a
	mov	dptr,#_SCR_TimeCurrentLow
	mov	a,_SCR_TimeStoreBasePlusElapsedNs_sloc0_1_0
	movx	@dptr,a
	mov	a,(_SCR_TimeStoreBasePlusElapsedNs_sloc0_1_0 + 1)
	inc	dptr
	movx	@dptr,a
	mov	a,(_SCR_TimeStoreBasePlusElapsedNs_sloc0_1_0 + 2)
	inc	dptr
	movx	@dptr,a
	mov	a,(_SCR_TimeStoreBasePlusElapsedNs_sloc0_1_0 + 3)
	inc	dptr
	movx	@dptr,a
;	../SCR/main.c:214: SCR_TimeCurrentHigh = baseHigh + SCR_TimeElapsedHigh;
	mov	dptr,#_SCR_TimeElapsedHigh
	movx	a,@dptr
	mov	_SCR_TimeStoreBasePlusElapsedNs_sloc1_1_0,a
	inc	dptr
	movx	a,@dptr
	mov	(_SCR_TimeStoreBasePlusElapsedNs_sloc1_1_0 + 1),a
	inc	dptr
	movx	a,@dptr
	mov	(_SCR_TimeStoreBasePlusElapsedNs_sloc1_1_0 + 2),a
	inc	dptr
	movx	a,@dptr
	mov	(_SCR_TimeStoreBasePlusElapsedNs_sloc1_1_0 + 3),a
	mov	dptr,#_SCR_TimeStoreBasePlusElapsedNs_PARM_2
	movx	a,@dptr
	mov	r4,a
	inc	dptr
	movx	a,@dptr
	mov	r5,a
	inc	dptr
	movx	a,@dptr
	mov	r6,a
	inc	dptr
	movx	a,@dptr
	mov	r7,a
	mov	a,_SCR_TimeStoreBasePlusElapsedNs_sloc1_1_0
	add	a,r4
	mov	r4,a
	mov	a,(_SCR_TimeStoreBasePlusElapsedNs_sloc1_1_0 + 1)
	addc	a,r5
	mov	r5,a
	mov	a,(_SCR_TimeStoreBasePlusElapsedNs_sloc1_1_0 + 2)
	addc	a,r6
	mov	r6,a
	mov	a,(_SCR_TimeStoreBasePlusElapsedNs_sloc1_1_0 + 3)
	addc	a,r7
	mov	r7,a
	mov	dptr,#_SCR_TimeCurrentHigh
	mov	a,r4
	movx	@dptr,a
	mov	a,r5
	inc	dptr
	movx	@dptr,a
	mov	a,r6
	inc	dptr
	movx	@dptr,a
	mov	a,r7
	inc	dptr
	movx	@dptr,a
;	../SCR/main.c:215: if (SCR_TimeCurrentLow < baseLow)
	clr	c
	mov	a,_SCR_TimeStoreBasePlusElapsedNs_sloc0_1_0
	subb	a,r0
	mov	a,(_SCR_TimeStoreBasePlusElapsedNs_sloc0_1_0 + 1)
	subb	a,r1
	mov	a,(_SCR_TimeStoreBasePlusElapsedNs_sloc0_1_0 + 2)
	subb	a,r2
	mov	a,(_SCR_TimeStoreBasePlusElapsedNs_sloc0_1_0 + 3)
	subb	a,r3
	jnc	.00140
.00152:
;	../SCR/main.c:217: SCR_TimeCurrentHigh++;
	mov	dptr,#_SCR_TimeCurrentHigh
	mov	a,#0x01
	add	a,r4
	movx	@dptr,a
	clr	a
	addc	a,r5
	inc	dptr
	movx	@dptr,a
	clr	a
	addc	a,r6
	inc	dptr
	movx	@dptr,a
	clr	a
	addc	a,r7
	inc	dptr
	movx	@dptr,a
.00140:
;	../SCR/main.c:220: SCR_TimeStoreU32(offset, SCR_TimeCurrentHigh);
	mov	dptr,#_SCR_TimeStoreBasePlusElapsedNs_offset_65536_32
	movx	a,@dptr
	mov	r7,a
	mov	dptr,#_SCR_TimeCurrentHigh
	movx	a,@dptr
	mov	r3,a
	inc	dptr
	movx	a,@dptr
	mov	r4,a
	inc	dptr
	movx	a,@dptr
	mov	r5,a
	inc	dptr
	movx	a,@dptr
	mov	r6,a
	mov	dptr,#_SCR_TimeStoreU32_PARM_2
	mov	a,r3
	movx	@dptr,a
	mov	a,r4
	inc	dptr
	movx	@dptr,a
	mov	a,r5
	inc	dptr
	movx	@dptr,a
	mov	a,r6
	inc	dptr
	movx	@dptr,a
	mov	dpl,r7
	push	ar7
	lcall	_SCR_TimeStoreU32
	pop	ar7
;	../SCR/main.c:221: SCR_TimeStoreU32((uint8)(offset + 4u), SCR_TimeCurrentLow);
	mov	a,r7
	add	a,#0x04
	mov	r7,a
	mov	dptr,#_SCR_TimeCurrentLow
	movx	a,@dptr
	mov	r3,a
	inc	dptr
	movx	a,@dptr
	mov	r4,a
	inc	dptr
	movx	a,@dptr
	mov	r5,a
	inc	dptr
	movx	a,@dptr
	mov	r6,a
	mov	dptr,#_SCR_TimeStoreU32_PARM_2
	mov	a,r3
	movx	@dptr,a
	mov	a,r4
	inc	dptr
	movx	@dptr,a
	mov	a,r5
	inc	dptr
	movx	@dptr,a
	mov	a,r6
	inc	dptr
	movx	@dptr,a
	mov	dpl,r7
	lcall	_SCR_TimeStoreU32
.00141:
;	../SCR/main.c:222: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'SCR_RtcReadCounter'
;------------------------------------------------------------
;sloc0                     Allocated with name '_SCR_RtcReadCounter_sloc0_1_0'
;cnt3Before                Allocated with name '_SCR_RtcReadCounter_cnt3Before_65536_37'
;cnt3After                 Allocated with name '_SCR_RtcReadCounter_cnt3After_65536_37'
;cnt2Before                Allocated with name '_SCR_RtcReadCounter_cnt2Before_65536_37'
;cnt2After                 Allocated with name '_SCR_RtcReadCounter_cnt2After_65536_37'
;cnt1Before                Allocated with name '_SCR_RtcReadCounter_cnt1Before_65536_37'
;cnt1After                 Allocated with name '_SCR_RtcReadCounter_cnt1After_65536_37'
;cnt2                      Allocated with name '_SCR_RtcReadCounter_cnt2_65536_37'
;cnt1                      Allocated with name '_SCR_RtcReadCounter_cnt1_65536_37'
;cnt0                      Allocated with name '_SCR_RtcReadCounter_cnt0_65536_37'
;------------------------------------------------------------
;	../SCR/main.c:236: static uint32 SCR_RtcReadCounter(void)
;	-----------------------------------------
;	 function SCR_RtcReadCounter
;	-----------------------------------------
	.section .text.code.SCR_RtcReadCounter,"ax" ;code for function SCR_RtcReadCounter
	.type   SCR_RtcReadCounter, @function
_SCR_RtcReadCounter:
	.using 0
;	../SCR/main.c:248: do
.00155:
;	../SCR/main.c:250: cnt3Before = SCR_RTC_CNT3;
	mov	dptr,#_SCR_RtcReadCounter_cnt3Before_65536_37
	mov	a,_SCR_RTC_CNT3
	movx	@dptr,a
;	../SCR/main.c:251: cnt2Before = SCR_RTC_CNT2;
	mov	dptr,#_SCR_RtcReadCounter_cnt2Before_65536_37
	mov	a,_SCR_RTC_CNT2
	movx	@dptr,a
;	../SCR/main.c:252: cnt1Before = SCR_RTC_CNT1;
	mov	dptr,#_SCR_RtcReadCounter_cnt1Before_65536_37
	mov	a,_SCR_RTC_CNT1
	movx	@dptr,a
;	../SCR/main.c:253: cnt0 = SCR_RTC_CNT0;
	mov	dptr,#_SCR_RtcReadCounter_cnt0_65536_37
	mov	a,_SCR_RTC_CNT0
	movx	@dptr,a
;	../SCR/main.c:254: cnt3After = SCR_RTC_CNT3;
	mov	dptr,#_SCR_RtcReadCounter_cnt3After_65536_37
	mov	a,_SCR_RTC_CNT3
	movx	@dptr,a
;	../SCR/main.c:255: cnt2After = SCR_RTC_CNT2;
	mov	dptr,#_SCR_RtcReadCounter_cnt2After_65536_37
	mov	a,_SCR_RTC_CNT2
	movx	@dptr,a
;	../SCR/main.c:256: cnt1After = SCR_RTC_CNT1;
	mov	dptr,#_SCR_RtcReadCounter_cnt1After_65536_37
	mov	a,_SCR_RTC_CNT1
	movx	@dptr,a
;	../SCR/main.c:257: } while((cnt3Before != cnt3After) ||
	mov	dptr,#_SCR_RtcReadCounter_cnt3Before_65536_37
	movx	a,@dptr
	mov	r7,a
	mov	dptr,#_SCR_RtcReadCounter_cnt3After_65536_37
	movx	a,@dptr
	mov	r6,a
	mov	a,r7
	cjne	a,ar6,.00176
	sjmp	.00177
.00176:
	sjmp	.00155
.00177:
;	../SCR/main.c:258: (cnt2Before != cnt2After) ||
	mov	dptr,#_SCR_RtcReadCounter_cnt2Before_65536_37
	movx	a,@dptr
	mov	r7,a
	mov	dptr,#_SCR_RtcReadCounter_cnt2After_65536_37
	movx	a,@dptr
	mov	r5,a
	mov	a,r7
	cjne	a,ar5,.00178
	sjmp	.00179
.00178:
	sjmp	.00155
.00179:
;	../SCR/main.c:259: (cnt1Before != cnt1After));
	mov	dptr,#_SCR_RtcReadCounter_cnt1Before_65536_37
	movx	a,@dptr
	mov	r7,a
	mov	dptr,#_SCR_RtcReadCounter_cnt1After_65536_37
	movx	a,@dptr
	mov	r4,a
	mov	a,r7
	cjne	a,ar4,.00180
	sjmp	.00181
.00180:
	sjmp	.00155
.00181:
;	../SCR/main.c:264: return (((uint32)cnt3After << 24) |
	mov	r7,#0x00
	mov	(_SCR_RtcReadCounter_sloc0_1_0 + 3),r6
;	1-genFromRTrack replaced	mov	_SCR_RtcReadCounter_sloc0_1_0,#0x00
	mov	_SCR_RtcReadCounter_sloc0_1_0,r7
;	1-genFromRTrack replaced	mov	(_SCR_RtcReadCounter_sloc0_1_0 + 1),#0x00
	mov	(_SCR_RtcReadCounter_sloc0_1_0 + 1),r7
;	1-genFromRTrack replaced	mov	(_SCR_RtcReadCounter_sloc0_1_0 + 2),#0x00
	mov	(_SCR_RtcReadCounter_sloc0_1_0 + 2),r7
;	../SCR/main.c:265: ((uint32)cnt2 << 16) |
	mov	ar0,r5
	mov	r1,#0x00
	mov	ar7,r1
	mov	ar5,r0
	clr	a
	mov	r0,a
	mov	r1,a
	mov	a,_SCR_RtcReadCounter_sloc0_1_0
	orl	ar0,a
	mov	a,(_SCR_RtcReadCounter_sloc0_1_0 + 1)
	orl	ar1,a
	mov	a,(_SCR_RtcReadCounter_sloc0_1_0 + 2)
	orl	ar5,a
	mov	a,(_SCR_RtcReadCounter_sloc0_1_0 + 3)
	orl	ar7,a
;	../SCR/main.c:266: ((uint32)cnt1 << 8) |
	mov	r6,#0x00
	mov	r3,#0x00
	mov	ar2,r3
	mov	ar3,r6
	mov	ar6,r4
	clr	a
	orl	ar0,a
	mov	a,r6
	orl	ar1,a
	mov	a,r3
	orl	ar5,a
	mov	a,r2
	orl	ar7,a
;	../SCR/main.c:267: (uint32)cnt0);
	mov	dptr,#_SCR_RtcReadCounter_cnt0_65536_37
	movx	a,@dptr
	mov	r6,a
	clr	a
	mov	r4,a
	mov	r3,a
	mov	r2,a
	mov	a,r6
	orl	ar0,a
	mov	a,r4
	orl	ar1,a
	mov	a,r3
	orl	ar5,a
	mov	a,r2
	orl	ar7,a
	mov	dpl,r0
	mov	dph,r1
	mov	b,r5
	mov	a,r7
.00158:
;	../SCR/main.c:268: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'SCR_RtcCompareIsr'
;------------------------------------------------------------
;flags                     Allocated with name '_SCR_RtcCompareIsr_flags_65536_40'
;previousLow               Allocated with name '_SCR_RtcCompareIsr_previousLow_65536_40'
;------------------------------------------------------------
;	../SCR/main.c:278: void SCR_RtcCompareIsr(void) __interrupt(XINTR13)
;	-----------------------------------------
;	 function SCR_RtcCompareIsr
;	-----------------------------------------
	.section .text.code.SCR_RtcCompareIsr,"ax" ;code for function SCR_RtcCompareIsr
	.type   SCR_RtcCompareIsr, @function
_SCR_RtcCompareIsr:
	.using 0
	push	bits
	push	acc
	push	b
	push	dpl
	push	dph
	push	(0+7)
	push	(0+6)
	push	(0+5)
	push	(0+4)
	push	(0+3)
	push	(0+2)
	push	(0+1)
	push	(0+0)
	push	psw
	mov	psw,#0x00
;	../SCR/main.c:288: SCR_UNLOCK_PROTECTED_BITS();
	mov	_SCR_PASSWD,#0x98
;	../SCR/main.c:289: SCR_RTC_CON &= (uint8)(~CFRTC_MASK);
	anl	_SCR_RTC_CON,#0xBF
;	../SCR/main.c:290: SCR_RTC_CON &= (uint8)(~RTCC_MASK);
	anl	_SCR_RTC_CON,#0xFE
;	../SCR/main.c:291: SCR_RTC_CNT3 = 0u;
	mov	_SCR_RTC_CNT3,#0x00
;	../SCR/main.c:292: SCR_RTC_CNT2 = 0u;
	mov	_SCR_RTC_CNT2,#0x00
;	../SCR/main.c:293: SCR_RTC_CNT1 = 0u;
	mov	_SCR_RTC_CNT1,#0x00
;	../SCR/main.c:294: SCR_RTC_CNT0 = 0u;
	mov	_SCR_RTC_CNT0,#0x00
;	../SCR/main.c:295: SCR_RTC_CON |= RTCC_MASK;
	orl	_SCR_RTC_CON,#0x01
;	../SCR/main.c:296: SCR_LOCK_PROTECTED_BITS();
	mov	_SCR_PASSWD,#0xA8
;	../SCR/main.c:298: previousLow = SCR_TotalElapsedTicksLow;
	mov	dptr,#_SCR_TotalElapsedTicksLow
	movx	a,@dptr
	mov	r4,a
	inc	dptr
	movx	a,@dptr
	mov	r5,a
	inc	dptr
	movx	a,@dptr
	mov	r6,a
	inc	dptr
	movx	a,@dptr
	mov	r7,a
	mov	dptr,#_SCR_RtcCompareIsr_previousLow_65536_40
	mov	a,r4
	movx	@dptr,a
	mov	a,r5
	inc	dptr
	movx	@dptr,a
	mov	a,r6
	inc	dptr
	movx	@dptr,a
	mov	a,r7
	inc	dptr
	movx	@dptr,a
;	../SCR/main.c:299: SCR_TotalElapsedTicksLow += (uint32)SCR_TIME_RTC_COMPARE_TICKS;
	mov	dptr,#_SCR_TotalElapsedTicksLow
	mov	a,#0x80
	add	a,r4
	movx	@dptr,a
	mov	a,#0x84
	addc	a,r5
	inc	dptr
	movx	@dptr,a
	mov	a,#0x1E
	addc	a,r6
	inc	dptr
	movx	@dptr,a
	clr	a
	addc	a,r7
	inc	dptr
	movx	@dptr,a
;	../SCR/main.c:300: if (SCR_TotalElapsedTicksLow < previousLow)
	mov	dptr,#_SCR_TotalElapsedTicksLow
	movx	a,@dptr
	mov	r4,a
	inc	dptr
	movx	a,@dptr
	mov	r5,a
	inc	dptr
	movx	a,@dptr
	mov	r6,a
	inc	dptr
	movx	a,@dptr
	mov	r7,a
	mov	dptr,#_SCR_RtcCompareIsr_previousLow_65536_40
	movx	a,@dptr
	mov	r0,a
	inc	dptr
	movx	a,@dptr
	mov	r1,a
	inc	dptr
	movx	a,@dptr
	mov	r2,a
	inc	dptr
	movx	a,@dptr
	mov	r3,a
	clr	c
	mov	a,r4
	subb	a,r0
	mov	a,r5
	subb	a,r1
	mov	a,r6
	subb	a,r2
	mov	a,r7
	subb	a,r3
	jnc	.00183
.00213:
;	../SCR/main.c:302: SCR_TotalElapsedTicksHigh++;
	mov	dptr,#_SCR_TotalElapsedTicksHigh
	movx	a,@dptr
	add	a,#0x01
	movx	@dptr,a
	inc	dptr
	movx	a,@dptr
	addc	a,#0x00
	movx	@dptr,a
	inc	dptr
	movx	a,@dptr
	addc	a,#0x00
	movx	@dptr,a
	inc	dptr
	movx	a,@dptr
	addc	a,#0x00
	movx	@dptr,a
.00183:
;	../SCR/main.c:305: SCR_TimeStoreU32(SCR_TIME_OFFSET_RTC_LAST_TICKS, SCR_TotalElapsedTicksLow);
	mov	dptr,#_SCR_TimeStoreU32_PARM_2
	mov	a,r4
	movx	@dptr,a
	mov	a,r5
	inc	dptr
	movx	@dptr,a
	mov	a,r6
	inc	dptr
	movx	@dptr,a
	mov	a,r7
	inc	dptr
	movx	@dptr,a
	mov	dpl,#0x14
	lcall	_SCR_TimeStoreU32
;	../SCR/main.c:306: SCR_TimeStoreU32(SCR_TIME_OFFSET_RTC_LAST_TICKS_HIGH, SCR_TotalElapsedTicksHigh);
	mov	dptr,#_SCR_TotalElapsedTicksHigh
	movx	a,@dptr
	mov	r4,a
	inc	dptr
	movx	a,@dptr
	mov	r5,a
	inc	dptr
	movx	a,@dptr
	mov	r6,a
	inc	dptr
	movx	a,@dptr
	mov	r7,a
	mov	dptr,#_SCR_TimeStoreU32_PARM_2
	mov	a,r4
	movx	@dptr,a
	mov	a,r5
	inc	dptr
	movx	@dptr,a
	mov	a,r6
	inc	dptr
	movx	@dptr,a
	mov	a,r7
	inc	dptr
	movx	@dptr,a
	mov	dpl,#0x3F
	lcall	_SCR_TimeStoreU32
;	../SCR/main.c:307: SCR_TimeStoreU32(SCR_TIME_OFFSET_ELAPSED_TICKS, SCR_TotalElapsedTicksLow);
	mov	dptr,#_SCR_TotalElapsedTicksLow
	movx	a,@dptr
	mov	r4,a
	inc	dptr
	movx	a,@dptr
	mov	r5,a
	inc	dptr
	movx	a,@dptr
	mov	r6,a
	inc	dptr
	movx	a,@dptr
	mov	r7,a
	mov	dptr,#_SCR_TimeStoreU32_PARM_2
	mov	a,r4
	movx	@dptr,a
	mov	a,r5
	inc	dptr
	movx	@dptr,a
	mov	a,r6
	inc	dptr
	movx	@dptr,a
	mov	a,r7
	inc	dptr
	movx	@dptr,a
	mov	dpl,#0x18
	lcall	_SCR_TimeStoreU32
;	../SCR/main.c:308: SCR_TimeStoreU32(SCR_TIME_OFFSET_ELAPSED_TICKS_HIGH, SCR_TotalElapsedTicksHigh);
	mov	dptr,#_SCR_TotalElapsedTicksHigh
	movx	a,@dptr
	mov	r4,a
	inc	dptr
	movx	a,@dptr
	mov	r5,a
	inc	dptr
	movx	a,@dptr
	mov	r6,a
	inc	dptr
	movx	a,@dptr
	mov	r7,a
	mov	dptr,#_SCR_TimeStoreU32_PARM_2
	mov	a,r4
	movx	@dptr,a
	mov	a,r5
	inc	dptr
	movx	@dptr,a
	mov	a,r6
	inc	dptr
	movx	@dptr,a
	mov	a,r7
	inc	dptr
	movx	@dptr,a
	mov	dpl,#0x3B
	lcall	_SCR_TimeStoreU32
;	../SCR/main.c:310: flags = SCR_TimeLoadU8(SCR_TIME_OFFSET_FLAGS);
	mov	dpl,#0x07
	lcall	_SCR_TimeLoadU8
	mov	a,dpl
;	../SCR/main.c:311: if (((flags & SCR_TIME_FLAG_ARMED) != 0u) && (SCR_TimeBaseActive != 0u))
	jb	acc.0,.00214
	ljmp	.00191
.00214:
	mov	dptr,#_SCR_TimeBaseActive
	movx	a,@dptr
	jnz	.00215
	ljmp	.00191
.00215:
;	../SCR/main.c:313: previousLow = SCR_TimeBaseUtcLow;
	mov	dptr,#_SCR_TimeBaseUtcLow
	movx	a,@dptr
	mov	r4,a
	inc	dptr
	movx	a,@dptr
	mov	r5,a
	inc	dptr
	movx	a,@dptr
	mov	r6,a
	inc	dptr
	movx	a,@dptr
	mov	r7,a
	mov	dptr,#_SCR_RtcCompareIsr_previousLow_65536_40
	mov	a,r4
	movx	@dptr,a
	mov	a,r5
	inc	dptr
	movx	@dptr,a
	mov	a,r6
	inc	dptr
	movx	@dptr,a
	mov	a,r7
	inc	dptr
	movx	@dptr,a
;	../SCR/main.c:314: SCR_TimeBaseUtcLow += (uint32)SCR_TIME_RTC_COMPARE_NS;
	mov	dptr,#_SCR_TimeBaseUtcLow
	mov	a,r4
	movx	@dptr,a
	mov	a,#0xE1
	add	a,r5
	inc	dptr
	movx	@dptr,a
	mov	a,#0xF5
	addc	a,r6
	inc	dptr
	movx	@dptr,a
	mov	a,#0x05
	addc	a,r7
	inc	dptr
	movx	@dptr,a
;	../SCR/main.c:315: if (SCR_TimeBaseUtcLow < previousLow)
	mov	dptr,#_SCR_TimeBaseUtcLow
	movx	a,@dptr
	mov	r4,a
	inc	dptr
	movx	a,@dptr
	mov	r5,a
	inc	dptr
	movx	a,@dptr
	mov	r6,a
	inc	dptr
	movx	a,@dptr
	mov	r7,a
	mov	dptr,#_SCR_RtcCompareIsr_previousLow_65536_40
	movx	a,@dptr
	mov	r0,a
	inc	dptr
	movx	a,@dptr
	mov	r1,a
	inc	dptr
	movx	a,@dptr
	mov	r2,a
	inc	dptr
	movx	a,@dptr
	mov	r3,a
	clr	c
	mov	a,r4
	subb	a,r0
	mov	a,r5
	subb	a,r1
	mov	a,r6
	subb	a,r2
	mov	a,r7
	subb	a,r3
	jnc	.00185
.00216:
;	../SCR/main.c:317: SCR_TimeBaseUtcHigh++;
	mov	dptr,#_SCR_TimeBaseUtcHigh
	movx	a,@dptr
	add	a,#0x01
	movx	@dptr,a
	inc	dptr
	movx	a,@dptr
	addc	a,#0x00
	movx	@dptr,a
	inc	dptr
	movx	a,@dptr
	addc	a,#0x00
	movx	@dptr,a
	inc	dptr
	movx	a,@dptr
	addc	a,#0x00
	movx	@dptr,a
.00185:
;	../SCR/main.c:320: previousLow = SCR_TimeBaseVehicleLow;
	mov	dptr,#_SCR_TimeBaseVehicleLow
	movx	a,@dptr
	mov	r4,a
	inc	dptr
	movx	a,@dptr
	mov	r5,a
	inc	dptr
	movx	a,@dptr
	mov	r6,a
	inc	dptr
	movx	a,@dptr
	mov	r7,a
	mov	dptr,#_SCR_RtcCompareIsr_previousLow_65536_40
	mov	a,r4
	movx	@dptr,a
	mov	a,r5
	inc	dptr
	movx	@dptr,a
	mov	a,r6
	inc	dptr
	movx	@dptr,a
	mov	a,r7
	inc	dptr
	movx	@dptr,a
;	../SCR/main.c:321: SCR_TimeBaseVehicleLow += (uint32)SCR_TIME_RTC_COMPARE_NS;
	mov	dptr,#_SCR_TimeBaseVehicleLow
	mov	a,r4
	movx	@dptr,a
	mov	a,#0xE1
	add	a,r5
	inc	dptr
	movx	@dptr,a
	mov	a,#0xF5
	addc	a,r6
	inc	dptr
	movx	@dptr,a
	mov	a,#0x05
	addc	a,r7
	inc	dptr
	movx	@dptr,a
;	../SCR/main.c:322: if (SCR_TimeBaseVehicleLow < previousLow)
	mov	dptr,#_SCR_TimeBaseVehicleLow
	movx	a,@dptr
	mov	r4,a
	inc	dptr
	movx	a,@dptr
	mov	r5,a
	inc	dptr
	movx	a,@dptr
	mov	r6,a
	inc	dptr
	movx	a,@dptr
	mov	r7,a
	mov	dptr,#_SCR_RtcCompareIsr_previousLow_65536_40
	movx	a,@dptr
	mov	r0,a
	inc	dptr
	movx	a,@dptr
	mov	r1,a
	inc	dptr
	movx	a,@dptr
	mov	r2,a
	inc	dptr
	movx	a,@dptr
	mov	r3,a
	clr	c
	mov	a,r4
	subb	a,r0
	mov	a,r5
	subb	a,r1
	mov	a,r6
	subb	a,r2
	mov	a,r7
	subb	a,r3
	jnc	.00187
.00217:
;	../SCR/main.c:324: SCR_TimeBaseVehicleHigh++;
	mov	dptr,#_SCR_TimeBaseVehicleHigh
	movx	a,@dptr
	add	a,#0x01
	movx	@dptr,a
	inc	dptr
	movx	a,@dptr
	addc	a,#0x00
	movx	@dptr,a
	inc	dptr
	movx	a,@dptr
	addc	a,#0x00
	movx	@dptr,a
	inc	dptr
	movx	a,@dptr
	addc	a,#0x00
	movx	@dptr,a
.00187:
;	../SCR/main.c:327: SCR_TimeStoreU32(SCR_TIME_OFFSET_UTC_NS, SCR_TimeBaseUtcHigh);
	mov	dptr,#_SCR_TimeBaseUtcHigh
	movx	a,@dptr
	mov	r4,a
	inc	dptr
	movx	a,@dptr
	mov	r5,a
	inc	dptr
	movx	a,@dptr
	mov	r6,a
	inc	dptr
	movx	a,@dptr
	mov	r7,a
	mov	dptr,#_SCR_TimeStoreU32_PARM_2
	mov	a,r4
	movx	@dptr,a
	mov	a,r5
	inc	dptr
	movx	@dptr,a
	mov	a,r6
	inc	dptr
	movx	@dptr,a
	mov	a,r7
	inc	dptr
	movx	@dptr,a
	mov	dpl,#0x08
	lcall	_SCR_TimeStoreU32
;	../SCR/main.c:328: SCR_TimeStoreU32((uint8)(SCR_TIME_OFFSET_UTC_NS + 4u), SCR_TimeBaseUtcLow);
	mov	dptr,#_SCR_TimeBaseUtcLow
	movx	a,@dptr
	mov	r4,a
	inc	dptr
	movx	a,@dptr
	mov	r5,a
	inc	dptr
	movx	a,@dptr
	mov	r6,a
	inc	dptr
	movx	a,@dptr
	mov	r7,a
	mov	dptr,#_SCR_TimeStoreU32_PARM_2
	mov	a,r4
	movx	@dptr,a
	mov	a,r5
	inc	dptr
	movx	@dptr,a
	mov	a,r6
	inc	dptr
	movx	@dptr,a
	mov	a,r7
	inc	dptr
	movx	@dptr,a
	mov	dpl,#0x0C
	lcall	_SCR_TimeStoreU32
;	../SCR/main.c:329: SCR_TimeStoreU32(SCR_TIME_OFFSET_VEHICLE_NS, SCR_TimeBaseVehicleHigh);
	mov	dptr,#_SCR_TimeBaseVehicleHigh
	movx	a,@dptr
	mov	r4,a
	inc	dptr
	movx	a,@dptr
	mov	r5,a
	inc	dptr
	movx	a,@dptr
	mov	r6,a
	inc	dptr
	movx	a,@dptr
	mov	r7,a
	mov	dptr,#_SCR_TimeStoreU32_PARM_2
	mov	a,r4
	movx	@dptr,a
	mov	a,r5
	inc	dptr
	movx	@dptr,a
	mov	a,r6
	inc	dptr
	movx	@dptr,a
	mov	a,r7
	inc	dptr
	movx	@dptr,a
	mov	dpl,#0x24
	lcall	_SCR_TimeStoreU32
;	../SCR/main.c:330: SCR_TimeStoreU32((uint8)(SCR_TIME_OFFSET_VEHICLE_NS + 4u), SCR_TimeBaseVehicleLow);
	mov	dptr,#_SCR_TimeBaseVehicleLow
	movx	a,@dptr
	mov	r4,a
	inc	dptr
	movx	a,@dptr
	mov	r5,a
	inc	dptr
	movx	a,@dptr
	mov	r6,a
	inc	dptr
	movx	a,@dptr
	mov	r7,a
	mov	dptr,#_SCR_TimeStoreU32_PARM_2
	mov	a,r4
	movx	@dptr,a
	mov	a,r5
	inc	dptr
	movx	@dptr,a
	mov	a,r6
	inc	dptr
	movx	@dptr,a
	mov	a,r7
	inc	dptr
	movx	@dptr,a
	mov	dpl,#0x28
	lcall	_SCR_TimeStoreU32
.00191:
;	../SCR/main.c:332: }
	pop	psw
	pop	(0+0)
	pop	(0+1)
	pop	(0+2)
	pop	(0+3)
	pop	(0+4)
	pop	(0+5)
	pop	(0+6)
	pop	(0+7)
	pop	dph
	pop	dpl
	pop	b
	pop	acc
	pop	bits
	reti
;------------------------------------------------------------
;Allocation info for local variables in function 'SCR_TimeInit'
;------------------------------------------------------------
;flags                     Allocated with name '_SCR_TimeInit_flags_65536_46'
;------------------------------------------------------------
;	../SCR/main.c:335: static void SCR_TimeInit(void)
;	-----------------------------------------
;	 function SCR_TimeInit
;	-----------------------------------------
	.section .text.code.SCR_TimeInit,"ax" ;code for function SCR_TimeInit
	.type   SCR_TimeInit, @function
_SCR_TimeInit:
	.using 0
;	../SCR/main.c:339: SCR_TimeBaseActive = 0u;
	mov	dptr,#_SCR_TimeBaseActive
	clr	a
	movx	@dptr,a
;	../SCR/main.c:341: flags = SCR_TimeLoadU8(SCR_TIME_OFFSET_FLAGS);
	mov	dpl,#0x07
	lcall	_SCR_TimeLoadU8
	mov	r7,dpl
;	../SCR/main.c:343: SCR_TimeStoreU8(SCR_TIME_OFFSET_DEBUG_INIT_STATUS, SCR_TIME_INIT_STATUS_ENTERED);
	mov	dptr,#_SCR_TimeStoreU8_PARM_2
	mov	a,#0x10
	movx	@dptr,a
	mov	dpl,#0x3A
	push	ar7
	lcall	_SCR_TimeStoreU8
	pop	ar7
;	../SCR/main.c:344: SCR_TimeStoreU8(SCR_TIME_OFFSET_DEBUG_FLAGS_READ, flags);
	mov	dptr,#_SCR_TimeStoreU8_PARM_2
	mov	a,r7
	movx	@dptr,a
	mov	dpl,#0x33
	push	ar7
	lcall	_SCR_TimeStoreU8
;	../SCR/main.c:345: SCR_TimeStoreU8(SCR_TIME_OFFSET_DEBUG_MAGIC0_READ, SCR_TimeLoadU8(SCR_TIME_OFFSET_MAGIC));
	mov	dpl,#0x00
	lcall	_SCR_TimeLoadU8
	mov	r6,dpl
	mov	dptr,#_SCR_TimeStoreU8_PARM_2
	mov	a,r6
	movx	@dptr,a
	mov	dpl,#0x34
	lcall	_SCR_TimeStoreU8
;	../SCR/main.c:346: SCR_TimeStoreU8(SCR_TIME_OFFSET_DEBUG_MAGIC1_READ, SCR_TimeLoadU8((uint8)(SCR_TIME_OFFSET_MAGIC + 1u)));
	mov	dpl,#0x01
	lcall	_SCR_TimeLoadU8
	mov	r6,dpl
	mov	dptr,#_SCR_TimeStoreU8_PARM_2
	mov	a,r6
	movx	@dptr,a
	mov	dpl,#0x35
	lcall	_SCR_TimeStoreU8
;	../SCR/main.c:347: SCR_TimeStoreU8(SCR_TIME_OFFSET_DEBUG_MAGIC2_READ, SCR_TimeLoadU8((uint8)(SCR_TIME_OFFSET_MAGIC + 2u)));
	mov	dpl,#0x02
	lcall	_SCR_TimeLoadU8
	mov	r6,dpl
	mov	dptr,#_SCR_TimeStoreU8_PARM_2
	mov	a,r6
	movx	@dptr,a
	mov	dpl,#0x36
	lcall	_SCR_TimeStoreU8
;	../SCR/main.c:348: SCR_TimeStoreU8(SCR_TIME_OFFSET_DEBUG_MAGIC3_READ, SCR_TimeLoadU8((uint8)(SCR_TIME_OFFSET_MAGIC + 3u)));
	mov	dpl,#0x03
	lcall	_SCR_TimeLoadU8
	mov	r6,dpl
	mov	dptr,#_SCR_TimeStoreU8_PARM_2
	mov	a,r6
	movx	@dptr,a
	mov	dpl,#0x37
	lcall	_SCR_TimeStoreU8
;	../SCR/main.c:349: SCR_TimeStoreU8(SCR_TIME_OFFSET_DEBUG_VERSION_READ, SCR_TimeLoadU8(SCR_TIME_OFFSET_VERSION));
	mov	dpl,#0x04
	lcall	_SCR_TimeLoadU8
	mov	r6,dpl
	mov	dptr,#_SCR_TimeStoreU8_PARM_2
	mov	a,r6
	movx	@dptr,a
	mov	dpl,#0x38
	lcall	_SCR_TimeStoreU8
;	../SCR/main.c:350: SCR_TimeStoreU8(SCR_TIME_OFFSET_DEBUG_VALID_READ, SCR_TimeLoadU8(SCR_TIME_OFFSET_VALID));
	mov	dpl,#0x05
	lcall	_SCR_TimeLoadU8
	mov	r6,dpl
	mov	dptr,#_SCR_TimeStoreU8_PARM_2
	mov	a,r6
	movx	@dptr,a
	mov	dpl,#0x39
	lcall	_SCR_TimeStoreU8
;	../SCR/main.c:355: SCR_UNLOCK_PROTECTED_BITS();
	mov	_SCR_PASSWD,#0x98
;	../SCR/main.c:356: SCR_SCU_PAGE = MOD_PAGE_1;
	mov	_SCR_SCU_PAGE,#0x01
;	../SCR/main.c:357: SCR_SCU_PMCON1 &= (uint8)(~RTC_DIS_MASK);
	anl	_SCR_SCU_PMCON1,#0xFB
;	../SCR/main.c:358: SCR_SCU_DBG_MODSUSP &= (uint8)(~RTCSUSP_MASK);
	anl	_SCR_SCU_DBG_MODSUSP,#0xFD
;	../SCR/main.c:359: SCR_LOCK_PROTECTED_BITS();
	mov	_SCR_PASSWD,#0xA8
;	../SCR/main.c:375: SCR_UNLOCK_PROTECTED_BITS();
	mov	_SCR_PASSWD,#0x98
;	../SCR/scr_rtc_sdcc.h:196: SCR_RTC_CON &= (~RTCC_MASK);
	anl	_SCR_RTC_CON,#0xFE
;	../SCR/main.c:379: SCR_RTC_CON = 0u;
	mov	_SCR_RTC_CON,#0x00
;	../SCR/main.c:380: SCR_RTC_CON = (uint8)(RTCCLKSEL_MASK | RTPBYP_MASK | ECRTC_MASK);
	mov	_SCR_RTC_CON,#0x16
;	../SCR/main.c:382: SCR_RTC_CNT3 = 0u;
	mov	_SCR_RTC_CNT3,#0x00
;	../SCR/main.c:383: SCR_RTC_CNT2 = 0u;
	mov	_SCR_RTC_CNT2,#0x00
;	../SCR/main.c:384: SCR_RTC_CNT1 = 0u;
	mov	_SCR_RTC_CNT1,#0x00
;	../SCR/main.c:385: SCR_RTC_CNT0 = 0u;
	mov	_SCR_RTC_CNT0,#0x00
;	../SCR/main.c:387: SCR_RTC_CR3 = (uint8)((uint32)SCR_TIME_RTC_COMPARE_TICKS >> 24);
	mov	_SCR_RTC_CR3,#0x00
;	../SCR/main.c:388: SCR_RTC_CR2 = (uint8)((uint32)SCR_TIME_RTC_COMPARE_TICKS >> 16);
	mov	_SCR_RTC_CR2,#0x1E
;	../SCR/main.c:389: SCR_RTC_CR1 = (uint8)((uint32)SCR_TIME_RTC_COMPARE_TICKS >> 8);
	mov	_SCR_RTC_CR1,#0x84
;	../SCR/main.c:390: SCR_RTC_CR0 = (uint8)(SCR_TIME_RTC_COMPARE_TICKS);
	mov	_SCR_RTC_CR0,#0x80
;	../SCR/main.c:392: SCR_RTC_CON &= (uint8)(~CFRTC_MASK);
	anl	_SCR_RTC_CON,#0xBF
;	../SCR/main.c:394: SCR_LOCK_PROTECTED_BITS();
	mov	_SCR_PASSWD,#0xA8
;	../SCR/main.c:396: if (SCR_TimeLoadU32(SCR_TIME_OFFSET_MAGIC) != SCR_TIME_MAGIC)
	mov	dpl,#0x00
	lcall	_SCR_TimeLoadU32
	mov	r3,dpl
	mov	r4,dph
	mov	r5,b
	mov	r6,a
	pop	ar7
	cjne	r3,#0x52,.00256
	cjne	r4,#0x43,.00256
	cjne	r5,#0x53,.00256
	cjne	r6,#0x54,.00256
	sjmp	.00228
.00256:
;	../SCR/main.c:398: SCR_TimeStoreU8(SCR_TIME_OFFSET_DEBUG_INIT_STATUS, SCR_TIME_INIT_STATUS_MAGIC_FAIL);
	mov	dptr,#_SCR_TimeStoreU8_PARM_2
	mov	a,#0xE1
	movx	@dptr,a
	mov	dpl,#0x3A
	lcall	_SCR_TimeStoreU8
	ljmp	.00229
.00228:
;	../SCR/main.c:400: else if (SCR_TimeLoadU8(SCR_TIME_OFFSET_VERSION) != SCR_TIME_VERSION)
	mov	dpl,#0x04
	push	ar7
	lcall	_SCR_TimeLoadU8
	mov	r6,dpl
	pop	ar7
	cjne	r6,#0x03,.00257
	sjmp	.00225
.00257:
;	../SCR/main.c:402: SCR_TimeStoreU8(SCR_TIME_OFFSET_DEBUG_INIT_STATUS, SCR_TIME_INIT_STATUS_VERSION_FAIL);
	mov	dptr,#_SCR_TimeStoreU8_PARM_2
	mov	a,#0xE2
	movx	@dptr,a
	mov	dpl,#0x3A
	lcall	_SCR_TimeStoreU8
	ljmp	.00229
.00225:
;	../SCR/main.c:404: else if (SCR_TimeLoadU8(SCR_TIME_OFFSET_VALID) != SCR_TIME_VALID)
	mov	dpl,#0x05
	push	ar7
	lcall	_SCR_TimeLoadU8
	mov	r6,dpl
	pop	ar7
	cjne	r6,#0x01,.00258
	sjmp	.00222
.00258:
;	../SCR/main.c:406: SCR_TimeStoreU8(SCR_TIME_OFFSET_DEBUG_INIT_STATUS, SCR_TIME_INIT_STATUS_VALID_FAIL);
	mov	dptr,#_SCR_TimeStoreU8_PARM_2
	mov	a,#0xE3
	movx	@dptr,a
	mov	dpl,#0x3A
	lcall	_SCR_TimeStoreU8
	ljmp	.00229
.00222:
;	../SCR/main.c:408: else if ((flags & SCR_TIME_FLAG_ARMED) == 0u)
	mov	a,r7
	jb	acc.0,.00219
.00259:
;	../SCR/main.c:410: SCR_TimeStoreU8(SCR_TIME_OFFSET_DEBUG_INIT_STATUS, SCR_TIME_INIT_STATUS_ARMED_FAIL);
	mov	dptr,#_SCR_TimeStoreU8_PARM_2
	mov	a,#0xE4
	movx	@dptr,a
	mov	dpl,#0x3A
	lcall	_SCR_TimeStoreU8
	ljmp	.00229
.00219:
;	../SCR/main.c:414: SCR_TimeBaseUtcHigh = SCR_TimeLoadU32(SCR_TIME_OFFSET_UTC_NS);
	mov	dpl,#0x08
	lcall	_SCR_TimeLoadU32
	mov	r4,dpl
	mov	r5,dph
	mov	r6,b
	mov	r7,a
	mov	dptr,#_SCR_TimeBaseUtcHigh
	mov	a,r4
	movx	@dptr,a
	mov	a,r5
	inc	dptr
	movx	@dptr,a
	mov	a,r6
	inc	dptr
	movx	@dptr,a
	mov	a,r7
	inc	dptr
	movx	@dptr,a
;	../SCR/main.c:415: SCR_TimeBaseUtcLow = SCR_TimeLoadU32((uint8)(SCR_TIME_OFFSET_UTC_NS + 4u));
	mov	dpl,#0x0C
	lcall	_SCR_TimeLoadU32
	mov	r4,dpl
	mov	r5,dph
	mov	r6,b
	mov	r7,a
	mov	dptr,#_SCR_TimeBaseUtcLow
	mov	a,r4
	movx	@dptr,a
	mov	a,r5
	inc	dptr
	movx	@dptr,a
	mov	a,r6
	inc	dptr
	movx	@dptr,a
	mov	a,r7
	inc	dptr
	movx	@dptr,a
;	../SCR/main.c:416: SCR_TimeBaseVehicleHigh = SCR_TimeLoadU32(SCR_TIME_OFFSET_VEHICLE_NS);
	mov	dpl,#0x24
	lcall	_SCR_TimeLoadU32
	mov	r4,dpl
	mov	r5,dph
	mov	r6,b
	mov	r7,a
	mov	dptr,#_SCR_TimeBaseVehicleHigh
	mov	a,r4
	movx	@dptr,a
	mov	a,r5
	inc	dptr
	movx	@dptr,a
	mov	a,r6
	inc	dptr
	movx	@dptr,a
	mov	a,r7
	inc	dptr
	movx	@dptr,a
;	../SCR/main.c:417: SCR_TimeBaseVehicleLow = SCR_TimeLoadU32((uint8)(SCR_TIME_OFFSET_VEHICLE_NS + 4u));
	mov	dpl,#0x28
	lcall	_SCR_TimeLoadU32
	mov	r4,dpl
	mov	r5,dph
	mov	r6,b
	mov	r7,a
	mov	dptr,#_SCR_TimeBaseVehicleLow
	mov	a,r4
	movx	@dptr,a
	mov	a,r5
	inc	dptr
	movx	@dptr,a
	mov	a,r6
	inc	dptr
	movx	@dptr,a
	mov	a,r7
	inc	dptr
	movx	@dptr,a
;	../SCR/main.c:419: SCR_TimeBaseActive = 1u;
	mov	dptr,#_SCR_TimeBaseActive
	mov	a,#0x01
	movx	@dptr,a
;	../SCR/main.c:422: SCR_TotalElapsedTicksLow = 0u;
	mov	dptr,#_SCR_TotalElapsedTicksLow
	clr	a
	movx	@dptr,a
	inc	dptr
	movx	@dptr,a
	inc	dptr
	movx	@dptr,a
	inc	dptr
	movx	@dptr,a
;	../SCR/main.c:423: SCR_TotalElapsedTicksHigh = 0u;
	mov	dptr,#_SCR_TotalElapsedTicksHigh
	movx	@dptr,a
	inc	dptr
	movx	@dptr,a
	inc	dptr
	movx	@dptr,a
	inc	dptr
	movx	@dptr,a
;	../SCR/main.c:429: SCR_TimeStoreU32(SCR_TIME_OFFSET_RTC_START_TICKS, 0u);
	mov	dptr,#_SCR_TimeStoreU32_PARM_2
	movx	@dptr,a
	inc	dptr
	movx	@dptr,a
	inc	dptr
	movx	@dptr,a
	inc	dptr
	movx	@dptr,a
	mov	dpl,#0x10
	lcall	_SCR_TimeStoreU32
;	../SCR/main.c:430: SCR_TimeStoreU32(SCR_TIME_OFFSET_RTC_LAST_TICKS, 0u);
	mov	dptr,#_SCR_TimeStoreU32_PARM_2
	clr	a
	movx	@dptr,a
	inc	dptr
	movx	@dptr,a
	inc	dptr
	movx	@dptr,a
	inc	dptr
	movx	@dptr,a
	mov	dpl,#0x14
	lcall	_SCR_TimeStoreU32
;	../SCR/main.c:431: SCR_TimeStoreU32(SCR_TIME_OFFSET_ELAPSED_TICKS, 0u);
	mov	dptr,#_SCR_TimeStoreU32_PARM_2
	clr	a
	movx	@dptr,a
	inc	dptr
	movx	@dptr,a
	inc	dptr
	movx	@dptr,a
	inc	dptr
	movx	@dptr,a
	mov	dpl,#0x18
	lcall	_SCR_TimeStoreU32
;	../SCR/main.c:432: SCR_TimeStoreU32(SCR_TIME_OFFSET_RTC_LAST_TICKS_HIGH, 0u);
	mov	dptr,#_SCR_TimeStoreU32_PARM_2
	clr	a
	movx	@dptr,a
	inc	dptr
	movx	@dptr,a
	inc	dptr
	movx	@dptr,a
	inc	dptr
	movx	@dptr,a
	mov	dpl,#0x3F
	lcall	_SCR_TimeStoreU32
;	../SCR/main.c:433: SCR_TimeStoreU32(SCR_TIME_OFFSET_ELAPSED_TICKS_HIGH, 0u);
	mov	dptr,#_SCR_TimeStoreU32_PARM_2
	clr	a
	movx	@dptr,a
	inc	dptr
	movx	@dptr,a
	inc	dptr
	movx	@dptr,a
	inc	dptr
	movx	@dptr,a
	mov	dpl,#0x3B
	lcall	_SCR_TimeStoreU32
;	../SCR/main.c:434: SCR_TimeStoreU8(SCR_TIME_OFFSET_DEBUG_INIT_STATUS, SCR_TIME_INIT_STATUS_ACTIVE);
	mov	dptr,#_SCR_TimeStoreU8_PARM_2
	mov	a,#0xA5
	movx	@dptr,a
	mov	dpl,#0x3A
	lcall	_SCR_TimeStoreU8
.00229:
;	../SCR/main.c:441: SCR_UNLOCK_PROTECTED_BITS();
	mov	_SCR_PASSWD,#0x98
;	../SCR/scr_rtc_sdcc.h:191: SCR_RTC_CON |= RTCC_MASK;
	orl	_SCR_RTC_CON,#0x01
;	../SCR/main.c:443: SCR_LOCK_PROTECTED_BITS();
	mov	_SCR_PASSWD,#0xA8
;	../SCR/main.c:446: if (SCR_TimeBaseActive != 0u)
	mov	dptr,#_SCR_TimeBaseActive
	movx	a,@dptr
	jz	.00231
.00260:
;	../SCR/main.c:448: SCR_IEN1 |= (uint8)(1u << 7u);  /* IEN1.ECCIP3: enable XINTR13 RTC */
	orl	_SCR_IEN1,#0x80
;	../SCR/main.c:449: SCR_IEN0_EA = 1u;               /* global interrupt enable */
;	assignBit
	setb	_SCR_IEN0_EA
.00231:
;	../SCR/main.c:453: SCR_TimeStoreU8(SCR_TIME_OFFSET_DEBUG_RTC_CON, SCR_RTC_CON);
	mov	dptr,#_SCR_TimeStoreU8_PARM_2
	mov	a,_SCR_RTC_CON
	movx	@dptr,a
	mov	dpl,#0x2C
	lcall	_SCR_TimeStoreU8
;	../SCR/main.c:455: SCR_SCU_PAGE = MOD_PAGE_1;
	mov	_SCR_SCU_PAGE,#0x01
;	../SCR/main.c:456: SCR_TimeStoreU8(SCR_TIME_OFFSET_DEBUG_PMCON1, SCR_SCU_PMCON1);
	mov	dptr,#_SCR_TimeStoreU8_PARM_2
	mov	a,_SCR_SCU_PMCON1
	movx	@dptr,a
	mov	dpl,#0x2D
	lcall	_SCR_TimeStoreU8
;	../SCR/main.c:457: SCR_TimeStoreU8(SCR_TIME_OFFSET_DEBUG_TIME_ACTIVE, SCR_TimeBaseActive);
	mov	dptr,#_SCR_TimeBaseActive
	movx	a,@dptr
	mov	dptr,#_SCR_TimeStoreU8_PARM_2
	movx	@dptr,a
	mov	dpl,#0x32
	lcall	_SCR_TimeStoreU8
.00234:
;	../SCR/main.c:458: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'SCR_TimeUpdate'
;------------------------------------------------------------
;flags                     Allocated with name '_SCR_TimeUpdate_flags_65536_60'
;------------------------------------------------------------
;	../SCR/main.c:460: static void SCR_TimeUpdate(void)
;	-----------------------------------------
;	 function SCR_TimeUpdate
;	-----------------------------------------
	.section .text.code.SCR_TimeUpdate,"ax" ;code for function SCR_TimeUpdate
	.type   SCR_TimeUpdate, @function
_SCR_TimeUpdate:
	.using 0
;	../SCR/main.c:469: *G_BOOT_STAGE_ADDR = BOOT_STAGE_TIME_UPDATE_ENTRY;
;	../SCR/main.c:480: *G_BOOT_STAGE_ADDR = BOOT_STAGE_TIME_COUNTER_READ;
	mov	dptr,#0x1769
	mov	a,#0x50
	movx	@dptr,a
	inc	a
	movx	@dptr,a
;	../SCR/main.c:485: *G_BOOT_STAGE_ADDR = BOOT_STAGE_TIME_TICKS_STORED;
	mov	dptr,#0x1769
	inc	a
	movx	@dptr,a
;	../SCR/main.c:488: SCR_TimeStoreU8(SCR_TIME_OFFSET_DEBUG_RTC_CON,   SCR_RTC_CON);
	mov	dptr,#_SCR_TimeStoreU8_PARM_2
	mov	a,_SCR_RTC_CON
	movx	@dptr,a
	mov	dpl,#0x2C
	lcall	_SCR_TimeStoreU8
;	../SCR/main.c:489: SCR_TimeStoreU8(SCR_TIME_OFFSET_DEBUG_RTC_CNT0,  SCR_RTC_CNT0);
	mov	dptr,#_SCR_TimeStoreU8_PARM_2
	mov	a,_SCR_RTC_CNT0
	movx	@dptr,a
	mov	dpl,#0x2E
	lcall	_SCR_TimeStoreU8
;	../SCR/main.c:490: SCR_TimeStoreU8(SCR_TIME_OFFSET_DEBUG_RTC_CNT1,  SCR_RTC_CNT1);
	mov	dptr,#_SCR_TimeStoreU8_PARM_2
	mov	a,_SCR_RTC_CNT1
	movx	@dptr,a
	mov	dpl,#0x2F
	lcall	_SCR_TimeStoreU8
;	../SCR/main.c:491: SCR_TimeStoreU8(SCR_TIME_OFFSET_DEBUG_RTC_CNT2,  SCR_RTC_CNT2);
	mov	dptr,#_SCR_TimeStoreU8_PARM_2
	mov	a,_SCR_RTC_CNT2
	movx	@dptr,a
	mov	dpl,#0x30
	lcall	_SCR_TimeStoreU8
;	../SCR/main.c:492: SCR_TimeStoreU8(SCR_TIME_OFFSET_DEBUG_RTC_CNT3,  SCR_RTC_CNT3);
	mov	dptr,#_SCR_TimeStoreU8_PARM_2
	mov	a,_SCR_RTC_CNT3
	movx	@dptr,a
	mov	dpl,#0x31
	lcall	_SCR_TimeStoreU8
;	../SCR/main.c:493: SCR_TimeStoreU8(SCR_TIME_OFFSET_DEBUG_TIME_ACTIVE, SCR_TimeBaseActive);
	mov	dptr,#_SCR_TimeBaseActive
	movx	a,@dptr
	mov	dptr,#_SCR_TimeStoreU8_PARM_2
	movx	@dptr,a
	mov	dpl,#0x32
	lcall	_SCR_TimeStoreU8
;	../SCR/main.c:495: flags = SCR_TimeLoadU8(SCR_TIME_OFFSET_FLAGS);
	mov	dpl,#0x07
	lcall	_SCR_TimeLoadU8
	mov	r7,dpl
;	../SCR/main.c:496: SCR_TimeStoreU8(SCR_TIME_OFFSET_DEBUG_FLAGS_READ, flags);
	mov	dptr,#_SCR_TimeStoreU8_PARM_2
	mov	a,r7
	movx	@dptr,a
	mov	dpl,#0x33
	push	ar7
	lcall	_SCR_TimeStoreU8
	pop	ar7
;	../SCR/main.c:497: *G_BOOT_STAGE_ADDR = BOOT_STAGE_TIME_FLAGS_READ;
	mov	dptr,#0x1769
	mov	a,#0x53
	movx	@dptr,a
;	../SCR/main.c:498: if ((flags & SCR_TIME_FLAG_ARMED) != 0u)
	mov	a,r7
	jnb	acc.0,.00264
.00275:
;	../SCR/main.c:500: (*d1)++;
	mov	dptr,#0x1800
	movx	a,@dptr
	mov	r7,a
	inc	r7
	mov	dptr,#0x1800
	mov	a,r7
	movx	@dptr,a
;	../SCR/main.c:508: if (SCR_TimeBaseActive != 0u)
	mov	dptr,#_SCR_TimeBaseActive
	movx	a,@dptr
	jz	.00264
.00276:
;	../SCR/main.c:510: (*d2)++;
	mov	dptr,#0x1801
	movx	a,@dptr
	mov	r7,a
	inc	r7
	mov	dptr,#0x1801
	mov	a,r7
	movx	@dptr,a
.00264:
;	../SCR/main.c:526: *G_BOOT_STAGE_ADDR = BOOT_STAGE_TIME_UPDATE_DONE;
	mov	dptr,#0x1769
	mov	a,#0x56
	movx	@dptr,a
.00265:
;	../SCR/main.c:527: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'WCAN_WaitForSelectiveWakeAck'
;------------------------------------------------------------
;expected                  Allocated with name '_WCAN_WaitForSelectiveWakeAck_expected_65536_63'
;timeout                   Allocated with name '_WCAN_WaitForSelectiveWakeAck_timeout_65536_64'
;------------------------------------------------------------
;	../SCR/main.c:529: static uint8 WCAN_WaitForSelectiveWakeAck(uint8 expected)
;	-----------------------------------------
;	 function WCAN_WaitForSelectiveWakeAck
;	-----------------------------------------
	.section .text.code.WCAN_WaitForSelectiveWakeAck,"ax" ;code for function WCAN_WaitForSelectiveWakeAck
	.type   WCAN_WaitForSelectiveWakeAck, @function
_WCAN_WaitForSelectiveWakeAck:
	.using 0
	mov	a,dpl
	mov	dptr,#_WCAN_WaitForSelectiveWakeAck_expected_65536_63
	movx	@dptr,a
;	../SCR/main.c:533: SCR_WCAN_PAGE = MOD_PAGE_1;
	mov	_SCR_WCAN_PAGE,#0x01
;	../SCR/main.c:535: while(((SCR_WCAN_INTESTAT0 & WCAN_STATUS0_SWACK_MASK) != expected) && (timeout > 0u))
	movx	a,@dptr
	mov	r7,a
	mov	r5,#0xFF
	mov	r6,#0xFF
.00278:
	mov	r3,_SCR_WCAN_INTESTAT0
	anl	ar3,#0x01
	mov	r4,#0x00
	mov	ar1,r7
	mov	r2,#0x00
	mov	a,r3
	cjne	a,ar1,.00302
	mov	a,r4
	cjne	a,ar2,.00302
	sjmp	.00280
.00302:
	mov	a,r5
	orl	a,r6
	jz	.00280
.00303:
;	../SCR/main.c:537: timeout--;
	dec	r5
	cjne	r5,#0xFF,.00304
	dec	r6
.00304:
	sjmp	.00278
.00280:
;	../SCR/main.c:540: return (timeout > 0u) ? 1u : 0u;
	mov	a,r5
	orl	a,r6
	jz	.00283
.00305:
	mov	r6,#0x01
	mov	r7,#0x00
	sjmp	.00284
.00283:
	mov	r6,#0x00
	mov	r7,#0x00
.00284:
	mov	dpl,r6
.00281:
;	../SCR/main.c:541: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'WCAN_ClearEvents'
;------------------------------------------------------------
;	../SCR/main.c:543: static void WCAN_ClearEvents(void)
;	-----------------------------------------
;	 function WCAN_ClearEvents
;	-----------------------------------------
	.section .text.code.WCAN_ClearEvents,"ax" ;code for function WCAN_ClearEvents
	.type   WCAN_ClearEvents, @function
_WCAN_ClearEvents:
	.using 0
;	../SCR/main.c:545: SCR_WCAN_PAGE = MOD_PAGE_1;
	mov	_SCR_WCAN_PAGE,#0x01
;	../SCR/main.c:546: SCR_WCAN_INTESCLR0 = WCAN_STATUS0_CLEAR_MASK;
	mov	_SCR_WCAN_INTESCLR0,#0xFC
;	../SCR/main.c:547: SCR_WCAN_INTESCLR1 = WCAN_STATUS1_CLEAR_MASK;
	mov	_SCR_WCAN_INTESCLR1,#0x0B
.00306:
;	../SCR/main.c:548: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'SCR_RequestStandbyWake'
;------------------------------------------------------------
;wakeReason                Allocated with name '_SCR_RequestStandbyWake_wakeReason_65536_68'
;combinedWakeReason        Allocated with name '_SCR_RequestStandbyWake_combinedWakeReason_65536_69'
;------------------------------------------------------------
;	../SCR/main.c:550: static void SCR_RequestStandbyWake(uint8 wakeReason)
;	-----------------------------------------
;	 function SCR_RequestStandbyWake
;	-----------------------------------------
	.section .text.code.SCR_RequestStandbyWake,"ax" ;code for function SCR_RequestStandbyWake
	.type   SCR_RequestStandbyWake, @function
_SCR_RequestStandbyWake:
	.using 0
	mov	a,dpl
	mov	dptr,#_SCR_RequestStandbyWake_wakeReason_65536_68
	movx	@dptr,a
;	../SCR/main.c:554: combinedWakeReason = (uint8)(*G_WAKE_REASON_ADDR | wakeReason);
	mov	dptr,#0x1765
	movx	a,@dptr
	mov	r7,a
	mov	dptr,#_SCR_RequestStandbyWake_wakeReason_65536_68
	movx	a,@dptr
	mov	r6,a
	orl	ar7,a
;	../SCR/main.c:555: *G_WAKE_REASON_ADDR = combinedWakeReason;
	mov	dptr,#0x1765
	mov	a,r7
	movx	@dptr,a
;	../SCR/main.c:558: (uint8)(SCR_TimeLoadU8(SCR_TIME_OFFSET_WAKE_REASON) | wakeReason));
	mov	dpl,#0x20
	push	ar6
	lcall	_SCR_TimeLoadU8
	mov	a,dpl
	pop	ar6
	mov	dptr,#_SCR_TimeStoreU8_PARM_2
	orl	a,r6
	movx	@dptr,a
	mov	dpl,#0x20
	lcall	_SCR_TimeStoreU8
;	../SCR/main.c:560: SCR_SCU_PAGE = MOD_PAGE_1;
	mov	_SCR_SCU_PAGE,#0x01
;	../SCR/main.c:562: WCANWKSEL_MASK | WDTWKSEL_MASK | ECCWKSEL_MASK | SCRWKP_MASK);
	orl	_SCR_SCU_STDBYWKP,#0x1D
.00308:
;	../SCR/main.c:563: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'SCR_ConfigureFaultWake'
;------------------------------------------------------------
;	../SCR/main.c:565: static void SCR_ConfigureFaultWake(void)
;	-----------------------------------------
;	 function SCR_ConfigureFaultWake
;	-----------------------------------------
	.section .text.code.SCR_ConfigureFaultWake,"ax" ;code for function SCR_ConfigureFaultWake
	.type   SCR_ConfigureFaultWake, @function
_SCR_ConfigureFaultWake:
	.using 0
;	../SCR/main.c:567: SCR_SCU_PAGE = MOD_PAGE_1;
	mov	_SCR_SCU_PAGE,#0x01
;	../SCR/main.c:568: SCR_SCU_PMCON1 &= (uint8)(~WDT_DIS_MASK);
	anl	_SCR_SCU_PMCON1,#0xEF
;	../SCR/main.c:569: SCR_SCU_RSTCON &= (uint8)(~(ECCRSTEN_MASK | WDTRSTEN_MASK));
	anl	_SCR_SCU_RSTCON,#0xFC
;	../SCR/main.c:570: SCR_SCU_NMISR = SCR_FAULT_NMISR_MASK;
	mov	_SCR_SCU_NMISR,#0x03
;	../SCR/main.c:572: SCR_NMICON_WDT_MASK | SCR_NMICON_RAMECC_MASK | SCR_NMICON_WAKE_MASK);
	orl	_SCR_SCU_NMICON,#0x43
;	../SCR/main.c:574: WCANWKSEL_MASK | WDTWKSEL_MASK | ECCWKSEL_MASK);
	orl	_SCR_SCU_STDBYWKP,#0x1C
;	../SCR/main.c:576: SCR_WDT_REL = SCR_WDT_RELOAD_100MS;
	mov	_SCR_WDT_REL,#0xC3
;	../SCR/main.c:577: SCR_WDT_WINB = 0u;
	mov	_SCR_WDT_WINB,#0x00
;	../SCR/main.c:578: SCR_WDT_CON = (uint8)(WDTIN_MASK | WDTRS_MASK | WDTEN_MASK);
	mov	_SCR_WDT_CON,#0x07
.00310:
;	../SCR/main.c:579: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'SCR_WatchdogService'
;------------------------------------------------------------
;	../SCR/main.c:581: static void SCR_WatchdogService(void)
;	-----------------------------------------
;	 function SCR_WatchdogService
;	-----------------------------------------
	.section .text.code.SCR_WatchdogService,"ax" ;code for function SCR_WatchdogService
	.type   SCR_WatchdogService, @function
_SCR_WatchdogService:
	.using 0
;	../SCR/main.c:586: SCR_WDT_CON = (uint8)(SCR_WDT_CON | WDTRS_MASK);
	orl	_SCR_WDT_CON,#0x02
.00312:
;	../SCR/main.c:590: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'SCR_CheckFaultWake'
;------------------------------------------------------------
;nmiStatus                 Allocated with name '_SCR_CheckFaultWake_nmiStatus_65536_75'
;rstStatus                 Allocated with name '_SCR_CheckFaultWake_rstStatus_65536_75'
;faultStatus               Allocated with name '_SCR_CheckFaultWake_faultStatus_65536_75'
;wakeReason                Allocated with name '_SCR_CheckFaultWake_wakeReason_65536_75'
;------------------------------------------------------------
;	../SCR/main.c:592: static void SCR_CheckFaultWake(void)
;	-----------------------------------------
;	 function SCR_CheckFaultWake
;	-----------------------------------------
	.section .text.code.SCR_CheckFaultWake,"ax" ;code for function SCR_CheckFaultWake
	.type   SCR_CheckFaultWake, @function
_SCR_CheckFaultWake:
	.using 0
;	../SCR/main.c:596: uint8 faultStatus = 0u;
	mov	dptr,#_SCR_CheckFaultWake_faultStatus_65536_75
	clr	a
	movx	@dptr,a
;	../SCR/main.c:597: uint8 wakeReason = 0u;
	mov	dptr,#_SCR_CheckFaultWake_wakeReason_65536_75
	movx	@dptr,a
;	../SCR/main.c:599: SCR_SCU_PAGE = MOD_PAGE_1;
	mov	_SCR_SCU_PAGE,#0x01
;	../SCR/main.c:600: nmiStatus = SCR_SCU_NMISR;
	mov	dptr,#_SCR_CheckFaultWake_nmiStatus_65536_75
	mov	a,_SCR_SCU_NMISR
	movx	@dptr,a
;	../SCR/main.c:601: rstStatus = SCR_SCU_RSTST;
	mov	dptr,#_SCR_CheckFaultWake_rstStatus_65536_75
	mov	a,_SCR_SCU_RSTST
	movx	@dptr,a
;	../SCR/main.c:603: if (((nmiStatus & 0x02u) != 0u) || ((rstStatus & ECCRST_MASK) != 0u))
	mov	dptr,#_SCR_CheckFaultWake_nmiStatus_65536_75
	movx	a,@dptr
	jb	acc.1,.00314
.00336:
	mov	dptr,#_SCR_CheckFaultWake_rstStatus_65536_75
	movx	a,@dptr
	jnb	acc.0,.00315
.00337:
.00314:
;	../SCR/main.c:605: faultStatus |= SCR_FAULT_STATUS_ECC_DBE;
	mov	dptr,#_SCR_CheckFaultWake_faultStatus_65536_75
	mov	a,#0x01
	movx	@dptr,a
;	../SCR/main.c:606: wakeReason |= SCR_FAULT_REASON_ECC_DBE;
	mov	dptr,#_SCR_CheckFaultWake_wakeReason_65536_75
	mov	a,#0x20
	movx	@dptr,a
.00315:
;	../SCR/main.c:609: if (((nmiStatus & 0x01u) != 0u) || ((rstStatus & WDTRST_MASK) != 0u))
	mov	dptr,#_SCR_CheckFaultWake_nmiStatus_65536_75
	movx	a,@dptr
	jb	acc.0,.00317
.00338:
	mov	dptr,#_SCR_CheckFaultWake_rstStatus_65536_75
	movx	a,@dptr
	jnb	acc.1,.00318
.00339:
.00317:
;	../SCR/main.c:611: faultStatus |= SCR_FAULT_STATUS_WDT;
	mov	dptr,#_SCR_CheckFaultWake_faultStatus_65536_75
	movx	a,@dptr
	orl	acc,#0x02
	movx	@dptr,a
;	../SCR/main.c:612: wakeReason |= SCR_FAULT_REASON_WDT;
	mov	dptr,#_SCR_CheckFaultWake_wakeReason_65536_75
	movx	a,@dptr
	orl	acc,#0x40
	movx	@dptr,a
.00318:
;	../SCR/main.c:615: if (faultStatus != 0u)
	mov	dptr,#_SCR_CheckFaultWake_faultStatus_65536_75
	movx	a,@dptr
	mov	r7,a
	movx	a,@dptr
	jz	.00322
.00340:
;	../SCR/main.c:619: (uint8)(SCR_TimeLoadU8(SCR_TIME_OFFSET_SCR_FAULT_STATUS) | faultStatus));
	mov	dpl,#0x21
	push	ar7
	lcall	_SCR_TimeLoadU8
	mov	a,dpl
	pop	ar7
	mov	dptr,#_SCR_TimeStoreU8_PARM_2
	orl	a,r7
	movx	@dptr,a
	mov	dpl,#0x21
	lcall	_SCR_TimeStoreU8
;	../SCR/main.c:620: SCR_TimeStoreU8(SCR_TIME_OFFSET_SCR_NMI_STATUS, nmiStatus);
	mov	dptr,#_SCR_CheckFaultWake_nmiStatus_65536_75
	movx	a,@dptr
	mov	r7,a
	mov	dptr,#_SCR_TimeStoreU8_PARM_2
	movx	@dptr,a
	mov	dpl,#0x22
	push	ar7
	lcall	_SCR_TimeStoreU8
;	../SCR/main.c:621: SCR_TimeStoreU8(SCR_TIME_OFFSET_SCR_RST_STATUS, rstStatus);
	mov	dptr,#_SCR_CheckFaultWake_rstStatus_65536_75
	movx	a,@dptr
	mov	r6,a
	mov	dptr,#_SCR_TimeStoreU8_PARM_2
	movx	@dptr,a
	mov	dpl,#0x23
	push	ar6
	lcall	_SCR_TimeStoreU8
;	../SCR/main.c:622: SCR_RequestStandbyWake(wakeReason);
	mov	dptr,#_SCR_CheckFaultWake_wakeReason_65536_75
	movx	a,@dptr
	mov	dpl,a
	lcall	_SCR_RequestStandbyWake
	pop	ar6
	pop	ar7
;	../SCR/main.c:624: SCR_SCU_PAGE = MOD_PAGE_1;
	mov	_SCR_SCU_PAGE,#0x01
;	../SCR/main.c:625: SCR_SCU_NMISR = (uint8)(nmiStatus & SCR_FAULT_NMISR_MASK);
	mov	a,#0x03
	anl	a,r7
	mov	_SCR_SCU_NMISR,a
;	../SCR/main.c:626: SCR_SCU_RSTST = (uint8)(rstStatus & SCR_FAULT_RSTST_MASK);
	mov	a,#0x03
	anl	a,r6
	mov	_SCR_SCU_RSTST,a
.00322:
;	../SCR/main.c:628: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'WCAN_ConfigureWakeFrame'
;------------------------------------------------------------
;	../SCR/main.c:630: static void WCAN_ConfigureWakeFrame(void)
;	-----------------------------------------
;	 function WCAN_ConfigureWakeFrame
;	-----------------------------------------
	.section .text.code.WCAN_ConfigureWakeFrame,"ax" ;code for function WCAN_ConfigureWakeFrame
	.type   WCAN_ConfigureWakeFrame, @function
_WCAN_ConfigureWakeFrame:
	.using 0
;	../SCR/main.c:632: SCR_WCAN_PAGE = MOD_PAGE_2;
	mov	_SCR_WCAN_PAGE,#0x02
;	../SCR/main.c:633: SCR_WCAN_ID0_CTRL = 0x00u;
	mov	_SCR_WCAN_ID0_CTRL,#0x00
;	../SCR/main.c:634: SCR_WCAN_ID1_CTRL = 0x00u;
	mov	_SCR_WCAN_ID1_CTRL,#0x00
;	../SCR/main.c:635: SCR_WCAN_ID2_CTRL = WCAN_WAKE_ID2_VALUE;
	mov	_SCR_WCAN_ID2_CTRL,#0x40
;	../SCR/main.c:636: SCR_WCAN_ID3_CTRL = WCAN_WAKE_ID3_VALUE;
	mov	_SCR_WCAN_ID3_CTRL,#0x08
;	../SCR/main.c:637: SCR_WCAN_MASK_ID0_CTRL = 0xFFu;
	mov	_SCR_WCAN_MASK_ID0_CTRL,#0xFF
;	../SCR/main.c:638: SCR_WCAN_MASK_ID1_CTRL = 0xFFu;
	mov	_SCR_WCAN_MASK_ID1_CTRL,#0xFF
;	../SCR/main.c:639: SCR_WCAN_MASK_ID2_CTRL = WCAN_WAKE_ID2_MASK;
	mov	_SCR_WCAN_MASK_ID2_CTRL,#0xF3
;	../SCR/main.c:640: SCR_WCAN_MASK_ID3_CTRL = WCAN_WAKE_ID3_MASK;
	mov	_SCR_WCAN_MASK_ID3_CTRL,#0x7D
;	../SCR/main.c:642: SCR_WCAN_PAGE = MOD_PAGE_3;
	mov	_SCR_WCAN_PAGE,#0x03
;	../SCR/main.c:643: SCR_WCAN_DATA0_CTRL = 0xFFu;
	mov	_SCR_WCAN_DATA0_CTRL,#0xFF
;	../SCR/main.c:644: SCR_WCAN_DATA1_CTRL = 0xFFu;
	mov	_SCR_WCAN_DATA1_CTRL,#0xFF
;	../SCR/main.c:645: SCR_WCAN_DATA2_CTRL = 0xFFu;
	mov	_SCR_WCAN_DATA2_CTRL,#0xFF
;	../SCR/main.c:646: SCR_WCAN_DATA3_CTRL = 0xFFu;
	mov	_SCR_WCAN_DATA3_CTRL,#0xFF
;	../SCR/main.c:647: SCR_WCAN_DATA4_CTRL = 0xFFu;
	mov	_SCR_WCAN_DATA4_CTRL,#0xFF
;	../SCR/main.c:648: SCR_WCAN_DATA5_CTRL = 0xFFu;
	mov	_SCR_WCAN_DATA5_CTRL,#0xFF
;	../SCR/main.c:649: SCR_WCAN_DATA6_CTRL = 0xFFu;
	mov	_SCR_WCAN_DATA6_CTRL,#0xFF
;	../SCR/main.c:650: SCR_WCAN_DATA7_CTRL = 0xFFu;
	mov	_SCR_WCAN_DATA7_CTRL,#0xFF
.00341:
;	../SCR/main.c:651: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'WCAN_CheckWake'
;------------------------------------------------------------
;wcanStatus0               Allocated with name '_WCAN_CheckWake_wcanStatus0_65536_82'
;wcanStatus1               Allocated with name '_WCAN_CheckWake_wcanStatus1_65536_82'
;p00In                     Allocated with name '_WCAN_CheckWake_p00In_65536_82'
;wakeReason                Allocated with name '_WCAN_CheckWake_wakeReason_65536_82'
;------------------------------------------------------------
;	../SCR/main.c:653: static void WCAN_CheckWake(void)
;	-----------------------------------------
;	 function WCAN_CheckWake
;	-----------------------------------------
	.section .text.code.WCAN_CheckWake,"ax" ;code for function WCAN_CheckWake
	.type   WCAN_CheckWake, @function
_WCAN_CheckWake:
	.using 0
;	../SCR/main.c:658: uint8 wakeReason = 0u;
	mov	dptr,#_WCAN_CheckWake_wakeReason_65536_82
	clr	a
	movx	@dptr,a
;	../SCR/main.c:660: SCR_WCAN_PAGE = MOD_PAGE_1;
	mov	_SCR_WCAN_PAGE,#0x01
;	../SCR/main.c:661: wcanStatus0 = SCR_WCAN_INTESTAT0;
	mov	dptr,#_WCAN_CheckWake_wcanStatus0_65536_82
	mov	a,_SCR_WCAN_INTESTAT0
	movx	@dptr,a
;	../SCR/main.c:662: wcanStatus1 = SCR_WCAN_INTESTAT1;
	mov	dptr,#_WCAN_CheckWake_wcanStatus1_65536_82
	mov	a,_SCR_WCAN_INTESTAT1
	movx	@dptr,a
;	../SCR/main.c:664: SCR_IO_PAGE = MOD_PAGE_0;
	mov	_SCR_IO_PAGE,#0x00
;	../SCR/main.c:665: p00In = SCR_IO_P00_IN;
	mov	dptr,#_WCAN_CheckWake_p00In_65536_82
	mov	a,_SCR_IO_P00_IN
	movx	@dptr,a
;	../SCR/main.c:667: *G_WCAN_STAT0_ADDR = wcanStatus0;
	mov	dptr,#_WCAN_CheckWake_wcanStatus0_65536_82
	movx	a,@dptr
	mov	dptr,#0x1761
	movx	@dptr,a
;	../SCR/main.c:668: *G_WCAN_STAT1_ADDR = wcanStatus1;
	mov	dptr,#_WCAN_CheckWake_wcanStatus1_65536_82
	movx	a,@dptr
	mov	dptr,#0x1762
	movx	@dptr,a
;	../SCR/main.c:669: *G_P00_IN_ADDR = p00In;
	mov	dptr,#_WCAN_CheckWake_p00In_65536_82
	movx	a,@dptr
	mov	r7,a
	mov	dptr,#0x1763
	movx	@dptr,a
;	../SCR/main.c:671: SCR_SCU_PAGE = MOD_PAGE_1;
	mov	_SCR_SCU_PAGE,#0x01
;	../SCR/main.c:672: *G_STDBYWKP_ADDR = SCR_SCU_STDBYWKP;
	mov	dptr,#0x1767
	mov	a,_SCR_SCU_STDBYWKP
	movx	@dptr,a
;	../SCR/main.c:674: if((p00In & CANFD_RXD_PIN_MASK) == 0u)
	mov	a,r7
	jb	acc.5,.00344
.00384:
;	../SCR/main.c:676: (*G_RX_LOW_COUNT_ADDR)++;
	mov	dptr,#0x1764
	movx	a,@dptr
	mov	r7,a
	inc	r7
	mov	dptr,#0x1764
	mov	a,r7
	movx	@dptr,a
.00344:
;	../SCR/main.c:679: if((wcanStatus1 & WCAN_STATUS1_WUF_MASK) != 0u)
	mov	dptr,#_WCAN_CheckWake_wcanStatus1_65536_82
	movx	a,@dptr
	mov	r7,a
	jnb	acc.0,.00351
.00385:
;	../SCR/main.c:681: wakeReason = WCAN_WAKE_REASON_WUF;
	mov	dptr,#_WCAN_CheckWake_wakeReason_65536_82
	mov	a,#0x01
	movx	@dptr,a
;	../SCR/main.c:683: else if((WCAN_WAKE_ON_FD_FRAME != 0u) &&
	sjmp	.00355
.00351:
;	../SCR/main.c:684: ((wcanStatus1 & WCAN_STATUS1_FDF_MASK) != 0u) &&
	mov	a,r7
	jnb	acc.4,.00347
.00386:
;	../SCR/main.c:685: ((*G_FDF_BASELINE_ADDR & WCAN_STATUS1_FDF_MASK) == 0u))
	mov	dptr,#0x1768
	movx	a,@dptr
	mov	r7,a
	jb	acc.4,.00347
.00387:
;	../SCR/main.c:687: wakeReason = WCAN_WAKE_REASON_FDF;
	mov	dptr,#_WCAN_CheckWake_wakeReason_65536_82
	mov	a,#0x10
	movx	@dptr,a
;	../SCR/main.c:689: else if((WCAN_WAKE_ON_SYNC_FRAME != 0u) && ((wcanStatus1 & WCAN_STATUS1_SYNC_MASK) != 0u))
	sjmp	.00355
.00347:
	mov	dptr,#_WCAN_CheckWake_wcanStatus1_65536_82
	movx	a,@dptr
	jnb	acc.1,.00355
.00388:
;	../SCR/main.c:691: wakeReason = WCAN_WAKE_REASON_SYNC;
	mov	dptr,#_WCAN_CheckWake_wakeReason_65536_82
	mov	a,#0x02
	movx	@dptr,a
.00355:
;	../SCR/main.c:694: if(wakeReason != 0u)
	mov	dptr,#_WCAN_CheckWake_wakeReason_65536_82
	movx	a,@dptr
	mov	r7,a
	movx	a,@dptr
	jz	.00358
.00389:
;	../SCR/main.c:696: SCR_RequestStandbyWake(wakeReason);
	mov	dpl,r7
	lcall	_SCR_RequestStandbyWake
.00358:
;	../SCR/main.c:698: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'WCAN_Init'
;------------------------------------------------------------
;result                    Allocated with name '_WCAN_Init_result_65536_89'
;TempVar                   Allocated with name '_WCAN_Init_TempVar_65536_89'
;------------------------------------------------------------
;	../SCR/main.c:700: char WCAN_Init(void)
;	-----------------------------------------
;	 function WCAN_Init
;	-----------------------------------------
	.section .text.code.WCAN_Init,"ax" ;code for function WCAN_Init
	.type   WCAN_Init, @function
_WCAN_Init:
	.using 0
;	../SCR/main.c:702: char result = 0;
	mov	dptr,#_WCAN_Init_result_65536_89
	clr	a
	movx	@dptr,a
;	../SCR/main.c:703: unsigned char volatile TempVar = 0;
	mov	dptr,#_WCAN_Init_TempVar_65536_89
	movx	@dptr,a
;	../SCR/main.c:705: *G_BOOT_STAGE_ADDR = BOOT_STAGE_WCAN_INIT_ENTRY;
	mov	dptr,#0x1769
	mov	a,#0x21
	movx	@dptr,a
;	../SCR/main.c:713: SCR_IO_PAGE = 0x1;
	mov	_SCR_IO_PAGE,#0x01
;	../SCR/main.c:715: SCR_IO_P00_IOCR5 = ScrPortMode_inputPullUp; // WCANRXDD(P33.5 / SCR_P00.5)
	mov	_SCR_IO_P00_IOCR5,#0x10
;	../SCR/main.c:717: SCR_IO_PAGE = 0x2;
	mov	_SCR_IO_PAGE,#0x02
;	../SCR/main.c:718: SCR_IO_P00_PDISC = 0xDF ; // WCANRXDD(P33.5 / SCR_P00.5)
	mov	_SCR_IO_P00_PDISC,#0xDF
;	../SCR/main.c:719: *G_BOOT_STAGE_ADDR = BOOT_STAGE_WCAN_PIN_CFG;
	mov	dptr,#0x1769
	inc	a
	movx	@dptr,a
;	../SCR/main.c:721: SCR_SCU_PAGE = 0x1;
	mov	_SCR_SCU_PAGE,#0x01
;	../SCR/main.c:722: SCR_SCU_RSTCON = 0x0 ; // Disable WDT and ECC reset
	mov	_SCR_SCU_RSTCON,#0x00
;	../SCR/main.c:723: SCR_SCU_STDBYWKP = (uint8)(WCANWKSEL_MASK | WDTWKSEL_MASK | ECCWKSEL_MASK);
	mov	_SCR_SCU_STDBYWKP,#0x1C
;	../SCR/main.c:724: *G_BOOT_STAGE_ADDR = BOOT_STAGE_WCAN_WAKE_CFG;
	mov	dptr,#0x1769
	inc	a
	movx	@dptr,a
;	../SCR/main.c:742: SCR_SCU_PAGE = 1;
	mov	_SCR_SCU_PAGE,#0x01
;	../SCR/main.c:743: SCR_SCU_PMCON1 &= ~(1<< 3); // enable WCAN clock (bit_3), default: WCAN is disabled
	anl	_SCR_SCU_PMCON1,#0xF7
;	../SCR/main.c:744: *G_BOOT_STAGE_ADDR = BOOT_STAGE_WCAN_CLOCK_ENABLED;
	mov	dptr,#0x1769
	mov	a,#0x24
	movx	@dptr,a
;	../SCR/main.c:746: SCR_SCU_PAGE = 0x2;
	mov	_SCR_SCU_PAGE,#0x02
;	../SCR/main.c:747: TempVar = SCR_SCU_MODPISEL0 & MaskWCANRXDIS; // Mask WCAN bits
	mov	a,_SCR_SCU_MODPISEL0
	mov	dptr,#_WCAN_Init_TempVar_65536_89
	anl	a,#0x0F
	movx	@dptr,a
;	../SCR/main.c:748: SCR_SCU_MODPISEL0 = TempVar | (SetWCANRXDD<<4); // Enable CAN on P33.5
	movx	a,@dptr
	orl	a,#0x30
	mov	_SCR_SCU_MODPISEL0,a
;	../SCR/main.c:749: *G_BOOT_STAGE_ADDR = BOOT_STAGE_WCAN_PIN_SELECTED;
	mov	dptr,#0x1769
	mov	a,#0x25
	movx	@dptr,a
;	../SCR/main.c:751: SCR_WCAN_PAGE = 0x0 ;
	mov	_SCR_WCAN_PAGE,#0x00
;	../SCR/main.c:752: SCR_WCAN_CFG = WCAN_CFG_CCE | WCAN_CFG_WCAN_EN; // CCE=1, SELWK_EN=0, WCAN_EN=1 --> according to UM
	mov	_SCR_WCAN_CFG,#0x09
;	../SCR/main.c:753: SCR_WCAN_INTMRSLT = WCAN_INT_ENABLE_WUF_ONLY;
	mov	_SCR_WCAN_INTMRSLT,#0x04
;	../SCR/main.c:754: SCR_WCAN_FD_CTRL = 0x01 ; // Enable CAN FD tolerant mode
	mov	_SCR_WCAN_FD_CTRL,#0x01
;	../SCR/main.c:755: *G_BOOT_STAGE_ADDR = BOOT_STAGE_WCAN_CFG_WRITTEN;
	mov	dptr,#0x1769
	inc	a
	movx	@dptr,a
;	../SCR/main.c:760: SCR_WCAN_PAGE = 0x1;
	mov	_SCR_WCAN_PAGE,#0x01
;	../SCR/main.c:761: SCR_WCAN_FRMERRCNT = (1<<6); // Do not count CAN FD frames as wake-up frame errors
	mov	_SCR_WCAN_FRMERRCNT,#0x40
;	../SCR/main.c:762: SCR_WCAN_DLC_CTRL = WCAN_WAKE_DLC_VALUE ; // 8 bytes of wake data
	mov	_SCR_WCAN_DLC_CTRL,#0x08
;	../SCR/main.c:763: SCR_WCAN_BTL1_CTRL = 0x64 ; // Configure nominal Baud Rate of 500 kbit/s
	mov	_SCR_WCAN_BTL1_CTRL,#0x64
;	../SCR/main.c:764: SCR_WCAN_BTL2_CTRL = (1<<6) | (0x33<<0) ; // BRP=01(Divide by 2) and SP=0x33 represents ~80%SP
	mov	_SCR_WCAN_BTL2_CTRL,#0x73
;	../SCR/main.c:765: WCAN_ClearEvents();
	lcall	_WCAN_ClearEvents
;	../SCR/main.c:766: *G_BOOT_STAGE_ADDR = BOOT_STAGE_WCAN_BITTIMING;
	mov	dptr,#0x1769
	mov	a,#0x27
	movx	@dptr,a
;	../SCR/main.c:772: SCR_WCAN_PAGE = 0x0 ;
	mov	_SCR_WCAN_PAGE,#0x00
;	../SCR/main.c:773: SCR_WCAN_INTMRSLT = WCAN_INT_ENABLE_WUF_ONLY;
	mov	_SCR_WCAN_INTMRSLT,#0x04
;	../SCR/main.c:774: SCR_WCAN_CFG &= ~(WCAN_CFG_CCE | WCAN_CFG_SELWK_EN); // CCE=0, SELWK_EN=0, WCAN_EN remains enabled
	anl	_SCR_WCAN_CFG,#0xF3
;	../SCR/main.c:776: if(WCAN_WaitForSelectiveWakeAck(0u) == 0u)
	mov	dpl,#0x00
	lcall	_WCAN_WaitForSelectiveWakeAck
	mov	a,dpl
	jnz	.00391
.00404:
;	../SCR/main.c:778: result |= WCAN_INIT_SWACK_CLEAR_TIMEOUT;
	mov	dptr,#_WCAN_Init_result_65536_89
	mov	a,#0x01
	movx	@dptr,a
.00391:
;	../SCR/main.c:780: *G_BOOT_STAGE_ADDR = BOOT_STAGE_SWACK_CLEAR_DONE;
	mov	dptr,#0x1769
	mov	a,#0x28
	movx	@dptr,a
;	../SCR/main.c:782: WCAN_ConfigureWakeFrame();
	lcall	_WCAN_ConfigureWakeFrame
;	../SCR/main.c:783: *G_BOOT_STAGE_ADDR = BOOT_STAGE_WUF_CFG_DONE;
	mov	dptr,#0x1769
	mov	a,#0x29
	movx	@dptr,a
;	../SCR/main.c:785: WCAN_ClearEvents();
	lcall	_WCAN_ClearEvents
;	../SCR/main.c:787: SCR_WCAN_PAGE = MOD_PAGE_0;
	mov	_SCR_WCAN_PAGE,#0x00
;	../SCR/main.c:788: SCR_WCAN_CFG |= WCAN_CFG_SELWK_EN;
	orl	_SCR_WCAN_CFG,#0x04
;	../SCR/main.c:790: if(WCAN_WaitForSelectiveWakeAck(WCAN_STATUS0_SWACK_MASK) == 0u)
	mov	dpl,#0x01
	lcall	_WCAN_WaitForSelectiveWakeAck
	mov	a,dpl
	jnz	.00393
.00405:
;	../SCR/main.c:792: result |= WCAN_INIT_SWACK_SET_TIMEOUT;
	mov	dptr,#_WCAN_Init_result_65536_89
	movx	a,@dptr
	orl	acc,#0x02
	movx	@dptr,a
.00393:
;	../SCR/main.c:794: *G_BOOT_STAGE_ADDR = BOOT_STAGE_SWACK_SET_DONE;
	mov	dptr,#0x1769
	mov	a,#0x2A
	movx	@dptr,a
;	../SCR/main.c:796: WCAN_ClearEvents();
	lcall	_WCAN_ClearEvents
;	../SCR/main.c:797: SCR_WCAN_PAGE = MOD_PAGE_1;
	mov	_SCR_WCAN_PAGE,#0x01
;	../SCR/main.c:798: *G_FDF_BASELINE_ADDR = (SCR_WCAN_INTESTAT1 & WCAN_STATUS1_FDF_MASK);
	mov	a,_SCR_WCAN_INTESTAT1
	anl	a,#0x10
	mov	dptr,#0x1768
	movx	@dptr,a
;	../SCR/main.c:799: *G_WCAN_INIT_ADDR = (uint8)result;
	mov	dptr,#_WCAN_Init_result_65536_89
	movx	a,@dptr
	mov	r7,a
	mov	dptr,#0x1766
	movx	@dptr,a
;	../SCR/main.c:800: *G_BOOT_STAGE_ADDR = BOOT_STAGE_WCAN_INIT_DONE;
	mov	dptr,#0x1769
	mov	a,#0x2B
	movx	@dptr,a
;	../SCR/main.c:802: return result;
	mov	dpl,r7
.00394:
;	../SCR/main.c:803: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'main'
;------------------------------------------------------------
;__1310720213              Allocated with name '_main___1310720213_131072_95'
;div                       Allocated with name '_main_div_196608_96'
;------------------------------------------------------------
;	../SCR/main.c:805: void main(void)
;	-----------------------------------------
;	 function main
;	-----------------------------------------
	.section .text.code.main,"ax" ;code for function main
	.type   main, @function
_main:
	.using 0
;	../SCR/main.c:807: *G_COUNTER_ADDR = 0u;
	mov	dptr,#0x1760
	clr	a
	movx	@dptr,a
;	../SCR/main.c:808: *G_WCAN_STAT0_ADDR = 0u;
	mov	dptr,#0x1761
	movx	@dptr,a
;	../SCR/main.c:809: *G_WCAN_STAT1_ADDR = 0u;
	mov	dptr,#0x1762
	movx	@dptr,a
;	../SCR/main.c:810: *G_P00_IN_ADDR = 0u;
	mov	dptr,#0x1763
	movx	@dptr,a
;	../SCR/main.c:811: *G_RX_LOW_COUNT_ADDR = 0u;
	mov	dptr,#0x1764
	movx	@dptr,a
;	../SCR/main.c:812: *G_WAKE_REASON_ADDR = 0u;
	mov	dptr,#0x1765
	movx	@dptr,a
;	../SCR/main.c:813: *G_WCAN_INIT_ADDR = 0u;
	mov	dptr,#0x1766
	movx	@dptr,a
;	../SCR/main.c:814: *G_STDBYWKP_ADDR = 0u;
	mov	dptr,#0x1767
	movx	@dptr,a
;	../SCR/main.c:815: *G_FDF_BASELINE_ADDR = 0u;
	mov	dptr,#0x1768
	movx	@dptr,a
;	../SCR/main.c:816: *G_BOOT_STAGE_ADDR = BOOT_STAGE_MAIN_ENTRY;
	mov	dptr,#0x1769
	mov	a,#0x11
	movx	@dptr,a
;	../SCR/scr_common.h:63: SCR_UNLOCK_PROTECTED_BITS();          /* Open access to write protected bits  */
	mov	_SCR_PASSWD,#0x98
;	../SCR/scr_common.h:65: SCR_SET_SCU_PAGE(MOD_PAGE_1);
	mov	_SCR_SCU_PAGE,#0x01
;	../SCR/scr_common.h:70: SCR_SCU_CMCON = ((div & DIV_MASK) | OSCWAKE_MASK);
	mov	_SCR_SCU_CMCON,#0x10
;	../SCR/scr_common.h:72: SCR_LOCK_PROTECTED_BITS();            /* Close access to write protected bits */
	mov	_SCR_PASSWD,#0xA8
;	../SCR/main.c:818: *G_BOOT_STAGE_ADDR = BOOT_STAGE_CLOCK_SET;
	mov	dptr,#0x1769
	inc	a
	movx	@dptr,a
;	../SCR/main.c:819: SCR_TimeInit();
	lcall	_SCR_TimeInit
;	../SCR/main.c:820: WCAN_Init();
	lcall	_WCAN_Init
;	../SCR/main.c:821: SCR_ConfigureFaultWake();
	lcall	_SCR_ConfigureFaultWake
;	../SCR/main.c:823: while(1)
.00407:
;	../SCR/main.c:825: *G_BOOT_STAGE_ADDR = BOOT_STAGE_LOOP_RUNNING;
	mov	dptr,#0x1769
	mov	a,#0x40
	movx	@dptr,a
;	../SCR/main.c:826: SCR_TimeUpdate();
	lcall	_SCR_TimeUpdate
;	../SCR/main.c:827: *G_BOOT_STAGE_ADDR = BOOT_STAGE_LOOP_TIME_DONE;
	mov	dptr,#0x1769
	mov	a,#0x41
	movx	@dptr,a
;	../SCR/main.c:828: SCR_CheckFaultWake();
	lcall	_SCR_CheckFaultWake
;	../SCR/main.c:829: *G_BOOT_STAGE_ADDR = BOOT_STAGE_LOOP_FAULT_DONE;
	mov	dptr,#0x1769
	mov	a,#0x42
	movx	@dptr,a
;	../SCR/main.c:830: SCR_WatchdogService();
	lcall	_SCR_WatchdogService
;	../SCR/main.c:831: *G_BOOT_STAGE_ADDR = BOOT_STAGE_LOOP_WDT_DONE;
	mov	dptr,#0x1769
	mov	a,#0x43
	movx	@dptr,a
;	../SCR/main.c:832: WCAN_CheckWake();
	lcall	_WCAN_CheckWake
;	../SCR/main.c:833: *G_BOOT_STAGE_ADDR = BOOT_STAGE_LOOP_WCAN_DONE;
	mov	dptr,#0x1769
	mov	a,#0x44
	movx	@dptr,a
;	../SCR/main.c:834: (*G_COUNTER_ADDR)++;
	mov	dptr,#0x1760
	movx	a,@dptr
	mov	r7,a
	inc	r7
	mov	dptr,#0x1760
	mov	a,r7
	movx	@dptr,a
	sjmp	.00407
.00410:
;	../SCR/main.c:836: }
	ret
;--------------------------------------------------------
; xinit 
;--------------------------------------------------------
	.section .roxdata.init,"ax";xinit_name ;area
__xinit__SCR_TimeBaseActive:
	.byte	#0x00	; 0
