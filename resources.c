#include "resources.h"
#include "helper.h"

#define GenerateIP2(a, b, c, d) (((uint32_t)a << 24) + ((uint32_t)b << 16) + ((uint32_t)c << 8) + d)
#define GenerateIP(arg) GenerateIP2(arg)

const MAC_Address_t OwnMACAddress = {{MAC_OWN}};
const IP_Address_t OwnIPAddress = CPU_TO_BE32(GenerateIP(IP_OWN));
const IP_Address_t BroadcastIPAddress = (CPU_TO_BE32(GenerateIP(IP_OWN)) | ~NETMASK_BE);
const IP_Address_t RouterIPAddress = CPU_TO_BE32(GenerateIP(IP_ROUTER));
const IP_Address_t SNTPIPAddress = CPU_TO_BE32(GenerateIP(IP_SNTP));

bool IP_compareNet(const IP_Address_t *a, const IP_Address_t *b)
{
	return (*a & NETMASK_BE) == (*b & NETMASK_BE);
}

