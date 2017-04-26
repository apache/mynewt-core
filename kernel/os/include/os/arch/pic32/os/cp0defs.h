/*-------------------------------------------------------------------------
 *
 * Copyright (c) 2014, Microchip Technology Inc. and its subsidiaries ("Microchip")
 * All rights reserved.
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

#pragma once

#ifndef __CP0DEFS__H__
#define __CP0DEFS__H__
/*
 * Contains register definitions for the CP0 registers and bits and macros
 * to access the CP0 registers.  This file is included in xc.h and
 * depends on macro definitions found in that file.  To use this file
 * include xc.h
 */

#ifdef __LANGUAGE_ASSEMBLY__
/* CP0 Register Defines
 * #define _CP0_<register_name> <register_number>, <select_number>
 */
#define _CP0_INDEX                             $0, 0
#define _CP0_INX                               $0, 0
#define _CP0_RANDOM                            $1, 0
#define _CP0_RAND                              $1, 0
#define _CP0_ENTRYLO0                          $2, 0
#define _CP0_TLBLO0                            $2, 0
#define _CP0_ENTRYLO1                          $3, 0
#define _CP0_TLBLO1                            $3, 0
#define _CP0_CONTEXT                           $4, 0
#define _CP0_CTXT                              $4, 0
#define _CP0_USERLOCAL                         $4, 2
#define _CP0_PAGEMASK                          $5, 0
#define _CP0_PAGEGRAIN                         $5, 1
#define _CP0_WIRED                             $6, 0
#define _CP0_HWRENA                            $7, 0
#define _CP0_BADVADDR                          $8, 0
#define _CP0_COUNT                             $9, 0
#define _CP0_ENTRYHI                           $10, 0
#define _CP0_COMPARE                           $11, 0
#define _CP0_STATUS                            $12, 0
#define _CP0_INTCTL                            $12, 1
#define _CP0_SRSCTL                            $12, 2
#define _CP0_SRSMAP                            $12, 3
#define _CP0_VIEW_IPL                          $12, 4
#define _CP0_SRSMAP2                           $12, 5
#define _CP0_CAUSE                             $13, 0
#define _CP0_VIEW_RIPL                         $13, 1
#define _CP0_NESTEDEXC                         $13, 2
#define _CP0_EPC                               $14, 0
#define _CP0_NESTEDEPC                         $14, 1
#define _CP0_PRID                              $15, 0
#define _CP0_EBASE                             $15, 1
#define _CP0_CDMMBASE                          $15, 2
#define _CP0_CONFIG                            $16, 0
#define _CP0_CONFIG1                           $16, 1
#define _CP0_CONFIG2                           $16, 2
#define _CP0_CONFIG3                           $16, 3
#define _CP0_CONFIG4                           $16, 4
#define _CP0_CONFIG5                           $16, 5
#define _CP0_CONFIG7                           $16, 7
#define _CP0_LLADDR                            $17, 0
#define _CP0_WATCHLO                           $18, 0
#define _CP0_WATCHHI                           $19, 0
#define _CP0_DEBUG                             $23, 0
#define _CP0_TRACECONTROL                      $23, 1
#define _CP0_TRACECONTROL2                     $23, 2
#define _CP0_USERTRACEDATA                     $23, 3
#define _CP0_TRACEBPC                          $23, 4
#define _CP0_DEBUG2                            $23, 5
#define _CP0_DEPC                              $24, 0
#define _CP0_USERTRACEDATA2                    $24, 1
#define _CP0_PERFCNT0_CONTROL                  $25, 0
#define _CP0_PERFCNT0_COUNT                    $25, 1
#define _CP0_PERFCNT1_CONTROL                  $25, 2
#define _CP0_PERFCNT1_COUNT                    $25, 3
#define _CP0_ERRCTL                            $26, 0
#define _CP0_CACHEERR                          $27, 0
#define _CP0_TAGLO                             $28, 0
#define _CP0_DATALO                            $28, 1
#define _CP0_ERROREPC                          $30, 0
#define _CP0_DESAVE                            $31, 0

#else
#define _CP0_INDEX                             0
#define _CP0_INDEX_SELECT                      0
#define _CP0_INX                               0
#define _CP0_INX_SELECT                        0
#define _CP0_RANDOM                            1
#define _CP0_RANDOM_SELECT                     0
#define _CP0_RAND                              1
#define _CP0_RAND_SELECT                       0
#define _CP0_ENTRYLO0                          2
#define _CP0_ENTRYLO0_SELECT                   0
#define _CP0_TLBLO0                            2
#define _CP0_TLBLO0_SELECT                     0
#define _CP0_ENTRYLO1                          3
#define _CP0_ENTRYLO1_SELECT                   0
#define _CP0_TLBLO1                            3
#define _CP0_TLBLO1_SELECT                     0
#define _CP0_CONTEXT                           4
#define _CP0_CONTEXT_SELECT                    0
#define _CP0_CTXT                              4
#define _CP0_CTXT_SELECT                       0
#define _CP0_USERLOCAL                         4
#define _CP0_USERLOCAL_SELECT                  2
#define _CP0_PAGEMASK                          5
#define _CP0_PAGEMASK_SELECT                   0
#define _CP0_PAGEGRAIN                         5
#define _CP0_PAGEGRAIN_SELECT                  1
#define _CP0_WIRED                             6
#define _CP0_WIRED_SELECT                      0
#define _CP0_HWRENA                            7
#define _CP0_HWRENA_SELECT                     0
#define _CP0_BADVADDR                          8
#define _CP0_BADVADDR_SELECT                   0
#define _CP0_COUNT                             9
#define _CP0_COUNT_SELECT                      0
#define _CP0_ENTRYHI                           10
#define _CP0_ENTRYHI_SELECT                    0
#define _CP0_COMPARE                           11
#define _CP0_COMPARE_SELECT                    0
#define _CP0_STATUS                            12
#define _CP0_STATUS_SELECT                     0
#define _CP0_INTCTL                            12
#define _CP0_INTCTL_SELECT                     1
#define _CP0_SRSCTL                            12
#define _CP0_SRSCTL_SELECT                     2
#define _CP0_SRSMAP                            12
#define _CP0_SRSMAP_SELECT                     3
#define _CP0_VIEW_IPL                          12
#define _CP0_VIEW_IPL_SELECT                   4
#define _CP0_SRSMAP2                           12
#define _CP0_SRSMAP2_SELECT                    5
#define _CP0_CAUSE                             13
#define _CP0_CAUSE_SELECT                      0
#define _CP0_VIEW_RIPL                         13
#define _CP0_VIEW_RIPL_SELECT                  1
#define _CP0_NESTEDEXC                         13
#define _CP0_NESTEDEXC_SELECT                  2
#define _CP0_EPC                               14
#define _CP0_EPC_SELECT                        0
#define _CP0_NESTEDEPC                         14
#define _CP0_NESTEDEPC_SELECT                  1
#define _CP0_PRID                              15
#define _CP0_PRID_SELECT                       0
#define _CP0_EBASE                             15
#define _CP0_EBASE_SELECT                      1
#define _CP0_CDMMBASE                          15
#define _CP0_CDMMBASE_SELECT                   2
#define _CP0_CONFIG                            16
#define _CP0_CONFIG_SELECT                     0
#define _CP0_CONFIG1                           16
#define _CP0_CONFIG1_SELECT                    1
#define _CP0_CONFIG2                           16
#define _CP0_CONFIG2_SELECT                    2
#define _CP0_CONFIG3                           16
#define _CP0_CONFIG3_SELECT                    3
#define _CP0_CONFIG4                           16
#define _CP0_CONFIG4_SELECT                    4
#define _CP0_CONFIG5                           16
#define _CP0_CONFIG5_SELECT                    5
#define _CP0_CONFIG7                           16
#define _CP0_CONFIG7_SELECT                    7
#define _CP0_LLADDR                            17
#define _CP0_LLADDR_SELECT                     0
#define _CP0_WATCHLO                           18
#define _CP0_WATCHLO_SELECT                    0
#define _CP0_WATCHHI                           19
#define _CP0_WATCHHI_SELECT                    0
#define _CP0_DEBUG                             23
#define _CP0_DEBUG_SELECT                      0
#define _CP0_TRACECONTROL                      23
#define _CP0_TRACECONTROL_SELECT               1
#define _CP0_TRACECONTROL2                     23
#define _CP0_TRACECONTROL2_SELECT              2
#define _CP0_USERTRACEDATA                     23
#define _CP0_USERTRACEDATA_SELECT              3
#define _CP0_TRACEBPC                          23
#define _CP0_TRACEBPC_SELECT                   4
#define _CP0_DEBUG2                            23
#define _CP0_DEBUG2_SELEECT                    5
#define _CP0_DEPC                              24
#define _CP0_DEPC_SELECT                       0
#define _CP0_USERTRACEDATA2                    24
#define _CP0_USERTRACEDATA2_SELECT             1
#define _CP0_PERFCNT                           25
#define _CP0_PERFCNT0_CONTROL                  25
#define _CP0_PERFCNT0_CONTROL_SELECT           0
#define _CP0_PERFCNT0_COUNT                    25
#define _CP0_PERFCNT0_COUNT_SELECT             1
#define _CP0_PERFCNT1_CONTROL                  25
#define _CP0_PERFCNT1_CONTROL_SELECT           2
#define _CP0_PERFCNT1_COUNT                    25
#define _CP0_PERFCNT1_COUNT_SELECT             3
#define _CP0_ERRCTL                            26
#define _CP0_ERRCTL_SELECT                     0
#define _CP0_CACHEERR                          27
#define _CP0_CACHEERR_SELECT                   0
#define _CP0_TAGLO                             28
#define _CP0_TAGLO_SELECT                      0
#define _CP0_DATALO                            28
#define _CP0_DATALO_SELECT                     1
#define _CP0_ERROREPC                          30
#define _CP0_ERROREPC_SELECT                   0
#define _CP0_DESAVE                            31
#define _CP0_DESAVE_SELECT                     0

#define _CP0_GET_INDEX()      _mfc0 (_CP0_INDEX, _CP0_INDEX_SELECT)
#define _CP0_SET_INDEX(val)   _mtc0 (_CP0_INDEX, _CP0_INDEX_SELECT, val)
#define _CP0_XCH_INDEX(val)   _mxc0 (_CP0_INDEX, _CP0_INDEX_SELECT, val)
#define _CP0_BIC_INDEX(clr)   _bcc0 (_CP0_INDEX, _CP0_INDEX_SELECT, clr)
#define _CP0_BIS_INDEX(set)   _bsc0 (_CP0_INDEX, _CP0_INDEX_SELECT, set)
#define _CP0_BCS_INDEX(c,s)   _bcsc0(_CP0_INDEX, _CP0_INDEX_SELECT, c, s)

#define _CP0_GET_INX()      _mfc0 (_CP0_INX, _CP0_INX_SELECT)
#define _CP0_SET_INX(val)   _mtc0 (_CP0_INX, _CP0_INX_SELECT, val)
#define _CP0_XCH_INX(val)   _mxc0 (_CP0_INX, _CP0_INX_SELECT, val)
#define _CP0_BIC_INX(clr)   _bcc0 (_CP0_INX, _CP0_INX_SELECT, clr)
#define _CP0_BIS_INX(set)   _bsc0 (_CP0_INX, _CP0_INX_SELECT, set)
#define _CP0_BCS_INX(c,s)   _bcsc0(_CP0_INX, _CP0_INX_SELECT, c, s)

#define _CP0_GET_RANDOM()    _mfc0 (_CP0_RANDOM, _CP0_RANDOM_SELECT)

#define _CP0_GET_RAND()    _mfc0 (_CP0_RAND, _CP0_RAND_SELECT)

#define _CP0_GET_ENTRYLO0()      _mfc0 (_CP0_ENTRYLO0, _CP0_ENTRYLO0_SELECT)
#define _CP0_SET_ENTRYLO0(val)   _mtc0 (_CP0_ENTRYLO0, _CP0_ENTRYLO0_SELECT, val)
#define _CP0_XCH_ENTRYLO0(val)   _mxc0 (_CP0_ENTRYLO0, _CP0_ENTRYLO0_SELECT, val)
#define _CP0_BIC_ENTRYLO0(clr)   _bcc0 (_CP0_ENTRYLO0, _CP0_ENTRYLO0_SELECT, clr)
#define _CP0_BIS_ENTRYLO0(set)   _bsc0 (_CP0_ENTRYLO0, _CP0_ENTRYLO0_SELECT, set)
#define _CP0_BCS_ENTRYLO0(c,s)   _bcsc0(_CP0_ENTRYLO0, _CP0_ENTRYLO0_SELECT, c, s)

#define _CP0_GET_TLBLO0()      _mfc0 (_CP0_TLBLO0, _CP0_TLBLO0_SELECT)
#define _CP0_SET_TLBLO0(val)   _mtc0 (_CP0_TLBLO0, _CP0_TLBLO0_SELECT, val)
#define _CP0_XCH_TLBLO0(val)   _mxc0 (_CP0_TLBLO0, _CP0_TLBLO0_SELECT, val)
#define _CP0_BIC_TLBLO0(clr)   _bcc0 (_CP0_TLBLO0, _CP0_TLBLO0_SELECT, clr)
#define _CP0_BIS_TLBLO0(set)   _bsc0 (_CP0_TLBLO0, _CP0_TLBLO0_SELECT, set)
#define _CP0_BCS_TLBLO0(c,s)   _bcsc0(_CP0_TLBLO0, _CP0_TLBLO0_SELECT, c, s)

#define _CP0_GET_ENTRYLO1()      _mfc0 (_CP0_ENTRYLO1, _CP0_ENTRYLO1_SELECT)
#define _CP0_SET_ENTRYLO1(val)   _mtc0 (_CP0_ENTRYLO1, _CP0_ENTRYLO1_SELECT, val)
#define _CP0_XCH_ENTRYLO1(val)   _mxc0 (_CP0_ENTRYLO1, _CP0_ENTRYLO1_SELECT, val)
#define _CP0_BIC_ENTRYLO1(clr)   _bcc0 (_CP0_ENTRYLO1, _CP0_ENTRYLO1_SELECT, clr)
#define _CP0_BIS_ENTRYLO1(set)   _bsc0 (_CP0_ENTRYLO1, _CP0_ENTRYLO1_SELECT, set)
#define _CP0_BCS_ENTRYLO1(c,s)   _bcsc0(_CP0_ENTRYLO1, _CP0_ENTRYLO1_SELECT, c, s)

#define _CP0_GET_TLBLO1()      _mfc0 (_CP0_TLBLO1, _CP0_TLBLO1_SELECT)
#define _CP0_SET_TLBLO1(val)   _mtc0 (_CP0_TLBLO1, _CP0_TLBLO1_SELECT, val)
#define _CP0_XCH_TLBLO1(val)   _mxc0 (_CP0_TLBLO1, _CP0_TLBLO1_SELECT, val)
#define _CP0_BIC_TLBLO1(clr)   _bcc0 (_CP0_TLBLO1, _CP0_TLBLO1_SELECT, clr)
#define _CP0_BIS_TLBLO1(set)   _bsc0 (_CP0_TLBLO1, _CP0_TLBLO1_SELECT, set)
#define _CP0_BCS_TLBLO1(c,s)   _bcsc0(_CP0_TLBLO1, _CP0_TLBLO1_SELECT, c, s)

#define _CP0_GET_CONTEXT()      _mfc0 (_CP0_CONTEXT, _CP0_CONTEXT_SELECT)
#define _CP0_SET_CONTEXT(val)   _mtc0 (_CP0_CONTEXT, _CP0_CONTEXT_SELECT, val)
#define _CP0_XCH_CONTEXT(val)   _mxc0 (_CP0_CONTEXT, _CP0_CONTEXT_SELECT, val)
#define _CP0_BIC_CONTEXT(clr)   _bcc0 (_CP0_CONTEXT, _CP0_CONTEXT_SELECT, clr)
#define _CP0_BIS_CONTEXT(set)   _bsc0 (_CP0_CONTEXT, _CP0_CONTEXT_SELECT, set)
#define _CP0_BCS_CONTEXT(c,s)   _bcsc0(_CP0_CONTEXT, _CP0_CONTEXT_SELECT, c, s)

#define _CP0_GET_CTXT()      _mfc0 (_CP0_CTXT, _CP0_CTXT_SELECT)
#define _CP0_SET_CTXT(val)   _mtc0 (_CP0_CTXT, _CP0_CTXT_SELECT, val)
#define _CP0_XCH_CTXT(val)   _mxc0 (_CP0_CTXT, _CP0_CTXT_SELECT, val)
#define _CP0_BIC_CTXT(clr)   _bcc0 (_CP0_CTXT, _CP0_CTXT_SELECT, clr)
#define _CP0_BIS_CTXT(set)   _bsc0 (_CP0_CTXT, _CP0_CTXT_SELECT, set)
#define _CP0_BCS_CTXT(c,s)   _bcsc0(_CP0_CTXT, _CP0_CTXT_SELECT, c, s)

#define _CP0_GET_USERLOCAL()      _mfc0 (_CP0_USERLOCAL, _CP0_USERLOCAL_SELECT)
#define _CP0_SET_USERLOCAL(val)   _mtc0 (_CP0_USERLOCAL, _CP0_USERLOCAL_SELECT, val)

#define _CP0_GET_PAGEMASK()       _mfc0 (_CP0_PAGEMASK, _CP0_PAGEMASK_SELECT)
#define _CP0_SET_PAGEMASK(val)    _mtc0 (_CP0_PAGEMASK, _CP0_PAGEMASK_SELECT, val)

#define _CP0_GET_PAGEGRAIN()      _mfc0 (_CP0_PAGEGRAIN, _CP0_PAGEGRAIN_SELECT)
#define _CP0_SET_PAGEGRAIN(val)   _mtc0 (_CP0_PAGEGRAIN, _CP0_PAGEGRAIN_SELECT, val)
#define _CP0_XCH_PAGEGRAIN(val)   _mxc0 (_CP0_PAGEGRAIN, _CP0_PAGEGRAIN_SELECT, val)
#define _CP0_BIC_PAGEGRAIN(clr)   _bcc0 (_CP0_PAGEGRAIN, _CP0_PAGEGRAIN_SELECT, clr)
#define _CP0_BIS_PAGEGRAIN(set)   _bsc0 (_CP0_PAGEGRAIN, _CP0_PAGEGRAIN_SELECT, set)
#define _CP0_BCS_PAGEGRAIN(c,s)   _bcsc0(_CP0_PAGEGRAIN, _CP0_PAGEGRAIN_SELECT, c, s)

#define _CP0_GET_WIRED()       _mfc0 (_CP0_WIRED, _CP0_WIRED_SELECT)
#define _CP0_SET_WIRED(val)    _mtc0 (_CP0_WIRED, _CP0_WIRED_SELECT, val)

#define _CP0_GET_HWRENA()      _mfc0 (_CP0_HWRENA, _CP0_HWRENA_SELECT)
#define _CP0_SET_HWRENA(val)   _mtc0 (_CP0_HWRENA, _CP0_HWRENA_SELECT, val)
#define _CP0_XCH_HWRENA(val)   _mxc0 (_CP0_HWRENA, _CP0_HWRENA_SELECT, val)
#define _CP0_BIC_HWRENA(clr)   _bcc0 (_CP0_HWRENA, _CP0_HWRENA_SELECT, clr)
#define _CP0_BIS_HWRENA(set)   _bsc0 (_CP0_HWRENA, _CP0_HWRENA_SELECT, set)
#define _CP0_BCS_HWRENA(c,s)   _bcsc0(_CP0_HWRENA, _CP0_HWRENA_SELECT, c, s)

#define _CP0_GET_BADVADDR()    _mfc0 (_CP0_BADVADDR, _CP0_BADVADDR_SELECT)

#define _CP0_GET_COUNT()       _mfc0 (_CP0_COUNT, _CP0_COUNT_SELECT)
#define _CP0_SET_COUNT(val)    _mtc0 (_CP0_COUNT, _CP0_COUNT_SELECT, val)

#define _CP0_GET_COUNT()       _mfc0 (_CP0_COUNT, _CP0_COUNT_SELECT)
#define _CP0_SET_COUNT(val)    _mtc0 (_CP0_COUNT, _CP0_COUNT_SELECT, val)

#define _CP0_GET_ENTRYHI()      _mfc0 (_CP0_ENTRYHI, _CP0_ENTRYHI_SELECT)
#define _CP0_SET_ENTRYHI(val)   _mtc0 (_CP0_ENTRYHI, _CP0_ENTRYHI_SELECT, val)
#define _CP0_XCH_ENTRYHI(val)   _mxc0 (_CP0_ENTRYHI, _CP0_ENTRYHI_SELECT, val)
#define _CP0_BIC_ENTRYHI(clr)   _bcc0 (_CP0_ENTRYHI, _CP0_ENTRYHI_SELECT, clr)
#define _CP0_BIS_ENTRYHI(set)   _bsc0 (_CP0_ENTRYHI, _CP0_ENTRYHI_SELECT, set)
#define _CP0_BCS_ENTRYHI(c,s)   _bcsc0(_CP0_ENTRYHI, _CP0_ENTRYHI_SELECT, c, s)

#define _CP0_GET_COMPARE()     _mfc0 (_CP0_COMPARE, _CP0_COMPARE_SELECT)
#define _CP0_SET_COMPARE(val)  _mtc0 (_CP0_COMPARE, _CP0_COMPARE_SELECT, val)

#define _CP0_GET_STATUS()      _mfc0 (_CP0_STATUS, _CP0_STATUS_SELECT)
#define _CP0_SET_STATUS(val)   _mtc0 (_CP0_STATUS, _CP0_STATUS_SELECT, val)
#define _CP0_XCH_STATUS(val)   _mxc0 (_CP0_STATUS, _CP0_STATUS_SELECT, val)
#define _CP0_BIC_STATUS(clr)   _bcc0 (_CP0_STATUS, _CP0_STATUS_SELECT, clr)
#define _CP0_BIS_STATUS(set)   _bsc0 (_CP0_STATUS, _CP0_STATUS_SELECT, set)
#define _CP0_BCS_STATUS(c,s)   _bcsc0(_CP0_STATUS, _CP0_STATUS_SELECT, c, s)

#define _CP0_GET_INTCTL()      _mfc0 (_CP0_INTCTL, _CP0_INTCTL_SELECT)
#define _CP0_SET_INTCTL(val)   _mtc0 (_CP0_INTCTL, _CP0_INTCTL_SELECT, val)
#define _CP0_XCH_INTCTL(val)   _mxc0 (_CP0_INTCTL, _CP0_INTCTL_SELECT, val)

#define _CP0_GET_SRSCTL()      _mfc0 (_CP0_SRSCTL, _CP0_SRSCTL_SELECT)
#define _CP0_SET_SRSCTL(val)   _mtc0 (_CP0_SRSCTL, _CP0_SRSCTL_SELECT, val)
#define _CP0_XCH_SRSCTL(val)   _mxc0 (_CP0_SRSCTL, _CP0_SRSCTL_SELECT, val)

#define _CP0_GET_SRSMAP()      _mfc0 (_CP0_SRSMAP, _CP0_SRSMAP_SELECT)
#define _CP0_SET_SRSMAP(val)   _mtc0 (_CP0_SRSMAP, _CP0_SRSMAP_SELECT, val)
#define _CP0_XCH_SRSMAP(val)   _mxc0 (_CP0_SRSMAP, _CP0_SRSMAP_SELECT, val)

#define _CP0_GET_VIEW_IPL()      _mfc0 (_CP0_VIEW_IPL, _CP0_VIEW_IPL_SELECT)
#define _CP0_SET_VIEW_IPL(val)   _mtc0 (_CP0_VIEW_IPL, _CP0_VIEW_IPL_SELECT, val)
#define _CP0_XCH_VIEW_IPL(val)   _mxc0 (_CP0_VIEW_IPL, _CP0_VIEW_IPL_SELECT, val)
#define _CP0_BIC_VIEW_IPL(clr)   _bcc0 (_CP0_VIEW_IPL, _CP0_VIEW_IPL_SELECT, clr)
#define _CP0_BIS_VIEW_IPL(set)   _bsc0 (_CP0_VIEW_IPL, _CP0_VIEW_IPL_SELECT, set)
#define _CP0_BCS_VIEW_IPL(c,s)   _bcsc0(_CP0_VIEW_IPL, _CP0_VIEW_IPL_SELECT, c, s)

#define _CP0_GET_SRSMAP2()      _mfc0 (_CP0_SRSMAP2, _CP0_SRSMAP2_SELECT)
#define _CP0_SET_SRSMAP2(val)   _mtc0 (_CP0_SRSMAP2, _CP0_SRSMAP2_SELECT, val)
#define _CP0_XCH_SRSMAP2(val)   _mxc0 (_CP0_SRSMAP2, _CP0_SRSMAP2_SELECT, val)

#define _CP0_GET_CAUSE()       _mfc0 (_CP0_CAUSE, _CP0_CAUSE_SELECT)
#define _CP0_SET_CAUSE(val)    _mtc0 (_CP0_CAUSE, _CP0_CAUSE_SELECT, val)
#define _CP0_XCH_CAUSE(val)    _mxc0 (_CP0_CAUSE, _CP0_CAUSE_SELECT, val)
#define _CP0_BIC_CAUSE(clr)    _bcc0 (_CP0_CAUSE, _CP0_CAUSE_SELECT, clr)
#define _CP0_BIS_CAUSE(set)    _bsc0 (_CP0_CAUSE, _CP0_CAUSE_SELECT, set)
#define _CP0_BCS_CAUSE(c,s)    _bcsc0(_CP0_CAUSE, _CP0_CAUSE_SELECT, c, s)

#define _CP0_GET_VIEW_RIPL()      _mfc0 (_CP0_VIEW_RIPL, _CP0_VIEW_RIPL_SELECT)
#define _CP0_SET_VIEW_RIPL(val)   _mtc0 (_CP0_VIEW_RIPL, _CP0_VIEW_RIPL_SELECT, val)
#define _CP0_XCH_VIEW_RIPL(val)   _mxc0 (_CP0_VIEW_RIPL, _CP0_VIEW_RIPL_SELECT, val)
#define _CP0_BIC_VIEW_RIPL(clr)   _bcc0 (_CP0_VIEW_RIPL, _CP0_VIEW_RIPL_SELECT, clr)
#define _CP0_BIS_VIEW_RIPL(set)   _bsc0 (_CP0_VIEW_RIPL, _CP0_VIEW_RIPL_SELECT, set)
#define _CP0_BCS_VIEW_RIPL(c,s)   _bcsc0(_CP0_VIEW_RIPL, _CP0_VIEW_RIPL_SELECT, c, s)

#define _CP0_GET_NESTEDEXC()      _mfc0 (_CP0_NESTEDEXC, _CP0_NESTEDEXC_SELECT)
#define _CP0_SET_NESTEDEXC(val)   _mtc0 (_CP0_NESTEDEXC, _CP0_NESTEDEXC_SELECT, val)
#define _CP0_XCH_NESTEDEXC(val)   _mxc0 (_CP0_NESTEDEXC, _CP0_NESTEDEXC_SELECT, val)
#define _CP0_BIC_NESTEDEXC(clr)   _bcc0 (_CP0_NESTEDEXC, _CP0_NESTEDEXC_SELECT, clr)
#define _CP0_BIS_NESTEDEXC(set)   _bsc0 (_CP0_NESTEDEXC, _CP0_NESTEDEXC_SELECT, set)
#define _CP0_BCS_NESTEDEXC(c,s)   _bcsc0(_CP0_NESTEDEXC, _CP0_NESTEDEXC_SELECT, c, s)

#define _CP0_GET_EPC()         _mfc0 (_CP0_EPC, _CP0_EPC_SELECT)
#define _CP0_SET_EPC(val)      _mtc0 (_CP0_EPC, _CP0_EPC_SELECT, val)

#define _CP0_GET_NESTEDEPC()    _mfc0 (_CP0_NESTEDEPC, _CP0_NESTEDEPC_SELECT)
#define _CP0_SET_NESTEDEPC(val) _mtc0 (_CP0_NESTEDEPC, _CP0_NESTEDEPC_SELECT, val)

#define _CP0_GET_PRID()        _mfc0 (_CP0_PRID, _CP0_PRID_SELECT)

#define _CP0_GET_EBASE()       _mfc0 (_CP0_EBASE, _CP0_EBASE_SELECT)
#define _CP0_SET_EBASE(val)    _mtc0 (_CP0_EBASE, _CP0_EBASE_SELECT, val)
#define _CP0_XCH_EBASE(val)    _mxc0 (_CP0_EBASE, _CP0_EBASE_SELECT, val)

#define _CP0_GET_CDMMBASE()      _mfc0 (_CP0_CDMMBASE, _CP0_CDMMBASE_SELECT)
#define _CP0_SET_CDMMBASE(val)   _mtc0 (_CP0_CDMMBASE, _CP0_CDMMBASE_SELECT, val)
#define _CP0_XCH_CDMMBASE(val)   _mxc0 (_CP0_CDMMBASE, _CP0_CDMMBASE_SELECT, val)
#define _CP0_BIC_CDMMBASE(clr)   _bcc0 (_CP0_CDMMBASE, _CP0_CDMMBASE_SELECT, clr)
#define _CP0_BIS_CDMMBASE(set)   _bsc0 (_CP0_CDMMBASE, _CP0_CDMMBASE_SELECT, set)
#define _CP0_BCS_CDMMBASE(c,s)   _bcsc0(_CP0_CDMMBASE, _CP0_CDMMBASE_SELECT, c, s)

#define _CP0_GET_CONFIG()      _mfc0 (_CP0_CONFIG, _CP0_CONFIG_SELECT)
#define _CP0_GET_CONFIG1()     _mfc0 (_CP0_CONFIG1, _CP0_CONFIG1_SELECT)
#define _CP0_GET_CONFIG2()     _mfc0 (_CP0_CONFIG2, _CP0_CONFIG2_SELECT)
#define _CP0_GET_CONFIG3()     _mfc0 (_CP0_CONFIG3, _CP0_CONFIG3_SELECT)
#define _CP0_GET_CONFIG4()     _mfc0 (_CP0_CONFIG4, _CP0_CONFIG4_SELECT)
#define _CP0_GET_CONFIG5()     _mfc0 (_CP0_CONFIG5, _CP0_CONFIG5_SELECT)
#define _CP0_GET_CONFIG7()     _mfc0 (_CP0_CONFIG7, _CP0_CONFIG7_SELECT)

#define _CP0_GET_LLADDR()     _mfc0 (_CP0_LLADDR, _CP0_LLADDR_SELECT)

#define _CP0_GET_WATCHLO()      _mfc0 (_CP0_WATCHLO, _CP0_WATCHLO_SELECT)
#define _CP0_SET_WATCHLO(val)   _mtc0 (_CP0_WATCHLO, _CP0_WATCHLO_SELECT, val)
#define _CP0_XCH_WATCHLO(val)   _mxc0 (_CP0_WATCHLO, _CP0_WATCHLO_SELECT, val)
#define _CP0_BIC_WATCHLO(clr)   _bcc0 (_CP0_WATCHLO, _CP0_WATCHLO_SELECT, clr)
#define _CP0_BIS_WATCHLO(set)   _bsc0 (_CP0_WATCHLO, _CP0_WATCHLO_SELECT, set)
#define _CP0_BCS_WATCHLO(c,s)   _bcsc0(_CP0_WATCHLO, _CP0_WATCHLO_SELECT, c, s)

#define _CP0_GET_WATCHHI()      _mfc0 (_CP0_WATCHHI, _CP0_WATCHHI_SELECT)
#define _CP0_SET_WATCHHI(val)   _mtc0 (_CP0_WATCHHI, _CP0_WATCHHI_SELECT, val)
#define _CP0_XCH_WATCHHI(val)   _mxc0 (_CP0_WATCHHI, _CP0_WATCHHI_SELECT, val)
#define _CP0_BIC_WATCHHI(clr)   _bcc0 (_CP0_WATCHHI, _CP0_WATCHHI_SELECT, clr)
#define _CP0_BIS_WATCHHI(set)   _bsc0 (_CP0_WATCHHI, _CP0_WATCHHI_SELECT, set)
#define _CP0_BCS_WATCHHI(c,s)   _bcsc0(_CP0_WATCHHI, _CP0_WATCHHI_SELECT, c, s)

#define _CP0_GET_DEBUG()       _mfc0 (_CP0_DEBUG, _CP0_DEBUG_SELECT)
#define _CP0_SET_DEBUG(val)    _mtc0 (_CP0_DEBUG, _CP0_DEBUG_SELECT, val)
#define _CP0_XCH_DEBUG(val)    _mxc0 (_CP0_DEBUG, _CP0_DEBUG_SELECT, val)
#define _CP0_BIC_DEBUG(clr)    _bcc0 (_CP0_DEBUG, _CP0_DEBUG_SELECT, clr)
#define _CP0_BIS_DEBUG(set)    _bsc0 (_CP0_DEBUG, _CP0_DEBUG_SELECT, set)
#define _CP0_BCS_DEBUG(c,s)    _bcsc0(_CP0_DEBUG, _CP0_DEBUG_SELECT, c, s)

#define _CP0_GET_TRACECONTROL() \
        _mfc0 (_CP0_TRACECONTROL, _CP0_TRACECONTROL_SELECT)
#define _CP0_SET_TRACECONTROL(val) \
        _mtc0 (_CP0_TRACECONTROL, _CP0_TRACECONTROL_SELECT, val)
#define _CP0_XCH_TRACECONTROL(val) \
        _mxc0 (_CP0_TRACECONTROL, _CP0_TRACECONTROL_SELECT, val)
#define _CP0_BIC_TRACECONTROL(clr) \
        _bcc0 (_CP0_TRACECONTROL, _CP0_TRACECONTROL_SELECT, clr)
#define _CP0_BIS_TRACECONTROL(set) \
        _bsc0 (_CP0_TRACECONTROL, _CP0_TRACECONTROL_SELECT, set)
#define _CP0_BCS_TRACECONTROL(c,s) \
        _bcsc0(_CP0_TRACECONTROL, _CP0_TRACECONTROL_SELECT, c, s)

#define _CP0_GET_TRACECONTROL2() \
        _mfc0 (_CP0_TRACECONTROL2, _CP0_TRACECONTROL2_SELECT)

#define _CP0_GET_USERTRACEDATA() \
        _mfc0 (_CP0_USERTRACEDATA, _CP0_USERTRACEDATA_SELECT)
#define _CP0_SET_USERTRACEDATA(val) \
        _mtc0 (_CP0_USERTRACEDATA, _CP0_USERTRACEDATA_SELECT, val)
#define _CP0_XCH_USERTRACEDATA(val) \
        _mxc0 (_CP0_USERTRACEDATA, _CP0_USERTRACEDATA_SELECT, val)

#define _CP0_GET_USERTRACEDATA2() \
        _mfc0 (_CP0_USERTRACEDATA2, _CP0_USERTRACEDATA2_SELECT)
#define _CP0_SET_USERTRACEDATA2(val) \
        _mtc0 (_CP0_USERTRACEDATA2, _CP0_USERTRACEDATA2_SELECT, val)
#define _CP0_XCH_USERTRACEDATA2(val) \
        _mxc0 (_CP0_USERTRACEDATA2, _CP0_USERTRACEDATA2_SELECT, val)

#define _CP0_GET_TRACEBPC()    _mfc0 (_CP0_TRACEBPC, _CP0_TRACEBPC_SELECT)
#define _CP0_SET_TRACEBPC(val) _mtc0 (_CP0_TRACEBPC, _CP0_TRACEBPC_SELECT, val)
#define _CP0_XCH_TRACEBPC(val) _mxc0 (_CP0_TRACEBPC, _CP0_TRACEBPC_SELECT, val)
#define _CP0_BIC_TRACEBPC(clr) _bcc0 (_CP0_TRACEBPC, _CP0_TRACEBPC_SELECT, clr)
#define _CP0_BIS_TRACEBPC(set) _bsc0 (_CP0_TRACEBPC, _CP0_TRACEBPC_SELECT, set)
#define _CP0_BCS_TRACEBPC(c,s) _bcsc0(_CP0_TRACEBPC, _CP0_TRACEBPC_SELECT, c, s)

#define _CP0_GET_DEBUG2()      _mfc0 (_CP0_DEBUG2, _CP0_DEBUG2_SELECT)

#define _CP0_GET_DEPC()        _mfc0 (_CP0_DEPC, _CP0_DEPC_SELECT)
#define _CP0_SET_DEPC(val)     _mtc0 (_CP0_DEPC, _CP0_DEPC_SELECT, val)
#define _CP0_XCH_DEPC(val)     _mxc0 (_CP0_DEPC, _CP0_DEPC_SELECT, val)

#define _CP0_GET_PERFCNT0_CONTROL()     _mfc0 (_CP0_PERFCNT0_CONTROL, _CP0_PERFCNT0_CONTROL_SELECT)
#define _CP0_SET_PERFCNT0_CONTROL(val)  _mtc0 (_CP0_PERFCNT0_CONTROL, _CP0_PERFCNT0_CONTROL_SELECT, val)
#define _CP0_XCH_PERFCNT0_CONTROL(val)  _mxc0 (_CP0_PERFCNT0_CONTROL, _CP0_PERFCNT0_CONTROL_SELECT, val)
#define _CP0_BIC_PERFCNT0_CONTROL(clr)  _bcc0 (_CP0_PERFCNT0_CONTROL, _CP0_PERFCNT0_CONTROL_SELECT, clr)
#define _CP0_BIS_PERFCNT0_CONTROL(set)  _bsc0 (_CP0_PERFCNT0_CONTROL, _CP0_PERFCNT0_CONTROL_SELECT, set)
#define _CP0_BCS_PERFCNT0_CONTROL(c,s)  _bcsc0(_CP0_PERFCNT0_CONTROL, _CP0_PERFCNT0_CONTROL_SELECT, c, s)

#define _CP0_GET_PERFCNT0_COUNT()       _mfc0 (_CP0_PERFCNT0_COUNT, _CP0_PERFCNT0_COUNT_SELECT)
#define _CP0_SET_PERFCNT0_COUNT(val)    _mtc0 (_CP0_PERFCNT0_COUNT, _CP0_PERFCNT0_COUNT_SELECT, val)

#define _CP0_GET_PERFCNT1_CONTROL()     _mfc0 (_CP0_PERFCNT1_CONTROL, _CP0_PERFCNT1_CONTROL_SELECT)
#define _CP0_SET_PERFCNT1_CONTROL(val)  _mtc0 (_CP0_PERFCNT1_CONTROL, _CP0_PERFCNT1_CONTROL_SELECT, val)
#define _CP0_XCH_PERFCNT1_CONTROL(val)  _mxc0 (_CP0_PERFCNT1_CONTROL, _CP0_PERFCNT1_CONTROL_SELECT, val)
#define _CP0_BIC_PERFCNT1_CONTROL(clr)  _bcc0 (_CP0_PERFCNT1_CONTROL, _CP0_PERFCNT1_CONTROL_SELECT, clr)
#define _CP0_BIS_PERFCNT1_CONTROL(set)  _bsc0 (_CP0_PERFCNT1_CONTROL, _CP0_PERFCNT1_CONTROL_SELECT, set)
#define _CP0_BCS_PERFCNT1_CONTROL(c,s)  _bcsc0(_CP0_PERFCNT1_CONTROL, _CP0_PERFCNT1_CONTROL_SELECT, c, s)

#define _CP0_GET_PERFCNT1_COUNT()      _mfc0 (_CP0_PERFCNT1_COUNT, _CP0_PERFCNT1_COUNT_SELECT)
#define _CP0_SET_PERFCNT1_COUNT(val)   _mtc0 (_CP0_PERFCNT1_COUNT, _CP0_PERFCNT1_COUNT_SELECT, val)

#define _CP0_GET_CACHEERR()    _mfc0 (_CP0_CACHEERR, _CP0_CACHEERR_SELECT)

#define _CP0_GET_TAGLO()       _mfc0 (_CP0_TAGLO, _CP0_TAGLO_SELECT)
#define _CP0_SET_TAGLO(val)    _mtc0 (_CP0_TAGLO, _CP0_TAGLO_SELECT, val)
#define _CP0_XCH_TAGLO(val)    _mxc0 (_CP0_TAGLO, _CP0_TAGLO_SELECT, val)
#define _CP0_BIC_TAGLO(clr)    _bcc0 (_CP0_TAGLO, _CP0_TAGLO_SELECT, clr)
#define _CP0_BIS_TAGLO(set)    _bsc0 (_CP0_TAGLO, _CP0_TAGLO_SELECT, set)
#define _CP0_BCS_TAGLO(c,s)    _bcsc0(_CP0_TAGLO, _CP0_TAGLO_SELECT, c, s)

#define _CP0_GET_DATALO()      _mfc0 (_CP0_DATALO, _CP0_DATALO_SELECT)
#define _CP0_SET_DATALO(val)   _mtc0 (_CP0_DATALO, _CP0_DATALO_SELECT, val)

#define _CP0_GET_ERROREPC()    _mfc0 (_CP0_ERROREPC, _CP0_ERROREPC_SELECT)
#define _CP0_SET_ERROREPC(val) _mtc0 (_CP0_ERROREPC, _CP0_ERROREPC_SELECT, val)
#define _CP0_XCH_ERROREPC(val) _mxc0 (_CP0_ERROREPC, _CP0_ERROREPC_SELECT, val)

#define _CP0_GET_DESAVE()      _mfc0 (_CP0_DESAVE, _CP0_DESAVE_SELECT)
#define _CP0_SET_DESAVE(val)   _mtc0 (_CP0_DESAVE, _CP0_DESAVE_SELECT, val)
#define _CP0_XCH_DESAVE(val)   _mxc0 (_CP0_DESAVE, _CP0_DESAVE_SELECT, val)
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define _CP0_HWRENA_MASK_POSITION              0x00000000
#define _CP0_HWRENA_MASK_MASK                  0x0000000F
#define _CP0_HWRENA_MASK_LENGTH                0x00000004

#define _CP0_BADVADDR_ALL_POSITION             0x00000000
#define _CP0_BADVADDR_ALL_MASK                 0xFFFFFFFF
#define _CP0_BADVADDR_ALL_LENGTH               0x00000020

#define _CP0_COUNT_ALL_POSITION                0x00000000
#define _CP0_COUNT_ALL_MASK                    0xFFFFFFFF
#define _CP0_COUNT_ALL_LENGTH                  0x00000020

#define _CP0_COMPARE_ALL_POSITION              0x00000000
#define _CP0_COMPARE_ALL_MASK                  0xFFFFFFFF
#define _CP0_COMPARE_ALL_LENGTH                0x00000020

#define _CP0_STATUS_IE_POSITION                0x00000000
#define _CP0_STATUS_IE_MASK                    0x00000001
#define _CP0_STATUS_IE_LENGTH                  0x00000001

#define _CP0_STATUS_EXL_POSITION               0x00000001
#define _CP0_STATUS_EXL_MASK                   0x00000002
#define _CP0_STATUS_EXL_LENGTH                 0x00000001

#define _CP0_STATUS_ERL_POSITION               0x00000002
#define _CP0_STATUS_ERL_MASK                   0x00000004
#define _CP0_STATUS_ERL_LENGTH                 0x00000001

#define _CP0_STATUS_UM_POSITION                0x00000004
#define _CP0_STATUS_UM_MASK                    0x00000010
#define _CP0_STATUS_UM_LENGTH                  0x00000001

#define _CP0_STATUS_IM0_POSITION               0x00000008
#define _CP0_STATUS_IM0_MASK                   0x00000100
#define _CP0_STATUS_IM0_LENGTH                 0x00000001

#define _CP0_STATUS_IM1_POSITION               0x00000009
#define _CP0_STATUS_IM1_MASK                   0x00000200
#define _CP0_STATUS_IM1_LENGTH                 0x00000001

#define _CP0_STATUS_IPL_POSITION               0x0000000A
#define _CP0_STATUS_IPL_MASK                   0x0000FC00
#define _CP0_STATUS_IPL_LENGTH                 0x00000006

#define _CP0_STATUS_IM2_POSITION               0x0000000A
#define _CP0_STATUS_IM2_MASK                   0x00000400
#define _CP0_STATUS_IM2_LENGTH                 0x00000001

#define _CP0_STATUS_IM3_POSITION               0x0000000B
#define _CP0_STATUS_IM3_MASK                   0x00000800
#define _CP0_STATUS_IM3_LENGTH                 0x00000001

#define _CP0_STATUS_IM4_POSITION               0x0000000C
#define _CP0_STATUS_IM4_MASK                   0x00001000
#define _CP0_STATUS_IM4_LENGTH                 0x00000001

#define _CP0_STATUS_IM5_POSITION               0x0000000D
#define _CP0_STATUS_IM5_MASK                   0x00002000
#define _CP0_STATUS_IM5_LENGTH                 0x00000001

#define _CP0_STATUS_IM6_POSITION               0x0000000E
#define _CP0_STATUS_IM6_MASK                   0x00004000
#define _CP0_STATUS_IM6_LENGTH                 0x00000001

#define _CP0_STATUS_IM7_POSITION               0x0000000F
#define _CP0_STATUS_IM7_MASK                   0x00008000
#define _CP0_STATUS_IM7_LENGTH                 0x00000001

#define _CP0_STATUS_CEE_POSITION               0x00000011
#define _CP0_STATUS_CEE_MASK                   0x00020000
#define _CP0_STATUS_CEE_LENGTH                 0x00000001

#define _CP0_STATUS_NMI_POSITION               0x00000013
#define _CP0_STATUS_NMI_MASK                   0x00080000
#define _CP0_STATUS_NMI_LENGTH                 0x00000001

#define _CPO_STATUS_SR_POSITION                0x00000014
#define _CP0_STATUS_SR_MASK                    0x00100000
#define _CP0_STATUS_SR_LENGTH                  0x00000001

/* TLB Shutdown */
#define _CP0_STATUS_TS_POSITION                0x00000015
#define _CP0_STATUS_TS_MASK                    0x00200000
#define _CP0_STATUS_TS_LENGTH                  0x00000001

#define _CP0_STATUS_BEV_POSITION               0x00000016
#define _CP0_STATUS_BEV_MASK                   0x00400000
#define _CP0_STATUS_BEV_LENGTH                 0x00000001

/* enable MDMX/DSP ASE */
#define _CP0_STATUS_MX_POSITION                0x00000018
#define _CP0_STATUS_MX_MASK                    0x01000000
#define _CP0_STATUS_MX_LENGTH                  0x00000001

#define _CP0_STATUS_RE_POSITION                0x00000019
#define _CP0_STATUS_RE_MASK                    0x02000000
#define _CP0_STATUS_RE_LENGTH                  0x00000001

#define _CP0_STATUS_FR_POSITION                0x0000001A
#define _CP0_STATUS_FR_MASK                    0x04000000
#define _CP0_STATUS_FR_LENGTH                  0x00000001

#define _CP0_STATUS_RP_POSITION                0x0000001B
#define _CP0_STATUS_RP_MASK                    0x08000000
#define _CP0_STATUS_RP_LENGTH                  0x00000001

#define _CP0_STATUS_CU0_POSITION               0x0000001C
#define _CP0_STATUS_CU0_MASK                   0x10000000
#define _CP0_STATUS_CU0_LENGTH                 0x00000001

#define _CP0_STATUS_CU1_POSITION               0x0000001D
#define _CP0_STATUS_CU1_MASK                   0x20000000
#define _CP0_STATUS_CU1_LENGTH                 0x00000001

#define _CP0_STATUS_CU2_POSITION               0x0000001E
#define _CP0_STATUS_CU2_MASK                   0x40000000
#define _CP0_STATUS_CU2_LENGTH                 0x00000001

#define _CP0_STATUS_CU3_POSITION               0x0000001F
#define _CP0_STATUS_CU3_MASK                   0x80000000
#define _CP0_STATUS_CU3_LENGTH                 0x00000001

#define _CP0_INTCTL_VS_POSITION                0x00000005
#define _CP0_INTCTL_VS_MASK                    0x000003E0
#define _CP0_INTCTL_VS_LENGTH                  0x00000005

#define _CP0_INTCTL_IPPCI_POSITION             0x0000001A
#define _CP0_INTCTL_IPPCI_MASK                 0x1C000000
#define _CP0_INTCTL_IPPCI_LENGTH               0x00000003

#define _CP0_INTCTL_IPTI_POSITION              0x0000001D
#define _CP0_INTCTL_IPTI_MASK                  0xE0000000
#define _CP0_INTCTL_IPTI_LENGTH                0x00000003

#define _CP0_SRSCTL_CSS_POSITION               0x00000000
#define _CP0_SRSCTL_CSS_MASK                   0x0000000F
#define _CP0_SRSCTL_CSS_LENGTH                 0x00000004

#define _CP0_SRSCTL_PSS_POSITION               0x00000006
#define _CP0_SRSCTL_PSS_MASK                   0x000003C0
#define _CP0_SRSCTL_PSS_LENGTH                 0x00000004

#define _CP0_SRSCTL_ESS_POSITION               0x0000000C
#define _CP0_SRSCTL_ESS_MASK                   0x0000F000
#define _CP0_SRSCTL_ESS_LENGTH                 0x00000004

#define _CP0_SRSCTL_EICSS_POSITION             0x00000012
#define _CP0_SRSCTL_EICSS_MASK                 0x003C0000
#define _CP0_SRSCTL_EICSS_LENGTH               0x00000004

#define _CP0_SRSCTL_HSS_POSITION               0x0000001A
#define _CP0_SRSCTL_HSS_MASK                   0x3C000000
#define _CP0_SRSCTL_HSS_LENGTH                 0x00000004

#define _CP0_SRSMAP_SSV0_POSITION              0x00000000
#define _CP0_SRSMAP_SSV0_MASK                  0x0000000F
#define _CP0_SRSMAP_SSV0_LENGTH                0x00000004

#define _CP0_SRSMAP_SSV1_POSITION              0x00000004
#define _CP0_SRSMAP_SSV1_MASK                  0x000000F0
#define _CP0_SRSMAP_SSV1_LENGTH                0x00000004

#define _CP0_SRSMAP_SSV2_POSITION              0x00000008
#define _CP0_SRSMAP_SSV2_MASK                  0x00000F00
#define _CP0_SRSMAP_SSV2_LENGTH                0x00000004

#define _CP0_SRSMAP_SSV3_POSITION              0x0000000C
#define _CP0_SRSMAP_SSV3_MASK                  0x0000F000
#define _CP0_SRSMAP_SSV3_LENGTH                0x00000004

#define _CP0_SRSMAP_SSV4_POSITION              0x00000010
#define _CP0_SRSMAP_SSV4_MASK                  0x000F0000
#define _CP0_SRSMAP_SSV4_LENGTH                0x00000004

#define _CP0_SRSMAP_SSV5_POSITION              0x00000014
#define _CP0_SRSMAP_SSV5_MASK                  0x00F00000
#define _CP0_SRSMAP_SSV5_LENGTH                0x00000004

#define _CP0_SRSMAP_SSV6_POSITION              0x00000018
#define _CP0_SRSMAP_SSV6_MASK                  0x0F000000
#define _CP0_SRSMAP_SSV6_LENGTH                0x00000004

#define _CP0_SRSMAP_SSV7_POSITION              0x0000001C
#define _CP0_SRSMAP_SSV7_MASK                  0xF0000000
#define _CP0_SRSMAP_SSV7_LENGTH                0x00000004

#define _CP0_CAUSE_EXCCODE_POSITION            0x00000002
#define _CP0_CAUSE_EXCCODE_MASK                0x0000007C
#define _CP0_CAUSE_EXCCODE_LENGTH              0x00000005

#define _CP0_CAUSE_IP0_POSITION                0x00000008
#define _CP0_CAUSE_IP0_MASK                    0x00000100
#define _CP0_CAUSE_IP0_LENGTH                  0x00000001

#define _CP0_CAUSE_IP1_POSITION                0x00000009
#define _CP0_CAUSE_IP1_MASK                    0x00000200
#define _CP0_CAUSE_IP1_LENGTH                  0x00000001

#define _CP0_CAUSE_RIPL_POSITION               0x0000000A
#define _CP0_CAUSE_RIPL_MASK                   0x0000FC00
#define _CP0_CAUSE_RIPL_LENGTH                 0x00000006

#define _CP0_CAUSE_IP2_POSITION                0x0000000A
#define _CP0_CAUSE_IP2_MASK                    0x00000400
#define _CP0_CAUSE_IP2_LENGTH                  0x00000001

#define _CP0_CAUSE_IP3_POSITION                0x0000000B
#define _CP0_CAUSE_IP3_MASK                    0x00000800
#define _CP0_CAUSE_IP3_LENGTH                  0x00000001

#define _CP0_CAUSE_IP4_POSITION                0x0000000C
#define _CP0_CAUSE_IP4_MASK                    0x00001000
#define _CP0_CAUSE_IP4_LENGTH                  0x00000001

#define _CP0_CAUSE_IP5_POSITION                0x0000000D
#define _CP0_CAUSE_IP5_MASK                    0x00002000
#define _CP0_CAUSE_IP5_LENGTH                  0x00000001

#define _CP0_CAUSE_IP6_POSITION                0x0000000E
#define _CP0_CAUSE_IP6_MASK                    0x00004000
#define _CP0_CAUSE_IP6_LENGTH                  0x00000001

#define _CP0_CAUSE_IP7_POSITION                0x0000000F
#define _CP0_CAUSE_IP7_MASK                    0x00008000
#define _CP0_CAUSE_IP7_LENGTH                  0x00000001

#define _CP0_CAUSE_WP_POSITION                 0x00000016
#define _CP0_CAUSE_WP_MASK                     0x00400000
#define _CP0_CAUSE_WP_LENGTH                   0x00000001

#define _CP0_CAUSE_IV_POSITION                 0x00000017
#define _CP0_CAUSE_IV_MASK                     0x00800000
#define _CP0_CAUSE_IV_LENGTH                   0x00000001

#define _CP0_CAUSE_PCI_POSITION                0x0000001A
#define _CP0_CAUSE_PCI_MASK                    0x04000000
#define _CP0_CAUSE_PCI_LENGTH                  0x00000001

#define _CP0_CAUSE_DC_POSITION                 0x0000001B
#define _CP0_CAUSE_DC_MASK                     0x08000000
#define _CP0_CAUSE_DC_LENGTH                   0x00000001

#define _CP0_CAUSE_CE_POSITION                 0x0000001C
#define _CP0_CAUSE_CE_MASK                     0x30000000
#define _CP0_CAUSE_CE_LENGTH                   0x00000002

#define _CP0_CAUSE_TI_POSITION                 0x0000001E
#define _CP0_CAUSE_TI_MASK                     0x40000000
#define _CP0_CAUSE_TI_LENGTH                   0x00000001

#define _CP0_CAUSE_BD_POSITION                 0x0000001F
#define _CP0_CAUSE_BD_MASK                     0x80000000
#define _CP0_CAUSE_BD_LENGTH                   0x00000001

#define _EXCCODE_INT                           0x00
#define _EXCCODE_MOD                           0x01        /* tlb modification */
#define _EXCCODE_TLBL                          0x02        /* tlb miss (load/i-fetch) */
#define _EXCCODE_TLBS                          0x03        /* tlb miss (store) */
#define _EXCCODE_ADEL                          0x04
#define _EXCCODE_ADES                          0x05
#define _EXCCODE_IBE                           0x06
#define _EXCCODE_DBE                           0x07
#define _EXCCODE_SYS                           0x08
#define _EXCCODE_BP                            0x09
#define _EXCCODE_RI                            0x0A
#define _EXCCODE_CPU                           0x0B
#define _EXCCODE_OV                            0x0C
#define _EXCCODE_TR                            0x0D
#define _EXCCODE_IS1                           0x10
#define _EXCCODE_CEU                           0x11
#define _EXCCODE_C2E                           0x12
#define _EXCCODE_DSPU                          0x1A        /* dsp unusable */

#define _CP0_EPC_ALL_POSITION                  0x00000000
#define _CP0_EPC_ALL_MASK                      0xFFFFFFFF
#define _CP0_EPC_ALL_LENGTH                    0x00000020

#define _CP0_PRID_REVISION_POSITION            0x00000000
#define _CP0_PRID_REVISION_MASK                0x000000FF
#define _CP0_PRID_REVISION_LENGTH              0x00000020

#define _CP0_PRID_PATCHREV_POSITION            0x00000000
#define _CP0_PRID_PATCHREV_MASK                0x00000003
#define _CP0_PRID_PATCHREV_LENGTH              0x00000002

#define _CP0_PRID_MINORREV_POSITION            0x00000002
#define _CP0_PRID_MINORREV_MASK                0x0000001C
#define _CP0_PRID_MINORREV_LENGTH              0x00000003

#define _CP0_PRID_MAJORREV_POSITION            0x00000005
#define _CP0_PRID_MAJORREV_MASK                0x000000E0
#define _CP0_PRID_MAJORREV_LENGTH              0x00000003

#define _CP0_PRID_PROCESSORID_POSITION         0x00000008
#define _CP0_PRID_PROCESSORID_MASK             0x0000FF00
#define _CP0_PRID_PROCESSORID_LENGTH           0x00000008

#define _CP0_PRID_COMPANYID_POSITION           0x00000010
#define _CP0_PRID_COMPANYID_MASK               0x00FF0000
#define _CP0_PRID_COMPANYID_LENGTH             0x00000008

#define _CP0_EBASE_CPUNUM_POSITION             0x00000000
#define _CP0_EBASE_CPUNUM_MASK                 0x000003FF
#define _CP0_EBASE_CPUNUM_LENGTH               0x0000000A

#define _CP0_EBASE_EBASE_POSITION              0x0000000C
#define _CP0_EBASE_EBASE_MASK                  0x3FFFF000
#define _CP0_EBASE_EBASE_LENGTH                0x0000000E

/* Kseg0 coherency algorithm */
#define _CP0_CONFIG_K0_POSITION                0x00000000
#define _CP0_CONFIG_K0_MASK                    0x00000007
#define _CP0_CONFIG_K0_LENGTH                  0x00000003

/* MMU Type */
#define _CP0_CONFIG_MT_POSITION                0x00000007
#define _CP0_CONFIG_MT_MASK                    0x00000380
#define _CP0_CONFIG_MT_LENGTH                  0x00000003
#define   _CP0_CONFIG_MT_NONE                  (0<<7)
#define   _CP0_CONFIG_MT_TLB                   (1<<7)
#define   _CP0_CONFIG_MT_BAT                   (2<<7)
#define   _CP0_CONFIG_MT_NONSTD                (3<<7)

#define _CP0_CONFIG_AR_POSITION                0x0000000A
#define _CP0_CONFIG_AR_MASK                    0x00001C00
#define _CP0_CONFIG_AR_LENGTH                  0x00000003

#define _CP0_CONFIG_AT_POSITION                0x0000000D
#define _CP0_CONFIG_AT_MASK                    0x00006000
#define _CP0_CONFIG_AT_LENGTH                  0x00000002

#define _CP0_CONFIG_BE_POSITION                0x0000000F
#define _CP0_CONFIG_BE_MASK                    0x00008000
#define _CP0_CONFIG_BE_LENGTH                  0x00000001

#define _CP0_CONFIG_DS_POSITION                0x00000010
#define _CP0_CONFIG_DS_MASK                    0x00010000
#define _CP0_CONFIG_DS_LENGTH                  0x00000001

#define _CP0_CONFIG_MDU_POSITION               0x00000014
#define _CP0_CONFIG_MDU_MASK                   0x00100000
#define _CP0_CONFIG_MDU_LENGTH                 0x00000001

#define _CP0_CONFIG_SB_POSITION                0x00000015
#define _CP0_CONFIG_SB_MASK                    0x00200000
#define _CP0_CONFIG_SB_LENGTH                  0x00000001

#define _CP0_CONFIG_UDI_POSITION               0x00000016
#define _CP0_CONFIG_UDI_MASK                   0x00400000
#define _CP0_CONFIG_UDI_LENGTH                 0x00000001

#define _CP0_CONFIG_KU_POSITION                0x00000019
#define _CP0_CONFIG_KU_MASK                    0x70000000
#define _CP0_CONFIG_KU_LENGTH                  0x00000003

#define _CP0_CONFIG_M_POSITION                 0x0000001F
#define _CP0_CONFIG_M_MASK                     0x80000000
#define _CP0_CONFIG_M_LENGTH                   0x00000001

#define _CP0_CONFIG1_FP_POSITION               0x00000000
#define _CP0_CONFIG1_FP_MASK                   0x00000001
#define _CP0_CONFIG1_FP_LENGTH                 0x00000001

#define _CP0_CONFIG1_EP_POSITION               0x00000001
#define _CP0_CONFIG1_EP_MASK                   0x00000002
#define _CP0_CONFIG1_EP_LENGTH                 0x00000001

#define _CP0_CONFIG1_CA_POSITION               0x00000002
#define _CP0_CONFIG1_CA_MASK                   0x00000004
#define _CP0_CONFIG1_CA_LENGTH                 0x00000001

#define _CP0_CONFIG1_WR_POSITION               0x00000003
#define _CP0_CONFIG1_WR_MASK                   0x00000008
#define _CP0_CONFIG1_WR_LENGTH                 0x00000001

#define _CP0_CONFIG1_PC_POSITION               0x00000004
#define _CP0_CONFIG1_PC_MASK                   0x00000010
#define _CP0_CONFIG1_PC_LENGTH                 0x00000001

#define _CP0_CONFIG1_MD_POSITION               0x00000005
#define _CP0_CONFIG1_MD_MASK                   0x00000020
#define _CP0_CONFIG1_MD_LENGTH                 0x00000001

#define _CP0_CONFIG1_C2_POSITION               0x00000006
#define _CP0_CONFIG1_C2_MASK                   0x00000040
#define _CP0_CONFIG1_C2_LENGTH                 0x00000001

#define _CP0_CONFIG1_DA_POSITION               0x00000007
#define _CP0_CONFIG1_DA_MASK                   0x00000380
#define _CP0_CONFIG1_DA_LENGTH                 0x00000003

#define _CP0_CONFIG1_DL_POSITION               0x0000000A
#define _CP0_CONFIG1_DL_MASK                   0x00001C00
#define _CP0_CONFIG1_DL_LENGTH                 0x00000003

#define _CP0_CONFIG1_DS_POSITION               0x0000000D
#define _CP0_CONFIG1_DS_MASK                   0x0000E000
#define _CP0_CONFIG1_DS_LENGTH                 0x00000003

#define _CP0_CONFIG1_IA_POSITION               0x00000010
#define _CP0_CONFIG1_IA_MASK                   0x00070000
#define _CP0_CONFIG1_IA_LENGTH                 0x00000003

#define _CP0_CONFIG1_IL_POSITION               0x00000013
#define _CP0_CONFIG1_IL_MASK                   0x00380000
#define _CP0_CONFIG1_IL_LENGTH                 0x00000003

#define _CP0_CONFIG1_IS_POSITION               0x00000016
#define _CP0_CONFIG1_IS_MASK                   0x01C00000
#define _CP0_CONFIG1_IS_LENGTH                 0x00000003

#define _CP0_CONFIG1_MMUSIZE_POSITION          0x00000019
#define _CP0_CONFIG1_MMUSIZE_MASK              0x7E000000
#define _CP0_CONFIG1_MMUSIZE_LENGTH            0x00000006

#define _CP0_CONFIG1_M_POSITION                0x0000001F
#define _CP0_CONFIG1_M_MASK                    0x80000000
#define _CP0_CONFIG1_M_LENGTH                  0x00000001

#define _CP0_CONFIG2_M_POSITION                0x0000001F
#define _CP0_CONFIG2_M_MASK                    0x80000000
#define _CP0_CONFIG2_M_LENGTH                  0x00000001

#define _CP0_CONFIG3_TL_POSITION               0x00000000
#define _CP0_CONFIG3_TL_MASK                   0x00000001
#define _CP0_CONFIG3_TL_LENGTH                 0x00000001

#define _CP0_CONFIG3_SM_POSITION               0x00000001
#define _CP0_CONFIG3_SM_MASK                   0x00000002
#define _CP0_CONFIG3_SM_LENGTH                 0x00000001

#define _CP0_CONFIG3_SP_POSITION               0x00000004
#define _CP0_CONFIG3_SP_MASK                   0x00000010
#define _CP0_CONFIG3_SP_LENGTH                 0x00000001

#define _CP0_CONFIG3_VINT_POSITION             0x00000005
#define _CP0_CONFIG3_VINT_MASK                 0x00000020
#define _CP0_CONFIG3_VINT_LENGTH               0x00000001

#define _CP0_CONFIG3_VEIC_POSITION             0x00000006
#define _CP0_CONFIG3_VEIC_MASK                 0x00000040
#define _CP0_CONFIG3_VEIC_LENGTH               0x00000001

#define _CP0_CONFIG3_ITL_POSITION              0x00000008
#define _CP0_CONFIG3_ITL_MASK                  0x00000100
#define _CP0_CONFIG3_ITL_LENGTH                0x00000001

/* DSP ASE present */
#define _CP0_CONFIG3_DSPP_POSITION             0x0000000A
#define _CP0_CONFIG3_DSPP_MASK                 0x00000400
#define _CP0_CONFIG3_DSPP_LENGTH               0x00000001

#define _CP0_CONFIG3_DSP2P_POSITION            0x0000000B
#define _CP0_CONFIG3_DSP2P_MASK                0x00000800
#define _CP0_CONFIG3_DSP2P_LENGTH              0x00000001

#define _CP0_CONFIG3_M_POSITION                0x0000001F
#define _CP0_CONFIG3_M_MASK                    0x80000000
#define _CP0_CONFIG3_M_LENGTH                  0x00000001

#define _CP0_DEBUG_DSS_POSITION                0x00000000
#define _CP0_DEBUG_DSS_MASK                    0x00000001
#define _CP0_DEBUG_DSS_LENGTH                  0x00000001

#define _CP0_DEBUG_DBP_POSITION                0x00000001
#define _CP0_DEBUG_DBP_MASK                    0x00000002
#define _CP0_DEBUG_DBP_LENGTH                  0x00000001

#define _CP0_DEBUG_DDBL_POSITION               0x00000002
#define _CP0_DEBUG_DDBL_MASK                   0x00000004
#define _CP0_DEBUG_DDBL_LENGTH                 0x00000001

#define _CP0_DEBUG_DDBS_POSITION               0x00000003
#define _CP0_DEBUG_DDBS_MASK                   0x00000008
#define _CP0_DEBUG_DDBS_LENGTH                 0x00000001

#define _CP0_DEBUG_DIB_POSITION                0x00000004
#define _CP0_DEBUG_DIB_MASK                    0x00000010
#define _CP0_DEBUG_DIB_LENGTH                  0x00000001

#define _CP0_DEBUG_DINT_POSITION               0x00000005
#define _CP0_DEBUG_DINT_MASK                   0x00000020
#define _CP0_DEBUG_DINT_LENGTH                 0x00000001

#define _CP0_DEBUG_DIBIMPR_POSITION            0x00000006
#define _CP0_DEBUG_DIBIMPR_MASK                0x00000040
#define _CP0_DEBUG_DIBIMPR_LENGTH              0x00000001

#define _CP0_DEBUG_R_POSITION                  0x00000007
#define _CP0_DEBUG_R_MASK                      0x00000080
#define _CP0_DEBUG_R_LENGTH                    0x00000001

#define _CP0_DEBUG_SST_POSITION                0x00000008
#define _CP0_DEBUG_SST_MASK                    0x00000100
#define _CP0_DEBUG_SST_LENGTH                  0x00000001

#define _CP0_DEBUG_NOSST_POSITION              0x00000009
#define _CP0_DEBUG_NOSST_MASK                  0x00000200
#define _CP0_DEBUG_NOSST_LENGTH                0x00000001

#define _CP0_DEBUG_DEXCCODE_POSITION           0x0000000A
#define _CP0_DEBUG_DEXCCODE_MASK               0x00007C00
#define _CP0_DEBUG_DEXCCODE_LENGTH             0x00000005

#define _CP0_DEBUG_VER_POSITION                0x0000000F
#define _CP0_DEBUG_VER_MASK                    0x00038000
#define _CP0_DEBUG_VER_LENGTH                  0x00000003

#define _CP0_DEBUG_DDBLIMPR_POSITION           0x00000012
#define _CP0_DEBUG_DDBLIMPR_MASK               0x00040000
#define _CP0_DEBUG_DDBLIMPR_LENGTH             0x00000001

#define _CP0_DEBUG_DDBSIMPR_POSITION           0x00000013
#define _CP0_DEBUG_DDBSIMPR_MASK               0x00080000
#define _CP0_DEBUG_DDBSIMPR_LENGTH             0x00000001

#define _CP0_DEBUG_IEXI_POSITION               0x00000014
#define _CP0_DEBUG_IEXI_MASK                   0x00100000
#define _CP0_DEBUG_IEXI_LENGTH                 0x00000001

#define _CP0_DEBUG_DBUSEP_POSITION             0x00000015
#define _CP0_DEBUG_DBUSEP_MASK                 0x00200000
#define _CP0_DEBUG_DBUSEP_LENGTH               0x00000001

#define _CP0_DEBUG_CACHEEP_POSITION            0x00000016
#define _CP0_DEBUG_CACHEEP_MASK                0x00400000
#define _CP0_DEBUG_CACHEEP_LENGTH              0x00000001

#define _CP0_DEBUG_MCHECKP_POSITION            0x00000017
#define _CP0_DEBUG_MCHECKP_MASK                0x00800000
#define _CP0_DEBUG_MCHECKP_LENGTH              0x00000001

#define _CP0_DEBUG_IBUSEP_POSITION             0x00000018
#define _CP0_DEBUG_IBUSEP_MASK                 0x01000000
#define _CP0_DEBUG_IBUSEP_LENGTH               0x00000001

#define _CP0_DEBUG_COUNTDM_POSITION            0x00000019
#define _CP0_DEBUG_COUNTDM_MASK                0x02000000
#define _CP0_DEBUG_COUNTDM_LENGTH              0x00000001

#define _CP0_DEBUG_HALT_POSITION               0x0000001A
#define _CP0_DEBUG_HALT_MASK                   0x04000000
#define _CP0_DEBUG_HALT_LENGTH                 0x00000001

#define _CP0_DEBUG_DOZE_POSITION               0x0000001B
#define _CP0_DEBUG_DOZE_MASK                   0x08000000
#define _CP0_DEBUG_DOZE_LENGTH                 0x00000001

#define _CP0_DEBUG_LSNM_POSITION               0x0000001C
#define _CP0_DEBUG_LSNM_MASK                   0x10000000
#define _CP0_DEBUG_LSNM_LENGTH                 0x00000001

#define _CP0_DEBUG_NODCR_POSITION              0x0000001D
#define _CP0_DEBUG_NODCR_MASK                  0x20000000
#define _CP0_DEBUG_NODCR_LENGTH                0x00000001

#define _CP0_DEBUG_DM_POSITION                 0x0000001E
#define _CP0_DEBUG_DM_MASK                     0x40000000
#define _CP0_DEBUG_DM_LENGTH                   0x00000001

#define _CP0_DEBUG_DBD_POSITION                0x0000001F
#define _CP0_DEBUG_DBD_MASK                    0x80000000
#define _CP0_DEBUG_DBD_LENGTH                  0x00000001

#define _CP0_TRACECONTROL_ON_POSITION          0x00000000
#define _CP0_TRACECONTROL_ON_MASK              0x00000001
#define _CP0_TRACECONTROL_ON_LENGTH            0x00000001

#define _CP0_TRACECONTROL_MODE_POSITION        0x00000001
#define _CP0_TRACECONTROL_MODE_MASK            0x0000000E
#define _CP0_TRACECONTROL_MODE_LENGTH          0x00000003

#define _CP0_TRACECONTROL_G_POSITION           0x00000004
#define _CP0_TRACECONTROL_G_MASK               0x00000010
#define _CP0_TRACECONTROL_G_LENGTH             0x00000001

#define _CP0_TRACECONTROL_ASID_POSITION        0x00000005
#define _CP0_TRACECONTROL_ASID_MASK            0x00001FE0
#define _CP0_TRACECONTROL_ASID_LENGTH          0x00000008

#define _CP0_TRACECONTROL_ASID_M_POSITION      0x0000000D
#define _CP0_TRACECONTROL_ASID_M_MASK          0x001FE000
#define _CP0_TRACECONTROL_ASID_M_LENGTH        0x00000008

#define _CP0_TRACECONTROL_U_POSITION           0x00000015
#define _CP0_TRACECONTROL_U_MASK               0x00200000
#define _CP0_TRACECONTROL_U_LENGTH             0x00000001

#define _CP0_TRACECONTROL_0_POSITION           0x00000016
#define _CP0_TRACECONTROL_0_MASK               0x00400000
#define _CP0_TRACECONTROL_0_LENGTH             0x00000001

#define _CP0_TRACECONTROL_K_POSITION           0x00000017
#define _CP0_TRACECONTROL_K_MASK               0x00800000
#define _CP0_TRACECONTROL_K_LENGTH             0x00000001

#define _CP0_TRACECONTROL_E_POSITION           0x00000018
#define _CP0_TRACECONTROL_E_MASK               0x01000000
#define _CP0_TRACECONTROL_E_LENGTH             0x00000001

#define _CP0_TRACECONTROL_D_POSITION           0x00000019
#define _CP0_TRACECONTROL_D_MASK               0x02000000
#define _CP0_TRACECONTROL_D_LENGTH             0x00000001

#define _CP0_TRACECONTROL_IO_POSITION          0x0000001A
#define _CP0_TRACECONTROL_IO_MASK              0x04000000
#define _CP0_TRACECONTROL_IO_LENGTH            0x00000001

#define _CP0_TRACECONTROL_TB_POSITION          0x0000001B
#define _CP0_TRACECONTROL_TB_MASK              0x08000000
#define _CP0_TRACECONTROL_TB_LENGTH            0x00000001

#define _CP0_TRACECONTROL_UT_POSITION          0x0000001E
#define _CP0_TRACECONTROL_UT_MASK              0x40000000
#define _CP0_TRACECONTROL_UT_LENGTH            0x00000001

#define _CP0_TRACECONTROL_TS_POSITION          0x0000001F
#define _CP0_TRACECONTROL_TS_MASK              0x80000000
#define _CP0_TRACECONTROL_TS_LENGTH            0x00000001

#define _CP0_TRACECONTROL2_SYP_POSITION        0x00000000
#define _CP0_TRACECONTROL2_SYP_MASK            0x00000007
#define _CP0_TRACECONTROL2_SYP_LENGTH          0x00000003

#define _CP0_TRACECONTROL2_TBU_POSITION        0x00000003
#define _CP0_TRACECONTROL2_TBU_MASK            0x00000008
#define _CP0_TRACECONTROL2_TBU_LENGTH          0x00000001

#define _CP0_TRACECONTROL2_TBI_POSITION        0x00000004
#define _CP0_TRACECONTROL2_TBI_MASK            0x00000010
#define _CP0_TRACECONTROL2_TBI_LENGTH          0x00000001

#define _CP0_TRACECONTROL2_VALIDMODES_POSITION 0x00000005
#define _CP0_TRACECONTROL2_VALIDMODES_MASK     0x00000060
#define _CP0_TRACECONTROL2_VALIDMODES_LENGTH   0x00000002

#define _CP0_USERTRACEDATA_DATA_POSITION       0x00000000
#define _CP0_USERTRACEDATA_DATA_MASK           0xFFFFFFFF
#define _CP0_USERTRACEDATA_DATA_LENGTH         0x00000020

#define _CP0_TRACEBPC_IBPON_POSITION           0x00000000
#define _CP0_TRACEBPC_IBPON_MASK               0x0000003F
#define _CP0_TRACEBPC_IBPON_LENGTH             0x00000006

#define _CP0_TRACEBPC_IE_POSITION              0x0000000F
#define _CP0_TRACEBPC_IE_MASK                  0x00008000
#define _CP0_TRACEBPC_IE_LENGTH                0x00000001

#define _CP0_TRACEBPC_DBPON_POSITION           0x00000010
#define _CP0_TRACEBPC_DBPON_MASK               0x00030000
#define _CP0_TRACEBPC_DBPON_LENGTH             0x00000002

#define _CP0_TRACEBPC_DE_POSITION              0x0000001F
#define _CP0_TRACEBPC_DE_MASK                  0x80000000
#define _CP0_TRACEBPC_DE_LENGTH                0x00000001

#define _CP0_DEBUG2_PACO_POSITION              0x00000000
#define _CP0_DEBUG2_PACO_MASK                  0x00000001
#define _CP0_DEBUG2_PACO_LENGTH                0x00000001

#define _CP0_DEBUG2_TUP_POSITION               0x00000001
#define _CP0_DEBUG2_TUP_MASK                   0x00000002
#define _CP0_DEBUG2_TUP_LENGTH                 0x00000001

#define _CP0_DEBUG2_DQ_POSITION                0x00000002
#define _CP0_DEBUG2_DQ_MASK                    0x00000004
#define _CP0_DEBUG2_DQ_LENGTH                  0x00000001

#define _CP0_DEBUG2_PRM_POSITION               0x00000003
#define _CP0_DEBUG2_PRM_MASK                   0x00000008
#define _CP0_DEBUG2_PRM_LENGTH                 0x00000001

#define _CP0_DEPC_ALL_POSITION                 0x00000000
#define _CP0_DEPC_ALL_MASK                     0xFFFFFFFF
#define _CP0_DEPC_ALL_LENGTH                   0x00000020

#define _CP0_ERROREPC_ALL_POSITION             0x00000000
#define _CP0_ERROREPC_ALL_MASK                 0xFFFFFFFF
#define _CP0_ERROREPC_ALL_LENGTH               0x00000020

#define _CP0_DESAVE_ALL_POSITION               0x00000000
#define _CP0_DESAVE_ALL_MASK                   0xFFFFFFFF
#define _CP0_DESAVE_ALL_LENGTH                 0x00000020

#ifdef __cplusplus
}
#endif

#endif
