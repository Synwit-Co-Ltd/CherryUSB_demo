#include "SWM350.h"

#undef USB_FEATURE_REMOTE_WAKEUP
#undef USB_FEATURE_ENDPOINT_HALT
#include "usbd_core.h"
#include "usbd_hid.h"


uint16_t DAC_nPoint = 100;	// DAC 波形点数
uint16_t DAC_Buffer[1024] = {
    2048,  2176,  2304,  2431,  2557,  2680,  2801,  2919,  3034,  3144,
    3251,  3352,  3449,  3540,  3625,  3704,  3776,  3841,  3900,  3951,
    3994,  4030,  4058,  4078,  4090,  4095,  4090,  4078,  4058,  4030,
    3994,  3951,  3900,  3841,  3776,  3704,  3625,  3540,  3449,  3352,
    3251,  3144,  3034,  2919,  2801,  2680,  2557,  2431,  2304,  2176,
    2047,  1919,  1791,  1664,  1538,  1415,  1294,  1176,  1061,   951,
     844,   743,   646,   555,   470,   391,   319,   254,   195,   144,
     101,    65,    37,    17,     5,     1,     5,    17,    37,    65,
     101,   144,   195,   254,   319,   391,   470,   555,   646,   743,
     844,   951,  1061,  1176,  1294,  1415,  1538,  1664,  1791,  1919,
};


void SerialInit(void);
void hid_custom_init(uint8_t busid, uintptr_t reg_base);

int main(void)
{
	SystemInit();
	
	SerialInit();
	
	PORT_Init(PORTD, PIN2, PORTD_PIN2_DAC0_OUT, 0);
	DAC_Init(DAC0, DAC_FORMAT_LSB12B);
	DAC_Open(DAC0);
	
	hid_custom_init(0, USBG_BASE);
   	
	while(1==1)
	{
		for(int i = 0; i < DAC_nPoint; i++)
		{
			SW_DelayUS(500);
			
			DAC0->DHR = DAC_Buffer[i];
			
			while(DAC0->SR & DAC_SR_DHRFULL_Msk) __NOP();
		}
	}
}


void SerialInit(void)
{
	UART_InitStructure UART_initStruct;
	
	PORT_Init(PORTA, PIN6, FUNMUX0_UART0_TXD, 0);
	PORT_Init(PORTA, PIN7, FUNMUX1_UART0_RXD, 1);
	
 	UART_initStruct.Baudrate = 115200;
	UART_initStruct.DataBits = UART_DATA_8BIT;
	UART_initStruct.Parity = UART_PARITY_NONE;
	UART_initStruct.StopBits = UART_STOP_1BIT;
	UART_initStruct.RXThreshold = 3;
	UART_initStruct.RXThresholdIEn = 0;
	UART_initStruct.TXThreshold = 3;
	UART_initStruct.TXThresholdIEn = 0;
	UART_initStruct.TimeoutTime = 10;
	UART_initStruct.TimeoutIEn = 0;
 	UART_Init(UART0, &UART_initStruct);
	UART_Open(UART0);
}

int _write(int fd, char *ptr, int len)
{
	for(int i = 0; i < len; i++)
	{
		UART_WriteByte(UART0, *ptr++);
		
		while(UART_IsTXBusy(UART0)) __NOP();
	}
	
	return len;
}
