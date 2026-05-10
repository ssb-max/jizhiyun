/**
 ************************************************************
 * @file    hal_stm32f10x.h
 * @brief   HAL 兼容层（适配机智云生成的 HAL 库代码到本工程的标准外设库）
 *
 * 用途：
 *   机智云在线生成的 Gizwits/Utils 代码使用 STM32 HAL 库 API（HAL_*、
 *   UART_HandleTypeDef、TIM_HandleTypeDef 等），而本工程使用的是标准外设
 *   库（StdPeriph）。本文件提供一层最小 HAL 兼容定义/函数实现，使得
 *   Gizwits/Utils 文件夹下的代码可以原封不动直接编译通过并运行。
 *
 *   后续若官方再次更新 Gizwits/Utils，可以直接覆盖那两个文件夹，
 *   无需任何修改即可继续使用本兼容层。
 *
 * 硬件资源映射（无需改动硬件接线）：
 *
 *   逻辑名   Instance(保留)  物理串口  引脚          用途
 *   huart2    USART2         USART1   PA9/PA10     Wi-Fi 模块（机智云协议）9600
 *   huart1    USART1         USART2   PA2/PA3      printf 调试            115200
 *   htim2     TIM3           TIM3     —            1ms 系统节拍
 *
 *   关键约束：gizwits_product.c 中硬编码了
 *     "if (UartHandle->Instance == USART2)"
 *   因此 huart2.Instance 必须保持 USART2，但通过 get_phys() 函数把实际
 *   I/O 透明重定向到物理 USART1（PA9/PA10），无需改动硬件接线。
 ***********************************************************/

#ifndef __HAL_STM32F10X_H__
#define __HAL_STM32F10X_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "stm32f10x.h"

/* ----------------- 通用 HAL 状态枚举 ----------------- */
typedef enum
{
    HAL_OK       = 0x00U,
    HAL_ERROR    = 0x01U,
    HAL_BUSY     = 0x02U,
    HAL_TIMEOUT  = 0x03U
} HAL_StatusTypeDef;

/* ----------------- UART 状态宏（与 ST HAL 保持一致的取值） ----------------- */
#define HAL_UART_STATE_RESET        0x00U
#define HAL_UART_STATE_READY        0x20U
#define HAL_UART_STATE_BUSY         0x24U
#define HAL_UART_STATE_BUSY_TX      0x21U
#define HAL_UART_STATE_BUSY_RX      0x22U
#define HAL_UART_STATE_BUSY_TX_RX   0x23U
#define HAL_UART_STATE_TIMEOUT      0xA0U
#define HAL_UART_STATE_ERROR        0xE0U

/* ----------------- UART 句柄（仿 HAL） ----------------- */
typedef struct __UART_HandleTypeDef
{
    USART_TypeDef     *Instance;        /*!< 关联的硬件 USART 寄存器指针 */
    uint8_t           *pRxBuffPtr;      /*!< 当前接收数据指针             */
    uint16_t           RxXferSize;      /*!< 期望接收字节数               */
    volatile uint16_t  RxXferCount;     /*!< 剩余接收字节数               */
    volatile uint32_t  gState;          /*!< 全局/发送状态（ Gizwits 会查询）*/
    volatile uint32_t  RxState;         /*!< 接收状态                     */
} UART_HandleTypeDef;

/* ----------------- TIM 句柄（仿 HAL） ----------------- */
typedef struct __TIM_HandleTypeDef
{
    TIM_TypeDef *Instance;              /*!< 关联的硬件 TIM 寄存器指针    */
} TIM_HandleTypeDef;

/* ----------------- Gizwits 期望的外部全局句柄 ----------------- */
extern UART_HandleTypeDef huart1;       /*!< printf 调试串口 -> USART1   */
extern UART_HandleTypeDef huart2;       /*!< 机智云 Wi-Fi 串口 -> USART2 */
extern TIM_HandleTypeDef  htim2;        /*!< 1ms 节拍定时器 -> TIM3      */

/* ----------------- HAL 兼容 API（Gizwits 代码会调用） ----------------- */
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *huart,
                                    uint8_t *pData,
                                    uint16_t Size,
                                    uint32_t Timeout);

HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef *huart,
                                       uint8_t *pData,
                                       uint16_t Size);

HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *huart,
                                      uint8_t *pData,
                                      uint16_t Size);

HAL_StatusTypeDef HAL_TIM_Base_Start_IT(TIM_HandleTypeDef *htim);

void HAL_NVIC_SystemReset(void);

/* Gizwits 在 gizwits_product.c 中实现下面两个回调，本兼容层负责调用 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim);

/* ----------------- 桥接函数（在标准库的 ISR 中调用） -----------------
 * 用法：在标准库的 USART2_IRQHandler / TIM3_IRQHandler 中分别调用：
 *      HAL_UART_IRQHandler(&huart2);
 *      HAL_TIM_IRQHandler(&htim2);
 * --------------------------------------------------------------------- */
void HAL_UART_IRQHandler(UART_HandleTypeDef *huart);
void HAL_TIM_IRQHandler(TIM_HandleTypeDef *htim);

#ifdef __cplusplus
}
#endif

#endif /* __HAL_STM32F10X_H__ */
