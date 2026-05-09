#include "stm32f10x.h"
#include <stdio.h>
#include <string.h>
#include "delay.h"
#include "sys.h"
#include "usart.h"
#include "math.h"

#include "DHT11.h"
#include "oled.h"
#include "IO_Init.h"
#include "timer.h"
#include "gizwits_product.h"
#include "hal_stm32f10x.h"   /* HAL adapter (bridges Gizwits HAL code to StdPeriph) */

/* ============================================================
 *  Hardware resource convention (must match the wiring!)
 *
 *  USART1 (PA9 TX / PA10 RX) -> printf debug   (115200 baud)
 *  USART2 (PA2 TX / PA3  RX) -> Wi-Fi module   (9600 baud, Gizwits)
 *
 *  NOTE: gizwits_product.c hard-codes
 *        "if (UartHandle->Instance == USART2)" inside
 *        HAL_UART_RxCpltCallback, so Wi-Fi MUST be on USART2.
 *
 *  TIM3 = 1ms system tick (logical name htim2 in Gizwits code).
 *  TIM3_IRQHandler -> HAL_TIM_IRQHandler(&htim2)
 *                  -> HAL_TIM_PeriodElapsedCallback (in gizwits_product.c)
 *                  -> keyHandle() + gizTimerMs()
 * ============================================================ */


u8 key;

/* Gizwits HAL version no longer exports wifi_sta. Provide a local
 * placeholder so the OLED display compiles. To track real Wi-Fi
 * connection state, set this in gizwitsEventProcess() inside
 * gizwits_product.c (e.g. WIFI_CON_M2M / WIFI_DISCON_M2M cases).
 * Since we do NOT modify Gizwits/Utils, it stays 0 here. */
uint8_t wifi_sta = 0;


/* gizwits_product.c calls keyHandle() inside HAL_TIM_PeriodElapsedCallback.
 * The header hal_key.h is not used here; provide an empty stub so the
 * project links. Real key scanning is done by Key_Scan() in the main loop. */
void keyHandle(void)
{
    /* intentionally empty */
}


/* ------------------------------------------------------------
 *  TIM3 ISR -- 1ms tick
 *
 *  IMPORTANT: Do NOT clear TIM_IT_Update here. The HAL bridge
 *  (HAL_TIM_IRQHandler) re-checks the flag, then clears it and
 *  calls HAL_TIM_PeriodElapsedCallback. If we cleared it first,
 *  the bridge would see RESET and never invoke the callback.
 * ------------------------------------------------------------ */
void TIM3_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM3, TIM_IT_Update) != RESET)
    {
        time_s++;
    }
    HAL_TIM_IRQHandler(&htim2);
}


/* Gizwits cloud init -- order matters */
void Gizwits_Init(void)
{
    /* TIM3: 1ms tick. arr=9 psc=7199 -> (9+1)*(7199+1)/72MHz = 1ms */
    TIM3_Int_Init(9, 7199);

    /* USART2 = Wi-Fi (Gizwits), 9600 baud */
    usart2_Init(9600);

    /* Reset user data points before protocol init */
    memset((uint8_t *)&currentDataPoint, 0, sizeof(dataPoint_t));

    /* Gizwits protocol layer (ring buffer + state machine) */
    gizwitsInit();

    /* Let Gizwits arm the first RX interrupt on USART2.
     * Internally calls HAL_UART_Receive_IT(&huart2, &aRxBuffer, 1). */
    uartInit();
}


int main(void)
{
    SystemInit();
    delay_init();
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

    OLED_Init();

    /* USART1 = printf debug (115200). Gizwits' built-in fputc routes
     * printf to huart1 (Instance=USART1) via HAL_UART_Transmit. */
    usart1_Init(115200);

    Relay_Init();
    Key_Init();
    Led_Init();

    Gizwits_Init();
    /* gizwitsSetMode(WIFI_SOFTAP_MODE); */

    while (1)
    {
        gizwitsHandle((dataPoint_t *)&currentDataPoint);

        key = Key_Scan();
        if (key == 1)
        {
            Relay = !Relay;
            Led   = !Led;
            if (Relay == 0)
                currentDataPoint.valueled = 0x01;
            else
                currentDataPoint.valueled = 0;
        }

        if (currentDataPoint.valueled == 0x01)
        {
            currentDataPoint.valueled = 0x01;
            Relay = 0;
            Led   = 0;
        }
        else
        {
            currentDataPoint.valueled = 0x00;
            Relay = 1;
            Led   = 1;
        }

        if (time_s > 1000)
        {
            time_s = 0;
            get_temperature_and_humidity();
            currentDataPoint.valuewendu = 99;
            /* currentDataPoint.valuehum = 66; */

            OLED_printf(0, 0, "Temp:%dC  ", temp);
            OLED_printf(0, 2, "Humi:%d%%  ", humi);
            if (Relay == 0)
                OLED_printf(0, 4, "Power:ON ");
            else
                OLED_printf(0, 4, "Power:OFF ");
            if (wifi_sta == 1)
                OLED_printf(0, 6, "WIFI OK     ");
            else
                OLED_printf(0, 6, "WIFI Init...");
        }
    }
}
