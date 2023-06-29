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

#ifndef __MCUX_COMMON_H_
#define __MCUX_COMMON_H_

#ifdef __cplusplus
extern "C" {
#endif

#if MYNEWT_VAL(MCU_K32L2A31A)
#include <K32L2A31A.h>
#elif MYNEWT_VAL(MCU_K32L2A41A)
#include <K32L2A41A.h>
#elif MYNEWT_VAL(MCU_K32L2B11A)
#include <K32L2B11A.h>
#elif MYNEWT_VAL(MCU_K32L2B21A)
#include <K32L2B21A.h>
#elif MYNEWT_VAL(MCU_K32L2B31A)
#include <K32L2B31A.h>
#elif MYNEWT_VAL(MCU_K32L3A60)
#include <K32L3A60.h>
#elif MYNEWT_VAL(MCU_LPC51U68)
#include <LPC51U68.h>
#elif MYNEWT_VAL(MCU_LPC54005)
#include <LPC54005.h>
#elif MYNEWT_VAL(MCU_LPC54016)
#include <LPC54016.h>
#elif MYNEWT_VAL(MCU_LPC54018)
#include <LPC54018.h>
#elif MYNEWT_VAL(MCU_LPC54018M)
#include <LPC54018M.h>
#elif MYNEWT_VAL(MCU_LPC54113)
#include <LPC54113.h>
#elif MYNEWT_VAL(MCU_LPC54114)
#include <LPC54114.h>
#elif MYNEWT_VAL(MCU_LPC54605)
#include <LPC54605.h>
#elif MYNEWT_VAL(MCU_LPC54606)
#include <LPC54606.h>
#elif MYNEWT_VAL(MCU_LPC54607)
#include <LPC54607.h>
#elif MYNEWT_VAL(MCU_LPC54608)
#include <LPC54608.h>
#elif MYNEWT_VAL(MCU_LPC54616)
#include <LPC54616.h>
#elif MYNEWT_VAL(MCU_LPC54618)
#include <LPC54618.h>
#elif MYNEWT_VAL(MCU_LPC54628)
#include <LPC54628.h>
#elif MYNEWT_VAL(MCU_LPC54S005)
#include <LPC54S005.h>
#elif MYNEWT_VAL(MCU_LPC54S016)
#include <LPC54S016.h>
#elif MYNEWT_VAL(MCU_LPC54S018)
#include <LPC54S018.h>
#elif MYNEWT_VAL(MCU_LPC54S018M)
#include <LPC54S018M.h>
#elif MYNEWT_VAL(MCU_LPC5502)
#include <LPC5502.h>
#elif MYNEWT_VAL(MCU_LPC5502CPXXXX)
#include <LPC5502CPXXXX.h>
#elif MYNEWT_VAL(MCU_LPC5504)
#include <LPC5504.h>
#elif MYNEWT_VAL(MCU_LPC5504CPXXXX)
#include <LPC5504CPXXXX.h>
#elif MYNEWT_VAL(MCU_LPC5506)
#include <LPC5506.h>
#elif MYNEWT_VAL(MCU_LPC5506CPXXXX)
#include <LPC5506CPXXXX.h>
#elif MYNEWT_VAL(MCU_LPC5512)
#include <LPC5512.h>
#elif MYNEWT_VAL(MCU_LPC5514)
#include <LPC5514.h>
#elif MYNEWT_VAL(MCU_LPC5516)
#include <LPC5516.h>
#elif MYNEWT_VAL(MCU_LPC5526)
#include <LPC5526.h>
#elif MYNEWT_VAL(MCU_LPC5528)
#include <LPC5528.h>
#elif MYNEWT_VAL(MCU_LPC55S04)
#include <LPC55S04.h>
#elif MYNEWT_VAL(MCU_LPC55S06)
#include <LPC55S06.h>
#elif MYNEWT_VAL(MCU_LPC55S14)
#include <LPC55S14.h>
#elif MYNEWT_VAL(MCU_LPC55S16)
#include <LPC55S16.h>
#elif MYNEWT_VAL(MCU_LPC55S26)
#include <LPC55S26.h>
#elif MYNEWT_VAL(MCU_LPC55S28)
#include <LPC55S28.h>
#elif MYNEWT_VAL(MCU_LPC55S66)
#include <LPC55S66.h>
#elif MYNEWT_VAL(MCU_LPC55S69)
#include <LPC55S69_cm33_core0.h>
#elif MYNEWT_VAL(MCU_LPC802)
#include <LPC802.h>
#elif MYNEWT_VAL(MCU_LPC804)
#include <LPC804.h>
#elif MYNEWT_VAL(MCU_LPC810)
#include <LPC810.h>
#elif MYNEWT_VAL(MCU_LPC811)
#include <LPC811.h>
#elif MYNEWT_VAL(MCU_LPC812)
#include <LPC812.h>
#elif MYNEWT_VAL(MCU_LPC822)
#include <LPC822.h>
#elif MYNEWT_VAL(MCU_LPC824)
#include <LPC824.h>
#elif MYNEWT_VAL(MCU_LPC832)
#include <LPC832.h>
#elif MYNEWT_VAL(MCU_LPC834)
#include <LPC834.h>
#elif MYNEWT_VAL(MCU_LPC844)
#include <LPC844.h>
#elif MYNEWT_VAL(MCU_LPC845)
#include <LPC845.h>
#elif MYNEWT_VAL(MCU_MCIMX7U3)
#include <MCIMX7U3.h>
#elif MYNEWT_VAL(MCU_MCIMX7U5)
#include <MCIMX7U5.h>
#elif MYNEWT_VAL(MCU_MIMX8DX1)
#include <MIMX8DX1.h>
#elif MYNEWT_VAL(MCU_MIMX8DX2)
#include <MIMX8DX2.h>
#elif MYNEWT_VAL(MCU_MIMX8DX3)
#include <MIMX8DX3.h>
#elif MYNEWT_VAL(MCU_MIMX8DX4)
#include <MIMX8DX4.h>
#elif MYNEWT_VAL(MCU_MIMX8DX5)
#include <MIMX8DX5.h>
#elif MYNEWT_VAL(MCU_MIMX8DX6)
#include <MIMX8DX6.h>
#elif MYNEWT_VAL(MCU_MIMX8MD6)
#include <MIMX8MD6.h>
#elif MYNEWT_VAL(MCU_MIMX8MD7)
#include <MIMX8MD7.h>
#elif MYNEWT_VAL(MCU_MIMX8ML3)
#include <MIMX8ML3.h>
#elif MYNEWT_VAL(MCU_MIMX8ML4)
#include <MIMX8ML4.h>
#elif MYNEWT_VAL(MCU_MIMX8ML6)
#include <MIMX8ML6.h>
#elif MYNEWT_VAL(MCU_MIMX8ML8)
#include <MIMX8ML8.h>
#elif MYNEWT_VAL(MCU_MIMX8MM1)
#include <MIMX8MM1.h>
#elif MYNEWT_VAL(MCU_MIMX8MM2)
#include <MIMX8MM2.h>
#elif MYNEWT_VAL(MCU_MIMX8MM3)
#include <MIMX8MM3.h>
#elif MYNEWT_VAL(MCU_MIMX8MM4)
#include <MIMX8MM4.h>
#elif MYNEWT_VAL(MCU_MIMX8MM5)
#include <MIMX8MM5.h>
#elif MYNEWT_VAL(MCU_MIMX8MM6)
#include <MIMX8MM6.h>
#elif MYNEWT_VAL(MCU_MIMX8MN1)
#include <MIMX8MN1.h>
#elif MYNEWT_VAL(MCU_MIMX8MN2)
#include <MIMX8MN2.h>
#elif MYNEWT_VAL(MCU_MIMX8MN3)
#include <MIMX8MN3.h>
#elif MYNEWT_VAL(MCU_MIMX8MN4)
#include <MIMX8MN4.h>
#elif MYNEWT_VAL(MCU_MIMX8MN5)
#include <MIMX8MN5.h>
#elif MYNEWT_VAL(MCU_MIMX8MN6)
#include <MIMX8MN6.h>
#elif MYNEWT_VAL(MCU_MIMX8MQ5)
#include <MIMX8MQ5.h>
#elif MYNEWT_VAL(MCU_MIMX8MQ6)
#include <MIMX8MQ6.h>
#elif MYNEWT_VAL(MCU_MIMX8MQ7)
#include <MIMX8MQ7.h>
#elif MYNEWT_VAL(MCU_MIMX8QM6)
#include <MIMX8QM6.h>
#elif MYNEWT_VAL(MCU_MIMX8QX1)
#include <MIMX8QX1.h>
#elif MYNEWT_VAL(MCU_MIMX8QX2)
#include <MIMX8QX2.h>
#elif MYNEWT_VAL(MCU_MIMX8QX3)
#include <MIMX8QX3.h>
#elif MYNEWT_VAL(MCU_MIMX8QX4)
#include <MIMX8QX4.h>
#elif MYNEWT_VAL(MCU_MIMX8QX5)
#include <MIMX8QX5.h>
#elif MYNEWT_VAL(MCU_MIMX8QX6)
#include <MIMX8QX6.h>
#elif MYNEWT_VAL(MCU_MIMX8UX5)
#include <MIMX8UX5.h>
#elif MYNEWT_VAL(MCU_MIMX8UX6)
#include <MIMX8UX6.h>
#elif MYNEWT_VAL(MCU_MIMXRT1011)
#include <MIMXRT1011.h>
#elif MYNEWT_VAL(MCU_MIMXRT1015)
#include <MIMXRT1015.h>
#elif MYNEWT_VAL(MCU_MIMXRT1021)
#include <MIMXRT1021.h>
#elif MYNEWT_VAL(MCU_MIMXRT1024)
#include <MIMXRT1024.h>
#elif MYNEWT_VAL(MCU_MIMXRT1051)
#include <MIMXRT1051.h>
#elif MYNEWT_VAL(MCU_MIMXRT1052)
#include <MIMXRT1052.h>
#elif MYNEWT_VAL(MCU_MIMXRT1061)
#include <MIMXRT1061.h>
#elif MYNEWT_VAL(MCU_MIMXRT1062)
#include <MIMXRT1062.h>
#elif MYNEWT_VAL(MCU_MIMXRT1064)
#include <MIMXRT1064.h>
#elif MYNEWT_VAL(MCU_MIMXRT1165)
#include <MIMXRT1165.h>
#elif MYNEWT_VAL(MCU_MIMXRT1166)
#include <MIMXRT1166.h>
#elif MYNEWT_VAL(MCU_MIMXRT1171)
#include <MIMXRT1171.h>
#elif MYNEWT_VAL(MCU_MIMXRT1172)
#include <MIMXRT1172.h>
#elif MYNEWT_VAL(MCU_MIMXRT1173)
#include <MIMXRT1173.h>
#elif MYNEWT_VAL(MCU_MIMXRT1175)
#include <MIMXRT1175.h>
#elif MYNEWT_VAL(MCU_MIMXRT1176)
#include <MIMXRT1176.h>
#elif MYNEWT_VAL(MCU_MIMXRT533S)
#include <MIMXRT533S.h>
#elif MYNEWT_VAL(MCU_MIMXRT555S)
#include <MIMXRT555S.h>
#elif MYNEWT_VAL(MCU_MIMXRT595S)
#include <MIMXRT595S.h>
#elif MYNEWT_VAL(MCU_MIMXRT633S)
#include <MIMXRT633S.h>
#elif MYNEWT_VAL(MCU_MIMXRT685S)
#include <MIMXRT685S.h>
#elif MYNEWT_VAL(MCU_MK02F12810)
#include <MK02F12810.h>
#elif MYNEWT_VAL(MCU_MK22F12810)
#include <MK22F12810.h>
#elif MYNEWT_VAL(MCU_MK22F25612)
#include <MK22F25612.h>
#elif MYNEWT_VAL(MCU_MK22F51212)
#include <MK22F51212.h>
#elif MYNEWT_VAL(MCU_MK24F12)
#include <MK24F12.h>
#elif MYNEWT_VAL(MCU_MK26F18)
#include <MK26F18.h>
#elif MYNEWT_VAL(MCU_MK27FA15)
#include <MK27FA15.h>
#elif MYNEWT_VAL(MCU_MK28FA15)
#include <MK28FA15.h>
#elif MYNEWT_VAL(MCU_MK63F12)
#include <MK63F12.h>
#elif MYNEWT_VAL(MCU_MK64F12)
#include <MK64F12.h>
#elif MYNEWT_VAL(MCU_MK65F18)
#include <MK65F18.h>
#elif MYNEWT_VAL(MCU_MK66F18)
#include <MK66F18.h>
#elif MYNEWT_VAL(MCU_MK80F25615)
#include <MK80F25615.h>
#elif MYNEWT_VAL(MCU_MK82F25615)
#include <MK82F25615.h>
#elif MYNEWT_VAL(MCU_MKE02Z4)
#include <MKE02Z4.h>
#elif MYNEWT_VAL(MCU_MKE04Z1284)
#include <MKE04Z1284.h>
#elif MYNEWT_VAL(MCU_MKE04Z4)
#include <MKE04Z4.h>
#elif MYNEWT_VAL(MCU_MKE06Z4)
#include <MKE06Z4.h>
#elif MYNEWT_VAL(MCU_MKE12Z7)
#include <MKE12Z7.h>
#elif MYNEWT_VAL(MCU_MKE13Z7)
#include <MKE13Z7.h>
#elif MYNEWT_VAL(MCU_MKE14F16)
#include <MKE14F16.h>
#elif MYNEWT_VAL(MCU_MKE14Z4)
#include <MKE14Z4.h>
#elif MYNEWT_VAL(MCU_MKE14Z7)
#include <MKE14Z7.h>
#elif MYNEWT_VAL(MCU_MKE15Z4)
#include <MKE15Z4.h>
#elif MYNEWT_VAL(MCU_MKE15Z7)
#include <MKE15Z7.h>
#elif MYNEWT_VAL(MCU_MKE16F16)
#include <MKE16F16.h>
#elif MYNEWT_VAL(MCU_MKE16Z4)
#include <MKE16Z4.h>
#elif MYNEWT_VAL(MCU_MKE17Z7)
#include <MKE17Z7.h>
#elif MYNEWT_VAL(MCU_MKE18F16)
#include <MKE18F16.h>
#elif MYNEWT_VAL(MCU_MKL17Z644)
#include <MKL17Z644.h>
#elif MYNEWT_VAL(MCU_MKL25Z4)
#include <MKL25Z4.h>
#elif MYNEWT_VAL(MCU_MKL27Z644)
#include <MKL27Z644.h>
#elif MYNEWT_VAL(MCU_MKM14ZA5)
#include <MKM14ZA5.h>
#elif MYNEWT_VAL(MCU_MKM33ZA5)
#include <MKM33ZA5.h>
#elif MYNEWT_VAL(MCU_MKM34Z7)
#include <MKM34Z7.h>
#elif MYNEWT_VAL(MCU_MKM34ZA5)
#include <MKM34ZA5.h>
#elif MYNEWT_VAL(MCU_MKM35Z7)
#include <MKM35Z7.h>
#elif MYNEWT_VAL(MCU_MKV10Z1287)
#include <MKV10Z1287.h>
#elif MYNEWT_VAL(MCU_MKV10Z7)
#include <MKV10Z7.h>
#elif MYNEWT_VAL(MCU_MKV11Z7)
#include <MKV11Z7.h>
#elif MYNEWT_VAL(MCU_MKV30F12810)
#include <MKV30F12810.h>
#elif MYNEWT_VAL(MCU_MKV31F12810)
#include <MKV31F12810.h>
#elif MYNEWT_VAL(MCU_MKV31F25612)
#include <MKV31F25612.h>
#elif MYNEWT_VAL(MCU_MKV31F51212)
#include <MKV31F51212.h>
#elif MYNEWT_VAL(MCU_MKV56F24)
#include <MKV56F24.h>
#elif MYNEWT_VAL(MCU_MKV58F24)
#include <MKV58F24.h>
#elif MYNEWT_VAL(MCU_MKW20Z4)
#include <MKW20Z4.h>
#elif MYNEWT_VAL(MCU_MKW21Z4)
#include <MKW21Z4.h>
#elif MYNEWT_VAL(MCU_MKW22D5)
#include <MKW22D5.h>
#elif MYNEWT_VAL(MCU_MKW24D5)
#include <MKW24D5.h>
#elif MYNEWT_VAL(MCU_MKW30Z4)
#include <MKW30Z4.h>
#elif MYNEWT_VAL(MCU_MKW31Z4)
#include <MKW31Z4.h>
#elif MYNEWT_VAL(MCU_MKW40Z4)
#include <MKW40Z4.h>
#elif MYNEWT_VAL(MCU_MKW41Z4)
#include <MKW41Z4.h>
#else
#error "Unsupported MCU"
#endif

#ifdef __cplusplus
}
#endif

#endif /* __MCUX_COMMON_H_ */

