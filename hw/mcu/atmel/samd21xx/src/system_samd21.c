/**
 * Copyright (c) 2015 Runtime Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "samd21.h"
#include "system.h"

void hal_system_init(void)
{
    /* Change default QOS values to have the best performance and correct USB behaviour */
    SBMATRIX->SFR[SBMATRIX_SLAVE_HMCRAMC0].reg = 2;
#if defined(ID_USB)
    USB->DEVICE.QOSCTRL.bit.CQOS = 2;
    USB->DEVICE.QOSCTRL.bit.DQOS = 2;
#endif
    DMAC->QOSCTRL.bit.DQOS = 2;
    DMAC->QOSCTRL.bit.FQOS = 2;
    DMAC->QOSCTRL.bit.WRBQOS = 2;

    /* Overwriting the default value of the NVMCTRL.CTRLB.MANW bit (errata reference 13134) */
    NVMCTRL->CTRLB.bit.MANW = 1;

    /* Configure GCLK and clock sources according to conf_clocks.h */
    system_init();
}
