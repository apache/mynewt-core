/*
 * Copyright (c) 2015, Freescale Semiconductor, Inc.
 * Copyright 2016-2020 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "fsl_qspi_edma.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/* Component ID definition, used by tools. */
#ifndef FSL_COMPONENT_ID
#define FSL_COMPONENT_ID "platform.drivers.qspi_edma"
#endif

/*<! Structure definition for qspi_edma_private_handle_t. The structure is private. */
typedef struct _qspi_edma_private_handle
{
    QuadSPI_Type *base;
    qspi_edma_handle_t *handle;
} qspi_edma_private_handle_t;

/* QSPI EDMA transfer handle. */
enum _qspi_edma_tansfer_states
{
    kQSPI_Idle,   /* TX idle. */
    kQSPI_BusBusy /* RX busy. */
};

/*!
 * @brief Used for conversion between `void*` and `uint32_t`.
 */
typedef union pvoid_to_u32
{
    void *pvoid;
    uint32_t u32;
} pvoid_to_u32_t;

/*******************************************************************************
 * Variables
 ******************************************************************************/

/*<! Private handle only used for internally. */
static qspi_edma_private_handle_t s_edmaPrivateHandle[FSL_FEATURE_SOC_QuadSPI_COUNT][2];

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*!
 * @brief QSPI EDMA send finished callback function.
 *
 * This function is called when QSPI EDMA send finished. It disables the QSPI
 * TX EDMA request and sends @ref kStatus_QSPI_TxIdle to QSPI callback.
 *
 * @param handle The EDMA handle.
 * @param param Callback function parameter.
 */
static void QSPI_SendEDMACallback(edma_handle_t *handle, void *param, bool transferDone, uint32_t tcds);

/*!
 * @brief QSPI EDMA receive finished callback function.
 *
 * This function is called when QSPI EDMA receive finished. It disables the QSPI
 * RX EDMA request and sends @ref kStatus_QSPI_RxIdle to QSPI callback.
 *
 * @param handle The EDMA handle.
 * @param param Callback function parameter.
 */
static void QSPI_ReceiveEDMACallback(edma_handle_t *handle, void *param, bool transferDone, uint32_t tcds);

/*******************************************************************************
 * Code
 ******************************************************************************/

static void QSPI_SendEDMACallback(edma_handle_t *handle, void *param, bool transferDone, uint32_t tcds)
{
    qspi_edma_private_handle_t *qspiPrivateHandle = (qspi_edma_private_handle_t *)param;

    /* Avoid the warning for unused variables. */
    handle = handle;
    tcds   = tcds;

    if (transferDone)
    {
        QSPI_TransferAbortSendEDMA(qspiPrivateHandle->base, qspiPrivateHandle->handle);

        if (NULL != qspiPrivateHandle->handle->callback)
        {
            qspiPrivateHandle->handle->callback(qspiPrivateHandle->base, qspiPrivateHandle->handle, kStatus_QSPI_Idle,
                                                qspiPrivateHandle->handle->userData);
        }
    }
}

static void QSPI_ReceiveEDMACallback(edma_handle_t *handle, void *param, bool transferDone, uint32_t tcds)
{
    qspi_edma_private_handle_t *qspiPrivateHandle = (qspi_edma_private_handle_t *)param;

    /* Avoid warning for unused parameters. */
    handle = handle;
    tcds   = tcds;

    if (transferDone)
    {
        /* Disable transfer. */
        QSPI_TransferAbortReceiveEDMA(qspiPrivateHandle->base, qspiPrivateHandle->handle);

        if (NULL != qspiPrivateHandle->handle->callback)
        {
            qspiPrivateHandle->handle->callback(qspiPrivateHandle->base, qspiPrivateHandle->handle, kStatus_QSPI_Idle,
                                                qspiPrivateHandle->handle->userData);
        }
    }
}

/*!
 * brief Initializes the QSPI handle for send which is used in transactional functions and set the callback.
 *
 * param base QSPI peripheral base address
 * param handle Pointer to qspi_edma_handle_t structure
 * param callback QSPI callback, NULL means no callback.
 * param userData User callback function data.
 * param rxDmaHandle User requested eDMA handle for eDMA transfer
 */
void QSPI_TransferTxCreateHandleEDMA(QuadSPI_Type *base,
                                     qspi_edma_handle_t *handle,
                                     qspi_edma_callback_t callback,
                                     void *userData,
                                     edma_handle_t *dmaHandle)
{
    assert(handle);

    uint32_t instance = QSPI_GetInstance(base);

    s_edmaPrivateHandle[instance][0].base   = base;
    s_edmaPrivateHandle[instance][0].handle = handle;

    (void)memset(handle, 0, sizeof(*handle));

    handle->state     = (uint32_t)kQSPI_Idle;
    handle->dmaHandle = dmaHandle;

    handle->callback = callback;
    handle->userData = userData;

    /* Get the watermark value */
    handle->count = (uint8_t)base->TBCT + 1U;

    /* Configure TX edma callback */
    EDMA_SetCallback(handle->dmaHandle, QSPI_SendEDMACallback, &s_edmaPrivateHandle[instance][0]);
}

/*!
 * brief Initializes the QSPI handle for receive which is used in transactional functions and set the callback.
 *
 * param base QSPI peripheral base address
 * param handle Pointer to qspi_edma_handle_t structure
 * param callback QSPI callback, NULL means no callback.
 * param userData User callback function data.
 * param rxDmaHandle User requested eDMA handle for eDMA transfer
 */
void QSPI_TransferRxCreateHandleEDMA(QuadSPI_Type *base,
                                     qspi_edma_handle_t *handle,
                                     qspi_edma_callback_t callback,
                                     void *userData,
                                     edma_handle_t *dmaHandle)
{
    assert(handle);

    uint32_t instance = QSPI_GetInstance(base);

    s_edmaPrivateHandle[instance][1].base   = base;
    s_edmaPrivateHandle[instance][1].handle = handle;

    (void)memset(handle, 0, sizeof(*handle));

    handle->state     = (uint32_t)kQSPI_Idle;
    handle->dmaHandle = dmaHandle;

    handle->callback = callback;
    handle->userData = userData;

    /* Get the watermark value */
    handle->count = ((uint8_t)base->RBCT & QuadSPI_RBCT_WMRK_MASK) + 1U;

    /* Configure RX edma callback */
    EDMA_SetCallback(handle->dmaHandle, QSPI_ReceiveEDMACallback, &s_edmaPrivateHandle[instance][1]);
}

/*!
 * brief Transfers QSPI data using an eDMA non-blocking method.
 *
 * This function writes data to the QSPI transmit FIFO. This function is non-blocking.
 * param base Pointer to QuadSPI Type.
 * param handle Pointer to qspi_edma_handle_t structure
 * param xfer QSPI transfer structure.
 */
status_t QSPI_TransferSendEDMA(QuadSPI_Type *base, qspi_edma_handle_t *handle, qspi_transfer_t *xfer)
{
    assert(handle && (handle->dmaHandle));

    edma_transfer_config_t xferConfig;
    status_t status;
    pvoid_to_u32_t destAddr;

    /* If previous TX not finished. */
    if ((uint8_t)kQSPI_BusBusy == handle->state)
    {
        status = kStatus_QSPI_Busy;
    }
    else
    {
        handle->state = (uint32_t)kQSPI_BusBusy;

        destAddr.u32 = QSPI_GetTxDataRegisterAddress(base);
        /* Prepare transfer. */
        EDMA_PrepareTransfer(&xferConfig, xfer->data, sizeof(uint32_t), destAddr.pvoid, sizeof(uint32_t),
                             (sizeof(uint32_t) * (uint32_t)handle->count), xfer->dataSize, kEDMA_MemoryToPeripheral);

        /* Store the initially configured eDMA minor byte transfer count into the QSPI handle */
        handle->nbytes = (sizeof(uint32_t) * handle->count);

        /* Submit transfer. */
        do
        {
            status = EDMA_SubmitTransfer(handle->dmaHandle, &xferConfig);
        } while (status != kStatus_Success);

        EDMA_StartTransfer(handle->dmaHandle);

        /* Enable QSPI TX EDMA. */
        QSPI_EnableDMA(base, (uint32_t)kQSPI_TxBufferFillDMAEnable, true);

        status = kStatus_Success;
    }

    return status;
}

/*!
 * brief Receives data using an eDMA non-blocking method.
 *
 * This function receive data from the QSPI receive buffer/FIFO. This function is non-blocking. Users shall notice that
 * this receive size shall not bigger than 64 bytes. As this interface is used to read flash status registers.
 * For flash contents read, please use AHB bus read, this is much more efficiency.
 *
 * param base Pointer to QuadSPI Type.
 * param handle Pointer to qspi_edma_handle_t structure
 * param xfer QSPI transfer structure.
 */
status_t QSPI_TransferReceiveEDMA(QuadSPI_Type *base, qspi_edma_handle_t *handle, qspi_transfer_t *xfer)
{
    assert(handle && (handle->dmaHandle));

    edma_transfer_config_t xferConfig;
    status_t status;
    pvoid_to_u32_t srcAddr;

    /* If previous TX not finished. */
    if ((uint32_t)kQSPI_BusBusy == handle->state)
    {
        status = kStatus_QSPI_Busy;
    }
    else
    {
        handle->state = (uint32_t)kQSPI_BusBusy;

        srcAddr.u32 = QSPI_GetRxDataRegisterAddress(base);
        /* Prepare transfer. */
        EDMA_PrepareTransfer(&xferConfig, srcAddr.pvoid, sizeof(uint32_t), xfer->data, sizeof(uint32_t),
                             (sizeof(uint32_t) * (uint32_t)handle->count), xfer->dataSize, kEDMA_MemoryToMemory);

        /* Store the initially configured eDMA minor byte transfer count into the QSPI handle */
        handle->nbytes = (sizeof(uint32_t) * handle->count);
        /* Submit transfer. */
        do
        {
            status = EDMA_SubmitTransfer(handle->dmaHandle, &xferConfig);
        } while (status != kStatus_Success);

        handle->dmaHandle->base->TCD[handle->dmaHandle->channel].ATTR |= DMA_ATTR_SMOD(0x5U);
        EDMA_StartTransfer(handle->dmaHandle);

        /* Enable QSPI TX EDMA. */
        QSPI_EnableDMA(base, (uint32_t)kQSPI_RxBufferDrainDMAEnable, true);

        status = kStatus_Success;
    }

    return status;
}

/*!
 * brief Aborts the sent data using eDMA.
 *
 * This function aborts the sent data using eDMA.
 *
 * param base QSPI peripheral base address.
 * param handle Pointer to qspi_edma_handle_t structure
 */
void QSPI_TransferAbortSendEDMA(QuadSPI_Type *base, qspi_edma_handle_t *handle)
{
    assert(handle && (handle->dmaHandle));

    /* Disable QSPI TX EDMA. */
    QSPI_EnableDMA(base, (uint32_t)kQSPI_TxBufferFillDMAEnable, false);

    /* Stop transfer. */
    EDMA_AbortTransfer(handle->dmaHandle);

    handle->state = (uint32_t)kQSPI_Idle;
}

/*!
 * brief Aborts the receive data using eDMA.
 *
 * This function abort receive data which using eDMA.
 *
 * param base QSPI peripheral base address.
 * param handle Pointer to qspi_edma_handle_t structure
 */
void QSPI_TransferAbortReceiveEDMA(QuadSPI_Type *base, qspi_edma_handle_t *handle)
{
    assert(handle && (handle->dmaHandle));

    /* Disable QSPI RX EDMA. */
    QSPI_EnableDMA(base, (uint32_t)kQSPI_RxBufferDrainDMAEnable, false);

    /* Stop transfer. */
    EDMA_AbortTransfer(handle->dmaHandle);

    handle->state = (uint32_t)kQSPI_Idle;
}

/*!
 * brief Gets the transferred counts of send.
 *
 * param base Pointer to QuadSPI Type.
 * param handle Pointer to qspi_edma_handle_t structure.
 * param count Bytes sent.
 * retval kStatus_Success Succeed get the transfer count.
 * retval kStatus_NoTransferInProgress There is not a non-blocking transaction currently in progress.
 */
status_t QSPI_TransferGetSendCountEDMA(QuadSPI_Type *base, qspi_edma_handle_t *handle, size_t *count)
{
    assert(handle);

    status_t status = kStatus_Success;

    if (handle->state != (uint32_t)kQSPI_BusBusy)
    {
        status = kStatus_NoTransferInProgress;
    }
    else
    {
        *count = (handle->transferSize -
                  (uint32_t)handle->nbytes *
                      EDMA_GetRemainingMajorLoopCount(handle->dmaHandle->base, handle->dmaHandle->channel));
    }

    return status;
}

/*!
 * brief Gets the status of the receive transfer.
 *
 * param base Pointer to QuadSPI Type.
 * param handle Pointer to qspi_edma_handle_t structure
 * param count Bytes received.
 * retval kStatus_Success Succeed get the transfer count.
 * retval kStatus_NoTransferInProgress There is not a non-blocking transaction currently in progress.
 */
status_t QSPI_TransferGetReceiveCountEDMA(QuadSPI_Type *base, qspi_edma_handle_t *handle, size_t *count)
{
    assert(handle);

    status_t status = kStatus_Success;

    if (handle->state != (uint32_t)kQSPI_BusBusy)
    {
        status = kStatus_NoTransferInProgress;
    }
    else
    {
        *count = (handle->transferSize -
                  (uint32_t)handle->nbytes *
                      EDMA_GetRemainingMajorLoopCount(handle->dmaHandle->base, handle->dmaHandle->channel));
    }

    return status;
}
