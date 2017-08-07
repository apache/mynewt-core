/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <xc.h>

.align	2
.set	nomips16
.set	noreorder
.set    noat

.text

.weak	_nmi_handler
.ent	_nmi_handler
_nmi_handler:
    mfc0    k0, _CP0_STATUS
    lui     k1, ~(_CP0_STATUS_BEV_MASK >> 16) & 0xffff
    ori     k1, k1, ~_CP0_STATUS_BEV_MASK & 0xffff
    and     k0, k0, k1
    mtc0    k0, _CP0_STATUS
    ehb
    eret
    nop
.end	_nmi_handler

.weak	_bootstrap_exception_handler
.ent	_bootstrap_exception_handler
_bootstrap_exception_handler:
    j  _bootstrap_exception_handler
    nop
.end	_bootstrap_exception_handler


.weak	_cache_err_exception_handler
.ent	_cache_err_exception_handler
_cache_err_exception_handler:
    j   _cache_err_exception_handler
    nop
.end	_cache_err_exception_handler

.weak	_simple_tlb_refill_exception_handler
.ent	_simple_tlb_refill_exception_handler
_simple_tlb_refill_exception_handler:
    j   _simple_tlb_refill_exception_handler
    nop
.end	_simple_tlb_refill_exception_handler

.weak	_on_bootstrap
.ent	_on_bootstrap
_on_bootstrap:
    jr  ra
    nop
.end	_on_bootstrap

.weak	_on_reset
.ent	_on_reset
_on_reset:
    jr  ra
    nop
.end	_on_reset
