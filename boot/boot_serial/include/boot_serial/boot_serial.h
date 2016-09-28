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

#ifndef __BOOT_SERIAL_H__
#define __BOOT_SERIAL_H__

/*
 * Create a task for uploading image0 over serial.
 *
 * Task opens console serial port and waits for download command.
 * Return code 0 means new image was uploaded, non-zero means that
 * there was an error.
 */
int boot_serial_task_init(struct os_task *task, uint8_t prio,
  os_stack_t *stack, uint16_t stack_size, int max_input);

#endif /*  __BOOT_SERIAL_H__ */
