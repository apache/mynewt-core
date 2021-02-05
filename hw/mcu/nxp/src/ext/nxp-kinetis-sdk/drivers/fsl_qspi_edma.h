/*
 * Copyright (c) 2015, Freescale Semiconductor, Inc.
 * Copyright 2016-2020 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _FSL_QSPI_EDMA_H_
#define _FSL_QSPI_EDMA_H_

#include "fsl_qspi.h"
#include "fsl_edma.h"

/*!
 * @addtogroup qspi_edma_driver
 * @{
 */

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/*! @name Driver version */
/*@{*/
/*! @brief QSPI EDMA driver version 2.2.1. */
#define FSL_QSPI_EDMA_DRIVER_VERSION (MAKE_VERSION(2, 2, 1))
/*@}*/

typedef struct _qspi_edma_handle qspi_edma_handle_t;

/*! @brief QSPI eDMA transfer callback function for finish and error */
typedef void (*qspi_edma_callback_t)(QuadSPI_Type *base, qspi_edma_handle_t *handle, status_t status, void *userData);

/*! @brief QSPI DMA transfer handle, users should not touch the content of the handle.*/
struct _qspi_edma_handle
{
    edma_handle_t *dmaHandle;      /*!< eDMA handler for QSPI send */
    size_t transferSize;           /*!< Bytes need to transfer. */
    uint8_t nbytes;                /*!< eDMA minor byte transfer count initially configured. */
    uint8_t count;                 /*!< The transfer data count in a DMA request */
    uint32_t state;                /*!< Internal state for QSPI eDMA transfer */
    qspi_edma_callback_t callback; /*!< Callback for users while transfer finish or error occurred */
    void *userData;                /*!< User callback parameter */
};

/*******************************************************************************
 * APIs
 ******************************************************************************/
#if defined(__cplusplus)
extern "C" {
#endif

/*!
 * @name eDMA Transactional
 * @{
 */

/*!
 * @brief Initializes the QSPI handle for send which is used in transactional functions and set the callback.
 *
 * @param base QSPI peripheral base address
 * @param handle Pointer to qspi_edma_handle_t structure
 * @param callback QSPI callback, NULL means no callback.
 * @param userData User callback function data.
 * @param dmaHandle User requested eDMA handle for eDMA transfer
 */
void QSPI_TransferTxCreateHandleEDMA(QuadSPI_Type *base,
                                     qspi_edma_handle_t *handle,
                                     qspi_edma_callback_t callback,
                                     void *userData,
                                     edma_handle_t *dmaHandle);

/*!
 * @brief Initializes the QSPI handle for receive which is used in transactional functions and set the callback.
 *
 * @param base QSPI peripheral base address
 * @param handle Pointer to qspi_edma_handle_t structure
 * @param callback QSPI callback, NULL means no callback.
 * @param userData User callback function data.
 * @param dmaHandle User requested eDMA handle for eDMA transfer
 */
void QSPI_TransferRxCreateHandleEDMA(QuadSPI_Type *base,
                                     qspi_edma_handle_t *handle,
                                     qspi_edma_callback_t callback,
                                     void *userData,
                                     edma_handle_t *dmaHandle);

/*!
 * @brief Transfers QSPI data using an eDMA non-blocking method.
 *
 * This function writes data to the QSPI transmit FIFO. This function is non-blocking.
 * @param base Pointer to QuadSPI Type.
 * @param handle Pointer to qspi_edma_handle_t structure
 * @param xfer QSPI transfer structure.
 */
status_t QSPI_TransferSendEDMA(QuadSPI_Type *base, qspi_edma_handle_t *handle, qspi_transfer_t *xfer);

/*!
 * @brief Receives data using an eDMA non-blocking method.
 *
 * This function receive data from the QSPI receive buffer/FIFO. This function is non-blocking. Users shall notice that
 * this receive size shall not bigger than 64 bytes. As this interface is used to read flash status registers.
 * For flash contents read, please use AHB bus read, this is much more efficiency.
 *
 * @param base Pointer to QuadSPI Type.
 * @param handle Pointer to qspi_edma_handle_t structure
 * @param xfer QSPI transfer structure.
 */
status_t QSPI_TransferReceiveEDMA(QuadSPI_Type *base, qspi_edma_handle_t *handle, qspi_transfer_t *xfer);

/*!
 * @brief Aborts the sent data using eDMA.
 *
 * This function aborts the sent data using eDMA.
 *
 * @param base QSPI peripheral base address.
 * @param handle Pointer to qspi_edma_handle_t structure
 */
void QSPI_TransferAbortSendEDMA(QuadSPI_Type *base, qspi_edma_handle_t *handle);

/*!
 * @brief Aborts the receive data using eDMA.
 *
 * This function abort receive data which using eDMA.
 *
 * @param base QSPI peripheral base address.
 * @param handle Pointer to qspi_edma_handle_t structure
 */
void QSPI_TransferAbortReceiveEDMA(QuadSPI_Type *base, qspi_edma_handle_t *handle);

/*!
 * @brief Gets the transferred counts of send.
 *
 * @param base Pointer to QuadSPI Type.
 * @param handle Pointer to qspi_edma_handle_t structure.
 * @param count Bytes sent.
 * @retval kStatus_Success Succeed get the transfer count.
 * @retval kStatus_NoTransferInProgress There is not a non-blocking transaction currently in progress.
 */
status_t QSPI_TransferGetSendCountEDMA(QuadSPI_Type *base, qspi_edma_handle_t *handle, size_t *count);

/*!
 * @brief Gets the status of the receive transfer.
 *
 * @param base Pointer to QuadSPI Type.
 * @param handle Pointer to qspi_edma_handle_t structure
 * @param count Bytes received.
 * @retval kStatus_Success Succeed get the transfer count.
 * @retval kStatus_NoTransferInProgress There is not a non-blocking transaction currently in progress.
 */
status_t QSPI_TransferGetReceiveCountEDMA(QuadSPI_Type *base, qspi_edma_handle_t *handle, size_t *count);

/* @} */

#if defined(__cplusplus)
}
#endif

/* @} */

#endif /* _FSL_QSPI_EDMA_H_ */
