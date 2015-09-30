#ifndef _USB_H_
#define _USB_H_

#include <LUFA/Common/Common.h>

bool USB_prepareTS(void) ATTR_WARN_UNUSED_RESULT;
void USB_Send(void);
bool USB_Received(void) ATTR_WARN_UNUSED_RESULT;
void USB_EnableReceiver(void);
bool USB_isReady(void) ATTR_WARN_UNUSED_RESULT ATTR_PURE;

#endif //_USB_H_
