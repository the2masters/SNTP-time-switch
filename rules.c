#include <stdint.h>
#include <stdbool.h>
#include "rules.h"
#include "resources.h"
#include "helper.h"
#include "USB.h"
#include "Lib/Ethernet.h"
#include "Lib/SNTP.h"
#include "Lib/ASCII.h"

typedef uint8_t ruleNum_t;

#if true != 1
	#error "(bool)true is expected to be 1"
#endif

#define boolToRuleValue(val) (val + 1)
#define RuleValueToBool(val) (val - 1)

typedef enum {
	pvUnknown = 0,
	pvOff = boolToRuleValue(false),
	pvOn = boolToRuleValue(true),
	pvOffChanged = -pvOff,
	pvOnChanged = -pvOn,
} ruleValue_t;

typedef enum {
	ptLogic,
	ptRelay,
	ptTimeSwitch,
	ptTimeLimit,
	ptSunrise,
	ptSunset,
	ptSNTP = -1,	// negative = remote dependency
	ptRemote = -2
} ruleType_t;

typedef enum {
	LogicOff,
	LogicOn,
	LogicNOT,
	LogicAND,
	LogicOR,
	LogicNAND,
	LogicNOR,
} LogicType_t;

typedef struct {
	ruleType_t		typ;
	ruleNum_t		dependIndex; // In case of negativ typ this is the port on the remote machine
	union {
		IP_Address_t	IP;
		struct {
			bool	backward;
			int8_t	hour;
			int8_t	min;
			int8_t	wdays;
		} time;
		struct {
			ruleNum_t second; // Zero means no argument
			ruleNum_t third;
			ruleNum_t fourth;
			LogicType_t type;
		} logic;
		struct {
			HWADDR port;		// Something like &PORTC
			uint8_t bitValue;	// Something like _BV(PORTC1)
		} hw;
	} data;
} ruleData_t;

typedef struct {
	time_t		timer;
	ruleValue_t	value;
} ruleState_t;
#define timerOff UINT32_MAX

static const __flash ruleData_t ruleData[] = {
	[0] = {.typ = ptSNTP, .dependIndex = 123, .data.IP = CPU_TO_BE32(0xC0A8C801)},
	[1] = {.typ = ptRemote, .dependIndex = 1, .data.IP = CPU_TO_BE32(0xC0A8C802)},
	[2] = {.typ = ptTimeSwitch, .dependIndex = 0, .data.time = {.backward = false, .hour = 12, .min = 0, .wdays = 0b01111111}},
	[3] = {.typ = ptRelay, .dependIndex = 3, .data.hw = {.port = &PORTC, .bitValue = _BV(5)}},
};

static ruleState_t ruleState[ARRAY_SIZE(ruleData)] = {{0}};



// This is mktime without changing it's arguments but recalculate dst each time
// { struct tm tm = *time; tm.tm_isdst = -1; return mktime(&tm); }
static time_t my_mktime(const struct tm *time)
{
	// extract internals out of time.h implementation
	extern int32_t __utc_offset;
	extern int16_t (*__dst_ptr)(const time_t *, int32_t *);

	// Convert localtime as UTC and correct it by UTC offset
	time_t retVal = mk_gmtime(time) - __utc_offset;
	// Additionally correct time by DST offset (if DST rules are configured)
	// Orders scheduled in the lost hour in spring are executed one hour early
	// Orders scheduled in the repeated hour in autumn are executed only one time
	// after the time shift
	if(__dst_ptr)
		return retVal - __dst_ptr(&retVal, &__utc_offset);
	else
		return retVal;
}

ATTR_PURE static bool checkDependency(ruleNum_t dependIndex, bool *value, bool *changed)
{
	ruleValue_t dependency = ruleState[dependIndex].value;
	if(dependency == pvUnknown)
		return false;

	if(dependency < pvUnknown)
	{
		*changed = true;
		dependency = -dependency;
	}
	*value = RuleValueToBool(dependency);

	return true;
}

void checkRules(void)
{
	time_t now = time(NULL);

	for(ruleNum_t rule = 0; rule < ARRAY_SIZE(ruleData); rule++)
	{
		bool dependValue = false;
		bool dependChanged = false;

		// Do we depend on another rule?
		if(ruleData[rule].typ >= 0 && ruleData[rule].dependIndex != rule)
		{
			if(!checkDependency(ruleData[rule].dependIndex, &dependValue, &dependChanged))
				continue;
		}

		uint8_t ptlogic_dependCount = 0;
		bool ptlogic_dependValues[3];
		if(ruleData[rule].typ == ptLogic && ruleData[rule].data.logic.type > LogicNOT)
		{
			if(ruleData[rule].data.logic.second)
			{
				if(!checkDependency(ruleData[rule].data.logic.second, ptlogic_dependValues + ptlogic_dependCount, &dependChanged))
					continue;
				else
					ptlogic_dependCount++;
			}
			if(ruleData[rule].data.logic.third)
			{
				if(!checkDependency(ruleData[rule].data.logic.third, ptlogic_dependValues + ptlogic_dependCount, &dependChanged))
					continue;
				else
					ptlogic_dependCount++;
			}
			if(ruleData[rule].data.logic.fourth)
			{
				if(!checkDependency(ruleData[rule].data.logic.fourth, ptlogic_dependValues + ptlogic_dependCount, &dependChanged))
					continue;
				else
					ptlogic_dependCount++;
			}
		}

		// If we depend on an rule which didn't changed in this run and our timer has not expired, skip this rule
		if(!dependChanged && ruleState[rule].timer < now)
			continue;

		// Set default for result of this rule
		ruleValue_t newValue = boolToRuleValue(dependValue);
		time_t newTimer = timerOff;

		switch(ruleData[rule].typ)
		{
			case ptLogic:
				switch(ruleData[rule].data.logic.type)
				{
					case LogicOff:	dependValue = false; break;
					case LogicOn:	dependValue = true; break;
					case LogicNOT:	dependValue = !dependValue; break;
					case LogicAND:	for(uint8_t i = ptlogic_dependCount; i; --i) 
								dependValue &= ptlogic_dependValues[i];
							break;
					case LogicOR:	for(uint8_t i = ptlogic_dependCount; i; --i) 
								dependValue |= ptlogic_dependValues[i];
							break;
					case LogicNAND:	for(uint8_t i = ptlogic_dependCount; i; --i) 
								dependValue &= ptlogic_dependValues[i];
							dependValue = !dependValue; 
							break;
					case LogicNOR:	for(uint8_t i = ptlogic_dependCount; i; --i) 
								dependValue |= ptlogic_dependValues[i];
							dependValue = !dependValue; 
							break;
				}
				newValue = boolToRuleValue(dependValue);
				break;
			case ptRelay:
				if(dependValue)
					*ruleData[rule].data.hw.port |= ruleData[rule].data.hw.bitValue;
				else
					*ruleData[rule].data.hw.port &= ~ruleData[rule].data.hw.bitValue;
				break;

			case ptSNTP:
			case ptRemote:
				newValue = pvUnknown;
				Packet_t sendPacket;
				// Check if USB Queue is free
				if(USB_isReady() && USB_prepareTS(&sendPacket))
				{
					int8_t length;

					if(ruleData[rule].typ == ptSNTP)
						length = SNTP_GenerateRequest(sendPacket.data, &ruleData[rule].data.IP, ruleData[rule].dependIndex);
					else // typ = ptRemote
						length = ASCII_GenerateRequest(sendPacket.data, &ruleData[rule].data.IP, ruleData[rule].dependIndex);

					if(length < 0)
					{
						newTimer = now + 1;	// Timeout 1s in case of missing ARP entry
						sendPacket.len = -length;
					} else {
						newTimer = now + 2;	// Timeout 2s in case of no answer
						sendPacket.len = length;
					}
					USB_Send(sendPacket);
				} else { // USB queue blocked, try again on next run
					newTimer = 0;
				}
				break;
		}

		// Did the result of our rule change? Then set changeflag (inverted value)
		if(newValue != ruleState[rule].value)
			ruleState[rule].value = -newValue;
		ruleState[rule].timer = newTimer;
	}
}

// After each rule is processed, send network packets for each changed rule and reset changeflag
void sendChangedRules(void)
{
	for(ruleNum_t rule = 0; rule < ARRAY_SIZE(ruleData); rule++)
	{
		Packet_t sendPacket;
		if(ruleState[rule].value < pvUnknown && USB_isReady() && USB_prepareTS(&sendPacket))
		{	// Reset changeflag
			ruleState[rule].value = -ruleState[rule].value;

			sendPacket.len = ASCII_GenerateBroadcast(sendPacket.data, rule, RuleValueToBool(ruleState[rule].value));
			USB_Send(sendPacket);
		}
	}
}

void processNetworkPackets(void)
{
	Packet_t packet;
	if(USB_isReady() && USB_Received(&packet))
	{	// Calls UDP_Callback in case of received UDP Packet
		bool reflect = Ethernet_ProcessPacket(packet.data, packet.len);

		if(reflect)
			USB_Send(packet);
		else
			USB_EnableReceiver();
	}
}

ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1)
bool UDP_Callback_Request(uint8_t packet[], UDP_Port_t destinationPort, uint16_t length)
{
	// If rules are in changed state (value < pvUnknown) they will be later broadcasted
	if(destinationPort < ARRAY_SIZE(ruleData) && ruleState[destinationPort].value > pvUnknown)
	{
		return ASCII_ProcessRequest(packet, length, RuleValueToBool(ruleState[destinationPort].value));
	}
	return false;
}

ATTR_NON_NULL_PTR_ARG(1, 2)
void UDP_Callback_Reply(uint8_t packet[], const IP_Address_t *sourceIP, UDP_Port_t sourcePort, uint16_t length)
{
	for(ruleNum_t rule = 0; rule < ARRAY_SIZE(ruleData); rule++)
	{
		if(ruleData[rule].typ >= 0) continue;
		if(ruleData[rule].dependIndex != sourcePort) continue;
		if(ruleData[rule].data.IP != *sourceIP) continue;

		// Now we know the packet is for the current rule
		ruleValue_t newState;
		if(ruleData[rule].typ == ptSNTP)
		{
			time_t newTime = SNTP_ProcessPacket(packet, length);
			if(!newTime) break;

			newState = pvOn;
			ruleState[rule].timer = newTime + SNTP_TimeBetweenQueries;
		} else { // ptRemote
			bool retVal;
			if(!ASCII_ProcessReply(packet, length, &retVal))
				break;

			newState = boolToRuleValue(retVal);
			ruleState[rule].timer = timerOff;
		}
		if(ruleState[rule].value != newState)
		{
			ruleState[rule].value = -newState;
		}
		// Dont check this packet agains later rules
		break;
	}
}
