/**
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

#ifndef H_BLE_SVC_DIS_
#define H_BLE_SVC_DIS_

#define BLE_SVC_DIS_UUID16					0x180A
#define BLE_SVC_DIS_CHR_UUID16_MODEL_NUMBER			0x2A24
#define BLE_SVC_DIS_CHR_UUID16_SERIAL_NUMBER			0x2A25
#define BLE_SVC_DIS_CHR_UUID16_FIRMWARE_REVISION 		0x2A26
#define BLE_SVC_DIS_CHR_UUID16_HARDWARE_REVISION 		0x2A27
#define BLE_SVC_DIS_CHR_UUID16_SOFTWARE_REVISION 		0x2A28
#define BLE_SVC_DIS_CHR_UUID16_MANUFACTURER_NAME		0x2A29


struct ble_svc_dis_data {
    char *model_number;
    char *serial_number;
    char *firmware_revision; 
    char *hardware_revision;
    char *software_revision;
    char *manufacturer_name;
};

extern struct ble_svc_dis_data ble_svc_dis_data;


void ble_svc_dis_init(void);

#endif
