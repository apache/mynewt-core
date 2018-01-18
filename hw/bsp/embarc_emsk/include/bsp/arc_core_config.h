/* ------------------------------------------
 * Copyright (c) 2017, Synopsys, Inc. All rights reserved.

 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:

 * 1) Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.

 * 2) Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation and/or
 * other materials provided with the distribution.

 * 3) Neither the name of the Synopsys, Inc., nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.

 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
--------------------------------------------- */
#ifndef __core_config_h
	#define __core_config_h  1
	#define	core_config_cir_identity	0x00000142
	#define	core_config_cir_identity_chipid	0
	#define	core_config_cir_identity_arcnum	1
	#define	core_config_cir_identity_arcver	66
	#define	core_config_cir_identity_family	4
	#define	core_config_cir_identity_corever	2
	#define	core_config_cir_aux_dccm	0x80000000
	#define	core_config_bcr_bcr_ver	0x00000002
	#define	core_config_bcr_bcr_ver_version	2
	#define	core_config_bcr_vecbase_ac_build	0x00000010
	#define	core_config_bcr_rf_build	0x0000c802
	#define	core_config_bcr_rf_build_version	2
	#define	core_config_bcr_rf_build_p	0
	#define	core_config_bcr_rf_build_e	0
	#define	core_config_bcr_rf_build_r	0
	#define	core_config_bcr_rf_build_b	1
	#define	core_config_bcr_rf_build_d	3
	#define	core_config_bcr_dccm_build	0x00000a04
	#define	core_config_bcr_dccm_build_w	0
	#define	core_config_bcr_dccm_build_cycles	0
	#define	core_config_bcr_dccm_build_interleave	0
	#define	core_config_bcr_dccm_build_size1	0
	#define	core_config_bcr_dccm_build_size0	10
	#define	core_config_bcr_dccm_build_version	4
	#define	core_config_bcr_timer_build	0x00010304
	#define	core_config_bcr_timer_build_sp1	0
	#define	core_config_bcr_timer_build_sp0	0
	#define	core_config_bcr_timer_build_p1	0
	#define	core_config_bcr_timer_build_p0	1
	#define	core_config_bcr_timer_build_st1	0
	#define	core_config_bcr_timer_build_st0	0
	#define	core_config_bcr_timer_build_rtc	0
	#define	core_config_bcr_timer_build_rtsc_ver	1
	#define	core_config_bcr_timer_build_rtsc	0
	#define	core_config_bcr_timer_build_t0	1
	#define	core_config_bcr_timer_build_t1	1
	#define	core_config_bcr_timer_build_version	4
	#define	core_config_bcr_iccm_build	0x00000a04
	#define	core_config_bcr_iccm_build_iccm1_size1	0
	#define	core_config_bcr_iccm_build_iccm0_size1	0
	#define	core_config_bcr_iccm_build_iccm1_size0	0
	#define	core_config_bcr_iccm_build_iccm0_size0	10
	#define	core_config_bcr_iccm_build_version	4
	#define	core_config_bcr_dsp_build	0x00001121
	#define	core_config_bcr_dsp_build_wide	0
	#define	core_config_bcr_dsp_build_itu_pa	0
	#define	core_config_bcr_dsp_build_acc_shift	2
	#define	core_config_bcr_dsp_build_comp	0
	#define	core_config_bcr_dsp_build_divsqrt	1
	#define	core_config_bcr_dsp_build_version	33
	#define	core_config_bcr_multiply_build	0x00022a06
	#define	core_config_bcr_multiply_build_version16x16	2
	#define	core_config_bcr_multiply_build_dsp	2
	#define	core_config_bcr_multiply_build_cyc	2
	#define	core_config_bcr_multiply_build_type	2
	#define	core_config_bcr_multiply_build_version32x32	6
	#define	core_config_bcr_swap_build	0x00000003
	#define	core_config_bcr_swap_build_version	3
	#define	core_config_bcr_norm_build	0x00000003
	#define	core_config_bcr_norm_build_version	3
	#define	core_config_bcr_minmax_build	0x00000002
	#define	core_config_bcr_minmax_build_version	2
	#define	core_config_bcr_barrel_build	0x00000303
	#define	core_config_bcr_barrel_build_version	3
	#define	core_config_bcr_barrel_build_shift_option	3
	#define	core_config_bcr_isa_config	0x12447402
	#define	core_config_bcr_isa_config_res1	0
	#define	core_config_bcr_isa_config_d	1
	#define	core_config_bcr_isa_config_res2	0
	#define	core_config_bcr_isa_config_f	0
	#define	core_config_bcr_isa_config_c	2
	#define	core_config_bcr_isa_config_l	0
	#define	core_config_bcr_isa_config_n	1
	#define	core_config_bcr_isa_config_a	0
	#define	core_config_bcr_isa_config_b	0
	#define	core_config_bcr_isa_config_addr_size	4
	#define	core_config_bcr_isa_config_lpc_size	7
	#define	core_config_bcr_isa_config_pc_size	4
	#define	core_config_bcr_isa_config_version	2
	#define	core_config_bcr_stack_region_build	0x00000002
	#define	core_config_bcr_cprot_build	0x00000001
	#define	core_config_bcr_core_config	0x00000101
	#define	core_config_bcr_core_config_turbo_boost	1
	#define	core_config_bcr_core_config_version	1
	#define	core_config_bcr_irq_build	0x030b1401
	#define	core_config_bcr_irq_build_raz	0
	#define	core_config_bcr_irq_build_f	0
	#define	core_config_bcr_irq_build_p	3
	#define	core_config_bcr_irq_build_exts	11
	#define	core_config_bcr_irq_build_irqs	20
	#define	core_config_bcr_irq_build_version	1
	#define	core_config_cir_aux_iccm	0x00000000
	#define	core_config_cir_dmp_peripheral	0xf0000000
	#define	core_config_family	4
	#define	core_config_core_version	2
	#define	core_config_family_name	"arcv2em"
	#define	core_config_rgf_num_banks	2
	#define	core_config_rgf_banked_regs	32
	#define	core_config_rgf_num_wr_ports	1
	#define	core_config_endian	"little"
	#define	core_config_endian_little	1
	#define	core_config_endian_big	0
	#define	core_config_lpc_size	32
	#define	core_config_pc_size	32
	#define	core_config_addr_size	32
	#define	core_config_unaligned	1
	#define	core_config_code_density	1
	#define	core_config_div_rem	"radix2"
	#define	core_config_div_rem_radix2	1
	#define	core_config_turbo_boost	1
	#define	core_config_swap	1
	#define	core_config_bitscan	1
	#define	core_config_mpy_option	"mpyd"
	#define	core_config_mpy_option_num	8
	#define	core_config_shift_assist	1
	#define	core_config_barrel_shifter	1
	#define	core_config_dsp	1
	#define	core_config_dsp2	1
	#define	core_config_dsp_divsqrt	"radix2"
	#define	core_config_dsp_divsqrt_radix2	1
	#define	core_config_dsp_accshift	"full"
	#define	core_config_dsp_accshift_full	1
	#define	core_config_timer0	1
	#define	core_config_timer0_level	1
	#define	core_config_timer0_vector	16
	#define	core_config_timer1	1
	#define	core_config_timer1_level	0
	#define	core_config_timer1_vector	17
	#define	core_config_stack_check	1
	#define	core_config_code_protection	1
	#define	core_config_interrupts_present	1
	#define	core_config_interrupts_number	20
	#define	core_config_interrupts_priorities	4
	#define	core_config_interrupts_externals	11
	#define	core_config_interrupts	20
	#define	core_config_interrupt_priorities	4
	#define	core_config_ext_interrupts	11
	#define	core_config_interrupts_base	0x0
	#define	core_config_dccm_present	1
	#define	core_config_dccm_size	0x40000
	#define	core_config_dccm_base	0x80000000
	#define	core_config_iccm_present	1
	#define	core_config_iccm0_present	1
	#define	core_config_iccm_size	0x40000
	#define	core_config_iccm0_size	0x40000
	#define	core_config_iccm_base	0x00000000
	#define	core_config_iccm0_base	0x00000000
    #define	core_config_clock_speed	40
#endif /* __core_config_h */
