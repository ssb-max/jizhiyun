/**
 ************************************************************
 * @file    hal_stm32f10x.c
 * @brief   HAL 兼容层实现
 *
 * 串口物理映射说明（重要！不要改动，与硬件接线对应）：
 *
 *   逻辑名  Instance(保留) 物理串口  引脚         用途
 *   huart2   USART2        USART1   PA9/PA10    Wi-Fi 模块（机智云协议）
 *   huart1   USART1        USART2   PA2/PA3     printf 调试输出
 *
 *   为什么这样映射？
 *     gizwits_product.c 中硬编码了:
 *       if (UartHandle->Instance == USART2)  // 机智云写死的判断
 *     因此 huart2.Instance 必须保持 USART2，Gizwits 的回调才能正确触发。
 *     但物理 Wi-Fi 模块实际接在 USART1（PA9/PA10），所以通过 get_phys()
 *     把 huart2 的全部 I/O 透明重定向到物理 USART1，两者解耦。
 *
 *   结论：你的硬件接线无需任何改动（Wi-Fi 继续接 PA9/PA10）。
 ***********************************************************/

#include "hal_stm32f10x.h"

/* ============================================================
 *  全局 HAL 句柄
 *  Instance 字段只用于 Gizwits 内部的"身份校验"判断，
 *  实际物理 I/O 通过下方 get_phys() 重定向。
 * ============================================================ */
UART_HandleTypeDef huart1 =
{
    USART1,               /* Instance = USART1（仅供标识，物理 I/O 走 USART2） */
    (uint8_t *)0,
    0,
    0,
    HAL_UART_STATE_READY,
    HAL_UART_STATE_READY
};

UART_HandleTypeDef huart2 =
{
    USART2,               /* Instance = USART2（满足 Gizwits 回调中的 ==USART2 判断）*/
    (uint8_t *)0,         /* 物理 I/O 实际走 USART1（PA9/PA10）= Wi-Fi 接线 */
    0,
    0,
    HAL_UART_STATE_READY,
    HAL_UART_STATE_READY
};

TIM_HandleTypeDef htim2 =
{
    TIM3   /* 物理定时器 TIM3，命名沿用 htim2 以匹配 Gizwits 的 "&htim2" 引用 */
};

/* ============================================================
 *  物理串口重定向
 *
 *  huart2 (逻辑 Wi-Fi)  → 物理 USART1 (PA9 TX / PA10 RX)
 *  huart1 (逻辑 printf) → 物理 USART2 (PA2 TX / PA3  RX)
 *
 *  只修改这里即可适应不同的硬件接线，其余代码无需动。
 * ============================================================ */
static USART_TypeDef *get_phys(UART_HandleTypeDef *huart)
{
    if (huart == &huart2) return USART1;   /* Wi-Fi 物理串口 */
    if (huart == &huart1) return USART2;   /* 调试串口 */
    return huart->Instance;                /* 兜底：按 Instance 走 */
}

/* ============================================================
 *  HAL_UART_Transmit —— 阻塞发送
 *  Gizwits 的 fputc (printf 重定向) 调用此函数
 * ============================================================ */
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *huart,
                                    uint8_t *pData,
                                    uint16_t Size,
                                    uint32_t Timeout)
{
    USART_TypeDef *port;
    uint16_t i;

    if (!huart || !pData || !Size) return HAL_ERROR;

    port = get_phys(huart);
    huart->gState = HAL_UART_STATE_BUSY_TX;

    for (i = 0; i < Size; i++)
    {
        while (USART_GetFlagStatus(port, USART_FLAG_TXE) == RESET)
        {
            (void)Timeout;
        }
        USART_SendData(port, (uint16_t)pData[i]);
    }
    while (USART_GetFlagStatus(port, USART_FLAG_TC) == RESET) {}

    huart->gState = HAL_UART_STATE_READY;
    return HAL_OK;
}

/* ============================================================
 *  HAL_UART_Transmit_IT —— "中断"发送
 *  Gizwits 调用后立即自旋等待 gState==READY，等价于阻塞发送。
 * ============================================================ */
HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef *huart,
                                       uint8_t *pData,
                                       uint16_t Size)
{
    return HAL_UART_Transmit(huart, pData, Size, 0xFFFFFFFFU);
}

/* ============================================================
 *  HAL_UART_Receive_IT —— 挂起一次接收
 *  Gizwits 每次回调后重新调用此函数 Arm 下一字节。
 * ============================================================ */
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *huart,
                                      uint8_t *pData,
                                      uint16_t Size)
{
    if (!huart || !pData || !Size) return HAL_ERROR;

    huart->pRxBuffPtr  = pData;
    huart->RxXferSize  = Size;
    huart->RxXferCount = Size;
    huart->RxState     = HAL_UART_STATE_BUSY_RX;

    USART_ITConfig(get_phys(huart), USART_IT_RXNE, ENABLE);
    return HAL_OK;
}

/* ============================================================
 *  HAL_UART_IRQHandler —— 在物理串口的 ISR 中调用
 *
 *  注意：传入的 huart 是逻辑句柄（如 &huart2），
 *        内部通过 get_phys() 访问正确的物理寄存器。
 * ============================================================ */
void HAL_UART_IRQHandler(UART_HandleTypeDef *huart)
{
    USART_TypeDef *port;

    if (!huart) return;
    port = get_phys(huart);

    if (USART_GetITStatus(port, USART_IT_RXNE) != RESET)
    {
        uint8_t byte = (uint8_t)USART_ReceiveData(port);

        if (huart->pRxBuffPtr && huart->RxXferCount > 0U)
        {
            *(huart->pRxBuffPtr) = byte;
            huart->pRxBuffPtr++;
            huart->RxXferCount--;

            if (huart->RxXferCount == 0U)
            {
                huart->RxState = HAL_UART_STATE_READY;
                /* 关掉中断，等 Gizwits 回调里重新 Arm */
                USART_ITConfig(port, USART_IT_RXNE, DISABLE);
                /* 触发 Gizwits 的 HAL_UART_RxCpltCallback */
                HAL_UART_RxCpltCallback(huart);
            }
        }
    }
}

/* ============================================================
 *  HAL_TIM_Base_Start_IT —— 启动定时器并使能更新中断
 * ============================================================ */
HAL_StatusTypeDef HAL_TIM_Base_Start_IT(TIM_HandleTypeDef *htim)
{
    if (!htim) return HAL_ERROR;
    TIM_ITConfig(htim->Instance, TIM_IT_Update, ENABLE);
    TIM_Cmd(htim->Instance, ENABLE);
    return HAL_OK;
}

/* ============================================================
 *  HAL_TIM_IRQHandler —— 在物理定时器 ISR 中调用
 *
 *  设计要点：由外层 ISR（TIM3_IRQHandler）先检测到中断再调用本函数；
 *  本函数负责"再次检测 + 清标志 + 调回调"，形成标准 HAL 行为。
 *  外层 ISR 绝对不能提前清标志，否则本函数看不到 SET，回调不会触发。
 * ============================================================ */
void HAL_TIM_IRQHandler(TIM_HandleTypeDef *htim)
{
    if (!htim) return;

    if (TIM_GetITStatus(htim->Instance, TIM_IT_Update) != RESET)
    {
        TIM_ClearITPendingBit(htim->Instance, TIM_IT_Update);
        HAL_TIM_PeriodElapsedCallback(htim);
    }
}

/* ============================================================
 *  HAL_NVIC_SystemReset —— 软复位
 * ============================================================ */
void HAL_NVIC_SystemReset(void)
{
    NVIC_SystemReset();
}
