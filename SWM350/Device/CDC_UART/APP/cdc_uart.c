/*
 * Copyright (c) 2024, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbd_core.h"
#include "usbd_cdc_acm.h"

#undef USB_FEATURE_REMOTE_WAKEUP
#undef USB_FEATURE_ENDPOINT_HALT
#include "SWM350.h"

/*!< endpoint address */
#define CDC_IN_EP  0x81
#define CDC_OUT_EP 0x08
#define CDC_INT_EP 0x83

#define USBD_VID           0xFFFF
#define USBD_PID           0xFFFF
#define USBD_MAX_POWER     100
#define USBD_LANGID_STRING 1033

/*!< config descriptor size */
#define USB_CONFIG_SIZE (9 + CDC_ACM_DESCRIPTOR_LEN)

#ifdef CONFIG_USB_HS
#define CDC_MAX_MPS 512
#else
#define CDC_MAX_MPS 64
#endif

static const uint8_t device_descriptor[] = {
    USB_DEVICE_DESCRIPTOR_INIT(USB_2_0, 0xEF, 0x02, 0x01, USBD_VID, USBD_PID, 0x0100, 0x01)
};

static const uint8_t config_descriptor[] = {
    USB_CONFIG_DESCRIPTOR_INIT(USB_CONFIG_SIZE, 0x02, 0x01, USB_CONFIG_BUS_POWERED, USBD_MAX_POWER),
    CDC_ACM_DESCRIPTOR_INIT(0x00, CDC_INT_EP, CDC_OUT_EP, CDC_IN_EP, CDC_MAX_MPS, 0x02)
};

static const uint8_t device_quality_descriptor[] = {
    ///////////////////////////////////////
    /// device qualifier descriptor
    ///////////////////////////////////////
    0x0a,
    USB_DESCRIPTOR_TYPE_DEVICE_QUALIFIER,
    0x00,
    0x02,
    0x00,
    0x00,
    0x00,
    0x40,
    0x00,
    0x00,
};

static const char *string_descriptors[] = {
    (const char[]){ 0x09, 0x04 }, /* Langid */
    "CherryUSB",                  /* Manufacturer */
    "CherryUSB CDC UART",         /* Product */
    "2022123456",                 /* Serial Number */
};

static const uint8_t *device_descriptor_callback(uint8_t speed)
{
    return device_descriptor;
}

static const uint8_t *config_descriptor_callback(uint8_t speed)
{
    return config_descriptor;
}

static const uint8_t *device_quality_descriptor_callback(uint8_t speed)
{
    return device_quality_descriptor;
}

static const char *string_descriptor_callback(uint8_t speed, uint8_t index)
{
    if (index > 3) {
        return NULL;
    }
    return string_descriptors[index];
}

const struct usb_descriptor cdc_descriptor = {
    .device_descriptor_callback = device_descriptor_callback,
    .config_descriptor_callback = config_descriptor_callback,
    .device_quality_descriptor_callback = device_quality_descriptor_callback,
    .string_descriptor_callback = string_descriptor_callback
};


uint8_t read_buffer[CDC_MAX_MPS] __attribute__((aligned(4)));

static void usbd_event_handler(uint8_t busid, uint8_t event)
{
    switch (event) {
        case USBD_EVENT_RESET:
            break;
        case USBD_EVENT_CONNECTED:
            break;
        case USBD_EVENT_DISCONNECTED:
            break;
        case USBD_EVENT_RESUME:
            break;
        case USBD_EVENT_SUSPEND:
            break;
        case USBD_EVENT_CONFIGURED:
            /* setup first out ep read transfer */
            usbd_ep_start_read(busid, CDC_OUT_EP, read_buffer, CDC_MAX_MPS);
            break;
        case USBD_EVENT_SET_REMOTE_WAKEUP:
            break;
        case USBD_EVENT_CLR_REMOTE_WAKEUP:
            break;

        default:
            break;
    }
}


#define RX_BUFF_SIZE    1024 	// RX buffer size
#define TX_BUFF_SIZE    1024 	// TX buffer size

typedef struct {
	uint8_t  rx_buff[RX_BUFF_SIZE];
	uint16_t rx_bytes;
	uint16_t rx_head;
	uint16_t rx_tail;
	uint8_t  tx_buff[TX_BUFF_SIZE];
	uint16_t tx_bytes;
	uint16_t tx_head;
	uint16_t tx_tail;
	
	uint16_t hw_flow;	// BIT0: DTR(Data Terminal Ready) , BIT1: RTS(Request To Send)
	
	uint8_t  in_buff[CDC_MAX_MPS] __attribute__((aligned(4)));
	uint16_t in_bytes;
	uint8_t  out_buff[CDC_MAX_MPS];
	uint16_t out_bytes;
} VCOM;

volatile VCOM vcom;


void usbd_cdc_acm_bulk_out(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
	if(nbytes)
	{
		memcpy((uint8_t *)vcom.out_buff, read_buffer, nbytes);
		
		vcom.out_bytes = nbytes;
	}
	else
	{
		/* Ready to get next BULK out */
		usbd_ep_start_read(0, CDC_OUT_EP, read_buffer, CDC_MAX_MPS);
	}
}

void usbd_cdc_acm_bulk_in(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    if((nbytes % usbd_get_ep_mps(busid, ep)) == 0 && nbytes)
    {
        /* send zlp */
        usbd_ep_start_write(busid, CDC_IN_EP, NULL, 0);
    }
    else
    {
    	vcom.in_bytes = 0;
    }
}

/*!< endpoint call back */
struct usbd_endpoint cdc_out_ep = {
    .ep_addr = CDC_OUT_EP,
    .ep_cb = usbd_cdc_acm_bulk_out
};

struct usbd_endpoint cdc_in_ep = {
    .ep_addr = CDC_IN_EP,
    .ep_cb = usbd_cdc_acm_bulk_in
};

static struct usbd_interface intf0;
static struct usbd_interface intf1;

void cdc_acm_init(uint8_t busid, uintptr_t reg_base)
{
    usbd_desc_register(busid, &cdc_descriptor);

    usbd_add_interface(busid, usbd_cdc_acm_init_intf(busid, &intf0));
    usbd_add_interface(busid, usbd_cdc_acm_init_intf(busid, &intf1));
    usbd_add_endpoint(busid, &cdc_out_ep);
    usbd_add_endpoint(busid, &cdc_in_ep);
    usbd_initialize(busid, reg_base, usbd_event_handler);
}


struct cdc_line_coding LineCfg = {115200, 0, 0, 8};   // Baud rate : 115200, stop bits, parity bits, data bits

void usbd_cdc_acm_set_line_coding(uint8_t busid, uint8_t intf, struct cdc_line_coding *line_coding)
{
    (void)busid;
    (void)intf;
    
    memcpy(&LineCfg, line_coding, sizeof(LineCfg));
    
    uint8_t parity;
    switch(LineCfg.bParityType)
	{
	case 0:  parity = UART_PARITY_NONE; break;
	case 1:  parity = UART_PARITY_ODD;  break;
	case 2:  parity = UART_PARITY_EVEN; break;
	case 3:  parity = UART_PARITY_ONE;  break;
	case 4:  parity = UART_PARITY_ZERO; break;
	default: parity = UART_PARITY_NONE;	break;
	}
	
	UART0->CTRL &= ~(UART_CTRL_DATA9b_Msk | UART_CTRL_PARITY_Msk | UART_CTRL_STOP2b_Msk);
	UART0->CTRL |= ((LineCfg.bDataBits == 9)   << UART_CTRL_DATA9b_Pos) |
				   (parity                     << UART_CTRL_PARITY_Pos) |
				   ((LineCfg.bCharFormat == 2) << UART_CTRL_STOP2b_Pos);
	
	UART_SetBaudrate(UART0, LineCfg.dwDTERate);
}

void usbd_cdc_acm_get_line_coding(uint8_t busid, uint8_t intf, struct cdc_line_coding *line_coding)
{
    (void)busid;
    (void)intf;
    
    memcpy(line_coding, &LineCfg, sizeof(LineCfg));
}


/****************************************************************************************************************************************/
volatile uint32_t RX_Timeout = 0;

__attribute__((interrupt))
void UART0_Handler(void)
{
	uint32_t chr;
	
	if(UART_INTStat(UART0, UART_IT_RX_THR | UART_IT_RX_TOUT))
	{
		while(UART_IsRXFIFOEmpty(UART0) == 0)
		{
			UART_ReadByte(UART0, &chr);
			
			if(vcom.rx_bytes < RX_BUFF_SIZE)  	// Check if buffer full
			{
				vcom.rx_buff[vcom.rx_tail++] = chr;
				if(vcom.rx_tail >= RX_BUFF_SIZE)
					vcom.rx_tail = 0;
				vcom.rx_bytes++;
			}
		}
		
		if(UART_INTStat(UART0, UART_IT_RX_TOUT))
		{
			UART_INTClr(UART0, UART_IT_RX_TOUT);
			
			RX_Timeout = 1;
		}
	}
}


void VCOM_TransferData(void)
{
	/* Check whether USB is ready for next packet or not */
	if(vcom.in_bytes == 0)
	{
		/* Check whether we have new COM Rx data to send to USB or not */
		if((vcom.rx_bytes >= CDC_MAX_MPS) || RX_Timeout)
		{
			RX_Timeout = 0;
			
			int len = vcom.rx_bytes;
			if(len > CDC_MAX_MPS)
				len = CDC_MAX_MPS;
			
			for(int i = 0; i < len; i++)
			{
				vcom.in_buff[i] = vcom.rx_buff[vcom.rx_head++];
				if(vcom.rx_head >= RX_BUFF_SIZE)
					vcom.rx_head = 0;
			}
			
			uint32_t mstatus = SW_enter_critical();
			vcom.rx_bytes -= len;
			SW_exit_critical(mstatus);
			
			vcom.in_bytes = len;
			
			usbd_ep_start_write(0, CDC_IN_EP, (uint8_t *)vcom.in_buff, len);
		}
	}
	
	/* Process the Bulk out data when bulk out data is ready. */
	if(vcom.out_bytes && (vcom.out_bytes <= TX_BUFF_SIZE - vcom.tx_bytes))
	{
		for(int i = 0; i < vcom.out_bytes; i++)
		{
			vcom.tx_buff[vcom.tx_tail++] = vcom.out_buff[i];
			if(vcom.tx_tail >= TX_BUFF_SIZE)
				vcom.tx_tail = 0;
		}
		
		vcom.tx_bytes += vcom.out_bytes;
		
		vcom.out_bytes = 0;
		
		/* Ready to get next BULK out */
		usbd_ep_start_read(0, CDC_OUT_EP, read_buffer, CDC_MAX_MPS);
	}
	
	/* Process the software Tx FIFO */
	if(vcom.tx_bytes)
	{
		while(!UART_IsTXFIFOFull(UART0))
		{
			UART0->DATA = vcom.tx_buff[vcom.tx_head++];
			if(vcom.tx_head >= TX_BUFF_SIZE)
				vcom.tx_head = 0;
			vcom.tx_bytes--;
			
			if(vcom.tx_bytes == 0)
				break;
		}
	}
}
