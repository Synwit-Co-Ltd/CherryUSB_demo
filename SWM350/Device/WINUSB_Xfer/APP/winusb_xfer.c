/*
 * Copyright (c) 2024, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbd_core.h"

#define WINUSB_VENDOR_CODE 0x17

// note that if device is composite device, you should use USB_MSOSV2_COMP_ID_FUNCTION_WINUSB_MULTI_DESCRIPTOR_INIT
const uint8_t WINUSB_WCIDDescriptor[] = {
    USB_MSOSV2_COMP_ID_SET_HEADER_DESCRIPTOR_INIT(10 + USB_MSOSV2_COMP_ID_FUNCTION_WINUSB_SINGLE_DESCRIPTOR_LEN),
    USB_MSOSV2_COMP_ID_FUNCTION_WINUSB_SINGLE_DESCRIPTOR_INIT(),
};

const uint8_t USBD_BinaryObjectStoreDescriptor[] = {
    USB_BOS_HEADER_DESCRIPTOR_INIT(5 + USB_BOS_CAP_PLATFORM_WINUSB_DESCRIPTOR_LEN, 1),
    USB_BOS_CAP_PLATFORM_WINUSB_DESCRIPTOR_INIT(WINUSB_VENDOR_CODE, sizeof(WINUSB_WCIDDescriptor)),
};

const struct usb_msosv2_descriptor msosv2_desc = {
    .vendor_code = WINUSB_VENDOR_CODE,
    .compat_id = WINUSB_WCIDDescriptor,
    .compat_id_len = sizeof(WINUSB_WCIDDescriptor),
};

const struct usb_bos_descriptor bos_desc = {
    .string = USBD_BinaryObjectStoreDescriptor,
    .string_len = sizeof(USBD_BinaryObjectStoreDescriptor),
};

#define WINUSB_IN_EP  0x81
#define WINUSB_OUT_EP 0x08

#define USBD_VID           0xFFFE
#define USBD_PID           0xffff
#define USBD_MAX_POWER     100
#define USBD_LANGID_STRING 1033

#define USB_CONFIG_SIZE (9 + 9 + 7 + 7)

#ifdef CONFIG_USB_HS
#define WINUSB_EP_MPS 512
#else
#define WINUSB_EP_MPS 64
#endif

static const uint8_t device_descriptor[] = {
    USB_DEVICE_DESCRIPTOR_INIT(USB_2_1, 0x00, 0x00, 0x00, USBD_VID, USBD_PID, 0x0001, 0x01)
};

static const uint8_t config_descriptor[] = {
    USB_CONFIG_DESCRIPTOR_INIT(USB_CONFIG_SIZE, 0x01, 0x01, USB_CONFIG_BUS_POWERED, USBD_MAX_POWER),
    USB_INTERFACE_DESCRIPTOR_INIT(0x00, 0x00, 0x02, 0xff, 0xff, 0x00, 0x04),
    USB_ENDPOINT_DESCRIPTOR_INIT(WINUSB_IN_EP, 0x02, WINUSB_EP_MPS, 0x00),
    USB_ENDPOINT_DESCRIPTOR_INIT(WINUSB_OUT_EP, 0x02, WINUSB_EP_MPS, 0x00),
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
    "CherryUSB WINUSB Xfer",      /* Product */
    "2022123456",                 /* Serial Number */
    "CherryUSB WINUSB Xfer 1",    /* STRING4 */
    "CherryUSB WINUSB Xfer 2",    /* STRING5 */
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
    if (index > 5) {
        return NULL;
    }
    return string_descriptors[index];
}

const struct usb_descriptor winusbv2_descriptor = {
    .device_descriptor_callback = device_descriptor_callback,
    .config_descriptor_callback = config_descriptor_callback,
    .device_quality_descriptor_callback = device_quality_descriptor_callback,
    .string_descriptor_callback = string_descriptor_callback,
    .msosv2_descriptor = &msosv2_desc,
    .bos_descriptor = &bos_desc,
};

USB_MEM_ALIGNX uint8_t read_buffer[WINUSB_EP_MPS];
USB_MEM_ALIGNX uint8_t write_buffer[WINUSB_EP_MPS];

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
            usbd_ep_start_read(busid, WINUSB_OUT_EP, read_buffer, WINUSB_EP_MPS);
            break;
        case USBD_EVENT_SET_REMOTE_WAKEUP:
            break;
        case USBD_EVENT_CLR_REMOTE_WAKEUP:
            break;

        default:
            break;
    }
}


/*****************************************************************************/
/* HID Transfer Commands */
#define HID_CMD_SIGNATURE	0x43444948
#define HID_CMD_ERASE    	0x71
#define HID_CMD_WRITE    	0xC3
#define HID_CMD_READ     	0xD2
#define HID_CMD_NONE     	0x00

#define SECTOR_SIZE      	1024
#define SECTOR_COUNT		4

#ifdef CONFIG_USB_HS
#define PAGE_SIZE        	512
#else
#define PAGE_SIZE        	256
#endif

#define PAGE_COUNT			(SECTOR_SIZE * SECTOR_COUNT / PAGE_SIZE)

static uint8_t  RAMDisk[SECTOR_SIZE * SECTOR_COUNT] = {0};
static uint8_t  PageBuf[PAGE_SIZE] __attribute__((aligned(4))) = {0};	// Page buffer to upload/download through HID report
static uint32_t IndexInPageBuf = 0;


typedef struct __attribute__((__packed__))
{
	uint8_t  u8Cmd;
	uint8_t  u8Size;
	uint32_t u32Arg1;
	uint32_t u32Arg2;
	uint32_t u32Signature;
	uint32_t u32Checksum;
} CMD_T;

CMD_T gCmd;


int32_t HID_CmdEraseSectors(CMD_T *pCmd)
{
	uint32_t sector = pCmd->u32Arg1;
	uint32_t count  = pCmd->u32Arg2;
	
	memset(RAMDisk + sector * SECTOR_SIZE, 0xFF, count * SECTOR_SIZE);
	
	pCmd->u8Cmd = HID_CMD_NONE;		// note the command has been done
	
	return 0;
}


int32_t HID_CmdWritePage(CMD_T *pCmd)
{
	IndexInPageBuf = 0;
	
	return 0;
}

void usbd_winusb_in(uint8_t busid, uint8_t ep, uint32_t nbytes);
int32_t HID_CmdReadPage(CMD_T *pCmd)
{
	uint32_t page = pCmd->u32Arg1;
	
	/* Update data to page buffer to upload */
	memcpy(PageBuf, RAMDisk + page * PAGE_SIZE, sizeof(PageBuf));
	IndexInPageBuf = PAGE_SIZE;
	
	usbd_winusb_in(0, WINUSB_IN_EP, 0);
	
	return 0;
}


int32_t ProcessCommand(uint8_t *buf, uint32_t len)
{
	uint32_t i, sum = 0;
	
	memcpy((uint8_t *)&gCmd, buf, sizeof(gCmd));
	
	/* Check signature */
	if(gCmd.u32Signature != HID_CMD_SIGNATURE)
		return -1;
	
	/* Check checksum  */
	for(i = 0; i < gCmd.u8Size; i++) sum += ((uint8_t *)&gCmd)[i];
	if(sum != gCmd.u32Checksum)
		return -1;
	
	switch(gCmd.u8Cmd)
	{
	case HID_CMD_ERASE:
		HID_CmdEraseSectors(&gCmd);
		break;

	case HID_CMD_WRITE:
		HID_CmdWritePage(&gCmd);
		break;

	case HID_CMD_READ:
		HID_CmdReadPage(&gCmd);
		break;

	default:
		return -1;
	}
	
	return 0;
}


void usbd_winusb_out(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
	if(gCmd.u8Cmd == HID_CMD_WRITE)		// data phase of write command
	{
		memcpy(&PageBuf[IndexInPageBuf], read_buffer, nbytes);
		IndexInPageBuf += nbytes;
		
		if(IndexInPageBuf >= PAGE_SIZE)
		{
			uint32_t page = gCmd.u32Arg1;
			memcpy(RAMDisk + page * PAGE_SIZE, PageBuf, sizeof(PageBuf));
			
			gCmd.u8Cmd = HID_CMD_NONE;	// Write command complete
		}
	}
	else
	{
		ProcessCommand(read_buffer, nbytes);
	}
	
	usbd_ep_start_read(busid, ep, read_buffer, WINUSB_EP_MPS);
}

void usbd_winusb_in(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
	if(gCmd.u8Cmd == HID_CMD_READ)		// data phase of read command
	{
		if(IndexInPageBuf)
		{
			/* Prepare the data for next HID IN transfer */
			usbd_ep_start_write(busid, ep, &PageBuf[PAGE_SIZE - IndexInPageBuf], WINUSB_EP_MPS);
			
			IndexInPageBuf -= WINUSB_EP_MPS;
		}
		else
		{
			gCmd.u8Cmd = HID_CMD_NONE;	// Read command complete
		}
	}
}

struct usbd_endpoint winusb_out_ep1 = {
    .ep_addr = WINUSB_OUT_EP,
    .ep_cb = usbd_winusb_out
};

struct usbd_endpoint winusb_in_ep1 = {
    .ep_addr = WINUSB_IN_EP,
    .ep_cb = usbd_winusb_in
};

struct usbd_interface intf0;


void winusbv2_init(uint8_t busid, uintptr_t reg_base)
{
    usbd_desc_register(busid, &winusbv2_descriptor);

    usbd_add_interface(busid, &intf0);
    usbd_add_endpoint(busid, &winusb_out_ep1);
    usbd_add_endpoint(busid, &winusb_in_ep1);

    usbd_initialize(busid, reg_base, usbd_event_handler);
}
