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
  *
  ******************************************************************************
  */

#include <mcu/stm32_hal.h>
#include <syscfg/syscfg.h>

#if !defined(STM32F3) && !defined(STM32F7)

#define I2C_TIMEOUT_BUSY_FLAG     25U         /*!< Timeout 25 ms             */
#define I2C_NO_OPTION_FRAME       0xFFFF0000U /*!< XferOptions default value */

#define I2C_STATE_MSK             ((uint32_t)((HAL_I2C_STATE_BUSY_TX | HAL_I2C_STATE_BUSY_RX) & (~(uint32_t)HAL_I2C_STATE_READY))) /*!< Mask State define, keep only RX and TX bits            */
#define I2C_STATE_NONE            ((uint32_t)(HAL_I2C_MODE_NONE))                                                        /*!< Default Value                                          */
#define I2C_STATE_MASTER_BUSY_TX  ((uint32_t)((HAL_I2C_STATE_BUSY_TX & I2C_STATE_MSK) | HAL_I2C_MODE_MASTER))            /*!< Master Busy TX, combinaison of State LSB and Mode enum */
#define I2C_STATE_MASTER_BUSY_RX  ((uint32_t)((HAL_I2C_STATE_BUSY_RX & I2C_STATE_MSK) | HAL_I2C_MODE_MASTER))            /*!< Master Busy RX, combinaison of State LSB and Mode enum */

static const uint8_t HAL_I2C_MODE_MASTER_SEL = 0x11;

/**
  * @brief  This function handles I2C Communication Timeout.
  * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
  *         the configuration information for I2C module
  * @param  Flag specifies the I2C flag to check.
  * @param  Status The new Flag status (SET or RESET).
  * @param  Timeout Timeout duration
  * @param  Tickstart Tick start value
  * @retval HAL status
  */
static HAL_StatusTypeDef I2C_WaitOnFlagUntilTimeout(I2C_HandleTypeDef *hi2c,
        uint32_t Flag, FlagStatus Status, uint32_t Timeout, uint32_t Tickstart)
{
  /* Wait until flag is set */
  while((__HAL_I2C_GET_FLAG(hi2c, Flag) ? SET : RESET) == Status) 
  {
    /* Check for the Timeout */
    if(Timeout != HAL_MAX_DELAY)
    {
      if((Timeout == 0U)||((HAL_GetTick() - Tickstart ) > Timeout))
      {
        hi2c->PreviousState = I2C_STATE_NONE;
        hi2c->State= HAL_I2C_STATE_READY;
        hi2c->Mode = HAL_I2C_MODE_NONE;

        /* Process Unlocked */
        __HAL_UNLOCK(hi2c);

        return HAL_TIMEOUT;
      }
    }
  }

  return HAL_OK;
}

/**
  * @brief  This function handles I2C Communication Timeout for Master addressing phase.
  * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
  *         the configuration information for I2C module
  * @param  Flag specifies the I2C flag to check.
  * @param  Timeout Timeout duration
  * @param  Tickstart Tick start value
  * @retval HAL status
  */
static HAL_StatusTypeDef I2C_WaitOnMasterAddressFlagUntilTimeout(I2C_HandleTypeDef *hi2c,
        uint32_t Flag, uint32_t Timeout, uint32_t Tickstart)
{
  while(__HAL_I2C_GET_FLAG(hi2c, Flag) == RESET)
  {
    if(__HAL_I2C_GET_FLAG(hi2c, I2C_FLAG_AF) == SET)
    {
      /* Generate Stop */
      hi2c->Instance->CR1 |= I2C_CR1_STOP;

      /* Clear AF Flag */
      __HAL_I2C_CLEAR_FLAG(hi2c, I2C_FLAG_AF);

      hi2c->ErrorCode = HAL_I2C_ERROR_AF;
      hi2c->PreviousState = I2C_STATE_NONE;
      hi2c->State= HAL_I2C_STATE_READY;

      /* Process Unlocked */
      __HAL_UNLOCK(hi2c);

      return HAL_ERROR;
    }

    /* Check for the Timeout */
    if(Timeout != HAL_MAX_DELAY)
    {
      if((Timeout == 0U)||((HAL_GetTick() - Tickstart ) > Timeout))
      {
        hi2c->PreviousState = I2C_STATE_NONE;
        hi2c->State= HAL_I2C_STATE_READY;

        /* Process Unlocked */
        __HAL_UNLOCK(hi2c);

        return HAL_TIMEOUT;
      }
    }
  }
  return HAL_OK;
}

/**
  * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
  *         the configuration information for I2C module
  * @param  DevAddress Target device address The device 7 bits address value
  *         in datasheet must be shifted to the left before calling the interface
  * @param  Timeout Timeout duration
  * @param  Tickstart Tick start value
  * @retval HAL status
  */
static HAL_StatusTypeDef I2C_MasterRequestWrite(I2C_HandleTypeDef *hi2c,
        uint16_t DevAddress, uint32_t Timeout, uint32_t Tickstart)
{
  /* Declaration of temporary variable to prevent undefined behavior of volatile usage */
  uint32_t CurrentXferOptions = hi2c->XferOptions;

  /* Generate Start condition if first transfer */
  if((CurrentXferOptions == I2C_FIRST_AND_LAST_FRAME) || (CurrentXferOptions == I2C_FIRST_FRAME) || (CurrentXferOptions == I2C_NO_OPTION_FRAME))
  {
    /* Generate Start */
    hi2c->Instance->CR1 |= I2C_CR1_START;
  }
  else if(hi2c->PreviousState == I2C_STATE_MASTER_BUSY_RX)
  {
    /* Generate ReStart */
    hi2c->Instance->CR1 |= I2C_CR1_START;
  }

  /* Wait until SB flag is set */
  if(I2C_WaitOnFlagUntilTimeout(hi2c, I2C_FLAG_SB, RESET, Timeout, Tickstart) != HAL_OK)
  {
    return HAL_TIMEOUT;
  }

  if(hi2c->Init.AddressingMode == I2C_ADDRESSINGMODE_7BIT)
  {
    /* Send slave address */
    hi2c->Instance->DR = I2C_7BIT_ADD_WRITE(DevAddress);
  }
  else
  {
    /* Send header of slave address */
    hi2c->Instance->DR = I2C_10BIT_HEADER_WRITE(DevAddress);

    /* Wait until ADD10 flag is set */
    if(I2C_WaitOnMasterAddressFlagUntilTimeout(hi2c, I2C_FLAG_ADD10, Timeout, Tickstart) != HAL_OK)
    {
      if(hi2c->ErrorCode == HAL_I2C_ERROR_AF)
      {
        return HAL_ERROR;
      }
      else
      {
        return HAL_TIMEOUT;
      }
    }

    /* Send slave address */
    hi2c->Instance->DR = I2C_10BIT_ADDRESS(DevAddress);
  }

  /* Wait until ADDR flag is set */
  if(I2C_WaitOnMasterAddressFlagUntilTimeout(hi2c, I2C_FLAG_ADDR, Timeout, Tickstart) != HAL_OK)
  {
    if(hi2c->ErrorCode == HAL_I2C_ERROR_AF)
    {
      return HAL_ERROR;
    }
    else
    {
      return HAL_TIMEOUT;
    }
  }

  return HAL_OK;
}

/**
  * @brief  This function handles Acknowledge failed detection during an I2C Communication.
  * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
  *                the configuration information for the specified I2C.
  * @retval HAL status
  */
static HAL_StatusTypeDef I2C_IsAcknowledgeFailed(I2C_HandleTypeDef *hi2c)
{
  if(__HAL_I2C_GET_FLAG(hi2c, I2C_FLAG_AF) == SET)
  {
    /* Clear NACKF Flag */
    __HAL_I2C_CLEAR_FLAG(hi2c, I2C_FLAG_AF);

    hi2c->ErrorCode = HAL_I2C_ERROR_AF;
    hi2c->PreviousState = I2C_STATE_NONE;
    hi2c->State= HAL_I2C_STATE_READY;

    /* Process Unlocked */
    __HAL_UNLOCK(hi2c);

    return HAL_ERROR;
  }
  return HAL_OK;
}

/**
  * @brief  This function handles I2C Communication Timeout for specific usage of TXE flag.
  * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
  *                the configuration information for the specified I2C.
  * @param  Timeout Timeout duration
  * @param  Tickstart Tick start value
  * @retval HAL status
  */
static HAL_StatusTypeDef I2C_WaitOnTXEFlagUntilTimeout(I2C_HandleTypeDef *hi2c, uint32_t Timeout, uint32_t Tickstart)
{
  while(__HAL_I2C_GET_FLAG(hi2c, I2C_FLAG_TXE) == RESET)
  {
    /* Check if a NACK is detected */
    if(I2C_IsAcknowledgeFailed(hi2c) != HAL_OK)
    {
      return HAL_ERROR;
    }

    /* Check for the Timeout */
    if(Timeout != HAL_MAX_DELAY)
    {
      if((Timeout == 0U) || ((HAL_GetTick()-Tickstart) > Timeout))
      {
        hi2c->ErrorCode |= HAL_I2C_ERROR_TIMEOUT;
        hi2c->PreviousState = I2C_STATE_NONE;
        hi2c->State= HAL_I2C_STATE_READY;

        /* Process Unlocked */
        __HAL_UNLOCK(hi2c);

        return HAL_TIMEOUT;
      }
    }
  }
  return HAL_OK;
}

/**
  * @brief  This function handles I2C Communication Timeout for specific usage of BTF flag.
  * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
  *                the configuration information for the specified I2C.
  * @param  Timeout Timeout duration
  * @param  Tickstart Tick start value
  * @retval HAL status
  */
static HAL_StatusTypeDef I2C_WaitOnBTFFlagUntilTimeout(I2C_HandleTypeDef *hi2c, uint32_t Timeout, uint32_t Tickstart)
{
  while(__HAL_I2C_GET_FLAG(hi2c, I2C_FLAG_BTF) == RESET)
  {
    /* Check if a NACK is detected */
    if(I2C_IsAcknowledgeFailed(hi2c) != HAL_OK)
    {
      return HAL_ERROR;
    }

    /* Check for the Timeout */
    if(Timeout != HAL_MAX_DELAY)
    {
      if((Timeout == 0U) || ((HAL_GetTick()-Tickstart) > Timeout))
      {
        hi2c->ErrorCode |= HAL_I2C_ERROR_TIMEOUT;
        hi2c->PreviousState = I2C_STATE_NONE;
        hi2c->State= HAL_I2C_STATE_READY;

        /* Process Unlocked */
        __HAL_UNLOCK(hi2c);

        return HAL_TIMEOUT;
      }
    }
  }
  return HAL_OK;
}

/**
  * @brief  This function handles I2C Communication Timeout for specific usage of RXNE flag.
  * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
  *                the configuration information for the specified I2C.
  * @param  Timeout Timeout duration
  * @param  Tickstart Tick start value
  * @retval HAL status
  */
static HAL_StatusTypeDef I2C_WaitOnRXNEFlagUntilTimeout(I2C_HandleTypeDef *hi2c, uint32_t Timeout, uint32_t Tickstart)
{
  while(__HAL_I2C_GET_FLAG(hi2c, I2C_FLAG_RXNE) == RESET)
  {
    /* Check if a STOPF is detected */
    if(__HAL_I2C_GET_FLAG(hi2c, I2C_FLAG_STOPF) == SET)
    {
      /* Clear STOP Flag */
      __HAL_I2C_CLEAR_FLAG(hi2c, I2C_FLAG_STOPF);

      hi2c->ErrorCode = HAL_I2C_ERROR_NONE;
      hi2c->PreviousState = I2C_STATE_NONE;
      hi2c->State= HAL_I2C_STATE_READY;

      /* Process Unlocked */
      __HAL_UNLOCK(hi2c);

      return HAL_ERROR;
    }

    /* Check for the Timeout */
    if((Timeout == 0U) || ((HAL_GetTick()-Tickstart) > Timeout))
    {
      hi2c->ErrorCode |= HAL_I2C_ERROR_TIMEOUT;
      hi2c->State= HAL_I2C_STATE_READY;

      /* Process Unlocked */
      __HAL_UNLOCK(hi2c);

      return HAL_TIMEOUT;
    }
  }
  return HAL_OK;
}

/**
  * @brief  Transmits in master mode an amount of data in blocking mode.
  * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
  *                the configuration information for the specified I2C.
  * @param  DevAddress Target device address The device 7 bits address value
  *         in datasheet must be shifted to the left before calling the interface
  * @param  pData Pointer to data buffer
  * @param  Size Amount of data to be sent
  * @param  Timeout Timeout duration
  * @param  LastOp If set sends STOP, otherwise no STOP
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_I2C_Master_Transmit_Custom(I2C_HandleTypeDef *hi2c,
        uint16_t DevAddress, uint8_t *pData, uint16_t Size, uint32_t Timeout,
        uint8_t LastOp)
{
  uint32_t tickstart = 0x00U;

  /* Init tickstart for timeout management*/
  tickstart = HAL_GetTick();

  if(hi2c->State == HAL_I2C_STATE_READY)
  {
    if (hi2c->Mode != HAL_I2C_MODE_MASTER_SEL)
    {
      /* Wait until BUSY flag is reset */
      if(I2C_WaitOnFlagUntilTimeout(hi2c, I2C_FLAG_BUSY, SET, I2C_TIMEOUT_BUSY_FLAG, tickstart) != HAL_OK)
      {
        return HAL_BUSY;
      }
    }

    /* Process Locked */
    __HAL_LOCK(hi2c);

    /* Check if the I2C is already enabled */
    if((hi2c->Instance->CR1 & I2C_CR1_PE) != I2C_CR1_PE)
    {
      /* Enable I2C peripheral */
      __HAL_I2C_ENABLE(hi2c);
    }

    /* Disable Pos */
    hi2c->Instance->CR1 &= ~I2C_CR1_POS;

    hi2c->State     = HAL_I2C_STATE_BUSY_TX;
    hi2c->Mode      = HAL_I2C_MODE_MASTER;
    hi2c->ErrorCode = HAL_I2C_ERROR_NONE;

    /* Prepare transfer parameters */
    hi2c->pBuffPtr    = pData;
    hi2c->XferCount   = Size;
    hi2c->XferOptions = I2C_NO_OPTION_FRAME;
    hi2c->XferSize    = hi2c->XferCount;

    /* Send Slave Address */
    if(I2C_MasterRequestWrite(hi2c, DevAddress, Timeout, tickstart) != HAL_OK)
    {
      if(hi2c->ErrorCode == HAL_I2C_ERROR_AF)
      {
        /* Process Unlocked */
        __HAL_UNLOCK(hi2c);
        return HAL_ERROR;
      }
      else
      {
        /* Process Unlocked */
        __HAL_UNLOCK(hi2c);
        return HAL_TIMEOUT;
      }
    }

    /* Clear ADDR flag */
    __HAL_I2C_CLEAR_ADDRFLAG(hi2c);

    while(hi2c->XferSize > 0U)
    {
      /* Wait until TXE flag is set */
      if(I2C_WaitOnTXEFlagUntilTimeout(hi2c, Timeout, tickstart) != HAL_OK)
      {
        if(hi2c->ErrorCode == HAL_I2C_ERROR_AF)
        {
          /* Generate Stop */
          hi2c->Instance->CR1 |= I2C_CR1_STOP;
          return HAL_ERROR;
        }
        else
        {
          return HAL_TIMEOUT;
        }
      }

      /* Write data to DR */
      hi2c->Instance->DR = (*hi2c->pBuffPtr++);
      hi2c->XferCount--;
      hi2c->XferSize--;

      if((__HAL_I2C_GET_FLAG(hi2c, I2C_FLAG_BTF) == SET) && (hi2c->XferSize != 0U))
      {
        /* Write data to DR */
        hi2c->Instance->DR = (*hi2c->pBuffPtr++);
        hi2c->XferCount--;
        hi2c->XferSize--;
      }

      /* Wait until BTF flag is set */
      if(I2C_WaitOnBTFFlagUntilTimeout(hi2c, Timeout, tickstart) != HAL_OK)
      {
        if(hi2c->ErrorCode == HAL_I2C_ERROR_AF)
        {
          /* Generate Stop */
          hi2c->Instance->CR1 |= I2C_CR1_STOP;
          return HAL_ERROR;
        }
        else
        {
          return HAL_TIMEOUT;
        }
      }
    }

    hi2c->State = HAL_I2C_STATE_READY;

    if (LastOp)
    {
      hi2c->Instance->CR1 |= I2C_CR1_STOP;
      hi2c->Mode = HAL_I2C_MODE_NONE;
    }
    else
    {
      hi2c->Mode = HAL_I2C_MODE_MASTER_SEL;
    }

    /* Process Unlocked */
    __HAL_UNLOCK(hi2c);

    return HAL_OK;
  }
  else
  {
    return HAL_BUSY;
  }
}

/**
  * @brief  Master sends target device address for read request.
  * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
  *         the configuration information for I2C module
  * @param  DevAddress Target device address The device 7 bits address value
  *         in datasheet must be shifted to the left before calling the interface
  * @param  Timeout Timeout duration
  * @param  Tickstart Tick start value
  * @retval HAL status
  */
static HAL_StatusTypeDef I2C_MasterRequestRead(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint32_t Timeout, uint32_t Tickstart)
{
  /* Declaration of temporary variable to prevent undefined behavior of volatile usage */
  uint32_t CurrentXferOptions = hi2c->XferOptions;

  /* Enable Acknowledge */
  hi2c->Instance->CR1 |= I2C_CR1_ACK;

  /* Generate Start condition if first transfer */
  if((CurrentXferOptions == I2C_FIRST_AND_LAST_FRAME) || (CurrentXferOptions == I2C_FIRST_FRAME)  || (CurrentXferOptions == I2C_NO_OPTION_FRAME))
  {
    /* Generate Start */
    hi2c->Instance->CR1 |= I2C_CR1_START;
  }
  else if(hi2c->PreviousState == I2C_STATE_MASTER_BUSY_TX)
  {
    /* Generate ReStart */
    hi2c->Instance->CR1 |= I2C_CR1_START;
  }

  /* Wait until SB flag is set */
  if(I2C_WaitOnFlagUntilTimeout(hi2c, I2C_FLAG_SB, RESET, Timeout, Tickstart) != HAL_OK)
  {
    return HAL_TIMEOUT;
  }

  if(hi2c->Init.AddressingMode == I2C_ADDRESSINGMODE_7BIT)
  {
    /* Send slave address */
    hi2c->Instance->DR = I2C_7BIT_ADD_READ(DevAddress);
  }
  else
  {
    /* Send header of slave address */
    hi2c->Instance->DR = I2C_10BIT_HEADER_WRITE(DevAddress);

    /* Wait until ADD10 flag is set */
    if(I2C_WaitOnMasterAddressFlagUntilTimeout(hi2c, I2C_FLAG_ADD10, Timeout, Tickstart) != HAL_OK)
    {
      if(hi2c->ErrorCode == HAL_I2C_ERROR_AF)
      {
        return HAL_ERROR;
      }
      else
      {
        return HAL_TIMEOUT;
      }
    }

    /* Send slave address */
    hi2c->Instance->DR = I2C_10BIT_ADDRESS(DevAddress);

    /* Wait until ADDR flag is set */
    if(I2C_WaitOnMasterAddressFlagUntilTimeout(hi2c, I2C_FLAG_ADDR, Timeout, Tickstart) != HAL_OK)
    {
      if(hi2c->ErrorCode == HAL_I2C_ERROR_AF)
      {
        return HAL_ERROR;
      }
      else
      {
        return HAL_TIMEOUT;
      }
    }

    /* Clear ADDR flag */
    __HAL_I2C_CLEAR_ADDRFLAG(hi2c);

    /* Generate Restart */
    hi2c->Instance->CR1 |= I2C_CR1_START;

    /* Wait until SB flag is set */
    if(I2C_WaitOnFlagUntilTimeout(hi2c, I2C_FLAG_SB, RESET, Timeout, Tickstart) != HAL_OK)
    {
      return HAL_TIMEOUT;
    }

    /* Send header of slave address */
    hi2c->Instance->DR = I2C_10BIT_HEADER_READ(DevAddress);
  }

  /* Wait until ADDR flag is set */
  if(I2C_WaitOnMasterAddressFlagUntilTimeout(hi2c, I2C_FLAG_ADDR, Timeout, Tickstart) != HAL_OK)
  {
    if(hi2c->ErrorCode == HAL_I2C_ERROR_AF)
    {
      return HAL_ERROR;
    }
    else
    {
      return HAL_TIMEOUT;
    }
  }

  return HAL_OK;
}

/**
  * @brief  Receives in master mode an amount of data in blocking mode. 
  * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
  *                the configuration information for the specified I2C.
  * @param  DevAddress Target device address The device 7 bits address value
  *         in datasheet must be shifted to the left before calling the interface
  * @param  pData Pointer to data buffer
  * @param  Size Amount of data to be sent
  * @param  Timeout Timeout duration
  * @param  LastOp If set sends STOP, otherwise no STOP
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_I2C_Master_Receive_Custom(I2C_HandleTypeDef *hi2c,
        uint16_t DevAddress, uint8_t *pData, uint16_t Size, uint32_t Timeout,
        uint8_t LastOp)
{
  uint32_t tickstart = 0x00U;

  /* Init tickstart for timeout management*/
  tickstart = HAL_GetTick();

  if(hi2c->State == HAL_I2C_STATE_READY)
  {
    if (hi2c->Mode != HAL_I2C_MODE_MASTER_SEL)
    {
      /* Wait until BUSY flag is reset */
      if(I2C_WaitOnFlagUntilTimeout(hi2c, I2C_FLAG_BUSY, SET, I2C_TIMEOUT_BUSY_FLAG, tickstart) != HAL_OK)
      {
        return HAL_BUSY;
      }
    }

    /* Process Locked */
    __HAL_LOCK(hi2c);

    /* Check if the I2C is already enabled */
    if((hi2c->Instance->CR1 & I2C_CR1_PE) != I2C_CR1_PE)
    {
      /* Enable I2C peripheral */
      __HAL_I2C_ENABLE(hi2c);
    }

    /* Disable Pos */
    hi2c->Instance->CR1 &= ~I2C_CR1_POS;

    hi2c->State       = HAL_I2C_STATE_BUSY_RX;
    hi2c->Mode        = HAL_I2C_MODE_MASTER;
    hi2c->ErrorCode   = HAL_I2C_ERROR_NONE;

    /* Prepare transfer parameters */
    hi2c->pBuffPtr    = pData;
    hi2c->XferCount   = Size;
    hi2c->XferOptions = I2C_NO_OPTION_FRAME;
    hi2c->XferSize    = hi2c->XferCount;

    /* Send Slave Address */
    if(I2C_MasterRequestRead(hi2c, DevAddress, Timeout, tickstart) != HAL_OK)
    {
      if(hi2c->ErrorCode == HAL_I2C_ERROR_AF)
      {
        /* Process Unlocked */
        __HAL_UNLOCK(hi2c);
        return HAL_ERROR;
      }
      else
      {
        /* Process Unlocked */
        __HAL_UNLOCK(hi2c);
        return HAL_TIMEOUT;
      }
    }

    if(hi2c->XferSize == 0U)
    {
      /* Clear ADDR flag */
      __HAL_I2C_CLEAR_ADDRFLAG(hi2c);

      if (LastOp)
      {
        /* Generate Stop */
        hi2c->Instance->CR1 |= I2C_CR1_STOP;
      }
    }
    else if(hi2c->XferSize == 1U)
    {
      /* Disable Acknowledge */
      hi2c->Instance->CR1 &= ~I2C_CR1_ACK;

      /* Clear ADDR flag */
      __HAL_I2C_CLEAR_ADDRFLAG(hi2c);

      if (LastOp)
      {
        /* Generate Stop */
        hi2c->Instance->CR1 |= I2C_CR1_STOP;
      }
    }
    else if(hi2c->XferSize == 2U)
    {
      /* Disable Acknowledge */
      hi2c->Instance->CR1 &= ~I2C_CR1_ACK;

      /* Enable Pos */
      hi2c->Instance->CR1 |= I2C_CR1_POS;

      /* Clear ADDR flag */
      __HAL_I2C_CLEAR_ADDRFLAG(hi2c);
    }
    else
    {
      /* Enable Acknowledge */
      hi2c->Instance->CR1 |= I2C_CR1_ACK;

      /* Clear ADDR flag */
      __HAL_I2C_CLEAR_ADDRFLAG(hi2c);
    }

    while(hi2c->XferSize > 0U)
    {
      if(hi2c->XferSize <= 3U)
      {
        /* One byte */
        if(hi2c->XferSize == 1U)
        {
          /* Wait until RXNE flag is set */
          if(I2C_WaitOnRXNEFlagUntilTimeout(hi2c, Timeout, tickstart) != HAL_OK)      
          {
            if(hi2c->ErrorCode == HAL_I2C_ERROR_TIMEOUT)
            {
              return HAL_TIMEOUT;
            }
            else
            {
              return HAL_ERROR;
            }
          }

          /* Read data from DR */
          (*hi2c->pBuffPtr++) = hi2c->Instance->DR;
          hi2c->XferSize--;
          hi2c->XferCount--;
        }
        /* Two bytes */
        else if(hi2c->XferSize == 2U)
        {
          /* Wait until BTF flag is set */
          if(I2C_WaitOnFlagUntilTimeout(hi2c, I2C_FLAG_BTF, RESET, Timeout, tickstart) != HAL_OK)
          {
            return HAL_TIMEOUT;
          }

          if (LastOp)
          {
            /* Generate Stop */
            hi2c->Instance->CR1 |= I2C_CR1_STOP;
          }

          /* Read data from DR */
          (*hi2c->pBuffPtr++) = hi2c->Instance->DR;
          hi2c->XferSize--;
          hi2c->XferCount--;

          /* Read data from DR */
          (*hi2c->pBuffPtr++) = hi2c->Instance->DR;
          hi2c->XferSize--;
          hi2c->XferCount--;
        }
        /* 3 Last bytes */
        else
        {
          /* Wait until BTF flag is set */
          if(I2C_WaitOnFlagUntilTimeout(hi2c, I2C_FLAG_BTF, RESET, Timeout, tickstart) != HAL_OK)
          {
            return HAL_TIMEOUT;
          }

          /* Disable Acknowledge */
          hi2c->Instance->CR1 &= ~I2C_CR1_ACK;

          /* Read data from DR */
          (*hi2c->pBuffPtr++) = hi2c->Instance->DR;
          hi2c->XferSize--;
          hi2c->XferCount--;

          /* Wait until BTF flag is set */
          if(I2C_WaitOnFlagUntilTimeout(hi2c, I2C_FLAG_BTF, RESET, Timeout, tickstart) != HAL_OK)
          {
            return HAL_TIMEOUT;
          }

          if (LastOp)
          {
            /* Generate Stop */
            hi2c->Instance->CR1 |= I2C_CR1_STOP;
          }

          /* Read data from DR */
          (*hi2c->pBuffPtr++) = hi2c->Instance->DR;
          hi2c->XferSize--;
          hi2c->XferCount--;

          /* Read data from DR */
          (*hi2c->pBuffPtr++) = hi2c->Instance->DR;
          hi2c->XferSize--;
          hi2c->XferCount--;
        }
      }
      else
      {
        /* Wait until RXNE flag is set */
        if(I2C_WaitOnRXNEFlagUntilTimeout(hi2c, Timeout, tickstart) != HAL_OK)      
        {
          if(hi2c->ErrorCode == HAL_I2C_ERROR_TIMEOUT)
          {
            return HAL_TIMEOUT;
          }
          else
          {
            return HAL_ERROR;
          }
        }

        /* Read data from DR */
        (*hi2c->pBuffPtr++) = hi2c->Instance->DR;
        hi2c->XferSize--;
        hi2c->XferCount--;

        if(__HAL_I2C_GET_FLAG(hi2c, I2C_FLAG_BTF) == SET)
        {
          /* Read data from DR */
          (*hi2c->pBuffPtr++) = hi2c->Instance->DR;
          hi2c->XferSize--;
          hi2c->XferCount--;
        }
      }
    }

    hi2c->State = HAL_I2C_STATE_READY;

    if (LastOp) {
      hi2c->Mode = HAL_I2C_MODE_NONE;
    } else {
      hi2c->Mode = HAL_I2C_MODE_MASTER_SEL;
    }

    /* Process Unlocked */
    __HAL_UNLOCK(hi2c);

    return HAL_OK;
  }
  else
  {
    return HAL_BUSY;
  }
}

#endif /* !defined(STM32F3) && !defined(STM32F7) */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
