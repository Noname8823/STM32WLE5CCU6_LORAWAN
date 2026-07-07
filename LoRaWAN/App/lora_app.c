/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    lora_app.c
  * @author  MCD Application Team
  * @brief   Application of the LRWAN Middleware
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "platform.h"
#include "Region.h" /* Needed for LORAWAN_DEFAULT_DATA_RATE */
#include "sys_app.h"
#include "lora_app.h"
#include "stm32_seq.h"
#include "stm32_timer.h"
#include "utilities_def.h"
#include "lora_app_version.h"
#include "lorawan_version.h"
#include "subghz_phy_version.h"
#include "lora_info.h"
#include "LmHandler.h"
#include "stm32_lpm.h"
#include "adc_if.h"
#include "sys_conf.h"
#include "CayenneLpp.h"
#include "sys_sensors.h"
#include "app_config.h"

/* USER CODE BEGIN Includes */
#include "isolated_input.h"
#include "rs485.h"
#include <string.h>
/* USER CODE END Includes */

/* External variables ---------------------------------------------------------*/
/* USER CODE BEGIN EV */

/* USER CODE END EV */

/* Private typedef -----------------------------------------------------------*/
/**
  * @brief LoRa State Machine states
  */
typedef enum TxEventType_e
{
  /**
    * @brief Appdata Transmission issue based on timer every TxDutyCycleTime
    */
  TX_ON_TIMER,
  /**
    * @brief Appdata Transmission external event plugged on OnSendEvent( )
    */
  TX_ON_EVENT
  /* USER CODE BEGIN TxEventType_t */

  /* USER CODE END TxEventType_t */
} TxEventType_t;

/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define APP_PAYLOAD_VERSION        0x01U
#define APP_MSG_BOARD_STATUS       0x10U

/*
 * Giới hạn RS485 data gửi lên LoRaWAN.
 * Để test ban đầu nên để nhỏ.
 */
#define APP_RS485_MAX_UPLINK       45U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private function prototypes -----------------------------------------------*/
/**
  * @brief  LoRa End Node send request
  */
static void SendTxData(void);

/**
  * @brief  TX timer callback function
  * @param  context ptr of timer context
  */
static void OnTxTimerEvent(void *context);

/**
  * @brief  join event callback function
  * @param  joinParams status of join
  */
static void OnJoinRequest(LmHandlerJoinParams_t *joinParams);

/**
  * @brief  tx event callback function
  * @param  params status of last Tx
  */
static void OnTxData(LmHandlerTxParams_t *params);

/**
  * @brief callback when LoRa application has received a frame
  * @param appData data received in the last Rx
  * @param params status of last Rx
  */
static void OnRxData(LmHandlerAppData_t *appData, LmHandlerRxParams_t *params);

/*!
 * Will be called each time a Radio IRQ is handled by the MAC layer
 *
 */
static void OnMacProcessNotify(void);

/* USER CODE BEGIN PFP */

static void LoRaWAN_ApplyStoredConfig(void);

/**
  * @brief  LED Tx timer callback function
  * @param  context ptr of LED context
  */
static void OnTxTimerLedEvent(void *context);

/**
  * @brief  LED Rx timer callback function
  * @param  context ptr of LED context
  */
static void OnRxTimerLedEvent(void *context);

/**
  * @brief  LED Join timer callback function
  * @param  context ptr of LED context
  */
static void OnJoinTimerLedEvent(void *context);

/* USER CODE END PFP */

/* Private variables ---------------------------------------------------------*/
static ActivationType_t ActivationType = LORAWAN_DEFAULT_ACTIVATION_TYPE;

/**
  * @brief LoRaWAN handler Callbacks
  */
static LmHandlerCallbacks_t LmHandlerCallbacks =
{
  .GetBatteryLevel =           GetBatteryLevel,
  .GetTemperature =            GetTemperatureLevel,
  .GetUniqueId =               GetUniqueId,
  .GetDevAddr =                GetDevAddr,
  .OnMacProcess =              OnMacProcessNotify,
  .OnJoinRequest =             OnJoinRequest,
  .OnTxData =                  OnTxData,
  .OnRxData =                  OnRxData
};

/**
  * @brief LoRaWAN handler parameters
  */
static LmHandlerParams_t LmHandlerParams =
{
  .ActiveRegion =             ACTIVE_REGION,
  .DefaultClass =             LORAWAN_DEFAULT_CLASS,
  .AdrEnable =                LORAWAN_ADR_STATE,
  .TxDatarate =               LORAWAN_DEFAULT_DATA_RATE,
  .PingPeriodicity =          LORAWAN_DEFAULT_PING_SLOT_PERIODICITY
};

/**
  * @brief Type of Event to generate application Tx
  */
static TxEventType_t EventType = TX_ON_TIMER;

/**
  * @brief Timer to handle the application Tx
  */
static UTIL_TIMER_Object_t TxTimer;

/* USER CODE BEGIN PV */
/**
  * @brief User application buffer
  */
static uint8_t AppDataBuffer[LORAWAN_APP_DATA_BUFFER_MAX_SIZE];

/**
  * @brief User application data structure
  */
static LmHandlerAppData_t AppData = { 0, 0, AppDataBuffer };

static uint8_t AppSeq = 0U;

/**
  * @brief Specifies the state of the application LED
  */
static uint8_t AppLedStateOn = RESET;

/**
  * @brief Timer to handle the application Tx Led to toggle
  */
static UTIL_TIMER_Object_t TxLedTimer;

/**
  * @brief Timer to handle the application Rx Led to toggle
  */
static UTIL_TIMER_Object_t RxLedTimer;

/**
  * @brief Timer to handle the application Join Led to toggle
  */
static UTIL_TIMER_Object_t JoinLedTimer;

/* USER CODE END PV */

/* Exported functions ---------------------------------------------------------*/
/* USER CODE BEGIN EF */

/* USER CODE END EF */

void LoRaWAN_Init(void)
{
  /* USER CODE BEGIN LoRaWAN_Init_1 */

  IsolatedInput_Init();
  RS485_Init();

  BSP_LED_Init(LED_BLUE);
  BSP_LED_Init(LED_GREEN);
  BSP_LED_Init(LED_RED);
  BSP_PB_Init(BUTTON_SW2, BUTTON_MODE_EXTI);

  /* Get LoRa APP version*/
  APP_LOG(TS_OFF, VLEVEL_M, "APP_VERSION:        V%X.%X.%X\r\n",
          (uint8_t)(__LORA_APP_VERSION >> __APP_VERSION_MAIN_SHIFT),
          (uint8_t)(__LORA_APP_VERSION >> __APP_VERSION_SUB1_SHIFT),
          (uint8_t)(__LORA_APP_VERSION >> __APP_VERSION_SUB2_SHIFT));

  /* Get MW LoraWAN info */
  APP_LOG(TS_OFF, VLEVEL_M, "MW_LORAWAN_VERSION: V%X.%X.%X\r\n",
          (uint8_t)(__LORAWAN_VERSION >> __APP_VERSION_MAIN_SHIFT),
          (uint8_t)(__LORAWAN_VERSION >> __APP_VERSION_SUB1_SHIFT),
          (uint8_t)(__LORAWAN_VERSION >> __APP_VERSION_SUB2_SHIFT));

  /* Get MW SubGhz_Phy info */
  APP_LOG(TS_OFF, VLEVEL_M, "MW_RADIO_VERSION:   V%X.%X.%X\r\n",
          (uint8_t)(__SUBGHZ_PHY_VERSION >> __APP_VERSION_MAIN_SHIFT),
          (uint8_t)(__SUBGHZ_PHY_VERSION >> __APP_VERSION_SUB1_SHIFT),
          (uint8_t)(__SUBGHZ_PHY_VERSION >> __APP_VERSION_SUB2_SHIFT));

  UTIL_TIMER_Create(&TxLedTimer, 0xFFFFFFFFU, UTIL_TIMER_ONESHOT, OnTxTimerLedEvent, NULL);
  UTIL_TIMER_Create(&RxLedTimer, 0xFFFFFFFFU, UTIL_TIMER_ONESHOT, OnRxTimerLedEvent, NULL);
  UTIL_TIMER_Create(&JoinLedTimer, 0xFFFFFFFFU, UTIL_TIMER_PERIODIC, OnJoinTimerLedEvent, NULL);
  UTIL_TIMER_SetPeriod(&TxLedTimer, 500);
  UTIL_TIMER_SetPeriod(&RxLedTimer, 500);
  UTIL_TIMER_SetPeriod(&JoinLedTimer, 500);

  /* USER CODE END LoRaWAN_Init_1 */

  UTIL_SEQ_RegTask((1 << CFG_SEQ_Task_LmHandlerProcess), UTIL_SEQ_RFU, LmHandlerProcess);
  UTIL_SEQ_RegTask((1 << CFG_SEQ_Task_LoRaSendOnTxTimerOrButtonEvent), UTIL_SEQ_RFU, SendTxData);
  /* Init Info table used by LmHandler*/
  LoraInfo_Init();

  /* Init the Lora Stack*/
  LmHandlerInit(&LmHandlerCallbacks);

  LmHandlerConfigure(&LmHandlerParams);

  /* USER CODE BEGIN LoRaWAN_Init_2 */
  UTIL_TIMER_Start(&JoinLedTimer);

  /* USER CODE END LoRaWAN_Init_2 */

  /* USER CODE BEGIN LoRaWAN_Config_Apply */
  LoRaWAN_ApplyStoredConfig();
  /* USER CODE END LoRaWAN_Config_Apply */

  LmHandlerJoin(ActivationType);

  if (EventType == TX_ON_TIMER)
  {
    /* send every time timer elapses */
    UTIL_TIMER_Create(&TxTimer,  0xFFFFFFFFU, UTIL_TIMER_ONESHOT, OnTxTimerEvent, NULL);
    UTIL_TIMER_SetPeriod(&TxTimer,  APP_TX_DUTYCYCLE);
    UTIL_TIMER_Start(&TxTimer);
  }
  else
  {
    /* USER CODE BEGIN LoRaWAN_Init_3 */

    /* send every time button is pushed */
    BSP_PB_Init(BUTTON_SW1, BUTTON_MODE_EXTI);
    /* USER CODE END LoRaWAN_Init_3 */
  }

  /* USER CODE BEGIN LoRaWAN_Init_Last */

  /* USER CODE END LoRaWAN_Init_Last */
}

/* USER CODE BEGIN PB_Callbacks */
/* Note: Current the stm32wlxx_it.c generated by STM32CubeMX does not support BSP for PB in EXTI mode. */
/* In order to get a push button IRS by code automatically generated */
/* HAL_GPIO_EXTI_Callback is today the only available possibility. */
/* Using HAL_GPIO_EXTI_Callback() shortcuts the BSP. */
/* If users wants to go through the BSP, stm32wlxx_it.c should be updated  */
/* in the USER CODE SESSION of the correspondent EXTIn_IRQHandler() */
/* to call the BSP_PB_IRQHandler() or the HAL_EXTI_IRQHandler(&H_EXTI_n);. */
/* Then the below HAL_GPIO_EXTI_Callback() can be replaced by BSP callback */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  switch (GPIO_Pin)
  {
    case  BUTTON_SW1_PIN:
      /* Note: when "EventType == TX_ON_TIMER" this GPIO is not initialized */
      UTIL_SEQ_SetTask((1 << CFG_SEQ_Task_LoRaSendOnTxTimerOrButtonEvent), CFG_SEQ_Prio_0);
      break;
    case  BUTTON_SW2_PIN:
      break;
    case  BUTTON_SW3_PIN:
      break;
    default:
      break;
  }
}

/* USER CODE END PB_Callbacks */

/* Private functions ---------------------------------------------------------*/
/* USER CODE BEGIN PrFD */

static void LoRaWAN_ApplyStoredConfig(void)
{
    if (AppConfig_Load() == true)
    {
        APP_LOG(TS_OFF, VLEVEL_M, "APP_CONFIG: loaded from Flash\r\n");

        if (g_app_config.use_custom_dev_eui != 0U)
        {
            if (LmHandlerSetDevEUI(g_app_config.dev_eui) == LORAMAC_HANDLER_SUCCESS)
            {
                APP_LOG(TS_OFF, VLEVEL_M, "APP_CONFIG: custom DevEUI applied\r\n");
            }
            else
            {
                APP_LOG(TS_OFF, VLEVEL_M, "APP_CONFIG: custom DevEUI failed\r\n");
            }
        }
        else
        {
            APP_LOG(TS_OFF, VLEVEL_M, "APP_CONFIG: use UID DevEUI\r\n");
        }

        /*
         * LmHandler dùng tên AppEUI, tương đương JoinEUI trong naming mới.
         */
        if (LmHandlerSetAppEUI(g_app_config.join_eui) == LORAMAC_HANDLER_SUCCESS)
        {
            APP_LOG(TS_OFF, VLEVEL_M, "APP_CONFIG: JoinEUI/AppEUI applied\r\n");
        }
        else
        {
            APP_LOG(TS_OFF, VLEVEL_M, "APP_CONFIG: JoinEUI/AppEUI failed\r\n");
        }

        /*
         * Với TTN/LoRaWAN 1.0.x trên stack ST, thường set cả AppKey và NwkKey.
         */
        if (LmHandlerSetAppKey(g_app_config.app_key) == LORAMAC_HANDLER_SUCCESS)
        {
            APP_LOG(TS_OFF, VLEVEL_M, "APP_CONFIG: AppKey applied\r\n");
        }
        else
        {
            APP_LOG(TS_OFF, VLEVEL_M, "APP_CONFIG: AppKey failed\r\n");
        }

        if (LmHandlerSetNwkKey(g_app_config.nwk_key) == LORAMAC_HANDLER_SUCCESS)
        {
            APP_LOG(TS_OFF, VLEVEL_M, "APP_CONFIG: NwkKey applied\r\n");
        }
        else
        {
            APP_LOG(TS_OFF, VLEVEL_M, "APP_CONFIG: NwkKey failed\r\n");
        }
    }
    else
    {
        APP_LOG(TS_OFF, VLEVEL_M, "APP_CONFIG: not found, use se-identity.h default\r\n");
    }
}

/* USER CODE END PrFD */

static void OnRxData(LmHandlerAppData_t *appData, LmHandlerRxParams_t *params)
{
  /* USER CODE BEGIN OnRxData_1 */
  if ((appData == NULL) || (params == NULL))
  {
    return;
  }

  BSP_LED_On(LED_BLUE);

  UTIL_TIMER_Start(&RxLedTimer);

  static const char *slotStrings[] = { "1", "2", "C", "C Multicast", "B Ping-Slot", "B Multicast Ping-Slot" };

  APP_LOG(TS_OFF, VLEVEL_M, "\r\n###### ========== MCPS-Indication ==========\r\n");
  APP_LOG(TS_OFF, VLEVEL_H, "###### D/L FRAME:%04d | SLOT:%s | PORT:%d | DR:%d | RSSI:%d | SNR:%d\r\n",
          params->DownlinkCounter, slotStrings[params->RxSlot], appData->Port, params->Datarate, params->Rssi, params->Snr);

  switch (appData->Port)
  {
    case LORAWAN_SWITCH_CLASS_PORT:
      if (appData->BufferSize == 1)
      {
        switch (appData->Buffer[0])
        {
          case 0:
            LmHandlerRequestClass(CLASS_A);
            break;
          case 1:
            LmHandlerRequestClass(CLASS_B);
            break;
          case 2:
            LmHandlerRequestClass(CLASS_C);
            break;
          default:
            break;
        }
      }
      break;

    case LORAWAN_USER_APP_PORT:
      if (appData->BufferSize == 1)
      {
        AppLedStateOn = appData->Buffer[0] & 0x01U;

        if (AppLedStateOn == RESET)
        {
          APP_LOG(TS_OFF, VLEVEL_H, "LED OFF\r\n");
          BSP_LED_Off(LED_RED);
        }
        else
        {
          APP_LOG(TS_OFF, VLEVEL_H, "LED ON\r\n");
          BSP_LED_On(LED_RED);
        }
      }
      break;

    default:
      break;
  }
  /* USER CODE END OnRxData_1 */
}

static void SendTxData(void)
{
  /* USER CODE BEGIN SendTxData_1 */
  UTIL_TIMER_Time_t nextTxIn = 0;
  uint32_t i = 0U;

  uint8_t input_state;
  uint8_t rs485_buf[APP_RS485_MAX_UPLINK];
  uint16_t rs485_len_16 = 0U;
  uint8_t rs485_len = 0U;
  RS485_Status_t rs485_status;

  AppData.Port = LORAWAN_USER_APP_PORT;

  /*
   * Đọc 4 input opto.
   *
   * Quy ước:
   * bit0 = IN1
   * bit1 = IN2
   * bit2 = IN3
   * bit3 = IN4
   *
   * 0 = có tín hiệu
   * 1 = không có tín hiệu
   *
   * Chưa có tín hiệu gì:
   * IN1=1, IN2=1, IN3=1, IN4=1
   * input_state = 0000 1111 = 0x0F
   */
  input_state = IsolatedInput_ReadRaw();
  uint8_t input_active = (uint8_t)((~input_state) & 0x0FU);

  /*
   * Đọc RS485 nếu đang có data.
   * Nếu chưa có data RS485 thì status = RS485_STATUS_NONE, len = 0.
   */
  rs485_status = RS485_ReadAvailable(rs485_buf,
                                     APP_RS485_MAX_UPLINK,
                                     &rs485_len_16,
                                     3U);

  if (rs485_status != RS485_STATUS_OK)
  {
    rs485_len_16 = 0U;
  }

  if (rs485_len_16 > APP_RS485_MAX_UPLINK)
  {
    rs485_len_16 = APP_RS485_MAX_UPLINK;
  }

  rs485_len = (uint8_t)rs485_len_16;

  /*
   * Payload format:
   *
   * Byte 0    = version
   * Byte 1    = msg_type
   * Byte 2    = sequence
   * Byte 3    = input_state
   * Byte 4    = rs485_status
   * Byte 5    = rs485_len
   * Byte 6... = rs485_data
   */
  AppData.Buffer[i++] = APP_PAYLOAD_VERSION;
  AppData.Buffer[i++] = APP_MSG_BOARD_STATUS;
  AppData.Buffer[i++] = AppSeq++;
  AppData.Buffer[i++] = input_active;
  AppData.Buffer[i++] = (uint8_t)rs485_status;
  AppData.Buffer[i++] = rs485_len;

  if (rs485_len > 0U)
  {
    (void)memcpy(&AppData.Buffer[i], rs485_buf, rs485_len);
    i += rs485_len;
  }

  AppData.BufferSize = i;

  APP_LOG(TS_ON, VLEVEL_M,
          "UL: input_raw=0x%02X input_active=0x%02X rs485_status=%d rs485_len=%d\r\n",
          input_state,
          input_active,
          rs485_status,
          rs485_len);

  if (LORAMAC_HANDLER_SUCCESS == LmHandlerSend(&AppData,
                                               LORAWAN_DEFAULT_CONFIRMED_MSG_STATE,
                                               &nextTxIn,
                                               false))
  {
    APP_LOG(TS_ON, VLEVEL_L, "SEND REQUEST\r\n");
  }
  else if (nextTxIn > 0U)
  {
    APP_LOG(TS_ON, VLEVEL_L,
            "Next Tx in  : ~%d second(s)\r\n",
            (nextTxIn / 1000U));
  }

  /* USER CODE END SendTxData_1 */
}

static void OnTxTimerEvent(void *context)
{
  /* USER CODE BEGIN OnTxTimerEvent_1 */

  /* USER CODE END OnTxTimerEvent_1 */
  UTIL_SEQ_SetTask((1 << CFG_SEQ_Task_LoRaSendOnTxTimerOrButtonEvent), CFG_SEQ_Prio_0);

  /*Wait for next tx slot*/
  UTIL_TIMER_Start(&TxTimer);
  /* USER CODE BEGIN OnTxTimerEvent_2 */

  /* USER CODE END OnTxTimerEvent_2 */
}

/* USER CODE BEGIN PrFD_LedEvents */
static void OnTxTimerLedEvent(void *context)
{
  BSP_LED_Off(LED_GREEN) ;
}

static void OnRxTimerLedEvent(void *context)
{
  BSP_LED_Off(LED_BLUE) ;
}

static void OnJoinTimerLedEvent(void *context)
{
  BSP_LED_Toggle(LED_RED) ;
}

/* USER CODE END PrFD_LedEvents */

static void OnTxData(LmHandlerTxParams_t *params)
{
  /* USER CODE BEGIN OnTxData_1 */
  if ((params != NULL))
  {
    /* Process Tx event only if its a mcps response to prevent some internal events (mlme) */
    if (params->IsMcpsConfirm != 0)
    {
      BSP_LED_On(LED_GREEN) ;
      UTIL_TIMER_Start(&TxLedTimer);

      APP_LOG(TS_OFF, VLEVEL_M, "\r\n###### ========== MCPS-Confirm =============\r\n");
      APP_LOG(TS_OFF, VLEVEL_H, "###### U/L FRAME:%04d | PORT:%d | DR:%d | PWR:%d", params->UplinkCounter,
              params->AppData.Port, params->Datarate, params->TxPower);

      APP_LOG(TS_OFF, VLEVEL_H, " | MSG TYPE:");
      if (params->MsgType == LORAMAC_HANDLER_CONFIRMED_MSG)
      {
        APP_LOG(TS_OFF, VLEVEL_H, "CONFIRMED [%s]\r\n", (params->AckReceived != 0) ? "ACK" : "NACK");
      }
      else
      {
        APP_LOG(TS_OFF, VLEVEL_H, "UNCONFIRMED\r\n");
      }
    }
  }
  /* USER CODE END OnTxData_1 */
}

static void OnJoinRequest(LmHandlerJoinParams_t *joinParams)
{
  /* USER CODE BEGIN OnJoinRequest_1 */
  if (joinParams != NULL)
  {
    if (joinParams->Status == LORAMAC_HANDLER_SUCCESS)
    {
      UTIL_TIMER_Stop(&JoinLedTimer);
      BSP_LED_Off(LED_RED) ;

      APP_LOG(TS_OFF, VLEVEL_M, "\r\n###### = JOINED = ");
      if (joinParams->Mode == ACTIVATION_TYPE_ABP)
      {
        APP_LOG(TS_OFF, VLEVEL_M, "ABP ======================\r\n");
      }
      else
      {
        APP_LOG(TS_OFF, VLEVEL_M, "OTAA =====================\r\n");
      }
    }
    else
    {
      APP_LOG(TS_OFF, VLEVEL_M, "\r\n###### = JOIN FAILED\r\n");
    }
  }
  /* USER CODE END OnJoinRequest_1 */
}

static void OnMacProcessNotify(void)
{
  /* USER CODE BEGIN OnMacProcessNotify_1 */

  /* USER CODE END OnMacProcessNotify_1 */
  UTIL_SEQ_SetTask((1 << CFG_SEQ_Task_LmHandlerProcess), CFG_SEQ_Prio_0);

  /* USER CODE BEGIN OnMacProcessNotify_2 */

  /* USER CODE END OnMacProcessNotify_2 */
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
