#ifndef _DESCRIPTORS_H_
#define _DESCRIPTORS_H_

#include <LUFA/Drivers/USB/USB.h>

#define CDC_NOTIFICATION_EPADDR        (ENDPOINT_DIR_IN  | 1)
#define CDC_TX_EPADDR                  (ENDPOINT_DIR_IN  | 3)
#define CDC_RX_EPADDR                  (ENDPOINT_DIR_OUT | 4)
#define CDC_NOTIFICATION_EPSIZE        16
#define CDC_TXRX_EPSIZE                64

typedef struct
{
	USB_Descriptor_Configuration_Header_t Config;

	// Ethernet CDC Control Interface
	USB_Descriptor_Interface_t              CDC_CCI_Interface;
	USB_CDC_Descriptor_FunctionalHeader_t   CDC_Functional_Header;
	USB_CDC_Descriptor_FunctionalUnion_t    CDC_Functional_Union;
	USB_CDC_Descriptor_FunctionalEthernet_t CDC_Functional_Ethernet;
	USB_Descriptor_Endpoint_t               CDC_NotificationEndpoint;

	// Ethernet CDC Data Interface
	USB_Descriptor_Interface_t              CDC_DCI_Interface;
	USB_Descriptor_Endpoint_t               Ethernet_DataOutEndpoint;
	USB_Descriptor_Endpoint_t               Ethernet_DataInEndpoint;
} USB_Descriptor_Configuration_t;

typedef struct
{
	USB_Descriptor_Configuration_Header_t Config;
	USB_Descriptor_Interface_t              Interface;
} USB_Descriptor_Configuration2_t;

enum InterfaceDescriptors_t
{
	INTERFACE_ID_CDC_CCI = 0,
	INTERFACE_ID_CDC_DCI = 1,
};

enum StringDescriptors_t
{
	STRING_ID_Language     = 0,
	STRING_ID_Manufacturer = 1,
	STRING_ID_Product      = 2,
	STRING_ID_MAC          = 3,
};

#endif
