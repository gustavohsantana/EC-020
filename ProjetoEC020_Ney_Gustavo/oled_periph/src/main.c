/***********
 *   Peripherals such as temp sensor, light sensor, accelerometer,
 *   and trim potentiometer are monitored and values are written to
 *   the OLED display.
 *
 *   Copyright(C) 2010, Embedded Artists AB
 *   All rights reserved.
 *
 **********/



#include "lpc17xx_pinsel.h"
#include "lpc17xx_i2c.h"
#include "lpc17xx_gpio.h"
#include "lpc17xx_ssp.h"
#include "lpc17xx_adc.h"
#include "lpc17xx_timer.h"
#include "lpc17xx.h"
#include "lpc17xx_uart.h"



#define UART_DEV LPC_UART3


#include "light.h"
#include "oled.h"
#include "temp.h"
#include "acc.h"


static uint32_t msTicks = 0;
static uint8_t buf[10];

uint32_t len;


static void init_uart3(void)
{
	PINSEL_CFG_Type PinCfg;
	UART_CFG_Type uartCfg;
	/* Initialize UART3 pin connect */
	PinCfg.Funcnum = 2;
	PinCfg.Pinnum = 0;
	PinCfg.Portnum = 0;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Pinnum = 1;
	PINSEL_ConfigPin(&PinCfg);

	uartCfg.Baud_rate = 115200;
	uartCfg.Databits = UART_DATABIT_8;
	uartCfg.Parity = UART_PARITY_NONE;
	uartCfg.Stopbits = UART_STOPBIT_1;

	UART_Init(UART_DEV, &uartCfg);
	UART_TxCmd(UART_DEV, ENABLE);
}


static void intToString(int value, uint8_t* pBuf, uint32_t len, uint32_t base)
{
    static const char* pAscii = "0123456789abcdefghijklmnopqrstuvwxyz";
    int pos = 0;
    int tmpValue = value;

    // the buffer must not be null and at least have a length of 2 to handle one
    // digit and null-terminator
    if (pBuf == NULL || len < 2)
    {
        return;
    }

    // a valid base cannot be less than 2 or larger than 36
    // a base value of 2 means binary representation. A value of 1 would mean only zeros
    // a base larger than 36 can only be used if a larger alphabet were used.
    if (base < 2 || base > 36)
    {
        return;
    }

    // negative value
    if (value < 0)
    {
        tmpValue = -tmpValue;
        value    = -value;
        pBuf[pos++] = '-';
    }

    // calculate the required length of the buffer
    do {
        pos++;
        tmpValue /= base;
    } while(tmpValue > 0);


    if (pos > len)
    {
        // the len parameter is invalid.
        return;
    }

    pBuf[pos] = '\0';

    do {
        pBuf[--pos] = pAscii[value % base];
        value /= base;
    } while(value > 0);

    return;

}

void SysTick_Handler(void) {
    msTicks++;
}

static uint32_t getTicks(void)
{
    return msTicks;
}

static void init_ssp(void)
{
	SSP_CFG_Type SSP_ConfigStruct;
	PINSEL_CFG_Type PinCfg;

	/*
	 * Initialize SPI pin connect
	 * P0.7 - SCK;
	 * P0.8 - MISO
	 * P0.9 - MOSI
	 * P2.2 - SSEL - used as GPIO
	 */
	PinCfg.Funcnum = 2;
	PinCfg.OpenDrain = 0;
	PinCfg.Pinmode = 0;
	PinCfg.Portnum = 0;
	PinCfg.Pinnum = 7;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Pinnum = 8;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Pinnum = 9;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Funcnum = 0;
	PinCfg.Portnum = 2;
	PinCfg.Pinnum = 2;
	PINSEL_ConfigPin(&PinCfg);

	SSP_ConfigStructInit(&SSP_ConfigStruct);

	// Initialize SSP peripheral with parameter given in structure above
	SSP_Init(LPC_SSP1, &SSP_ConfigStruct);

	// Enable SSP peripheral
	SSP_Cmd(LPC_SSP1, ENABLE);

}

static void init_i2c(void)
{
	PINSEL_CFG_Type PinCfg;

	/* Initialize I2C2 pin connect */
	PinCfg.Funcnum = 2;
	PinCfg.Pinnum = 10;
	PinCfg.Portnum = 0;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Pinnum = 11;
	PINSEL_ConfigPin(&PinCfg);

	// Initialize I2C peripheral
	I2C_Init(LPC_I2C2, 100000);

	/* Enable I2C1 operation */
	I2C_Cmd(LPC_I2C2, ENABLE);
}

static void init_adc(void)
{
	PINSEL_CFG_Type PinCfg;

	/*
	 * Init ADC pin connect
	 * AD0.0 on P0.23
	 * /

	PinCfg.Funcnum = 1;
	PinCfg.OpenDrain = 0;
	PinCfg.Pinmode = 0;
	PinCfg.Pinnum = 23;
	PinCfg.Portnum = 0;
	PINSEL_ConfigPin(&PinCfg);

	/* Configuration for ADC :
	 * 	Frequency at 1Mhz
	 *  ADC channel 0, no Interrupt
	 */
	ADC_Init(LPC_ADC, 1000000);
	ADC_IntConfig(LPC_ADC,ADC_CHANNEL_0,DISABLE);
	ADC_ChannelCmd(LPC_ADC,ADC_CHANNEL_0,ENABLE);

}

int main (void)
{

    uint8_t data;
    int32_t t = 0;
    uint32_t t2 = 0;
    uint32_t lux = 0;
    char str[10];
    char str2[10];




    init_uart3();
    init_i2c();
    init_ssp();
    init_adc();

    oled_init();
    light_init();
    acc_init();

    temp_init (&getTicks);


	if (SysTick_Config(SystemCoreClock / 1000)) {
		    while (1);  // Capture error
	}


    light_enable();
    light_setRange(LIGHT_RANGE_4000);

    oled_clearScreen(OLED_COLOR_WHITE);

    oled_putString(1,1,  (uint8_t*)"Temp  : ", OLED_COLOR_BLACK, OLED_COLOR_WHITE);
    oled_putString(1,9,  (uint8_t*)"Luz  : ", OLED_COLOR_BLACK, OLED_COLOR_WHITE);


    while(1) {


        /* Temperature */
        t = temp_read();
        t2 = (t*26)/257;

        /* light */
        lux = light_read();

        Timer0_Wait(100);

        len = UART_Receive(UART_DEV, &data, 1, NONE_BLOCKING);

        switch (data) {

              case '0':
                  intToString(t2, buf, 10, 10);
                  oled_fillRect((1+9*6),1, 80, 8, OLED_COLOR_WHITE);
                  oled_putString((1+9*6),1, buf, OLED_COLOR_BLACK, OLED_COLOR_WHITE);

                  uint32_t num = t2;

                  sprintf( str, "%u", num );

                  UART_SendString(UART_DEV, (uint8_t*) str);

                  UART_SendString(UART_DEV, (uint8_t*)" temperatura\r\n");

                  len = UART_Receive(UART_DEV, &data, 1, NONE_BLOCKING);

                  Timer0_Wait(50);

                  /* delay */
                  Timer0_Wait(100);
              break;

              case '1':
                  intToString(lux, buf, 10, 10);
                  oled_fillRect((1+9*6),9, 80, 16, OLED_COLOR_WHITE);
                  oled_putString((1+9*6),9, buf, OLED_COLOR_BLACK, OLED_COLOR_WHITE);

                   num = lux;

                  sprintf( str2, "%u", num );
                  UART_SendString(UART_DEV, (uint8_t*) str2);
                  UART_SendString(UART_DEV, (uint8_t*)"luminosidade \r\n");

                  len = UART_Receive(UART_DEV, &data, 1, NONE_BLOCKING);

                  Timer0_Wait(50);

                  /* delay */
                  Timer0_Wait(100);
              break;

              case '2':
                  intToString(lux, buf, 10, 10);
                  oled_fillRect((1+9*6),9, 80, 16, OLED_COLOR_WHITE);
                  oled_putString((1+9*6),9, buf, OLED_COLOR_BLACK, OLED_COLOR_WHITE);

                  intToString(t2, buf, 10, 10);
                  oled_fillRect((1+9*6),1, 80, 8, OLED_COLOR_WHITE);
                  oled_putString((1+9*6),1, buf, OLED_COLOR_BLACK, OLED_COLOR_WHITE);

                  num = t2;
                  sprintf( str, "%u", num );

                  UART_SendString(UART_DEV, (uint8_t*) str);
                  UART_SendString(UART_DEV, (uint8_t*)" temperatura  ");

                  num = lux;
                  sprintf( str2, "%u", num );

                  UART_SendString(UART_DEV, (uint8_t*) str2);
                  UART_SendString(UART_DEV, (uint8_t*)" luminosidade \r\n");

                  len = UART_Receive(UART_DEV, &data, 1, NONE_BLOCKING);

                  Timer0_Wait(50);

                  /* delay */
                  Timer0_Wait(100);
              break;

              default:

                  len = UART_Receive(UART_DEV, &data, 1, NONE_BLOCKING);
                  Timer0_Wait(50);

                  /* delay */
                  Timer0_Wait(100);


        }
    }

}

void check_failed(uint8_t *file, uint32_t line)
{
	/* User can add his own implementation to report the file name and line number,
	 ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

	/* Infinite loop */
	while(1);
}
