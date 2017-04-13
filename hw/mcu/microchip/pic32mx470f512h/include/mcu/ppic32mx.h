/*-------------------------------------------------------------------------
 * MPLAB C for PIC32MX processor defs
 *
 * WARNING:
 * This file is provided for backwards compatibility with old MPLAB C32
 * source code only. For new projects, use the corresponding macros from 
 * the /proc/<device>.h file instead of the macros from these files.
 *
 * ------------------------------------------------------------------------
 *
 * This software is developed by Microchip Technology Inc. and its
 * subsidiaries ("Microchip").
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are 
 * met:
 * 
 * 1.      Redistributions of source code must retain the above copyright
 *         notice, this list of conditions and the following disclaimer.
 * 2.      Redistributions in binary form must reproduce the above 
 *         copyright notice, this list of conditions and the following 
 *         disclaimer in the documentation and/or other materials provided 
 *         with the distribution.
 * 3.      Microchip's name may not be used to endorse or promote products
 *         derived from this software without specific prior written 
 *         permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY MICROCHIP "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR PURPOSE ARE DISCLAIMED. IN NO EVENT 
 * SHALL MICROCHIP BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING BUT NOT LIMITED TO
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWSOEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *-------------------------------------------------------------------------*/

#if !defined(_PPIC32MX_H)
#define _PPIC32MX_H

/*********************************************************************
 * INT Definitions
 *********************************************************************/
/* In future these defines will move to processor header file. */
#define _TIMER_23_VECTOR                         12
#define _TIMER_45_VECTOR                         20

/* From v1.03, all VECTOR symbols use "_x" convention. These         */
/* are provided for backward compatiblity.                           */
#define _SPI1_VECTOR                             _SPI_1_VECTOR
#define _SPI2_VECTOR                             _SPI_2_VECTOR
#define _UART1_VECTOR                            _UART_1_VECTOR
#define _UART2_VECTOR                            _UART_2_VECTOR
#define _I2C1_VECTOR                             _I2C_1_VECTOR
#define _I2C2_VECTOR                             _I2C_2_VECTOR
#define _DMA0_VECTOR                             _DMA_0_VECTOR
#define _DMA1_VECTOR                             _DMA_1_VECTOR
#define _DMA2_VECTOR                             _DMA_2_VECTOR
#define _DMA3_VECTOR                             _DMA_3_VECTOR
#define _USB1_VECTOR                             _USB_1_VECTOR



/*********************************************************************
 * INTCON bit defines
 *********************************************************************/
#define INTCON_FRZ_BIT          0x00004000L
#define INTCON_INT4EP_BIT       0x00000010L
#define INTCON_INT3EP_BIT       0x00000008L
#define INTCON_INT2EP_BIT       0x00000004L
#define INTCON_INT1EP_BIT       0x00000002L
#define INTCON_INT0EP_BIT       0x00000001L

#define INTCON_IPTMR_SHIFT      16
#define INTCON_TPC_SHIFT        8

#define INTCON_MASK             0xFFFF071F

/*********************************************************************
 * IF0 -IF2
 *********************************************************************/
#define IFS0_CT_BIT          0x00000001
#define IFS0_CS0_BIT         0x00000002
#define IFS0_CS1_BIT         0x00000004
#define IFS0_INT0_BIT        0x00000008
#define IFS0_T1_BIT          0x00000010
#define IFS0_IC1_BIT         0x00000020
#define IFS0_OC1_BIT         0x00000040
#define IFS0_INT1_BIT        0x00000080
#define IFS0_T2_BIT          0x00000100
#define IFS0_IC2_BIT         0x00000200
#define IFS0_OC2_BIT         0x00000400
#define IFS0_INT2_BIT        0x00000800
#define IFS0_T3_BIT          0x00001000
#define IFS0_IC3_BIT         0x00002000
#define IFS0_OC3_BIT         0x00004000
#define IFS0_INT3_BIT        0x00008000
#define IFS0_T4_BIT          0x00010000
#define IFS0_IC4_BIT         0x00020000
#define IFS0_OC4_BIT         0x00040000
#define IFS0_INT4_BIT        0x00080000
#define IFS0_T5_BIT          0x00100000
#define IFS0_IC5_BIT         0x00200000
#define IFS0_OC5_BIT         0x00400000
#define IFS0_CN_BIT          0x00800000
#define IFS0_SPI1E_BIT       0x01000000
#define IFS0_SPI1TX_BIT      0x02000000
#define IFS0_SPI1RX_BIT      0x04000000
#define IFS0_U1E_BIT         0x08000000
#define IFS0_U1RX_BIT        0x10000000
#define IFS0_U1TX_BIT        0x20000000
#define IFS0_I2C1BC_BIT      0x40000000
#define IFS0_I2C1S_BIT       0x80000000

#define IFS0_MASK            0xFFFFFFFF

#define IFS1_I2C1M_BIT       0x00000001
#define IFS1_AD1_BIT         0x00000002
#define IFS1_PMP_BIT         0x00000004
#define IFS1_CMP1_BIT        0x00000008
#define IFS1_CMP2_BIT        0x00000010
#define IFS1_SPI2E_BIT       0x00000020
#define IFS1_SPI2TX_BIT      0x00000040
#define IFS1_SPI2RX_BIT      0x00000080
#define IFS1_U2E_BIT         0x00000100
#define IFS1_U2RX_BIT        0x00000200
#define IFS1_U2TX_BIT        0x00000400
#define IFS1_I2C2BC_BIT      0x00000800
#define IFS1_I2C2S_BIT       0x00001000
#define IFS1_I2CSM_BIT       0x00002000
#define IFS1_FSCM_BIT        0x00004000
#define IFS1_FCE_BIT         0x00008000
#define IFS1_RTCC_BIT        0x00010000
#define IFS1_DMA0_BIT        0x00020000
#define IFS1_DMA1_BIT        0x00040000
#define IFS1_DMA2_BIT        0x00080000
#define IFS1_DMA3_BIT        0x00100000
#define IFS1_DMA4_BIT        0x00200000
#define IFS1_DMA5_BIT        0x00400000
#define IFS1_DMA6_BIT        0x00800000
#define IFS1_DMA7_BIT        0x01000000
#define IFS1_USB_BIT         0x02000000

#define IFS1_MASK            0x03FFFFFF


/*********************************************************************
 * IE0 -IE2
 *********************************************************************/
#define IEC0_CT_BIT          0x00000001
#define IEC0_CS0_BIT         0x00000002
#define IEC0_CS1_BIT         0x00000004
#define IEC0_INT0_BIT        0x00000008
#define IEC0_T1_BIT          0x00000010
#define IEC0_IC1_BIT         0x00000020
#define IEC0_OC1_BIT         0x00000040
#define IEC0_INT1_BIT        0x00000080
#define IEC0_T2_BIT          0x00000100
#define IEC0_IC2_BIT         0x00000200
#define IEC0_OC2_BIT         0x00000400
#define IEC0_INT2_BIT        0x00000800
#define IEC0_T3_BIT          0x00001000
#define IEC0_IC3_BIT         0x00002000
#define IEC0_OC3_BIT         0x00004000
#define IEC0_INT3_BIT        0x00008000
#define IEC0_T4_BIT          0x00010000
#define IEC0_IC4_BIT         0x00020000
#define IEC0_OC4_BIT         0x00040000
#define IEC0_INT4_BIT        0x00080000
#define IEC0_T5_BIT          0x00100000
#define IEC0_IC5_BIT         0x00200000
#define IEC0_OC5_BIT         0x00400000
#define IEC0_CN_BIT          0x00800000
#define IEC0_SPI1E_BIT       0x01000000
#define IEC0_SPI1TX_BIT      0x02000000
#define IEC0_SPI1RX_BIT      0x04000000
#define IEC0_U1E_BIT         0x08000000
#define IEC0_U1RX_BIT        0x10000000
#define IEC0_U1TX_BIT        0x20000000
#define IEC0_I2C1BC_BIT      0x40000000
#define IEC0_I2C1S_BIT       0x80000000

#define IEC0_MASK            0xFFFFFFFF

#define IEC1_I2C1M_BIT       0x00000001
#define IEC1_AD1_BIT         0x00000002
#define IEC1_PMP_BIT         0x00000004
#define IEC1_CMP1_BIT        0x00000008
#define IEC1_CMP2_BIT        0x00000010
#define IEC1_SPI2E_BIT       0x00000020
#define IEC1_SPI2TX_BIT      0x00000040
#define IEC1_SPI2RX_BIT      0x00000080
#define IEC1_U2E_BIT         0x00000100
#define IEC1_U2RX_BIT        0x00000200
#define IEC1_U2TX_BIT        0x00000400
#define IEC1_I2C2BC_BIT      0x00000800
#define IEC1_I2C2S_BIT       0x00001000
#define IEC1_I2CSM_BIT       0x00002000
#define IEC1_FSCM_BIT        0x00004000
#define IEC1_FCE_BIT         0x00008000
#define IEC1_RTCC_BIT        0x00010000
#define IEC1_DMA0_BIT        0x00020000
#define IEC1_DMA1_BIT        0x00040000
#define IEC1_DMA2_BIT        0x00080000
#define IEC1_DMA3_BIT        0x00100000
#define IEC1_DMA4_BIT        0x00200000
#define IEC1_DMA5_BIT        0x00400000
#define IEC1_DMA6_BIT        0x00800000
#define IEC1_DMA7_BIT        0x01000000
#define IEC1_USB_BIT         0x02000000

#define IEC1_MASK            0x03FFFFFF

/*********************************************************************
 * IPC0 -IPC15
 *********************************************************************/
#define IPC0_CTIS_SHIFT         0
#define IPC0_CTIP_SHIFT         2
#define IPC0_CS0IS_SHIFT        8
#define IPC0_CS0IP_SHIFT        10
#define IPC0_CS1IS_SHIFT        16
#define IPC0_CS1IP_SHIFT        18
#define IPC0_INT0IS_SHIFT       24
#define IPC0_INT0IP_SHIFT       26

#define IPC0_MASK               0x1F1F1F1F

#define IPC1_MASK               0x1F1F1F1F

#define IPC2_MASK               0x1F1F1F1F

#define IPC3_MASK               0x1F1F1F1F

#define IPC4_MASK               0x1F1F1F1F

#define IPC5_MASK               0x1F1F1F1F

#define IPC6_MASK               0x1F1F1F1F

#define IPC7_MASK               0x1F1F1F1F

#define IPC8_MASK               0x1F1F1F1F

#define IPC9_MASK               0x1F1F1F1F

#define IPC10_MASK              0x1F1F1F1F

#define IPC11_MASK              0x00001F1F



/*********************************************************************/
/* PMP (parallel master port) definitions */


/*********************************************************************/
/* UART definitions */
#define USTA_URXDA      0x00000001
#define	USTA_OERR       0x00000002
#define USTA_FERR	0x00000004
#define USTA_PERR	0x00000008
#define USTA_RIDLE	0x00000010
#define	USTA_ADDEN      0x00000020
#define	USTA_URXISEL0   0x00000040
#define	USTA_URXISEL1   0x00000080
#define USTA_TRMT	0x00000100
#define USTA_TXBF	0x00000200
#define	USTA_UTXEN      0x00000400
#define	USTA_UTXBRK     0x00000800
#define USTA_URXEN      0x00001000
#define	USTA_UTXINV     0x00002000
#define	USTA_UTXISEL0   0x00004000
#define USTA_UTXISEL1   0x00008000
#define	USTA_ADDR       0x00FF0000

#define USTA_URXISEL    0x000000C0
#define USTA_UTXISEL    0x0000C000

#define UMODE_STSEL     0x00000001
#define UMODE_PDSEL0    0x00000002
#define UMODE_PDSEL1    0x00000004
#define UMODE_BRGH      0x00000008
#define UMODE_RXINV     0x00000010
#define UMODE_ABAUD     0x00000020
#define UMODE_LPBACK    0x00000040
#define UMODE_WAKE      0x00000080
#define UMODE_UEN0      0x00000100
#define UMODE_UEN1      0x00000200
#define UMODE_ALTIO	0x00000400
#define UMODE_RTSMD     0x00000800
#define UMODE_IREN      0x00001000
#define UMODE_USIDL     0x00002000
#define UMODE_UFRZ      0x00004000
#define UMODE_UARTEN    0x00008000

#define UMODE_PDSEL     0x00000006
#define UMODE_UEN       0x00000300

/* SPI definitions */
#define		SPICON_MSTEN		0x00000020
#define		SPICON_CKP		0x00000040
#define		SPICON_SSEN		0x00000080
#define		SPICON_CKE		0x00000100
#define		SPICON_SMP		0x00000200
#define		SPICON_MODE16		0x00000400
#define		SPICON_MODE32		0x00000800
#define		SPICON_DISSDO		0x00001000
#define		SPICON_SIDL		0x00002000
#define		SPICON_FRZ		0x00004000
#define		SPICON_ON		0x00008000
#define		SPICON_SPIFE		0x00020000
#define		SPICON_FRMPOL		0x20000000
#define		SPICON_FRMSYNC		0x40000000
#define		SPICON_FRMEN		0x80000000

#define		SPISTAT_SPIRBF		0x00000001
#define		SPISTAT_SPITBE		0x00000008
#define		SPISTAT_SPIROV		0x00000040
#define		SPISTAT_BUSY		0x00000800

/* SPI1 definitions */
#ifdef _SPI1
#define					SPI1_CLK_PORT	IOPORT_F
#define					SPI1_CLK_BIT	BIT_6

#define					SPI1_SDO_PORT	IOPORT_F
#define					SPI1_SDO_BIT	BIT_8

#define					SPI1_SDI_PORT	IOPORT_F
#define					SPI1_SDI_BIT	BIT_7

#define					SPI1_SS_PORT	IOPORT_B
#define					SPI1_SS_BIT		BIT_2
#endif

/* SPI2 definitions */
#ifdef _SPI2
#define					SPI2_CLK_PORT	IOPORT_G
#define					SPI2_CLK_BIT	BIT_6

#define					SPI2_SDO_PORT	IOPORT_G
#define					SPI2_SDO_BIT	BIT_8

#define					SPI2_SDI_PORT	IOPORT_G
#define					SPI2_SDI_BIT	BIT_7

#define					SPI2_SS_PORT	IOPORT_G
#define					SPI2_SS_BIT		BIT_9

#endif

/* FLASH Controller definitions */
#define NVMCON_NVMOP           	0x0000000f
#define NVMCON_ERASE            0x00000040
#define NVMCON_WRERR            0x00002000
#define NVMCON_WREN             0x00004000
#define NVMCON_WR               0x00008000

#define NVMCON_NVMOP0           0x00000001
#define NVMCON_NVMOP1           0x00000002
#define NVMCON_NVMOP2           0x00000004
#define NVMCON_NVMOP3           0x00000008

#define NVMCON_PROGOP           0x0000000F

#define NVMCON_PROGOP0          0x00000001
#define NVMCON_PROGOP1          0x00000002
#define NVMCON_PROGOP2          0x00000004
#define NVMCON_PROGOP3          0x00000008


/* RTCC definitions */
#define		RTCCON_RTCOE		0x00000001
#define		RTCCON_HALFSEC		0x00000002
#define		RTCCON_RTCSYNC		0x00000004
#define		RTCCON_RTCWREN		0x00000008
#define		RTCCON_RTCCLKON		0x00000040
#define		RTCCON_RTSECSEL		0x00000080
#define		RTCCON_FRZ			0x00004000
#define		RTCCON_ON			0x00008000
#define		RTCCON_CAL			0x03ff0000


#define	RTCALRM_ARPT		0x000000ff
#define	RTCALRM_AMASK		0x00000f00
#define	RTCALRM_ALRMSYNC	0x00001000
#define	RTCALRM_PIV			0x00002000
#define	RTCALRM_CHIME		0x00004000
#define	RTCALRM_ALRMEN		0x00008000


#define	RTCTIME_SEC01		0x00000f00
#define	RTCTIME_SEC10		0x0000f000
#define	RTCTIME_MIN01		0x000f0000
#define	RTCTIME_MIN10		0x00f00000
#define	RTCTIME_HR01		0x0f000000
#define	RTCTIME_HR10		0xf0000000


#define	RTCDATE_WDAY01		0x0000000f
#define	RTCDATE_DAY01		0x00000f00
#define	RTCDATE_DAY10		0x0000f000
#define	RTCDATE_MONTH01		0x000f0000
#define	RTCDATE_MONTH10		0x00f00000
#define	RTCDATE_YEAR01		0x0f000000
#define	RTCDATE_YEAR10		0xf0000000

#define	ALRMTIME_SEC01		0x00000f00
#define	ALRMTIME_SEC10		0x0000f000
#define	ALRMTIME_MIN01		0x000f0000
#define	ALRMTIME_MIN10		0x00f00000
#define	ALRMTIME_HR01		0x0f000000
#define	ALRMTIME_HR10		0xf0000000


#define	ALRMDATE_WDAY01		0x0000000f
#define	ALRMDATE_DAY01		0x00000f00
#define	ALRMDATE_DAY10		0x0000f000
#define	ALRMDATE_MONTH01	0x000f0000
#define	ALRMDATE_MONTH10	0x00f00000


/* DMAC definitions */
#define	DMACON_SUSPEND		0x00001000
#define	DMACON_SIDL			0x00002000
#define	DMACON_FRZ			0x00004000
#define	DMACON_ON			0x00008000

#define	DMASTAT_DMACH		0x00000007
#define	DMASTAT_RDWR		0x00000008

#define	DCRCCON_CRCCH		0x00000007
#define	DCRCCON_CRCAPP		0x00000040
#define	DCRCCON_CRCEN		0x00000080
#define	DCRCCON_PLEN		0x00000f00

#define	DCHXCON_CHPRI		0x00000003
#define	DCHXCON_CHEDET		0x00000004
#define	DCHXCON_CHXM		0x00000008
#define	DCHXCON_CHAEN		0x00000010
#define	DCHXCON_CHCHN		0x00000020
#define	DCHXCON_CHAED		0x00000040
#define	DCHXCON_CHEN		0x00000080
#define	DCHXCON_CHCHNS		0x00000100

#define	DCHXECON_AIRQEN		0x00000008
#define	DCHXECON_SIRQEN		0x00000010
#define	DCHXECON_PATEN		0x00000020
#define	DCHXECON_CABORT		0x00000040
#define	DCHXECON_CFORCE		0x00000080
#define	DCHXECON_CHSIRQ		0x0000ff00
#define	DCHXECON_CHAIRQ		0x00ff0000

#define	DCHXINT_CHERIF		0x00000001
#define	DCHXINT_CHTAIF		0x00000002
#define	DCHXINT_CHCCIF		0x00000004
#define	DCHXINT_CHBCIF		0x00000008
#define	DCHXINT_CHDHIF		0x00000010
#define	DCHXINT_CHDDIF		0x00000020
#define	DCHXINT_CHSHIF		0x00000040
#define	DCHXINT_CHSDIF		0x00000080
#define	DCHXINT_CHERIE		0x00010000
#define	DCHXINT_CHTAIE		0x00020000
#define	DCHXINT_CHCCIE		0x00040000
#define	DCHXINT_CHBCIE		0x00080000
#define	DCHXINT_CHDHIE		0x00100000
#define	DCHXINT_CHDDIE		0x00200000
#define	DCHXINT_CHSHIE		0x00400000
#define	DCHXINT_CHSDIE		0x00800000



/* USB-OTG Definitions */
#if defined(_USB)

#define UOTGIR_AVBUS_VLD   0x00000001
#define UOTGIR_BSESS_END   0x00000004
#define UOTGIR_SESS_VLD    0x00000008
#define UOTGIR_ACTIVITY    0x00000010
#define UOTGIR_LSTATE      0x00000020
#define UOTGIR_1MSEC       0x00000040
#define UOTGIR_ID          0x00000080

#define UOTGIE_AVBUS_VLD   0x00000001
#define UOTGIE_BSESS_END   0x00000004
#define UOTGIE_SESS_VLD    0x00000008
#define UOTGIE_ACTIVITY    0x00000010
#define UOTGIE_LSTATE      0x00000020
#define UOTGIE_T1MSEC      0x00000040
#define UOTGIE_ID          0x00000080

#define UOTGSTAT_AVBUS_VLD   0x00000001
#define UOTGSTAT_BSESS_END   0x00000004
#define UOTGSTAT_SESS_VLD    0x00000008
#define UOTGSTAT_LSTATE      0x00000020
#define UOTGSTAT_ID          0x00000080

#define UOTGCTRL_VBUS_DSCHG  0x00000001
#define UOTGCTRL_VBUS_CHG    0x00000002
#define UOTGCTRL_OTG_EN      0x00000004
#define UOTGCTRL_VBUS_ON     0x00000008
#define UOTGCTRL_DM_LOW      0x00000010
#define UOTGCTRL_DP_LOW      0x00000020
#define UOTGCTRL_DM_HIGH     0x00000040
#define UOTGCTRL_DP_HIGH     0x00000080

#define UIR_USB_RST          0x00000001
#define UIR_UERR             0x00000002
#define UIR_SOF_TOK          0x00000004
#define UIR_TOK_DNE          0x00000008
#define UIR_UIDLE            0x00000010
#define UIR_RESUME           0x00000020
#define UIR_ATTACH           0x00000040
#define UIR_STALL            0x00000080

#define UIE_USB_RST          0x00000001
#define UIE_UERR             0x00000002
#define UIE_SOF_TOK          0x00000004
#define UIE_TOK_DNE          0x00000008
#define UIE_UIDLE            0x00000010
#define UIE_RESUME           0x00000020
#define UIE_ATTACH           0x00000040
#define UIE_STALL            0x00000080

#define UEIR_PID_ERR         0x00000001
#define UEIR_CRC5            0x00000002
#define UEIR_HOST_EOF        0x00000002
#define UEIR_CRC16           0x00000004
#define UEIR_DFN8            0x00000008
#define UEIR_BTO_ERR         0x00000010
#define UEIR_DMA_ERR         0x00000020
#define UEIR_BTS_ERR         0x00000080

#define UEIE_PID_ERR         0x00000001
#define UEIE_CRC5            0x00000002
#define UEIE_HOST_EOF        0x00000002
#define UEIE_CRC16           0x00000004
#define UEIE_DFN8            0x00000008
#define UEIE_BTO_ERR         0x00000010
#define UEIE_DMA_ERR         0x00000020
#define UEIE_BTS_ERR         0x00000080

#define USTAT_ODD            0x00000004
#define USTAT_TX             0x00000008
#define USTAT_ENDPT_MASK     0x000000F0

#define UCTRL_USB_EN         0x00000001
#define UCTRL_HOST_SOF_EN    0x00000001
#define UCTRL_ODD_RST        0x00000002
#define UCTRL_RESUME         0x00000004
#define UCTRL_HOST_EN        0x00000008
#define UCTRL_HOST_RESET     0x00000010
#define UCTRL_TXD_SUSPND     0x00000020
#define UCTRL_HOST_TOK_BUSY  0x00000020
#define UCTRL_SE0            0x00000040
#define UCTRL_HOST_JSTATE    0x00000080

#define UADDR_HOST_LS_EN     0x00000080

#define UBDTP2_BDT_BA_15_9_MASK  0x000000FE

#define UTOK_HOST_TOK_PID_MASK 0x000000F0
#define UTOK_HOST_TOK_EP_MASK  0x0000000F

#define USOF_THRESHOLD_VALUE_8BYTE_PKT   0x12;
#define USOF_THRESHOLD_VALUE_16BYTE_PKT  0x1A;
#define USOF_THRESHOLD_VALUE_32BYTE_PKT  0x2A;
#define USOF_THRESHOLD_VALUE_64BYTE_PKT  0x4A;
#define USOF_HOST_CNT_MASK   0x000000FF

#define UBDTP2_BDT_BA_23_16_MASK  0x000000FF

#define UBDTP2_BDT_BA_31_24_MASK  0x000000FF

#define UCNFG1_FSEN          0x00000004
#define UCNFG1_SUSPND        0x00000008
#define UCNFG1_PSIDLE        0x00000010
#define UCNFG1_PFRZ          0x00000020
#define UCNFG1_UOEMON        0x00000040
#define UCNFG1_UTEYE         0x00000080

#define UEP0_HSHK            0x00000001
#define UEP0_STALL           0x00000002
#define UEP0_EP_TX_EN        0x00000004
#define UEP0_EP_RX_EN        0x00000008
#define UEP0_EP_CTL_DIS      0x00000010
#define UEP0_RETRY_DIS       0x00000040
#define UEP0_HOST_WOHUB      0x00000080

#define UEPN_HSHK            0x00000001
#define UEPN_STALL           0x00000002
#define UEPN_EP_TX_EN        0x00000004
#define UEPN_EP_RX_EN        0x00000008
#define UEPN_EP_CTL_DIS      0x00000010

#endif		/* _USB */

/****************************************************************
 * ADC10 Definitions
*****************************************************************/
#define ADC_DONE  0x00
#define ADC_SAMP  0x01
#define ADC_ASAM  0x02
#define ADC_CLRASAM  0x08
#define ADC_SSRC0 0x20
#define ADC_SSRC1 0x40
#define ADC_SSRC2 0x80
#define ADC_FORM0 0x0100
#define ADC_FORM1 0x0200
#define ADC_FORM2 0x0400
#define ADC_SIDL  0x2000
#define ADC_FRZ   0x4000
#define ADC_ON    0x8000

#define ADC_ALTS   0x01
#define ADC_BUFM   0x02
#define ADC_SMPIO  0x04
#define ADC_SMPI1  0x08
#define ADC_SMPI2  0x10
#define ADC_SMPI3  0x20
#define ADC_BUFS   0x80

#define ADC_CSCNA  0x0100
#define ADC_OFFCAL 0x1000
#define ADC_VCFG0  0x2000
#define ADC_VCFG1  0x4000
#define ADC_VCFG2  0x8000

#define ADC_ADCS0  0x01
#define ADC_ADCS1  0x02
#define ADC_ADCS2  0x04
#define ADC_ADCS3  0x08
#define ADC_ADCS4  0x10
#define ADC_ADCS5  0x20
#define ADC_ADCS6  0x40
#define ADC_ADCS7  0x80

#define ADC_SAMC0  0x0100
#define ADC_SAMC1  0x0200
#define ADC_SAMC2  0x0400
#define ADC_SAMC3  0x0800
#define ADC_SAMC4  0x1000
#define ADC_ADRC   0x8000

#define ADC_CH0SA0 0x00010000
#define ADC_CH0SA1 0x00020000
#define ADC_CH0SA2 0x00040000
#define ADC_CH0SA3 0x00080000
#define ADC_CH0NA  0x00100000
#define ADC_CH0SB0 0x01000000
#define ADC_CH0SB1 0x02000000
#define ADC_CH0SB2 0x04000000
#define ADC_CH0SB3 0x08000000
#define ADC_CH0NB  0x80000000

#define ADC_CSSL0  0x01
#define ADC_CSSL1  0x02
#define ADC_CSSL2  0x04
#define ADC_CSSL3  0x08
#define ADC_CSSL4  0x10
#define ADC_CSSL5  0x20
#define ADC_CSSL6  0x40
#define ADC_CSSL7  0x80
#define ADC_CSSL8  0x0100
#define ADC_CSSL9  0x0200
#define ADC_CSSL10 0x0400
#define ADC_CSSL11 0x0800
#define ADC_CSSL12 0x1000
#define ADC_CSSL13 0x2000
#define ADC_CSSL14 0x4000
#define ADC_CSSL15 0x8000

#define ADC_PCFG0  0x01
#define ADC_PCFG1  0x02
#define ADC_PCFG2  0x04
#define ADC_PCFG3  0x08
#define ADC_PCFG4  0x10
#define ADC_PCFG5  0x20
#define ADC_PCFG6  0x40
#define ADC_PCFG7  0x80
#define ADC_PCFG8  0x0100
#define ADC_PCFG9  0x0200
#define ADC_PCFG10 0x0400
#define ADC_PCFG11 0x0800
#define ADC_PCFG12 0x1000
#define ADC_PCFG13 0x2000
#define ADC_PCFG14 0x4000
#define ADC_PCFG15 0x8000

/* AD1CON1 */
#define _DONE AD1CON1bits.DONE
#define _SAMP AD1CON1bits.SAMP
#define _ASAM AD1CON1bits.ASAM
#define _SSRC AD1CON1bits.SSRC
#define _FORM0 AD1CON1bits.FORM0
#define _FORM1 AD1CON1bits.FORM1
#define _ADSIDL AD1CON1bits.ADSIDL
#define _ADON AD1CON1bits.ADON
#define _SSRC0 AD1CON1bits.SSRC0
#define _SSRC1 AD1CON1bits.SSRC1
#define _SSRC2 AD1CON1bits.SSRC2
#define _FORM AD1CON1bits.FORM

/* AD1CON2 */
#define _ALTS AD1CON2bits.ALTS
#define _BUFM AD1CON2bits.BUFM
#define _SMPI AD1CON2bits.SMPI
#define _BUFS AD1CON2bits.BUFS
#define _CSCNA AD1CON2bits.CSCNA
#define _VCFG AD1CON2bits.VCFG
#define _SMPI0 AD1CON2bits.SMPI0
#define _SMPI1 AD1CON2bits.SMPI1
#define _SMPI2 AD1CON2bits.SMPI2
#define _SMPI3 AD1CON2bits.SMPI3
#define _VCFG0 AD1CON2bits.VCFG0
#define _VCFG1 AD1CON2bits.VCFG1
#define _VCFG2 AD1CON2bits.VCFG2

/* AD1CON3 */
#define _ADCS AD1CON3bits.ADCS
#define _SAMC AD1CON3bits.SAMC
#define _ADRC AD1CON3bits.ADRC
#define _ADCS0 AD1CON3bits.ADCS0
#define _ADCS1 AD1CON3bits.ADCS1
#define _ADCS2 AD1CON3bits.ADCS2
#define _ADCS3 AD1CON3bits.ADCS3
#define _ADCS4 AD1CON3bits.ADCS4
#define _ADCS5 AD1CON3bits.ADCS5
#define _ADCS6 AD1CON3bits.ADCS6
#define _ADCS7 AD1CON3bits.ADCS7
#define _SAMC0 AD1CON3bits.SAMC0
#define _SAMC1 AD1CON3bits.SAMC1
#define _SAMC2 AD1CON3bits.SAMC2
#define _SAMC3 AD1CON3bits.SAMC3
#define _SAMC4 AD1CON3bits.SAMC4

/* AD1CHS */
#define _CH0SA AD1CHSbits.CH0SA
#define _CH0NA AD1CHSbits.CH0NA
#define _CH0SB AD1CHSbits.CH0SB
#define _CH0NB AD1CHSbits.CH0NB
#define _CH0SA0 AD1CHSbits.CH0SA0
#define _CH0SA1 AD1CHSbits.CH0SA1
#define _CH0SA2 AD1CHSbits.CH0SA2
#define _CH0SA3 AD1CHSbits.CH0SA3
#define _CH0SB0 AD1CHSbits.CH0SB0
#define _CH0SB1 AD1CHSbits.CH0SB1
#define _CH0SB2 AD1CHSbits.CH0SB2
#define _CH0SB3 AD1CHSbits.CH0SB3

/* AD1PCFG */
#define _PCFG0 AD1PCFGbits.PCFG0
#define _PCFG1 AD1PCFGbits.PCFG1
#define _PCFG2 AD1PCFGbits.PCFG2
#define _PCFG3 AD1PCFGbits.PCFG3
#define _PCFG4 AD1PCFGbits.PCFG4
#define _PCFG5 AD1PCFGbits.PCFG5
#define _PCFG6 AD1PCFGbits.PCFG6
#define _PCFG7 AD1PCFGbits.PCFG7
#define _PCFG8 AD1PCFGbits.PCFG8
#define _PCFG9 AD1PCFGbits.PCFG9
#define _PCFG10 AD1PCFGbits.PCFG10
#define _PCFG11 AD1PCFGbits.PCFG11
#define _PCFG12 AD1PCFGbits.PCFG12
#define _PCFG13 AD1PCFGbits.PCFG13
#define _PCFG14 AD1PCFGbits.PCFG14
#define _PCFG15 AD1PCFGbits.PCFG15

/* AD1CSSL */
#define _CSSL0 AD1CSSLbits.CSSL0
#define _CSSL1 AD1CSSLbits.CSSL1
#define _CSSL2 AD1CSSLbits.CSSL2
#define _CSSL3 AD1CSSLbits.CSSL3
#define _CSSL4 AD1CSSLbits.CSSL4
#define _CSSL5 AD1CSSLbits.CSSL5
#define _CSSL6 AD1CSSLbits.CSSL6
#define _CSSL7 AD1CSSLbits.CSSL7
#define _CSSL8 AD1CSSLbits.CSSL8
#define _CSSL9 AD1CSSLbits.CSSL9
#define _CSSL10 AD1CSSLbits.CSSL10
#define _CSSL11 AD1CSSLbits.CSSL11
#define _CSSL12 AD1CSSLbits.CSSL12
#define _CSSL13 AD1CSSLbits.CSSL13
#define _CSSL14 AD1CSSLbits.CSSL14
#define _CSSL15 AD1CSSLbits.CSSL15

/****************************************************************
 * CVREF Definitions
*****************************************************************/
#define CVRCON_CVR0      0x0001
#define CVRCON_CVR1      0x0002
#define CVRCON_CVR2      0x0004
#define CVRCON_CVR3      0x0008
#define CVRCON_CVRSS     0x0010
#define CVRCON_CVRR      0x0020
#define CVRCON_CVROE     0x0040
#define CVRCON_ON        0x8000

/****************************************************************
 * CMP Definitions
*****************************************************************/
/* common CMxCON defines */
#define CMCON_CCH0      0x0001
#define CMCON_CCH1      0x0002
#define CMCON_CREF      0x0010
#define CMCON_EVPOL0    0x0040
#define CMCON_EVPOL1    0x0080
#define CMCON_COUT      0x0100
#define CMCON_CPOL      0x2000
#define CMCON_COE       0x4000
#define CMCON_FRZ       0x4000

/* CMSTAT defines */
#define CMCON_ON        0x8000
#define CMCON_SIDL      0x2000

/****************************************************************
 * OCMP Definitions
*****************************************************************/
#define OCCON_OCM0      0x0001
#define OCCON_OCM1      0x0002
#define OCCON_OCM2      0x0004
#define OCCON_OCTSEL    0x0008
#define OCCON_OCFLT     0x0010
#define OCCON_OCC32     0x0020
#define OCCON_SIDL      0x2000
#define OCCON_FRZ       0x4000
#define OCCON_ON        0x8000


/* WDTCON definitions */
#define WDT_CLR 	0x01
#define WDT_WEN		0x02	/* may have to change from enable to disable */
#define WDT_WDTPST0 	0x04
#define WDT_WDTPST1 	0x08
#define WDT_WDTPST2 	0x10
#define WDT_WDTPST3 	0x20
#define WDT_WDTPST4 	0x40
#define WDT_FRZ 	0x4000
#define WDT_ON 		0x8000


/* OSCCON definitions */
#define OSC_OSWEN 		0x01
#define OSC_SOCEN		0x02
#define OSC_URFCEN 		0x04
#define OSC_CF 			0x08
#define OSC_SLPEN	 	0x10
#define OSC_LOCK 		0x20
#define OSC_ULOCK 		0x40
#define OSC_CLKLOCK 	0x80

#define OSC_NOSC0	 	0x0100
#define OSC_NOSC1	 	0x0200
#define OSC_NOSC2 		0x0400
#define OSC_COSC0 		0x1000
#define OSC_COSC1 		0x2000
#define OSC_COSC2 		0x4000

#define OSC_PLLMULT0 		0x010000
#define OSC_PLLMULT1 		0x020000
#define OSC_PLLMULT2 		0x040000
#define OSC_PBDIV0 			0x080000
#define OSC_PBDIV1 			0x100000

#define OSC_FRCDIV0			0x01000000
#define OSC_FRCDIV1			0x02000000
#define OSC_FRCDIV2			0x04000000
#define OSC_PLLODIV0		0x08000000
#define OSC_PLLODIV1		0x10000000
#define OSC_PLLODIV2		0x20000000


/* OSCTUN definitions */
#define OSC_TUN0 		0x01
#define OSC_TUN1 		0x02
#define OSC_TUN2 		0x04
#define OSC_TUN3 		0x08
#define OSC_TUN4 		0x10


/* TRISA */
#define _TRISA0 TRISAbits.TRISA0
#define _TRISA1 TRISAbits.TRISA1
#define _TRISA2 TRISAbits.TRISA2
#define _TRISA3 TRISAbits.TRISA3
#define _TRISA4 TRISAbits.TRISA4
#define _TRISA5 TRISAbits.TRISA5
#define _TRISA6 TRISAbits.TRISA6
#define _TRISA7 TRISAbits.TRISA7
#define _TRISA9 TRISAbits.TRISA9
#define _TRISA10 TRISAbits.TRISA10
#define _TRISA14 TRISAbits.TRISA14
#define _TRISA15 TRISAbits.TRISA15

/* PORTA */
#define _RA0 PORTAbits.RA0
#define _RA1 PORTAbits.RA1
#define _RA2 PORTAbits.RA2
#define _RA3 PORTAbits.RA3
#define _RA4 PORTAbits.RA4
#define _RA5 PORTAbits.RA5
#define _RA6 PORTAbits.RA6
#define _RA7 PORTAbits.RA7
#define _RA9 PORTAbits.RA9
#define _RA10 PORTAbits.RA10
#define _RA14 PORTAbits.RA14
#define _RA15 PORTAbits.RA15

/* LATA */
#define _LATA0 LATAbits.LATA0
#define _LATA1 LATAbits.LATA1
#define _LATA2 LATAbits.LATA2
#define _LATA3 LATAbits.LATA3
#define _LATA4 LATAbits.LATA4
#define _LATA5 LATAbits.LATA5
#define _LATA6 LATAbits.LATA6
#define _LATA7 LATAbits.LATA7
#define _LATA9 LATAbits.LATA9
#define _LATA10 LATAbits.LATA10
#define _LATA14 LATAbits.LATA14
#define _LATA15 LATAbits.LATA15

/* TRISB */
#define _TRISB0 TRISBbits.TRISB0
#define _TRISB1 TRISBbits.TRISB1
#define _TRISB2 TRISBbits.TRISB2
#define _TRISB3 TRISBbits.TRISB3
#define _TRISB4 TRISBbits.TRISB4
#define _TRISB5 TRISBbits.TRISB5
#define _TRISB6 TRISBbits.TRISB6
#define _TRISB7 TRISBbits.TRISB7
#define _TRISB8 TRISBbits.TRISB8
#define _TRISB9 TRISBbits.TRISB9
#define _TRISB10 TRISBbits.TRISB10
#define _TRISB11 TRISBbits.TRISB11
#define _TRISB12 TRISBbits.TRISB12
#define _TRISB13 TRISBbits.TRISB13
#define _TRISB14 TRISBbits.TRISB14
#define _TRISB15 TRISBbits.TRISB15

/* PORTB */
#define _RB0 PORTBbits.RB0
#define _RB1 PORTBbits.RB1
#define _RB2 PORTBbits.RB2
#define _RB3 PORTBbits.RB3
#define _RB4 PORTBbits.RB4
#define _RB5 PORTBbits.RB5
#define _RB6 PORTBbits.RB6
#define _RB7 PORTBbits.RB7
#define _RB8 PORTBbits.RB8
#define _RB9 PORTBbits.RB9
#define _RB10 PORTBbits.RB10
#define _RB11 PORTBbits.RB11
#define _RB12 PORTBbits.RB12
#define _RB13 PORTBbits.RB13
#define _RB14 PORTBbits.RB14
#define _RB15 PORTBbits.RB15

/* LATB */
#define _LATB0 LATBbits.LATB0
#define _LATB1 LATBbits.LATB1
#define _LATB2 LATBbits.LATB2
#define _LATB3 LATBbits.LATB3
#define _LATB4 LATBbits.LATB4
#define _LATB5 LATBbits.LATB5
#define _LATB6 LATBbits.LATB6
#define _LATB7 LATBbits.LATB7
#define _LATB8 LATBbits.LATB8
#define _LATB9 LATBbits.LATB9
#define _LATB10 LATBbits.LATB10
#define _LATB11 LATBbits.LATB11
#define _LATB12 LATBbits.LATB12
#define _LATB13 LATBbits.LATB13
#define _LATB14 LATBbits.LATB14
#define _LATB15 LATBbits.LATB15

/* TRISC */
#define _TRISC1 TRISCbits.TRISC1
#define _TRISC2 TRISCbits.TRISC2
#define _TRISC3 TRISCbits.TRISC3
#define _TRISC4 TRISCbits.TRISC4
#define _TRISC12 TRISCbits.TRISC12
#define _TRISC13 TRISCbits.TRISC13
#define _TRISC14 TRISCbits.TRISC14
#define _TRISC15 TRISCbits.TRISC15

/* PORTC */
#define _RC1 PORTCbits.RC1
#define _RC2 PORTCbits.RC2
#define _RC3 PORTCbits.RC3
#define _RC4 PORTCbits.RC4
#define _RC12 PORTCbits.RC12
#define _RC13 PORTCbits.RC13
#define _RC14 PORTCbits.RC14
#define _RC15 PORTCbits.RC15

/* LATC */
#define _LATC1 LATCbits.LATC1
#define _LATC2 LATCbits.LATC2
#define _LATC3 LATCbits.LATC3
#define _LATC4 LATCbits.LATC4
#define _LATC12 LATCbits.LATC12
#define _LATC13 LATCbits.LATC13
#define _LATC14 LATCbits.LATC14
#define _LATC15 LATCbits.LATC15

/* TRISD */
#define _TRISD0 TRISDbits.TRISD0
#define _TRISD1 TRISDbits.TRISD1
#define _TRISD2 TRISDbits.TRISD2
#define _TRISD3 TRISDbits.TRISD3
#define _TRISD4 TRISDbits.TRISD4
#define _TRISD5 TRISDbits.TRISD5
#define _TRISD6 TRISDbits.TRISD6
#define _TRISD7 TRISDbits.TRISD7
#define _TRISD8 TRISDbits.TRISD8
#define _TRISD9 TRISDbits.TRISD9
#define _TRISD10 TRISDbits.TRISD10
#define _TRISD11 TRISDbits.TRISD11
#define _TRISD12 TRISDbits.TRISD12
#define _TRISD13 TRISDbits.TRISD13
#define _TRISD14 TRISDbits.TRISD14
#define _TRISD15 TRISDbits.TRISD15

/* PORTD */
#define _RD0 PORTDbits.RD0
#define _RD1 PORTDbits.RD1
#define _RD2 PORTDbits.RD2
#define _RD3 PORTDbits.RD3
#define _RD4 PORTDbits.RD4
#define _RD5 PORTDbits.RD5
#define _RD6 PORTDbits.RD6
#define _RD7 PORTDbits.RD7
#define _RD8 PORTDbits.RD8
#define _RD9 PORTDbits.RD9
#define _RD10 PORTDbits.RD10
#define _RD11 PORTDbits.RD11
#define _RD12 PORTDbits.RD12
#define _RD13 PORTDbits.RD13
#define _RD14 PORTDbits.RD14
#define _RD15 PORTDbits.RD15

/* LATD */
#define _LATD0 LATDbits.LATD0
#define _LATD1 LATDbits.LATD1
#define _LATD2 LATDbits.LATD2
#define _LATD3 LATDbits.LATD3
#define _LATD4 LATDbits.LATD4
#define _LATD5 LATDbits.LATD5
#define _LATD6 LATDbits.LATD6
#define _LATD7 LATDbits.LATD7
#define _LATD8 LATDbits.LATD8
#define _LATD9 LATDbits.LATD9
#define _LATD10 LATDbits.LATD10
#define _LATD11 LATDbits.LATD11
#define _LATD12 LATDbits.LATD12
#define _LATD13 LATDbits.LATD13
#define _LATD14 LATDbits.LATD14
#define _LATD15 LATDbits.LATD15

/* TRISE */
#define _TRISE0 TRISEbits.TRISE0
#define _TRISE1 TRISEbits.TRISE1
#define _TRISE2 TRISEbits.TRISE2
#define _TRISE3 TRISEbits.TRISE3
#define _TRISE4 TRISEbits.TRISE4
#define _TRISE5 TRISEbits.TRISE5
#define _TRISE6 TRISEbits.TRISE6
#define _TRISE7 TRISEbits.TRISE7
#define _TRISE8 TRISEbits.TRISE8
#define _TRISE9 TRISEbits.TRISE9

/* PORTE */
#define _RE0 PORTEbits.RE0
#define _RE1 PORTEbits.RE1
#define _RE2 PORTEbits.RE2
#define _RE3 PORTEbits.RE3
#define _RE4 PORTEbits.RE4
#define _RE5 PORTEbits.RE5
#define _RE6 PORTEbits.RE6
#define _RE7 PORTEbits.RE7
#define _RE8 PORTEbits.RE8
#define _RE9 PORTEbits.RE9

/* LATE */
#define _LATE0 LATEbits.LATE0
#define _LATE1 LATEbits.LATE1
#define _LATE2 LATEbits.LATE2
#define _LATE3 LATEbits.LATE3
#define _LATE4 LATEbits.LATE4
#define _LATE5 LATEbits.LATE5
#define _LATE6 LATEbits.LATE6
#define _LATE7 LATEbits.LATE7
#define _LATE8 LATEbits.LATE8
#define _LATE9 LATEbits.LATE9

/* TRISF */
#define _TRISF0 TRISFbits.TRISF0
#define _TRISF1 TRISFbits.TRISF1
#define _TRISF2 TRISFbits.TRISF2
#define _TRISF3 TRISFbits.TRISF3
#define _TRISF4 TRISFbits.TRISF4
#define _TRISF5 TRISFbits.TRISF5
#define _TRISF6 TRISFbits.TRISF6
#define _TRISF7 TRISFbits.TRISF7
#define _TRISF8 TRISFbits.TRISF8
#define _TRISF12 TRISFbits.TRISF12
#define _TRISF13 TRISFbits.TRISF13

/* PORTF */
#define _RF0 PORTFbits.RF0
#define _RF1 PORTFbits.RF1
#define _RF2 PORTFbits.RF2
#define _RF3 PORTFbits.RF3
#define _RF4 PORTFbits.RF4
#define _RF5 PORTFbits.RF5
#define _RF6 PORTFbits.RF6
#define _RF7 PORTFbits.RF7
#define _RF8 PORTFbits.RF8
#define _RF12 PORTFbits.RF12
#define _RF13 PORTFbits.RF13

/* LATF */
#define _LATF0 LATFbits.LATF0
#define _LATF1 LATFbits.LATF1
#define _LATF2 LATFbits.LATF2
#define _LATF3 LATFbits.LATF3
#define _LATF4 LATFbits.LATF4
#define _LATF5 LATFbits.LATF5
#define _LATF6 LATFbits.LATF6
#define _LATF7 LATFbits.LATF7
#define _LATF8 LATFbits.LATF8
#define _LATF12 LATFbits.LATF12
#define _LATF13 LATFbits.LATF13

/* TRISG */
#define _TRISG0 TRISGbits.TRISG0
#define _TRISG1 TRISGbits.TRISG1
#define _TRISG2 TRISGbits.TRISG2
#define _TRISG3 TRISGbits.TRISG3
#define _TRISG6 TRISGbits.TRISG6
#define _TRISG7 TRISGbits.TRISG7
#define _TRISG8 TRISGbits.TRISG8
#define _TRISG9 TRISGbits.TRISG9
#define _TRISG12 TRISGbits.TRISG12
#define _TRISG13 TRISGbits.TRISG13
#define _TRISG14 TRISGbits.TRISG14
#define _TRISG15 TRISGbits.TRISG15

/* PORTG */
#define _RG0 PORTGbits.RG0
#define _RG1 PORTGbits.RG1
#define _RG2 PORTGbits.RG2
#define _RG3 PORTGbits.RG3
#define _RG6 PORTGbits.RG6
#define _RG7 PORTGbits.RG7
#define _RG8 PORTGbits.RG8
#define _RG9 PORTGbits.RG9
#define _RG12 PORTGbits.RG12
#define _RG13 PORTGbits.RG13
#define _RG14 PORTGbits.RG14
#define _RG15 PORTGbits.RG15

/* LATG */
#define _LATG0 LATGbits.LATG0
#define _LATG1 LATGbits.LATG1
#define _LATG2 LATGbits.LATG2
#define _LATG3 LATGbits.LATG3
#define _LATG6 LATGbits.LATG6
#define _LATG7 LATGbits.LATG7
#define _LATG8 LATGbits.LATG8
#define _LATG9 LATGbits.LATG9
#define _LATG12 LATGbits.LATG12
#define _LATG13 LATGbits.LATG13
#define _LATG14 LATGbits.LATG14
#define _LATG15 LATGbits.LATG15

/* OSC defines */
#define OSC_OSWEN 		0x01
#define OSC_SOCEN		0x02
#define OSC_URFCEN 		0x04
#define OSC_CF 			0x08
#define OSC_SLPEN	 	0x10
#define OSC_LOCK 		0x20
#define OSC_ULOCK 		0x40
#define OSC_CLKLOCK 	0x80

#define OSC_NOSC0	 	0x0100
#define OSC_NOSC1	 	0x0200
#define OSC_NOSC2 		0x0400
#define OSC_COSC0 		0x1000
#define OSC_COSC1 		0x2000
#define OSC_COSC2 		0x4000

#define OSC_PLLMULT0 	0x010000
#define OSC_PLLMULT1 	0x020000
#define OSC_PLLMULT2 	0x040000
#define OSC_PBDIV0 		0x080000
#define OSC_PBDIV1 		0x100000

#define OSC_FRCDIV0		0x01000000
#define OSC_FRCDIV1		0x02000000
#define OSC_FRCDIV2		0x04000000
#define OSC_PLLODIV0	0x08000000
#define OSC_PLLODIV1	0x10000000
#define OSC_PLLODIV2	0x20000000

#define OSC_TUN0 		0x01
#define OSC_TUN1 		0x02
#define OSC_TUN2 		0x04
#define OSC_TUN3 		0x08
#define OSC_TUN4 		0x10


/* System-wide IRQ numbers
 * Device-specific definitions now appear in processor header
 * files.
 */
#ifndef _CORE_TIMER_IRQ
#define _CORE_TIMER_IRQ                       0
#define _CORE_SOFTWARE_0_IRQ                  1
#define _CORE_SOFTWARE_1_IRQ                  2
#define _EXTERNAL_0_IRQ                       3
#define _TIMER_1_IRQ                          4
#define _INPUT_CAPTURE_1_IRQ                  5
#define _OUTPUT_COMPARE_1_IRQ                 6
#define _EXTERNAL_1_IRQ                       7
#define _TIMER_2_IRQ                          8
#define _INPUT_CAPTURE_2_IRQ                  9
#define _OUTPUT_COMPARE_2_IRQ                 10
#define _EXTERNAL_2_IRQ                       11
#define _TIMER_3_IRQ                          12
#define _INPUT_CAPTURE_3_IRQ                  13
#define _OUTPUT_COMPARE_3_IRQ                 14
#define _EXTERNAL_3_IRQ                       15
#define _TIMER_4_IRQ                          16
#define _INPUT_CAPTURE_4_IRQ                  17
#define _OUTPUT_COMPARE_4_IRQ                 18
#define _EXTERNAL_4_IRQ                       19
#define _TIMER_5_IRQ                          20
#define _INPUT_CAPTURE_5_IRQ                  21
#define _OUTPUT_COMPARE_5_IRQ                 22
#define _SPI1_ERR_IRQ                         23
#define _SPI1_TX_IRQ                          24
#define _SPI1_RX_IRQ                          25
#define _UART1_ERR_IRQ                        26
#define _UART1_RX_IRQ                         27
#define _UART1_TX_IRQ                         28
#define _I2C1_BUS_IRQ                         29
#define _I2C1_SLAVE_IRQ                       30
#define _I2C1_MASTER_IRQ                      31
#define _CHANGE_NOTICE_IRQ                    32
#define _ADC_IRQ                              33
#define _PMP_IRQ                              34
#define _COMPARATOR_1_IRQ                     35
#define _COMPARATOR_2_IRQ                     36
#define _SPI2_ERR_IRQ                         37
#define _SPI2_TX_IRQ                          38
#define _SPI2_RX_IRQ                          39
#define _UART2_ERR_IRQ                        40
#define _UART2_RX_IRQ                         41
#define _UART2_TX_IRQ                         42
#define _I2C2_BUS_IRQ                         43
#define _I2C2_SLAVE_IRQ                       44
#define _I2C2_MASTER_IRQ                      45
#define _FAIL_SAFE_MONITOR_IRQ                46
#define _RTCC_IRQ                             47
#define _DMA0_IRQ                             48
#define _DMA1_IRQ                             49
#define _DMA2_IRQ                             50
#define _DMA3_IRQ                             51
#define _FLASH_CONTROL_IRQ                    56
#endif /* _CORE_TIMER_IRQ */


#endif
