#include "SWM350.h"
#include "FlashDisk.h"

#undef USB_FEATURE_REMOTE_WAKEUP
#undef USB_FEATURE_ENDPOINT_HALT
#include "usbd_core.h"
#include "usbd_msc.h"


void SerialInit(void);
void msc_flash_init(uint8_t busid, uintptr_t reg_base);

int main(void)
{
	SystemInit();
	
	SerialInit();
	
	FlashDiskInit();
	
	msc_flash_init(0, USBG_BASE);
	
	uint64_t t_flush = SysTick_Value();
   	
	while(1==1)
	{
		usbd_msc_polling(0);
		
		if(SysTick_Value() - t_flush > CyclesPerUs * 1000 * 20)
		{
			t_flush = SysTick_Value();
			
			FlashDiskFlush();
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
