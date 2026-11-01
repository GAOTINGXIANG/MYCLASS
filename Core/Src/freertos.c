/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "app_launcher.h"
#include "sys_sensor.h"
#include "sys_notify.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for LVGL */
osThreadId_t LVGLHandle;
const osThreadAttr_t LVGL_attributes = {
  .name = "LVGL",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityNormal7,
};
/* Definitions for sensor */
osThreadId_t sensorHandle;
const osThreadAttr_t sensor_attributes = {
  .name = "sensor",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for notify */
osThreadId_t notifyHandle;
const osThreadAttr_t notify_attributes = {
  .name = "notify",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityLow1,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);
void LVGL_TASK(void *argument);
void sensor_task(void *argument);
void notify_task(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* creation of LVGL */
  LVGLHandle = osThreadNew(LVGL_TASK, NULL, &LVGL_attributes);

  /* creation of sensor */
  sensorHandle = osThreadNew(sensor_task, NULL, &sensor_attributes);

  /* creation of notify */
  notifyHandle = osThreadNew(notify_task, NULL, &notify_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Header_LVGL_TASK */
/**
* @brief Function implementing the LVGL thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_LVGL_TASK */
void LVGL_TASK(void *argument)
{
  /* USER CODE BEGIN LVGL_TASK */
  lv_init();
  lv_port_disp_init();
  lv_port_indev_init();
  app_launcher_init();

  for(;;)
  {
    lv_tick_inc(5);
    lv_timer_handler();
    osDelay(5);
  }
  /* USER CODE END LVGL_TASK */
}

/* USER CODE BEGIN Header_sensor_task */
/**
* @brief Function implementing the sensor thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_sensor_task */
void sensor_task(void *argument)
{
  /* USER CODE BEGIN sensor_task */
  /* 阻塞型传感器集中采样：结果写入 g_sensor，UI 只读 */
  for(;;)
  {
    sys_sensor_poll();
    osDelay(20);
  }
  /* USER CODE END sensor_task */
}

/* USER CODE BEGIN Header_notify_task */
/**
* @brief Function implementing the notify thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_notify_task */
void notify_task(void *argument)
{
  /* USER CODE BEGIN notify_task */
  /* 声光提示服务：命令队列驱动，LED/蜂鸣器由本任务独占 */
  sys_notify_init();
  sys_notify_loop();
  /* USER CODE END notify_task */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

