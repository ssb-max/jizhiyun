/**
 ************************************************************
 * @file    hal_stm32f10x.c
 * @brief   HAL 兼容层实现：把机智云生成的 HAL 库 API 调用映射到本工程
 *          已有的 STM32 标准外设库实现。
 *
 *          详见 hal_stm32f10x.h 顶部说明。
 ***********************************************************/

#include "hal_stm32f10x.h"

/* ============================================================
 *  全局 HAL 句柄定义（Gizwits 用 extern 引用）
 * ============================================================ */
UART_HandleTypeDef huart1 =
{
    USART1,                 /* Instance       */
    (uint8_t *)0,           /* pRxBuffPtr     */
    0,                      /* RxXferSize     */
    0,                      /* RxXferCount    */
    HAL_UART_STATE_READY,   /* gState         */
    HAL_UART_STATE_READY    /* RxState        */
};

UART_HandleTypeDef huart2 =
{
    USART2,
    (uint8_t *)0,
    0,
    0,
    HAL_UART_STATE_READY,
    HAL_UART_STATE_READY
};

TIM_HandleTypeDef htim2 =
{
    TIM3        /* 本工程实际使用 TIM3 作为 1ms 节拍，名字仍叫 htim2
                   是为了兼容机智云生成代码的 "&htim2" 引用              */
};

/* ============================================================
 *  HAL_UART_Transmit  —— 阻塞式发送
 *  机智云的 fputc(printf 重定向) 会调用本函数
 * ============================================================ */
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *huart,
                                    uint8_t *pData,
                                    uint16_t Size,
                                    uint32_t Timeout)
{
    uint16_t i;

    if ((huart == 0) || (pData == 0) || (Size == 0U))
    {
        return HAL_ERROR;
    }

    huart->gState = HAL_UART_STATE_BUSY_TX;

    for (i = 0U; i < Size; i++)
    {
        /* 等待 TXE 为空（发送数据寄存器空） */
        while (USART_GetFlagStatus(huart->Instance, USART_FLAG_TXE) == RESET)
        {
            (void)Timeout; /* 简化处理，不严格按 Timeout 退出 */
        }
        USART_SendData(huart->Instance, (uint16_t)pData[i]);
    }

    /* 等待最后一个字节发送完成（TC=1） */
    while (USART_GetFlagStatus(huart->Instance, USART_FLAG_TC) == RESET) { }

    huart->gState = HAL_UART_STATE_READY;
    return HAL_OK;
}

/* ============================================================
 *  HAL_UART_Transmit_IT —— 中断方式发送（机智云协议层使用）
 *
 *  机智云调用模式如下：
 *      HAL_UART_Transmit_IT(&huart2, &byte, 1);
 *      while (huart2.gState != HAL_UART_STATE_READY);
 *  即每发完一字节都自旋等待 gState 回到 READY。因此这里实现成
 *  阻塞发送 + 立刻把 gState 置回 READY 即可，行为完全等价。
 * ============================================================ */
HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef *huart,
                                       uint8_t *pData,
                                       uint16_t Size)
{
    return HAL_UART_Transmit(huart, pData, Size, 0xFFFFFFFFU);
}

/* ============================================================
 *  HAL_UART_Receive_IT —— 启动一次接收（接收完成后回调）
 *  机智云只调用 HAL_UART_Receive_IT(&huart2, &aRxBuffer, 1)，
 *  每收满 Size 字节后框架调用 HAL_UART_RxCpltCallback。
 * ============================================================ */
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *huart,
                                      uint8_t *pData,
                                      uint16_t Size)
{
    if ((huart == 0) || (pData == 0) || (Size == 0U))
    {
        return HAL_ERROR;
    }

    huart->pRxBuffPtr  = pData;
    huart->RxXferSize  = Size;
    huart->RxXferCount = Size;
    huart->RxState     = HAL_UART_STATE_BUSY_RX;

    /* 使能 RXNE 中断（标准库实现） */
    USART_ITConfig(huart->Instance, USART_IT_RXNE, ENABLE);

    return HAL_OK;
}

/* ============================================================
 *  HAL_UART_IRQHandler —— 在标准库的 USARTx_IRQHandler 里调用
 *  负责：读取一个字节 -> 写入接收缓冲 -> 满包后回调
 * ============================================================ */
void HAL_UART_IRQHandler(UART_HandleTypeDef *huart)
{
    if (huart == 0) { return; }

    if (USART_GetITStatus(huart->Instance, USART_IT_RXNE) != RESET)
    {
        uint8_t byte = (uint8_t)USART_ReceiveData(huart->Instance);

        if ((huart->pRxBuffPtr != 0) && (huart->RxXferCount > 0U))
        {
            *(huart->pRxBuffPtr) = byte;
            huart->pRxBuffPtr++;
            huart->RxXferCount--;

            if (huart->RxXferCount == 0U)
            {
                huart->RxState = HAL_UART_STATE_READY;
                /* 关掉 RXNE，等回调里重新 Arm（机智云就是这么做的） */
                USART_ITConfig(huart->Instance, USART_IT_RXNE, DISABLE);

                /* 调用机智云在 gizwits_product.c 实现的回调 */
                HAL_UART_RxCpltCallback(huart);
            }
        }
        /* 即使没有挂接收缓冲也要清掉数据，防止溢出 */
    }

    /* 发送相关中断本兼容层未使能（采用阻塞发送），无需处理 */
}

/* ============================================================
 *  HAL_TIM_Base_Start_IT —— 使能基本定时器 + 更新中断
 * ============================================================ */
HAL_StatusTypeDef HAL_TIM_Base_Start_IT(TIM_HandleTypeDef *htim)
{
    if (htim == 0) { return HAL_ERROR; }

    TIM_ITConfig(htim->Instance, TIM_IT_Update, ENABLE);
    TIM_Cmd(htim->Instance, ENABLE);
    return HAL_OK;
}

/* ============================================================
 *  HAL_TIM_IRQHandler —— 在标准库 TIMx_IRQHandler 中调用
 * ============================================================ */
void HAL_TIM_IRQHandler(TIM_HandleTypeDef *htim)
{
    if (htim == 0) { return; }

    if (TIM_GetITStatus(htim->Instance, TIM_IT_Update) != RESET)
    {
        TIM_ClearITPendingBit(htim->Instance, TIM_IT_Update);
        HAL_TIM_PeriodElapsedCallback(htim);
    }
}

/* ============================================================
 *  HAL_NVIC_SystemReset —— 系统软复位
 * ============================================================ */
void HAL_NVIC_SystemReset(void)
{
    NVIC_SystemReset();
}
