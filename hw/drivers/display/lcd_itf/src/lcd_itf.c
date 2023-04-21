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

#include <os/os_time.h>
#include <os/os_cputime.h>
#include <bsp/bsp.h>
#include <lcd_itf.h>
#include <hal/hal_gpio.h>

static void
lcd_delay(uint32_t delay_us)
{
    if (os_time_ticks_to_ms32(2) * 1000 < delay_us) {
        os_time_delay(os_time_ms_to_ticks32(delay_us / 1000));
    } else {
        os_cputime_delay_usecs(delay_us);
    }
}

void
lcd_command_sequence(const uint8_t *cmds)
{
    const uint8_t *cmd;
    int cmd_length;

    /* Send all the commands */
    for (cmd = cmds; cmd[0] != 0xFF; ) {
        if (cmd[0] == LCD_SEQUENCE_DELAY_REQ) {
            lcd_delay((cmd[1] | (cmd[2] << 8)) * 1000);
            cmd += 3;
        } else if (cmd[0] == LCD_SEQUENCE_DELAY_US_REQ) {
            lcd_delay(cmd[1] | (cmd[2] << 8));
            cmd += 3;
        } else if (cmd[0] == LCD_SEQUENCE_GPIO_REQ) {
            hal_gpio_write(cmd[1], cmd[2]);
            cmd += 3;
        } else if (cmd[0] == LCD_SEQUENCE_LCD_RESET_ACTIVATE_REQ) {
            LCD_RESET_PIN_ACTIVE();
            ++cmd;
        } else if (cmd[0] == LCD_SEQUENCE_LCD_RESET_INACTIVATE_REQ) {
            LCD_RESET_PIN_INACTIVE();
            ++cmd;
        } else if (cmd[0] == LCD_SEQUENCE_LCD_CS_ACTIVATE_REQ) {
            LCD_CS_PIN_ACTIVE();
            ++cmd;
        } else if (cmd[0] == LCD_SEQUENCE_LCD_CS_INACTIVATE_REQ) {
            LCD_CS_PIN_INACTIVE();
            ++cmd;
        } else if (cmd[0] == LCD_SEQUENCE_LCD_DC_DATA_REQ) {
            LCD_DC_PIN_DATA();
            ++cmd;
        } else if (cmd[0] == LCD_SEQUENCE_LCD_DC_COMMAND_REQ) {
            LCD_DC_PIN_COMMAND();
            ++cmd;
        } else {
            cmd_length = cmd[0];
            lcd_ift_write_cmd(cmd + 1, cmd_length);
            cmd += cmd_length + 1;
        }
    }
}
