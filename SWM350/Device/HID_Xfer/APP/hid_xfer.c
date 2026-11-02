/*
 * Copyright (c) 2022 HPMicro
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "usbd_core.h"
#include "usbd_hid.h"

#define HIDRAW_IN_EP  0x81
#define HIDRAW_OUT_EP 0x08

#define USBD_VID           0xffff
#define USBD_PID           0xffff
#define USBD_MAX_POWER     100
#define USBD_LANGID_STRING 1033

/*!< config descriptor size */
#define USB_CONFIG_SIZE (9 + 9 + 9 + 7 + 7)

/*!< custom hid report descriptor size */
#define HID_CUSTOM_REPORT_DESC_SIZE 37

#ifdef CONFIG_USB_HS
#define HID_MAX_MPS        1024
#define HIDRAW_IN_INTERVAL 1
#else
#define HID_MAX_MPS        64
#define HIDRAW_IN_INTERVAL 1
#endif

static const uint8_t device_descriptor[] = {
    USB_DEVICE_DESCRIPTOR_INIT(USB_2_0, 0x00, 0x00, 0x00, USBD_VID, USBD_PID, 0x0002, 0x01)
};

static const uint8_t config_descriptor[] = {
    USB_CONFIG_DESCRIPTOR_INIT(USB_CONFIG_SIZE, 0x01, 0x01, USB_CONFIG_BUS_POWERED, USBD_MAX_POWER),
    HID_CUSTOM_INOUT_DESCRIPTOR_INIT(0x00, 0x00, HID_CUSTOM_REPORT_DESC_SIZE, HIDRAW_OUT_EP, HIDRAW_IN_EP, HID_MAX_MPS, HIDRAW_IN_INTERVAL),
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
    "CherryUSB HID Xfer",         /* Product */
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

const struct usb_descriptor hid_descriptor = {
    .device_descriptor_callback = device_descriptor_callback,
    .config_descriptor_callback = config_descriptor_callback,
    .device_quality_descriptor_callback = device_quality_descriptor_callback,
    .string_descriptor_callback = string_descriptor_callback
};

/*!< custom hid report descriptor */
static const uint8_t hid_custom_report_desc[HID_CUSTOM_REPORT_DESC_SIZE] = {
	0x06, 0x00, 0xFF,       // Usage Page = 0xFF00 (Vendor Defined Page 1)
	0x09, 0x01,             // Usage (Vendor Usage 1)
	0xA1, 0x01,             // Collection (Application)
	0x1A, 0x01, 0x00,       // Usage Minimum
	0x2A, (HID_MAX_MPS & 0xFF), (HID_MAX_MPS >> 8),	// Usage Maximum
	0x15, 0x00,             // Logical Minimum (data bytes in the report may have minimum value = 0x00)
	0x26, 0xFF, 0x00,       // Logical Maximum (data bytes in the report may have maximum value = 0x00FF = unsigned 255)
	0x75, 0x08,             // Report Size: 8-bit field size
	0x96, (HID_MAX_MPS & 0xFF), (HID_MAX_MPS >> 8),	// Report Count: count of 8-bit fields (the next time the parser hits an "Input", "Output", or "Feature" item)
	0x81, 0x00,             // Input (Data, Array, Abs): Instantiates input packet fields based on the above report size, count, logical min/max, and usage.
	0x1A, 0x01, 0x00,		// Usage Minimum
	0x2A, (HID_MAX_MPS & 0xFF), (HID_MAX_MPS >> 8),	// Usage Maximum
	0x96, (HID_MAX_MPS & 0xFF), (HID_MAX_MPS >> 8),
	0x91, 0x00,             // Output (Data, Array, Abs): Instantiates output packet fields. Uses same report size and count as "Input" fields, since nothing new/different was specified to the parser since the "Input" item.
	0xC0                    // End Collection
};

USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t read_buffer[HID_MAX_MPS];


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
        	usbd_ep_start_read(busid, HIDRAW_OUT_EP, read_buffer, HID_MAX_MPS);
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
#define PAGE_SIZE        	256
#define PAGE_COUNT			(SECTOR_SIZE * SECTOR_COUNT / PAGE_SIZE)

static uint8_t  RAMDisk[SECTOR_SIZE * SECTOR_COUNT] = {0};
static uint8_t  PageBuf[PAGE_SIZE] = {0};	// Page buffer to upload/download through HID report
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

static void usbd_hid_custom_in_callback(uint8_t busid, uint8_t ep, uint32_t nbytes);
int32_t HID_CmdReadPage(CMD_T *pCmd)
{
	uint32_t page = pCmd->u32Arg1;
	
	/* Update data to page buffer to upload */
	memcpy(PageBuf, RAMDisk + page * PAGE_SIZE, sizeof(PageBuf));
	IndexInPageBuf = PAGE_SIZE;
	
	usbd_hid_custom_in_callback(0, HIDRAW_IN_EP, 0);
	
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


static void usbd_hid_custom_in_callback(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
	if(gCmd.u8Cmd == HID_CMD_READ)		// data phase of read command
	{
		if(IndexInPageBuf)
		{
			/* Prepare the data for next HID IN transfer */
			usbd_ep_start_write(busid, ep, &PageBuf[PAGE_SIZE - IndexInPageBuf], HID_MAX_MPS);
			
			IndexInPageBuf -= HID_MAX_MPS;
		}
		else
		{
			gCmd.u8Cmd = HID_CMD_NONE;	// Read command complete
		}
	}
}

static void usbd_hid_custom_out_callback(uint8_t busid, uint8_t ep, uint32_t nbytes)
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
	
	usbd_ep_start_read(busid, ep, read_buffer, HID_MAX_MPS);
}

static struct usbd_endpoint custom_in_ep = {
    .ep_cb = usbd_hid_custom_in_callback,
    .ep_addr = HIDRAW_IN_EP
};

static struct usbd_endpoint custom_out_ep = {
    .ep_cb = usbd_hid_custom_out_callback,
    .ep_addr = HIDRAW_OUT_EP
};

/* function ------------------------------------------------------------------*/
/**
 * @brief            hid custom init
 * @pre              none
 * @param[in]        none
 * @retval           none
 */
struct usbd_interface intf0;

void hid_custom_init(uint8_t busid, uintptr_t reg_base)
{
    usbd_desc_register(busid, &hid_descriptor);

    usbd_add_interface(busid, usbd_hid_init_intf(busid, &intf0, hid_custom_report_desc, HID_CUSTOM_REPORT_DESC_SIZE));
    usbd_add_endpoint(busid, &custom_in_ep);
    usbd_add_endpoint(busid, &custom_out_ep);

    usbd_initialize(busid, reg_base, usbd_event_handler);
}
