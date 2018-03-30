/**
  * <h2><center>&copy; COPYRIGHT(c) 2016 STMicroelectronics</center></h2>
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  */

#include "os/mynewt.h"
#include "mcu/stm32_hal.h"

#define SPI_DEFAULT_TIMEOUT 100U

HAL_StatusTypeDef HAL_SPI_Slave_Queue_TransmitReceive(SPI_HandleTypeDef *hspi, uint8_t *pTxData, uint8_t *pRxData, uint16_t Size);
HAL_StatusTypeDef HAL_SPI_QueueTransmit(SPI_HandleTypeDef *hspi, uint8_t *pData, uint16_t Size);

#define SPI_HAS_FIFO (defined(STM32F3) || defined(STM32F7))

#if SPI_HAS_FIFO && (MYNEWT_VAL(SPI_0_SLAVE) || \
                     MYNEWT_VAL(SPI_1_SLAVE) || \
                     MYNEWT_VAL(SPI_2_SLAVE) || \
                     MYNEWT_VAL(SPI_3_SLAVE) || \
                     MYNEWT_VAL(SPI_4_SLAVE) || \
                     MYNEWT_VAL(SPI_5_SLAVE))
#error "This MCU currently does not support SPI slave"
#endif

/* XXX: This is copied from stm32l1xx_hal_spi_ex.c because it is a __weak
 * symbol defined in stm32l1xx_hal_spi.c that is overriden but due to our
 * archive process seems to not link correctly. Copying here enables the
 * linker to find the correct non-weak symbol.
 */
#if defined(STM32L152xC)
#include "stm32l1xx_hal.h"

HAL_StatusTypeDef HAL_SPI_Init(SPI_HandleTypeDef *hspi)
{
  /* Check the SPI handle allocation */
  if (hspi == NULL)
  {
    return HAL_ERROR;
  }

  /* Check the parameters */
  assert_param(IS_SPI_ALL_INSTANCE(hspi->Instance));
  assert_param(IS_SPI_MODE(hspi->Init.Mode));
  assert_param(IS_SPI_DATASIZE(hspi->Init.DataSize));
  assert_param(IS_SPI_CPOL(hspi->Init.CLKPolarity));
  assert_param(IS_SPI_CPHA(hspi->Init.CLKPhase));
  assert_param(IS_SPI_NSS(hspi->Init.NSS));
  assert_param(IS_SPI_BAUDRATE_PRESCALER(hspi->Init.BaudRatePrescaler));
  assert_param(IS_SPI_FIRST_BIT(hspi->Init.FirstBit));

  hspi->Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi->Init.CRCPolynomial = 1;

  if (hspi->State == HAL_SPI_STATE_RESET)
  {
    /* Allocate lock resource and initialize it */
    hspi->Lock = HAL_UNLOCKED;

    /* Init the low level hardware : GPIO, CLOCK, NVIC... */
    HAL_SPI_MspInit(hspi);
  }

  hspi->State = HAL_SPI_STATE_BUSY;

  /* Disble the selected SPI peripheral */
  __HAL_SPI_DISABLE(hspi);

  /*----------------------- SPIx CR1 & CR2 Configuration ---------------------*/
  /* Configure : SPI Mode, Communication Mode, Data size, Clock polarity and phase, NSS management,
  Communication speed, First bit and CRC calculation state */
  hspi->Instance->CR1 = (hspi->Init.Mode | hspi->Init.Direction | hspi->Init.DataSize |
                         hspi->Init.CLKPolarity | hspi->Init.CLKPhase | (hspi->Init.NSS & SPI_CR1_SSM) |
                         hspi->Init.BaudRatePrescaler | hspi->Init.FirstBit  | hspi->Init.CRCCalculation);

  /* Configure : NSS management */
  hspi->Instance->CR2 = (((hspi->Init.NSS >> 16U) & SPI_CR2_SSOE) | hspi->Init.TIMode);

  /*---------------------------- SPIx CRCPOLY Configuration ------------------*/
  /* Configure : CRC Polynomial */
  hspi->Instance->CRCPR = hspi->Init.CRCPolynomial;

#if defined(SPI_I2SCFGR_I2SMOD)
  /* Activate the SPI mode (Make sure that I2SMOD bit in I2SCFGR register is reset) */
  CLEAR_BIT(hspi->Instance->I2SCFGR, SPI_I2SCFGR_I2SMOD);
#endif

  hspi->ErrorCode = HAL_SPI_ERROR_NONE;
  hspi->State = HAL_SPI_STATE_READY;

  return HAL_OK;
}
#endif

static HAL_StatusTypeDef
SPI_WaitFlagStateUntilTimeout(SPI_HandleTypeDef *hspi, uint32_t Flag, uint32_t State, uint32_t Timeout, uint32_t Tickstart)
{
  while((((hspi->Instance->SR & Flag) == (Flag)) ? SET : RESET) != State) {
    if(Timeout != HAL_MAX_DELAY)
    {
      if((Timeout == 0U) || ((HAL_GetTick()-Tickstart) >= Timeout))
      {
        /* Disable the SPI and reset the CRC: the CRC value should be cleared
        on both master and slave sides in order to resynchronize the master
        and slave for their respective CRC calculation */

        /* Disable TXE, RXNE and ERR interrupts for the interrupt process */
        __HAL_SPI_DISABLE_IT(hspi, (SPI_IT_TXE | SPI_IT_RXNE | SPI_IT_ERR));

        hspi->State= HAL_SPI_STATE_READY;

        /* Process Unlocked */
        __HAL_UNLOCK(hspi);

        return HAL_TIMEOUT;
      }
    }
  }

  return HAL_OK;
}

#if !SPI_HAS_FIFO
static HAL_StatusTypeDef
SPI_CheckFlag_BSY(SPI_HandleTypeDef *hspi, uint32_t Timeout, uint32_t Tickstart)
{
  /* Control the BSY flag */
  if(SPI_WaitFlagStateUntilTimeout(hspi, SPI_FLAG_BSY, RESET, Timeout, Tickstart) != HAL_OK) {
    SET_BIT(hspi->ErrorCode, HAL_SPI_ERROR_FLAG);
    return HAL_TIMEOUT;
  }
  return HAL_OK;
}
#endif

#if SPI_HAS_FIFO
static HAL_StatusTypeDef SPI_WaitFifoStateUntilTimeout(SPI_HandleTypeDef *hspi, uint32_t Fifo, uint32_t State,
                                                       uint32_t Timeout, uint32_t Tickstart)
{
  __IO uint8_t tmpreg;

  while ((hspi->Instance->SR & Fifo) != State)
  {
    if ((Fifo == SPI_SR_FRLVL) && (State == SPI_FRLVL_EMPTY))
    {
      tmpreg = *((__IO uint8_t *)&hspi->Instance->DR);
      /* To avoid GCC warning */
      UNUSED(tmpreg);
    }

    if (Timeout != HAL_MAX_DELAY)
    {
      /* TODO: handle HAL_GetTick overflow */
      if ((Timeout == 0U) || ((HAL_GetTick() - Tickstart) >= Timeout))
      {
        /* Disable the SPI and reset the CRC: the CRC value should be cleared
           on both master and slave sides in order to resynchronize the master
           and slave for their respective CRC calculation */

        /* Disable TXE, RXNE and ERR interrupts for the interrupt process */
        __HAL_SPI_DISABLE_IT(hspi, (SPI_IT_TXE | SPI_IT_RXNE | SPI_IT_ERR));

        hspi->State = HAL_SPI_STATE_READY;

        /* Process Unlocked */
        __HAL_UNLOCK(hspi);

        return HAL_TIMEOUT;
      }
    }
  }

  return HAL_OK;
}

static HAL_StatusTypeDef SPI_EndRxTxTransaction(SPI_HandleTypeDef *hspi, uint32_t Timeout, uint32_t Tickstart)
{
  /* Control if the TX fifo is empty */
  if (SPI_WaitFifoStateUntilTimeout(hspi, SPI_FLAG_FTLVL, SPI_FTLVL_EMPTY, Timeout, Tickstart) != HAL_OK)
  {
    SET_BIT(hspi->ErrorCode, HAL_SPI_ERROR_FLAG);
    return HAL_TIMEOUT;
  }

  /* Control the BSY flag */
  if (SPI_WaitFlagStateUntilTimeout(hspi, SPI_FLAG_BSY, RESET, Timeout, Tickstart) != HAL_OK)
  {
    SET_BIT(hspi->ErrorCode, HAL_SPI_ERROR_FLAG);
    return HAL_TIMEOUT;
  }

  /* Control if the RX fifo is empty */
  if (SPI_WaitFifoStateUntilTimeout(hspi, SPI_FLAG_FRLVL, SPI_FRLVL_EMPTY, Timeout, Tickstart) != HAL_OK)
  {
    SET_BIT(hspi->ErrorCode, HAL_SPI_ERROR_FLAG);
    return HAL_TIMEOUT;
  }
  return HAL_OK;
}

static HAL_StatusTypeDef SPI_EndTxTransaction(SPI_HandleTypeDef *hspi, uint32_t Timeout, uint32_t Tickstart)
{
  /* Control if the TX fifo is empty */
  if (SPI_WaitFifoStateUntilTimeout(hspi, SPI_FLAG_FTLVL, SPI_FTLVL_EMPTY, Timeout, Tickstart) != HAL_OK)
  {
    SET_BIT(hspi->ErrorCode, HAL_SPI_ERROR_FLAG);
    return HAL_TIMEOUT;
  }
  return HAL_OK;
}
#endif

static void SPI_CloseTx_ISR(SPI_HandleTypeDef *hspi)
{
  uint32_t tickstart = 0U;
  __IO uint32_t count = SPI_DEFAULT_TIMEOUT * (SystemCoreClock / 24U / 1000U);

  /* Init tickstart for timeout management*/
  tickstart = HAL_GetTick();
  (void)count;

#if !SPI_HAS_FIFO
  /* Wait until TXE flag is set */
  do
  {
    if(count-- == 0U)
    {
      SET_BIT(hspi->ErrorCode, HAL_SPI_ERROR_FLAG);
      break;
    }
  }
  while((hspi->Instance->SR & SPI_FLAG_TXE) == RESET);
#endif

  /* Disable TXE and ERR interrupt */
  __HAL_SPI_DISABLE_IT(hspi, (SPI_IT_TXE | SPI_IT_ERR));

#if SPI_HAS_FIFO
  if (SPI_EndTxTransaction(hspi, SPI_DEFAULT_TIMEOUT, tickstart) != HAL_OK)
  {
    SET_BIT(hspi->ErrorCode, HAL_SPI_ERROR_FLAG);
  }
#else
  /* Check Busy flag */
  if(SPI_CheckFlag_BSY(hspi, SPI_DEFAULT_TIMEOUT, tickstart) != HAL_OK)
  {
    SET_BIT(hspi->ErrorCode, HAL_SPI_ERROR_FLAG);
  }
#endif

  /* Clear overrun flag in 2 Lines communication mode because received is not read */
  __HAL_SPI_CLEAR_OVRFLAG(hspi);

  hspi->State = HAL_SPI_STATE_READY;
  if(hspi->ErrorCode != HAL_SPI_ERROR_NONE)
  {
    HAL_SPI_ErrorCallback(hspi);
  }
  else
  {
    HAL_SPI_TxCpltCallback(hspi);
  }
}

static void SPI_CloseRxTx_ISR(SPI_HandleTypeDef *hspi)
{
  uint32_t tickstart = 0U;
  __IO uint32_t count = SPI_DEFAULT_TIMEOUT * (SystemCoreClock / 24U / 1000U);
  tickstart = HAL_GetTick();

  /* Disable ERR interrupt */
  __HAL_SPI_DISABLE_IT(hspi, SPI_IT_ERR);

#if SPI_HAS_FIFO
  (void)count;
  if (SPI_EndRxTxTransaction(hspi, SPI_DEFAULT_TIMEOUT, tickstart) != HAL_OK)
  {
    SET_BIT(hspi->ErrorCode, HAL_SPI_ERROR_FLAG);
  }
#else
  /* Wait until TXE flag is set */
  do
  {
    if(count-- == 0U)
    {
      SET_BIT(hspi->ErrorCode, HAL_SPI_ERROR_FLAG);
      break;
    }
  }
  while((hspi->Instance->SR & SPI_FLAG_TXE) == RESET);

  /* Check the end of the transaction */
  if(SPI_CheckFlag_BSY(hspi, SPI_DEFAULT_TIMEOUT, tickstart)!=HAL_OK)
  {
    SET_BIT(hspi->ErrorCode, HAL_SPI_ERROR_FLAG);
  }

  /* Clear overrun flag in 2 Lines communication mode because received is not read */
  __HAL_SPI_CLEAR_OVRFLAG(hspi);
#endif

  if(hspi->ErrorCode == HAL_SPI_ERROR_NONE)
  {
    if(hspi->State == HAL_SPI_STATE_BUSY_RX)
    {
      hspi->State = HAL_SPI_STATE_READY;
      HAL_SPI_RxCpltCallback(hspi);
    }
    else
    {
      hspi->State = HAL_SPI_STATE_READY;
      HAL_SPI_TxRxCpltCallback(hspi);
    }
  }
  else
  {
    hspi->State = HAL_SPI_STATE_READY;
    HAL_SPI_ErrorCallback(hspi);
  }
}

/**
  * @brief  Rx 8-bit handler for Transmit and Receive in Interrupt mode.
  * @param  hspi pointer to a SPI_HandleTypeDef structure that contains
  *               the configuration information for SPI module.
  * @retval None
  */
static void SPI_2linesRxISR_8BIT(struct __SPI_HandleTypeDef *hspi)
{
  if (hspi->Init.Mode == SPI_MODE_MASTER) {
    *hspi->pRxBuffPtr++ = *((__IO uint8_t *)&hspi->Instance->DR);
    hspi->RxXferCount--;
  } else {
    //FIXME: this block below is probably not required...
#if SPI_HAS_FIFO
    /* Receive data in packing mode */
    if (hspi->RxXferCount > 1U) {
      *((uint16_t *)hspi->pRxBuffPtr) = hspi->Instance->DR;
      hspi->pRxBuffPtr += sizeof(uint16_t);
      hspi->RxXferCount -= 2U;
    } else {
      /* Receive data in 8 Bit mode */
#endif
      *hspi->pRxBuffPtr++ = *((__IO uint8_t *)&hspi->Instance->DR);
      hspi->RxXferCount--;
#if SPI_HAS_FIFO
    }
#endif
  }

  /* check end of the reception */
  if(hspi->RxXferCount == 0U)
  {
    /* Disable RXNE interrupt */
    __HAL_SPI_DISABLE_IT(hspi, (SPI_IT_RXNE | SPI_IT_ERR));

    if(hspi->TxXferCount == 0U)
    {
      SPI_CloseRxTx_ISR(hspi);
    }
  }
}

/**
  * @brief  Tx 8-bit handler for Transmit and Receive in Interrupt mode.
  * @param  hspi pointer to a SPI_HandleTypeDef structure that contains
  *               the configuration information for SPI module.
  * @retval None
  */
static void SPI_2linesTxISR_8BIT(struct __SPI_HandleTypeDef *hspi)
{
#if SPI_HAS_FIFO
  /* Transmit data in packing Bit mode */
  if (hspi->TxXferCount >= 2U)
  {
    hspi->Instance->DR = *((uint16_t *)hspi->pTxBuffPtr);
    hspi->pTxBuffPtr += sizeof(uint16_t);
    hspi->TxXferCount -= 2U;
  }
  /* Transmit data in 8 Bit mode */
  else
  {
#endif
    *(__IO uint8_t *)&hspi->Instance->DR = (*hspi->pTxBuffPtr++);
    hspi->TxXferCount--;
#if SPI_HAS_FIFO
  }
#endif

  /* check the end of the transmission */
  if(hspi->TxXferCount == 0U)
  {
    /* Disable TXE interrupt */
    __HAL_SPI_DISABLE_IT(hspi, SPI_IT_TXE);

    if(hspi->RxXferCount == 0U)
    {
      SPI_CloseRxTx_ISR(hspi);
    }
  }
}

/**
  * @brief  Rx 16-bit handler for Transmit and Receive in Interrupt mode.
  * @param  hspi pointer to a SPI_HandleTypeDef structure that contains
  *               the configuration information for SPI module.
  * @retval None
  *
  * @note:  Copied verbatim from STM32Cube
  */
static void SPI_2linesRxISR_16BIT(struct __SPI_HandleTypeDef *hspi)
{
  /* Receive data in 16 Bit mode */
  *((uint16_t*)hspi->pRxBuffPtr) = hspi->Instance->DR;
  hspi->pRxBuffPtr += sizeof(uint16_t);
  hspi->RxXferCount--;

  if(hspi->RxXferCount == 0U)
  {
    /* Disable RXNE interrupt */
    __HAL_SPI_DISABLE_IT(hspi, SPI_IT_RXNE);

    if(hspi->TxXferCount == 0U)
    {
      SPI_CloseRxTx_ISR(hspi);
    }
  }
}

/**
  * @brief  Tx 16-bit handler for Transmit and Receive in Interrupt mode.
  * @param  hspi pointer to a SPI_HandleTypeDef structure that contains
  *               the configuration information for SPI module.
  * @retval None
  */
static void SPI_2linesTxISR_16BIT(struct __SPI_HandleTypeDef *hspi)
{
  /* Transmit data in 16 Bit mode */
  hspi->Instance->DR = *((uint16_t *)hspi->pTxBuffPtr);
  hspi->pTxBuffPtr += sizeof(uint16_t);
  hspi->TxXferCount--;

  if(hspi->TxXferCount == 0U)
  {
    /* Disable TXE interrupt */
    __HAL_SPI_DISABLE_IT(hspi, SPI_IT_TXE);

    if(hspi->RxXferCount == 0U)
    {
      SPI_CloseRxTx_ISR(hspi);
    }
  }
}

/**
  * @brief  Handle the data 8-bit transmit in Interrupt mode.
  * @param  hspi pointer to a SPI_HandleTypeDef structure that contains
  *               the configuration information for SPI module.
  * @retval None
  */
static void SPI_TxISR_8BIT(struct __SPI_HandleTypeDef *hspi)
{
#if SPI_HAS_FIFO
  /* Transmit data in packing Bit mode */
  if (hspi->TxXferCount >= 2U) {
    hspi->Instance->DR = *((uint16_t *)hspi->pTxBuffPtr);
    hspi->pTxBuffPtr += sizeof(uint16_t);
    hspi->TxXferCount -= 2U;
  } else {
#endif
    *(__IO uint8_t *)&hspi->Instance->DR = (*hspi->pTxBuffPtr++);
    hspi->TxXferCount--;
#if SPI_HAS_FIFO
  }
#endif

  if(hspi->TxXferCount == 0U)
  {
    SPI_CloseTx_ISR(hspi);
  }
}

/**
  * @brief  Handle the data 16-bit transmit in Interrupt mode.
  * @param  hspi pointer to a SPI_HandleTypeDef structure that contains
  *               the configuration information for SPI module.
  * @retval None
  */
static void SPI_TxISR_16BIT(struct __SPI_HandleTypeDef *hspi)
{
  /* Transmit data in 16 Bit mode */
  hspi->Instance->DR = *((uint16_t *)hspi->pTxBuffPtr);
  hspi->pTxBuffPtr += sizeof(uint16_t);
  hspi->TxXferCount--;

  if(hspi->TxXferCount == 0U)
  {
    SPI_CloseTx_ISR(hspi);
  }
}

/**
  * Transmit an amount of data in non-blocking mode with Interrupt.
  *
  * @param  hspi pointer to a SPI_HandleTypeDef structure that contains
  *               the configuration information for SPI module.
  * @param  pData pointer to data buffer
  * @param  Size amount of data to be sent
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_SPI_Transmit_IT_Custom(SPI_HandleTypeDef *hspi, uint8_t *pData,
        uint16_t Size)
{
  HAL_StatusTypeDef errorcode = HAL_OK;

  /* Process Locked */
  __HAL_LOCK(hspi);

  errorcode = HAL_SPI_QueueTransmit(hspi, pData, Size);
  if (errorcode) {
      goto error;
  }

  /* Enable TXE interrupt */
  __HAL_SPI_ENABLE_IT(hspi, (SPI_IT_TXE));

  /* Check if the SPI is already enabled */
  if ((hspi->Instance->CR1 &SPI_CR1_SPE) != SPI_CR1_SPE) {
    __HAL_SPI_ENABLE(hspi);
  }

error :
  __HAL_UNLOCK(hspi);
  return errorcode;
}

HAL_StatusTypeDef HAL_SPI_QueueTransmit(SPI_HandleTypeDef *hspi, uint8_t *pData, uint16_t Size)
{
  HAL_StatusTypeDef errorcode = HAL_OK;

  if((pData == NULL) || (Size == 0))
  {
    errorcode = HAL_ERROR;
    goto error;
  }

  if(hspi->State != HAL_SPI_STATE_READY)
  {
    errorcode = HAL_BUSY;
    goto error;
  }

  /* Set the transaction information */
  hspi->State       = HAL_SPI_STATE_BUSY_TX;
  hspi->ErrorCode   = HAL_SPI_ERROR_NONE;
  hspi->pTxBuffPtr  = (uint8_t *)pData;
  hspi->TxXferSize  = Size;
  hspi->TxXferCount = Size;

  /* Init field not used in handle to zero */
  hspi->pRxBuffPtr  = (uint8_t *)NULL;
  hspi->RxXferSize  = 0U;
  hspi->RxXferCount = 0U;
  hspi->RxISR       = NULL;

  /* Set the function for IT treatment */
  if(hspi->Init.DataSize > SPI_DATASIZE_8BIT )
  {
    hspi->TxISR = SPI_TxISR_16BIT;
  }
  else
  {
    hspi->TxISR = SPI_TxISR_8BIT;
  }

#if 0 /* MYNEWT: TODO */
  /* Enable TXE interrupt */
  __HAL_SPI_ENABLE_IT(hspi, (SPI_IT_TXE));
#endif

  /* MYNEWT: in slave mode write 1st byte to DR */
  if ((hspi->Instance->CR1 & SPI_CR1_MSTR) == 0) {
      hspi->TxISR(hspi);
  }

#if 0 /* MYNEWT: TODO */
  /* Check if the SPI is already enabled */
  if((hspi->Instance->CR1 &SPI_CR1_SPE) != SPI_CR1_SPE)
  {
    /* Enable SPI peripheral */
    __HAL_SPI_ENABLE(hspi);
  }
#endif

error :
  return errorcode;
}

/**
  * Transmit and Receive an amount of data in non-blocking mode with Interrupt.
  *
  * @param  hspi pointer to a SPI_HandleTypeDef structure that contains
  *               the configuration information for SPI module.
  * @param  pTxData pointer to transmission data buffer
  * @param  pRxData pointer to reception data buffer
  * @param  Size amount of data to be sent and received
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_SPI_TransmitReceive_IT_Custom(SPI_HandleTypeDef *hspi,
        uint8_t *pTxData, uint8_t *pRxData, uint16_t Size)
{
  HAL_StatusTypeDef errorcode = HAL_OK;

  /* Process locked */
  __HAL_LOCK(hspi);

  errorcode = HAL_SPI_Slave_Queue_TransmitReceive(hspi, pTxData, pRxData, Size);
  if (errorcode) {
      goto error;
  }

  /* Enable TXE, RXNE and ERR interrupt */
  __HAL_SPI_ENABLE_IT(hspi, (SPI_IT_TXE | SPI_IT_RXNE | SPI_IT_ERR));

  /* Check if the SPI is already enabled */
  if((hspi->Instance->CR1 &SPI_CR1_SPE) != SPI_CR1_SPE)
  {
    /* Enable SPI peripheral */
    __HAL_SPI_ENABLE(hspi);
  }

error:
  /* Process Unlocked */
  __HAL_UNLOCK(hspi);
  return errorcode;
}

HAL_StatusTypeDef HAL_SPI_Slave_Queue_TransmitReceive(SPI_HandleTypeDef *hspi, uint8_t *pTxData, uint8_t *pRxData, uint16_t Size)
{
  uint32_t tmp = 0U, tmp1 = 0U;
  HAL_StatusTypeDef errorcode = HAL_OK;

  tmp  = hspi->State;
  tmp1 = hspi->Init.Mode;

  if(!((tmp == HAL_SPI_STATE_READY) || ((tmp1 == SPI_MODE_MASTER) && (tmp == HAL_SPI_STATE_BUSY_RX))))
  {
    errorcode = HAL_BUSY;
    goto error;
  }

  if((pTxData == NULL ) || (pRxData == NULL ) || (Size == 0))
  {
    errorcode = HAL_ERROR;
    goto error;
  }

  /* Don't overwrite in case of HAL_SPI_STATE_BUSY_RX */
  if(hspi->State == HAL_SPI_STATE_READY)
  {
    hspi->State = HAL_SPI_STATE_BUSY_TX_RX;
  }

  /* Set the transaction information */
  hspi->ErrorCode   = HAL_SPI_ERROR_NONE;
  hspi->pTxBuffPtr  = (uint8_t *)pTxData;
  hspi->TxXferSize  = Size;
  hspi->TxXferCount = Size;
  hspi->pRxBuffPtr  = (uint8_t *)pRxData;
  hspi->RxXferSize  = Size;
  hspi->RxXferCount = Size;

  /* Set the function for IT treatment */
  if(hspi->Init.DataSize > SPI_DATASIZE_8BIT )
  {
    hspi->RxISR     = SPI_2linesRxISR_16BIT;
    hspi->TxISR     = SPI_2linesTxISR_16BIT;
  }
  else
  {
    hspi->RxISR     = SPI_2linesRxISR_8BIT;
    hspi->TxISR     = SPI_2linesTxISR_8BIT;
  }

  /* Enable TXE, RXNE and ERR interrupt */
  //__HAL_SPI_ENABLE_IT(hspi, (SPI_IT_TXE | SPI_IT_RXNE | SPI_IT_ERR));

  /* MYNEWT: in slave mode write 1st byte to DR */
  if ((hspi->Instance->CR1 & SPI_CR1_MSTR) == 0) {
      hspi->TxISR(hspi);
  }

#if 0
  /* Check if the SPI is already enabled */
  if((hspi->Instance->CR1 &SPI_CR1_SPE) != SPI_CR1_SPE)
  {
    /* Enable SPI peripheral */
    __HAL_SPI_ENABLE(hspi);
  }
#endif

error :
  return errorcode;
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
