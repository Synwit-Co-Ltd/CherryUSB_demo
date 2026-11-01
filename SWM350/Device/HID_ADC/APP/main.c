#include "SWM350.h"

#undef USB_FEATURE_REMOTE_WAKEUP
#undef USB_FEATURE_ENDPOINT_HALT
#include "usbd_core.h"
#include "usbd_hid.h"


uint16_t ADC_Result[8] __attribute__((aligned(4)));


void SerialInit(void);
void ADC_Config(void);
void hid_custom_init(uint8_t busid, uintptr_t reg_base);

int main(void)
{
	SystemInit();
	
	SerialInit();
	
	ADC_Config();
	
	hid_custom_init(0, USBG_BASE);
   	
	while(1==1)
	{
		uint32_t chn;
		
		ADC_Start(ADC0, ADC_SEQ0);
		for(int i = 0; i < 8; i++)
		{
			while(ADC_DataAvailable(ADC0, ADC_SEQ0) == 0) __NOP();
			ADC_Result[i] = ADC_Read(ADC0, ADC_SEQ0, &chn);
		}
		
		SW_DelayMS(1);
	}
}


void ADC_Config(void)
{
	ADC_InitStructure ADC_initStruct;
	ADC_SEQ_InitStructure ADC_SEQ_initStruct;
	
	PORT_Init(PORTD, PIN1,  PORTD_PIN1_ADC0_CH0,  0);
	PORT_Init(PORTD, PIN0,  PORTD_PIN0_ADC0_CH1,  0);
	PORT_Init(PORTD, PIN10, PORTD_PIN10_ADC0_CH2, 0);
	PORT_Init(PORTD, PIN9,  PORTD_PIN9_ADC0_CH3,  0);
	PORT_Init(PORTA, PIN5,  PORTA_PIN5_ADC0_CH4,  0);
	PORT_Init(PORTA, PIN12, PORTA_PIN12_ADC0_CH5, 0);
	PORT_Init(PORTA, PIN13, PORTA_PIN13_ADC0_CH6, 0);
	PORT_Init(PORTC, PIN4,  PORTC_PIN4_ADC0_CH7,  0);
	PORT_Init(PORTC, PIN5,  PORTC_PIN5_ADC0_CH8,  0);
	PORT_Init(PORTC, PIN6,  PORTC_PIN6_ADC0_CH9,  0);
	PORT_Init(PORTC, PIN8,  PORTC_PIN8_ADC0_CH10, 0);
	PORT_Init(PORTE, PIN0,  PORTE_PIN0_ADC0_CH11, 0);
	PORT_Init(PORTE, PIN1,  PORTE_PIN1_ADC0_CH12, 0);
	PORT_Init(PORTB, PIN2,  PORTB_PIN2_ADC0_CH13, 0);
	PORT_Init(PORTB, PIN9,  PORTB_PIN9_ADC0_CH14, 0);
	
	ADC_initStruct.clkdiv = 10;
	ADC_initStruct.refsrc = ADC_REF_VDD;
	ADC_initStruct.samplAvg = ADC_AVG_SAMPLE1;
	ADC_initStruct.EOC_IEn = 0;
	ADC_initStruct.HalfIEn = 0;
	ADC_Init(ADC0, &ADC_initStruct);					//配置ADC
	
	ADC_SEQ_initStruct.channels = ADC_CH0 | ADC_CH1 | ADC_CH2 | ADC_CH3 | ADC_CH4 | ADC_CH5 | ADC_CH6 | ADC_CH7;
	ADC_SEQ_initStruct.trig_src = ADC_TRIGGER_SW;
	ADC_SEQ_initStruct.conv_cnt = 1;
	ADC_SEQ_initStruct.samp_tim = ADC_SAMPLE_1CLOCK;
	ADC_SEQ_Init(ADC0, ADC_SEQ0, &ADC_SEQ_initStruct);
	
	ADC_Open(ADC0);										//使能ADC
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
