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

#include "os/mynewt.h"
#include "console/console.h"
#include "bus/bus.h"
#include "bus/drivers/i2c_common.h"
#include "lis2dh_node/lis2dh_node.h"

static struct bus_i2c_node g_lis2dh_node;

struct reg_val {
    uint8_t addr;
    uint8_t val[32];
};

static void
open_node_cb(struct bus_node *node)
{
    struct os_dev *odev = (struct os_dev *)node;
    struct reg_val reg;
    int rc;

    console_printf("%s: node %p\n", __func__, node);

    reg.addr = 0x0f; /* WHO_AM_I */
    rc = bus_node_simple_write_read_transact(odev, &reg.addr, 1, reg.val, 1);
    assert(rc == 0);
    assert(reg.val[0] == 0x33);

    reg.addr = 0x20; /* CTRL_REG1 */
    reg.val[0] = 0x37;
    rc = bus_node_simple_write(odev, &reg.addr, 2);
    assert(rc == 0);
}

static void
close_node_cb(struct bus_node *node)
{
    console_printf("%s: node %p\n", __func__, node);
}

int
lis2dh_node_read(struct os_dev *node, struct lis2dh_node_pos *pos)
{
    struct reg_val reg;
    int rc;

    assert(pos);

    reg.addr = 0x80 | 0x28; /* OUT_X_L */
    rc = bus_node_simple_write_read_transact(node, &reg.addr, 1, reg.val, 6);
    assert(rc == 0);

    pos->x = get_le16(&reg.val[0]);
    pos->y = get_le16(&reg.val[2]);
    pos->z = get_le16(&reg.val[4]);

    return 0;
}

int
lis2dh_node_i2c_create(const char *name, const struct bus_i2c_node_cfg *cfg)
{
    struct bus_node_callbacks cbs = {
        .init = NULL,
        .open = open_node_cb,
        .close = close_node_cb,
    };
    int rc;

    bus_node_set_callbacks((struct os_dev *)&g_lis2dh_node, &cbs);

    rc = bus_i2c_node_create(name, &g_lis2dh_node,
                             (struct bus_i2c_node_cfg *)cfg, NULL);

    return rc;
}
