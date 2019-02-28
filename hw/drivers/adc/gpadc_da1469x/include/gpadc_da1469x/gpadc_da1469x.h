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

#ifndef __GPADC_DA1469x__
#define __GPADC_DA1469x__

#include <adc/adc.h>

#ifdef __cplusplus
extern "C" {
#endif

struct da1469x_gpadc_dev {
    struct adc_dev dgd_adc;
    uint16_t *dgd_buf[2];
    int dgd_buf_idx;
    int dgd_buf_len;
    struct da1469x_gpadc_init_cfg *dgd_init_cfg;
    struct adc_chan_config dgd_adc_chan;
};

struct da1469x_gpadc_init_cfg {
    uint8_t dgic_adc_clk_div:1;
};

struct da1469x_gpadc_dev_cfg {
    uint32_t dgdc_gpadc_ctrl;
    uint32_t dgdc_gpadc_ctrl2;
    uint32_t dgdc_gpadc_ctrl3;
    uint8_t dgdc_gpadc_set_offp:1;
    uint8_t dgdc_gpadc_set_offn:1;
    uint32_t dgdc_gpadc_offp;
    uint32_t dgdc_gpadc_offn;
};

struct da1469x_gpadc_chan_cfg {
};

/**
 * Device initialization routine called by OS.
 *
 * @param dev Pointer to device being initialized.
 * @param arg NA
 *
 * @return 0 on success, non-zero when fail.
 */
int da1469x_gpadc_init(struct os_dev *, void *);

#ifdef __cplusplus
}
#endif


#endif /* __GPADC_DA1469x__ */

