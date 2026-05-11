#include "DHT11.h"

u8 temp , humi;

u8 get_temperature_and_humidity()
{	
  u8 buffer[5];

	if(dht11_read_data(buffer) == 0)
	{
		if(buffer[2]==0)
		{
			
		}
		else
		{
			humi = buffer[0] + (float)(buffer[1] / 10.0);
			temp = buffer[2] + (float)(buffer[3] / 10.0);
		}
		return 0;
	}
	return 1;
}

void dht11_gpio_input(void)
{
    GPIO_InitTypeDef g;
    
    RCC_APB2PeriphClockCmd(DHT11_RCC, ENABLE);
    
    g.GPIO_Pin = DHT11_GPIO_PIN;
    g.GPIO_Speed = GPIO_Speed_50MHz;
    g.GPIO_Mode = GPIO_Mode_IN_FLOATING; //
    
    GPIO_Init(DHT11_GPIO_TYPE, &g);
}

void dht11_gpio_output(void)
{
    GPIO_InitTypeDef g;
    
    RCC_APB2PeriphClockCmd(DHT11_RCC, ENABLE);
    
    g.GPIO_Pin = DHT11_GPIO_PIN;
    g.GPIO_Speed = GPIO_Speed_50MHz;
    g.GPIO_Mode = GPIO_Mode_Out_PP; //

    GPIO_Init(DHT11_GPIO_TYPE, &g);
}

void dht11_reset(void)
{
    // DHT11??
    dht11_gpio_output();
    DHT11_OUT_L;
    delay_us(19000);
    DHT11_OUT_H;
    delay_us(30);
    dht11_gpio_input();
}

u16 dht11_scan(void)
{
    return DHT11_IN;
}

/* ??? bit??????????????????????
 * ?? 0/1 ?? bit ???? 0xFF ??????? */
u16 dht11_read_bit(void)
{
    u32 guard = 0U;

    /* ?????????????? */
    while (DHT11_IN == RESET)
    {
        if (++guard > 200000U) return 0xFF;  /* ? 2.8ms ?? */
    }

    delay_us(40);

    if (DHT11_IN == SET)
    {
        guard = 0U;
        while (DHT11_IN == SET)
        {
            if (++guard > 200000U) break;    /* ???????? */
        }
        return 1;
    }
    else
    {
        return 0;
    }
}

u16 dht11_read_byte(void)
{
    u16 i;
    u16 data = 0;
    for (i = 0; i < 8; i++)
    {
        data <<= 1;
        data |= dht11_read_bit();
    }
    return data;
}

u16 dht11_read_data(u8 buffer[5])
{
    u16 i = 0;
    u8 checksum;
    u32 guard;

    dht11_reset();
    if (dht11_scan() == RESET)
    {
        /* ?? DHT11 ??????? */
        guard = 0U;
        while (dht11_scan() == RESET)
        {
            if (++guard > 200000U) return 1;  /* ????????? */
        }

        /* ?? DHT11 ??????? */
        guard = 0U;
        while (dht11_scan() == SET)
        {
            if (++guard > 200000U) return 1;
        }

        for (i = 0; i < 5; i++)
        {
            buffer[i] = (u8)dht11_read_byte();
        }

        /* ????????? */
        guard = 0U;
        while (dht11_scan() == RESET)
        {
            if (++guard > 200000U) break;
        }
        dht11_gpio_output();
        DHT11_OUT_H;

        checksum = buffer[0] + buffer[1] + buffer[2] + buffer[3];
        if (checksum != buffer[4])
        {
            return 1;   /* checksum error */
        }
    }

    return 0;
}


