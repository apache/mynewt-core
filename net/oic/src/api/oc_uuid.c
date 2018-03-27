/*
// Copyright (c) 2016 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "os/mynewt.h"

#include <hal/hal_bsp.h>

#include "oic/oc_uuid.h"

void
oc_str_to_uuid(const char *str, oc_uuid_t *uuid)
{
  int i, j = 0, k = 1;
  uint8_t c = 0;

  for (i = 0; i < strlen(str); i++) {
    if (str[i] == '-')
      continue;
    else if (isalpha((int)str[i])) {
      switch (str[i]) {
      case 65:
      case 97:
        c |= 0x0a;
        break;
      case 66:
      case 98:
        c |= 0x0b;
        break;
      case 67:
      case 99:
        c |= 0x0c;
        break;
      case 68:
      case 100:
        c |= 0x0d;
        break;
      case 69:
      case 101:
        c |= 0x0e;
        break;
      case 70:
      case 102:
        c |= 0x0f;
        break;
      }
    } else
      c |= str[i] - 48;
    if ((j + 1) * 2 == k) {
      uuid->id[j++] = c;
      c = 0;
    } else
      c = c << 4;
    k++;
  }
}

void
oc_uuid_to_str(const oc_uuid_t *uuid, char *buffer, int buflen)
{
  int i, j = 0;
  if (buflen < 37)
    return;
  for (i = 0; i < 16; i++) {
    switch (i) {
    case 4:
    case 6:
    case 8:
    case 10:
      snprintf(&buffer[j], 2, "-");
      j++;
      break;
    }
    snprintf(&buffer[j], 3, "%02x", uuid->id[i]);
    j += 2;
  }
}

void
oc_gen_uuid(oc_uuid_t *uuid)
{
  hal_bsp_hw_id(&uuid->id[0], sizeof(uuid->id));

  /*  From RFC 4122
      Set the two most significant bits of the
      clock_seq_hi_and_reserved (8th octet) to
      zero and one, respectively.
  */
  uuid->id[8] &= 0x3f;
  uuid->id[8] |= 0x40;

  /*  From RFC 4122
      Set the four most significant bits of the
      time_hi_and_version field (6th octect) to the
      4-bit version number from (0 1 0 0 => type 4)
      Section 4.1.3.
  */
  uuid->id[6] &= 0x0f;
  uuid->id[6] |= 0x40;
}
