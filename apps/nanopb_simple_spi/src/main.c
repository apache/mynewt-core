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

#include <console/console.h>
#include <assert.h>
#include <pb_decode.h>
#include <pb_encode.h>
#include "simple.pb.h"
#include "os/mynewt.h"
#include "bsp/bsp.h"
#include "hal/hal_gpio.h"
#include "hal/hal_spi.h"
#include "stats/stats.h"
#include "config/config.h"
#ifdef ARCH_sim
#include <mcu/mcu_sim.h>
#endif

/* Task 1 */
#define TASK1_PRIO       (1)
#define TASK1_STACK_SIZE OS_STACK_ALIGN(1024)
struct os_task task1;

/* For LED toggling */
int g_led_pin;

#define SPI_BAUDRATE      500
#define MESSAGE_BUFF_SIZE 5

#if MYNEWT_VAL(SPI_0_MASTER) || MYNEWT_VAL(SPI_1_MASTER) || MYNEWT_VAL(SPI_2_MASTER)
#define SPI_MASTER 1
#define SPI_SS_PIN (MYNEWT_VAL(PB_SPI_SS_PIN))
#if SPI_SS_PIN < 0
#error "PB_SPI_SS_PIN must be set in the target config."
#endif
#define SPI_M_NUM (MYNEWT_VAL(PB_SPI_M_NUM))
#endif

#if MYNEWT_VAL(SPI_0_SLAVE) || MYNEWT_VAL(SPI_1_SLAVE) || MYNEWT_VAL(SPI_2_SLAVE)
#define SPI_SLAVE 1
#define SPI_S_NUM (MYNEWT_VAL(PB_SPI_S_NUM))
#endif

#if defined(SPI_MASTER) && defined(SPI_SLAVE)
#if SPI_M_NUM == SPI_S_NUM
#error "SPI_M_NUM and SPI_S_NUM cannot be the same."
#endif
#endif

#ifdef SPI_MASTER
void
spi_irqm_handler(void *arg, int len)
{
    hal_gpio_write(SPI_SS_PIN, 1);
}

void
spim_cfg(int spi_num)
{
    struct hal_spi_settings my_spi;

    my_spi.data_order = HAL_SPI_MSB_FIRST;
    my_spi.data_mode = HAL_SPI_MODE0;
    my_spi.baudrate = SPI_BAUDRATE;
    my_spi.word_size = HAL_SPI_WORD_SIZE_8BIT;
    assert(hal_spi_config(spi_num, &my_spi) == 0);
}
#endif

#ifdef SPI_SLAVE
struct os_sem g_spi_sem;
int32_t g_lucky_number;
uint8_t g_spi_rx_buf[MESSAGE_BUFF_SIZE];

void
spi_irqs_handler(void *arg, int len)
{
    /* Allocate space for the decoded message. */
    SimpleMessage message = SimpleMessage_init_zero;
    pb_istream_t stream;
    bool status;

    /* Create a stream that reads from the buffer. */
    stream = pb_istream_from_buffer(g_spi_rx_buf, (size_t)len);
    /* Now we are ready to decode the message. */
    status = pb_decode(&stream, SimpleMessage_fields, &message);
    /* Check for errors... */
    assert(status);

    /* Save the data contained in the message. */
    g_lucky_number = message.lucky_number;

    os_sem_release(&g_spi_sem);
}

void
spis_cfg(int spi_num)
{
    struct hal_spi_settings my_spi;

    my_spi.data_order = HAL_SPI_MSB_FIRST;
    my_spi.data_mode = HAL_SPI_MODE0;
    my_spi.baudrate = SPI_BAUDRATE;
    my_spi.word_size = HAL_SPI_WORD_SIZE_8BIT;
    assert(hal_spi_config(spi_num, &my_spi) == 0);

    hal_spi_set_txrx_cb(spi_num, spi_irqs_handler, NULL);
}
#endif

#ifdef SPI_MASTER
void
spim_task_handler(void *arg)
{
    /* Allocate space on the stack to store the message data.
     *
     * Nanopb generates simple struct definitions for all the messages.
     * - check out the contents of simple.pb.h!
     * It is a good idea to always initialize your structures
     * so that you do not have garbage data from RAM in there.
     */
    SimpleMessage message = SimpleMessage_init_zero;
    /* This is the buffer where we will store our message. */
    uint8_t buffer[MESSAGE_BUFF_SIZE];
    pb_ostream_t stream;
    bool status;
    int rc;

    /* Initialize the lucky number */
    message.lucky_number = 0;

    /* Set the led pin */
    g_led_pin = LED_BLINK_PIN;
    hal_gpio_init_out(g_led_pin, 1);

    /* Configure SS pin */
    hal_gpio_init_out(SPI_SS_PIN, 1);
    spim_cfg(SPI_M_NUM);

    /* Set up the callback to use when non-blocking API used */
    hal_spi_set_txrx_cb(SPI_M_NUM, spi_irqm_handler, NULL);
    hal_spi_enable(SPI_M_NUM);

    while (1) {
        /* Send non-blocking */
        hal_gpio_write(SPI_SS_PIN, 0);
        /* Create a stream that will write to our buffer. */
        stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
        /* Increment the lucky number */
        message.lucky_number++;
        /* Now we are ready to encode the message! */
        status = pb_encode(&stream, SimpleMessage_fields, &message);
        /* Then just check for any errors... */
        assert(status);

        rc = hal_spi_txrx_noblock(SPI_M_NUM, buffer, NULL, (int)stream.bytes_written);
        assert(!rc);

        /* Wait one second */
        os_time_delay(OS_TICKS_PER_SEC);

        /* Toggle the LED */
        hal_gpio_toggle(g_led_pin);
    }
}
#endif

#ifdef SPI_SLAVE

void
spis_task_handler(void *arg)
{
    /* This is the buffer where we will store our message. */
    int rc;

    g_led_pin = LED_BLINK_PIN;
    hal_gpio_init_out(g_led_pin, 1);

    spis_cfg(SPI_S_NUM);
    hal_spi_enable(SPI_S_NUM);

    /* Make the default character 0x77 */
    hal_spi_slave_set_def_tx_val(SPI_S_NUM, 0x77);

    while (1) {
        rc = hal_spi_txrx_noblock(SPI_S_NUM, NULL, g_spi_rx_buf, MESSAGE_BUFF_SIZE);
        assert(rc == 0);

        os_sem_pend(&g_spi_sem, OS_TIMEOUT_NEVER);

        console_printf("Lucky number: %ld\n", g_lucky_number);

        /* Toggle the LED */
        hal_gpio_toggle(g_led_pin);
    }
}
#endif

/**
 * init_tasks
 *
 * Called by main.c after sysinit(). This function performs initializations
 * that are required before tasks are running.
 *
 * @return int 0 success; error otherwise.
 */
static void
init_tasks(void)
{
    os_stack_t *pstack;

    (void)pstack;

#if defined(SPI_MASTER)
    pstack = malloc(sizeof(os_stack_t) * TASK1_STACK_SIZE);
    assert(pstack);

    os_task_init(&task1, "spim", spim_task_handler, NULL, TASK1_PRIO,
                 OS_WAIT_FOREVER, pstack, TASK1_STACK_SIZE);
#endif

#if defined(SPI_SLAVE)
    /* Initialize semaphore */
    os_sem_init(&g_spi_sem, 0);

    pstack = malloc(sizeof(os_stack_t) * TASK1_STACK_SIZE);
    assert(pstack);

    os_task_init(&task1, "spis", spis_task_handler, NULL, TASK1_PRIO,
                 OS_WAIT_FOREVER, pstack, TASK1_STACK_SIZE);
#endif
}

/**
 * main
 *
 * The main task for the project. This function initializes the packages, calls
 * init_tasks to initialize additional tasks (and possibly other objects),
 * then starts serving events from default event queue.
 *
 * @return int NOTE: this function should never return!
 */
int
mynewt_main(int argc, char **argv)
{
    int rc;

    sysinit();
    init_tasks();

    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }
    /* Never returns */
    assert(0);

    return rc;
}
