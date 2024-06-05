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

/*
 * Memory regions placed in CCM
 * If stack or core data or other section should be place in RAM
 * <target_name>/link/include/target_config.ld.h should just do:
 *  #undef BSSNZ_RAM
 *  #undef COREBSS_RAM
 *  #undef COREDATA_RAM
 *  #undef STACK_REGION
 */

#define BSSNZ_RAM CCM
#define COREBSS_RAM CCM
#define COREDATA_RAM CCM
#define STACK_REGION CCM
