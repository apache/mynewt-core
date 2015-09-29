/**
  ******************************************************************************
  * @file    stm32f3xx_hal_ppp.h
  * @author  MCD Application Team
  * @version V1.1.1
  * @date    19-June-2015
  * @brief   Header file of PPP HAL module.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT(c) 2015 STMicroelectronics</center></h2>
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __STM32F3xx_HAL_PPP_H
#define __STM32F3xx_HAL_PPP_H

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f3xx_hal_def.h"

/** @addtogroup STM32F3xx_HAL_Driver
  * @{
  */

/** @addtogroup PPP PPP HAL module driver
  * @{
  */ 

/* Exported types ------------------------------------------------------------*/ 

/** @defgroup PPP_Exported_Types PPP Exported Types
  * @{
  */
   
/** 
  * @brief PPP 
  */

typedef struct
{
  __IO uint32_t REGx;       /*!< PPP register x             Address offset: 0x00 */
} PPP_TypeDef;

/** 
  * @brief  PPP Configuration Structure definition  
  */
typedef struct
{
  uint32_t Config1; /*!< Add Config1 description
                        This parameter can be any value of @ref PPP_CONFIGx_Define   */           

  uint32_t Config2;  /*!< Add Config2 description
                          This parameter can be any value of @ref PPP_CONFIGx_Define */        
                               
  uint32_t Config3;  /*!< Add Config3 description
                          This parameter can be any value of @ref PPP_CONFIGx_Define */            

  uint32_t Config4;  /*!< Add Config4 description
                          This parameter can be any value of @ref PPP_CONFIGx_Define */              

  uint32_t Config5;  /*!< Add Config5 description
                          This parameter can be any value of @ref PPP_CONFIGx_Define */                
                               
  uint32_t Config6;  /*!< Add Config6 description
                          This parameter can be any value of @ref PPP_CONFIGx_Define */           
} PPP_InitTypeDef;

/** 
  * @brief  HAL State structures definition  
  */ 
typedef enum
{
  HAL_PPP_STATE_READY      = 0x01,    /*!< Peripheral Initialized and ready for use           */
  HAL_PPP_STATE_BUSY       = 0x02,    /*!< an internal process is ongoing                     */   
  HAL_PPP_STATE_BUSY_TX    = 0x03,    /*!< Data Transmission process is ongoing               */ 
  HAL_PPP_STATE_BUSY_RX    = 0x04,    /*!< Data Reception process is ongoing                  */
  HAL_PPP_STATE_BUSY_TX_RX = 0x05,    /*!< Data Transmission and Reception process is ongoing */    
  HAL_PPP_STATE_TIMEOUT    = 0x06,    /*!< Timeout state                                      */  
  HAL_PPP_STATE_ERROR      = 0x07,    /*!< Error state                                        */      
  HAL_PPP_STATE_DISABLED   = 0x08     /*!< Disabled state                                     */      
                                                                        
}HAL_PPP_StateTypeDef;


/** 
  * @brief  PPP Handle Structure definition  
  */ 
typedef struct
{
  PPP_TypeDef       *Instance;    /*!< Register base address   */ 
  PPP_InitTypeDef   Config;       /*!< PPP required parameters */
  
  /* !!! Add HAL PPP required handler parameters !!! */
  
  HAL_StatusTypeDef Status;       /*!< PPP peripheral status   */
  HAL_LockTypeDef   Lock;         /*!< Locking object          */
  __IO HAL_PPP_StateTypeDef  State;  /*!< PPP communication state */
  
} PPP_HandleTypeDef;

/** 
  * @brief  PPP Configuration enumeration values definition  
  */
typedef enum 
{
  Control1     = 0,   /*!< Control related Config1 Paramater in PPP_InitTypeDef  */
  Control2     = 1,   /*!< Control related Config2 Paramater in PPP_InitTypeDef  */
  Control3     = 2,   /*!< Control related Config3 Paramater in PPP_InitTypeDef  */
  Control4     = 3,   /*!< Control related Config4 Paramater in PPP_InitTypeDef  */
  Control5     = 4,   /*!< Control related Config5 Paramater in PPP_InitTypeDef  */
  Control6     = 5    /*!< Control related Config6 Paramater in PPP_InitTypeDef  */
}PPP_ControlTypeDef;

/**
  * @}
  */

/* Exported constants --------------------------------------------------------*/
/** @defgroup PPP_Exported_Constants PPP Exported Constants
  * @{
  */

/** @defgroup PPP_CONFIGx_Define PPP CONFIGx Define
  * @{
  */  
#define PPP_CONFIGx_VALUE1       ((uint32_t)0x00)  /*!< PPP Config Value description      */
#define PPP_CONFIGx_VALUE2       ((uint32_t)0x01)  /*!< PPP Config Value description      */
#define PPP_CONFIGx_VALUE3       ((uint32_t)0x02)  /*!< PPP Config Value description      */

#define IS_PPP_CONFIGx(CONFIG)   (((CONFIG) == PPP_CONFIGx_VALUE1)  || \
                                  ((CONFIG) == PPP_CONFIGx_VALUE2)  || \
                                  ((CONFIG) == PPP_CONFIGx_VALUE3))

/**
  * @}
  */

/** @defgroup PPP_Flag_definition PPP Flag definition
  * @brief Flag definition
  *        Elements values convention: 0x00000ZZZZ
  *           - ZZZZ: Flag mask
  *
  * @#define PPP_FLAG_xxxx           ((uint32_t)0x0000ZZZZ)
  * @{
  */ 
#define PPP_FLAG_TC              ((uint32_t)0x00000002)
#define PPP_FLAG_RXNE            ((uint32_t)0x00000001)

#define IS_PPP_FLAG(FLAG)        (((FLAG) == PPP_FLAG_TC)   || \
                                  ((FLAG) == PPP_FLAG_RXNE))
/**
  * @}
  */


/** @defgroup PPP_Interrupt_definition PPP Interrupt definition
  * @brief PPP Interrupt definition
  *        Elements values convention: 0xXXYYZZZZ
  *           - XX  : Interrupt register Index
  *           - YY  : Interrupt Source Position
  *           - ZZZZ: Interrupt mask
  *
  * @#define PPP_IT_xxxx           ((uint32_t)0xXXYYZZZZ)
  * @{
  */ 
#define PPP_IT_TC             ((uint32_t)0x00000002)
#define PPP_IT_RXNE           ((uint32_t)0x00000001)

#define IS_PPP_IT(IT)         (((IT) == PPP_IT_TC)   || \
                               ((IT) == PPP_IT_RXNE))
/**
  * @}
  */

/** @defgroup PPP_Instance_definition PPP Instance definition
  * @{
  */ 
#define IS_PPP_ALL_INSTANCE(INSTANCE) (((INSTANCE) == PPP1) || \
                                       ((INSTANCE) == PPP2) || \
                                       ((INSTANCE) == PPP3) || \
                                       ((INSTANCE) == PPP4))
/**
  * @}
  */

/**
  * @}
  */ 
  
/* Exported macro ------------------------------------------------------------*/
/** @defgroup PPP_Exported_Macros PPP Exported Macros
  * @{
  */

/** @defgroup PPP_Interrupt_Clock PPP Interrupt Clock
 *  @brief macros to handle interrupts and specific clock configurations
 * @{
 */

#define __HAL_PPP_ENABLE(__HANDLE__)                    ((__HANDLE__)->Instance->REGx |= (ENABLE))
#define __HAL_PPP_DISABLE(__HANDLE__)                   ((__HANDLE__)->Instance->REGx &= ~(ENABLE))

#define __HAL_PPP_ENABLE_I(__HANDLE__, __INTERRUPT__)   ((__HANDLE__)->Instance->REGx |= (__INTERRUPT__))
#define __HAL_PPP_DISABLE_IT(__HANDLE__, __INTERRUPT__) ((__HANDLE__)->Instance->REGx &= ~(__INTERRUPT__))
#define __HAL_PPP_GET_IT(__HANDLE__, __INTERRUPT__)     ((__HANDLE__)->Instance->REGx & ((uint16_t)1 << ((__INTERRUPT__)>> 0x08))) 
#define __HAL_PPP_CLEAR_IT(__HANDLE__, __INTERRUPT__)   ((__HANDLE__)->Instance->REGx &= ((uint16_t)~((uint16_t)0x01 << (uint16_t)((__INTERRUPT__) >> 0x08)))) 
#define __HAL_PPP_GET_FLAG(__HANDLE__, __FLAG__)        (((__HANDLE__)->Instance->REGx & (__FLAG__)) == (__FLAG__))
#define __HAL_PPP_CLEAR_FLAG(__HANDLE__, __FLAG__)      ((__HANDLE__)->Instance->REGx &= ~(__FLAG__))

#define __HAL_PPP_PRESCALER(__HANDLE__, __PRESC__)
#define __HAL_PPP_YYYY(__HANDLE__, __PRESC__)

/**
  * @}
  */

/**
  * @}
  */

/* Exported functions --------------------------------------------------------*/
/** @addtogroup PPP_Exported_Functions PPP Exported Functions
  * @{
  */

/** @addtogroup PPP_Exported_Functions_Group1 Initialization and de-initialization functions
  * @{
  */
/* Initialization and de-initialization functions  ****************************/
HAL_StatusTypeDef HAL_PPP_Init(PPP_HandleTypeDef *hppp);
HAL_StatusTypeDef HAL_PPP_DeInit (PPP_HandleTypeDef *hppp);
void HAL_PPP_MspInit(PPP_HandleTypeDef *hppp);
void HAL_PPP_MspDeInit(PPP_HandleTypeDef *hppp);

/**
  * @}
  */

/** @addtogroup PPP_Exported_Functions_Group2 Input and Output operation functions 
  * @{
  */
/* IO operation functions *****************************************************/
 /* Blocking mode: Polling */
HAL_StatusTypeDef HAL_PPP_Transmit(PPP_HandleTypeDef *hppp, uint8_t *pData, uint16_t Size);
HAL_StatusTypeDef HAL_PPP_Receive(PPP_HandleTypeDef *hppp, uint8_t *pData, uint16_t Size);

 /* Non-Blocking mode: Interrupt */
HAL_StatusTypeDef HAL_PPP_Transmit_IT(PPP_HandleTypeDef *hppp, uint8_t *pData, uint16_t Size);
HAL_StatusTypeDef HAL_PPP_Receive_IT(PPP_HandleTypeDef *hppp, uint8_t *pData, uint16_t Size);
void HAL_PPP_IRQHandler(PPP_HandleTypeDef *hppp);

 /* Non-Blocking mode: DMA */
HAL_StatusTypeDef HAL_PPP_Transmit_DMA(PPP_HandleTypeDef *hppp, uint8_t *pData, uint16_t Size);
HAL_StatusTypeDef HAL_PPP_Receive_DMA(PPP_HandleTypeDef *hppp, uint8_t *pData, uint16_t Size);

 /* Callback in non blocking modes (Interrupt and DMA) */
void HAL_PPP_TxCpltCallback(PPP_HandleTypeDef *hppp);
void HAL_PPP_RxCpltCallback(PPP_HandleTypeDef *hppp);

/**
  * @}
  */

/** @addtogroup PPP_Exported_Functions_Group3 Peripheral Control functions 
  * @{
  */
/* Peripheral Control functions ***********************************************/
HAL_StatusTypeDef HAL_PPP_Ctl(PPP_HandleTypeDef *hppp, PPP_ControlTypeDef control, uint16_t *args);

/**
  * @}
  */

/** @addtogroup PPP_Exported_Functions_Group4 Peripheral State functions 
  * @{
  */
/* Peripheral State and Error functions ***************************************/
HAL_PPP_StateTypeDef HAL_PPP_GetState(PPP_HandleTypeDef *hppp);

/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */ 

/**
  * @}
  */ 

#ifdef __cplusplus
}
#endif

#endif /* __STM32F3xx_HAL_PPP_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
