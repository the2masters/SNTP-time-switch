#include <util/delay.h>
#include <stdint.h>
#include <stdbool.h>
#include "rules.h"
#include "resources.h"
#include "helper.h"
#include "USB.h"
#include "Lib/Ethernet.h"
#include "Lib/SNTP.h"
#include "Lib/UDP.h"

typedef int8_t ruleValue_t;
#define rvUnknown ((ruleValue_t)-128)

typedef enum {
	ptLogic,
	ptRelay,
	ptInput,
	ptTimeSwitch,
	ptTrigger,
	ptDaylight,
	ptSNTP = -1,	// negative = remote
	ptRemote = -2
} ruleType_t;

typedef enum {
	LogicXOR = _BV(7),
	LogicINV_Q = _BV(6),
	LogicINV_Q2 = _BV(5),
	LogicINV_Q1 = _BV(4),
	LogicINV_D = _BV(3),
	LogicINV_C = _BV(2),
	LogicINV_B = _BV(1),
	LogicINV_A = _BV(0),

	LogicORAB = 0,
	LogicORCD = 0,
	LogicORQ12 = 0,
	LogicANDAB = LogicINV_A ^ LogicINV_B ^ LogicINV_Q1,
	LogicANDCD = LogicINV_C ^ LogicINV_D ^ LogicINV_Q2,
	LogicANDQ12 = LogicINV_Q1 ^ LogicINV_Q2 ^ LogicINV_Q,
	// Helper for 4 inputs
	LogicORABCD = LogicORAB ^ LogicORCD ^ LogicORQ12,
	LogicANDABCD = LogicANDAB ^ LogicANDCD ^ LogicANDQ12,
	// Helper for 3 inputs (input D = 0)
	LogicORABC = LogicORABCD,
	LogicANDABC = LogicINV_D ^ LogicANDABCD,
	// Helper for 2 inputs: LogicORAB, LogicANDAB
	// Helper for 1 input (input B = 0)
	LogicRepeat = LogicORAB,
	LogicNOT = LogicRepeat ^ LogicINV_Q,
	LogicForceOff = LogicANDAB,
	LogicForceOn = LogicForceOff ^ LogicINV_Q,
} LogicType_t;

typedef enum {
	TriggerRising = _BV(0),
	TriggerFalling = _BV(1),
	TriggerMonoflopRetrigger = _BV(2),

	TriggerBoth = TriggerRising | TriggerFalling,
} TriggerType_t;

typedef enum RuleNames {
// Global
	SystemError = 0,	// This is the default dependency for all rules
	TimeOK,
	Daylight,
	SunriseTrigger,
	SunsetTrigger,

// Inputs
	Shutter1KeyUP,
	Shutter1KeyDown,

// Shutter 1
	Shutter1Error,
	Shutter1NoManualControl,
	Shutter1DriveTimeout,
	Shutter1Direction,

// Outputs
	Shutter1RelayDirection, // needs to be before override Relay!
	Shutter1RelayOverride,
} ruleNum_t;

typedef struct {
	ruleType_t		type;
	ruleNum_t		dependIndex;
	UDP_Port_t		networkPort;	// For normal rules this is the sourcePort of data packets. 
						// For remote rules this is the destination Port.
	union {
		IP_Address_t	IP;
		struct {
			uint8_t	wdays;		// Something like (_BV(SUNDAY) | _BV(SATURDAY))
			unsigned starthour : 6;
			unsigned startmin  : 6;
			unsigned stophour  : 6;
			unsigned stopmin   : 6;
		} ATTR_PACKED time;
		struct {
			TriggerType_t type;
			uint8_t	hour;		// If you provide a time, this acts like a monoflop
			uint8_t	min;
			uint8_t	sec;
		} ATTR_PACKED trigger;
		struct {
			LogicType_t type;
			ruleNum_t second;	// Zero means no argument
			ruleNum_t third;
			ruleNum_t fourth;
		} ATTR_PACKED logic;
		struct {
			HWADDR port;		// Something like &PORTC
			uint8_t bitValue;	// Something like _BV(PORTC1)
		} ATTR_PACKED hw;
	} data;
} ruleData_t;

typedef struct {
	time_t		timer;
	ruleValue_t	value;
} ruleState_t;
#define timerOff UINT32_MAX

static const __flash ruleData_t ruleData[] = {
// Global rules
	[SystemError]		= {.type = ptLogic, .data.logic = {.type = LogicForceOff }, .networkPort = 0},		// Should be ptSystemError
	[TimeOK]		= {.type = ptSNTP, .data.IP = CPU_TO_BE32(0xC0A8C803), .networkPort = 123},
	[Daylight]		= {.type = ptTimeSwitch, .dependIndex = TimeOK, .data.time = {.wdays = 0b01111111, .starthour = 0, .startmin = 9, .stophour = 0, .stopmin = 10}},	// Should be ptDaylight
	[SunriseTrigger]	= {.type = ptTrigger, .dependIndex = Daylight, .data.trigger = {.type = TriggerRising, .sec = 3}},
	[SunsetTrigger]		= {.type = ptTrigger, .dependIndex = Daylight, .data.trigger = {.type = TriggerFalling, .sec = 4}},

// Inputs
	[Shutter1KeyUP]		= {.type = ptLogic, .data.logic = {.type = LogicForceOff }},		// Should be ptInput
	[Shutter1KeyDown]	= {.type = ptLogic, .data.logic = {.type = LogicForceOff }},		// Should be ptInput

// Shutter 1
	[Shutter1Error]		= {.type = ptLogic, .dependIndex = Shutter1KeyUP, .data.logic = {.second = Shutter1KeyDown, .third = SunriseTrigger, .fourth = SunsetTrigger, .type = LogicANDAB ^ LogicANDCD ^ LogicORQ12}}, // (A ^ B) v (C ^ D)
	[Shutter1NoManualControl] = {.type = ptLogic, .dependIndex = Shutter1KeyUP, .data.logic = {.second = Shutter1KeyDown, .third = SunriseTrigger, .fourth = SunsetTrigger, .type = LogicINV_A ^ LogicINV_B ^ LogicANDAB ^ LogicORCD ^ LogicANDQ12}}, // (/A ^ /B) ^ (C v D)
	[Shutter1DriveTimeout]	= {.type = ptTrigger, .dependIndex = Shutter1NoManualControl, .data.trigger = {.type = TriggerRising, .min = 1 }},
	[Shutter1Direction]	= {.type = ptLogic, .dependIndex = Daylight, .data.logic = {.second = Shutter1DriveTimeout, .type = LogicANDAB}},

// Outputs
	[Shutter1RelayDirection]= {.type = ptRelay, .dependIndex = Shutter1Direction, .data.hw = {.port = &PORTC, .bitValue = _BV(4)}},		// K2
	[Shutter1RelayOverride]	= {.type = ptRelay, .dependIndex = Shutter1DriveTimeout, .data.hw = {.port = &PORTC, .bitValue = _BV(5)}},	// K1
};

static ruleState_t ruleState[] = {[0 ... ARRAY_SIZE(ruleData)-1] = {.timer = 5, .value = rvUnknown}};

_Static_assert(sizeof(ruleData->data) == 4, "data field grew over 4 bytes");

static time_t calculateTimestamp(struct tm *time, int8_t hour, int8_t min)
{
	time->tm_hour = hour, time->tm_min = min; // tm_sec already 0

	// Following is mktime without changing it's arguments but recalculate dst each time
	// time->tm_isdst = -1; return mktime(time);

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
static time_t _nextMidnight = 0;
static struct tm *getStructTM(time_t now)
{
	static struct tm tm = {0};
	if(now >= _nextMidnight)
	{
		localtime_r(&now, &tm);
		tm.tm_sec = 0;	// ignore seconds

		_nextMidnight = calculateTimestamp(&tm, 24, 0);
	}
	return &tm;
}
// Parameter to force usage only after getStructTM
static time_t getMidnight(struct tm *tm ATTR_MAYBE_UNUSED)
{
	return _nextMidnight;
}

static bool checkDependency(ruleNum_t dependIndex, ruleValue_t *value, bool *changed)
{
	if(!dependIndex)
		return false;

	ruleValue_t dependency = ruleState[dependIndex].value;
	if(dependency == rvUnknown)
		return true;

	if(dependency < 0)
	{
		*changed = true;
		dependency = ~dependency;
	}
	*value = dependency;

	return false;
}

void checkRules(void)
{
	time_t now = time(NULL);

	for(ruleNum_t rule = 0; rule < ARRAY_SIZE(ruleData); rule++)
	{
		ruleValue_t ruleValue = false;
		bool dependChanged = false;

		// Check if dependency is in state unknown, otherwise set ruleValue to dependencyValue
		if(checkDependency(ruleData[rule].dependIndex, &ruleValue, &dependChanged))
			continue;

		// For ptLogic type we need to check more dependencies for unknown and changed values.
		ruleValue_t dependValueB = false, dependValueC = false, dependValueD = false;
		if(ruleData[rule].type == ptLogic)
		{
			if(checkDependency(ruleData[rule].data.logic.second, &dependValueB, &dependChanged))
				continue;

			if(checkDependency(ruleData[rule].data.logic.third, &dependValueC, &dependChanged))
				continue;

			if(checkDependency(ruleData[rule].data.logic.fourth, &dependValueD, &dependChanged))
				continue;
		}


		// If we depend on a rule which didn't change in this run and our timer has not expired, skip this rule
		if(!dependChanged && now < ruleState[rule].timer)
			continue;

		switch(ruleData[rule].type)
		{
			case ptLogic:
			{
				LogicType_t logictype = ruleData[rule].data.logic.type;

				if((logictype & LogicINV_A) == LogicINV_A)
					ruleValue = !ruleValue;
				if((logictype & LogicINV_B) == LogicINV_B)
					dependValueB = !dependValueB;
				if((logictype & LogicINV_C) == LogicINV_C)
					dependValueC = !dependValueC;
				if((logictype & LogicINV_D) == LogicINV_D)
					dependValueD = !dependValueD;

				bool Q1, Q2;
				if((logictype & LogicXOR) == LogicXOR)
				{
					Q1 = ruleValue ^ dependValueB;
					Q2 = dependValueC ^ dependValueD;
				} else {
					Q1 = ruleValue | dependValueB;
					Q2 = dependValueC | dependValueD;
				}
				if((logictype & LogicINV_Q1) == LogicINV_Q1)
					Q1 = !Q1;
				if((logictype & LogicINV_Q2) == LogicINV_Q2)
					Q2 = !Q2;
				if((logictype & LogicXOR) == LogicXOR)
					ruleValue = Q1 ^ Q2;
				else
					ruleValue = Q1 | Q2;

				if((logictype & LogicINV_Q) == LogicINV_Q)
					ruleValue = !ruleValue;

				ruleState[rule].timer = timerOff;
			} break;

			case ptRelay:
			{
				if(ruleValue)
					*ruleData[rule].data.hw.port |= ruleData[rule].data.hw.bitValue;
				else
					*ruleData[rule].data.hw.port &= ~ruleData[rule].data.hw.bitValue;
				// Force a delay between each switching relay
				_delay_ms(50);

				ruleState[rule].timer = timerOff;
			} break;

			case ptTimeSwitch:
			{
				struct tm *tm = getStructTM(now);

				if((ruleData[rule].data.time.wdays & _BV(tm->tm_wday)) == 0)
				{
					ruleValue = false;
					ruleState[rule].timer = getMidnight(tm);
					break;
				}

				time_t starttime = calculateTimestamp(tm, ruleData[rule].data.time.starthour, ruleData[rule].data.time.startmin);
				if(now < starttime)
				{
					ruleValue = false;
					ruleState[rule].timer = starttime;
				} else {
	                                time_t stoptime = calculateTimestamp(tm, ruleData[rule].data.time.stophour, ruleData[rule].data.time.stopmin);
					if(now < stoptime)
					{
						ruleValue = true;
						ruleState[rule].timer = stoptime;
					} else {
						ruleValue = false;
						ruleState[rule].timer = getMidnight(tm);
					}
				}
			} break;

			case ptTrigger:
			{
				TriggerType_t triggertype = ruleData[rule].data.trigger.type;

				if(dependChanged && (((triggertype & TriggerMonoflopRetrigger) == TriggerMonoflopRetrigger) || ruleState[rule].timer == timerOff))
				{
					if((((triggertype & TriggerRising) == TriggerRising) && ruleValue) ||
					   (((triggertype & TriggerFalling) == TriggerFalling) && !ruleValue))
					{
						ruleValue = true;
						ruleState[rule].timer = now + (ruleData[rule].data.trigger.hour * (uint16_t)60 + ruleData[rule].data.trigger.min) * (uint32_t)60 + ruleData[rule].data.trigger.sec;
					} else {
						ruleValue = false;
					}
				}
				else if(ruleState[rule].timer != timerOff)
				{
					if(now < ruleState[rule].timer)
					{	// Stay on even if dependency switched off
						ruleValue = true;
					} else {
						ruleValue = false;
						ruleState[rule].timer = timerOff;
					}
				}
			} break;

			case ptSNTP:
			case ptRemote:
			{
				Packet_t sendPacket;
				// Check if USB Queue is free
				if(USB_isReady() && USB_prepareTS(&sendPacket))
				{
					int8_t length;
					const IP_Address_t ipCopy = ruleData[rule].data.IP;
					if(ruleData[rule].type == ptSNTP)
					{
						length = SNTP_GenerateRequest(sendPacket.data, &ipCopy, ruleData[rule].networkPort);
					} else { // ptRemote
						length = UDP_GenerateUnicast(sendPacket.data, &ipCopy, ruleData[rule].networkPort, sizeof(ruleValue_t));
						if(length > 0)
						{
							ruleValue_t *packetValue = (ruleValue_t *)(sendPacket.data + length);
							*packetValue = rvUnknown;
							length += sizeof(ruleValue_t);
						}
					}
					if(length > 0)
					{
						sendPacket.len = length;
						ruleState[rule].timer = now + 2;	// Timeout 2s in case of no answer;
					} else {
						sendPacket.len = -length;
						ruleState[rule].timer =  now + 1;	// Timeout 1s in case of missing ARP entry
					}
					USB_Send(sendPacket);
				}
				ruleValue = rvUnknown;
			} break;

			default:
			{
				ruleValue = rvUnknown;
				ruleState[rule].timer = timerOff;
			} break;
		}

		if(ruleValue == rvUnknown)
			continue;

		// Did the result of our rule change? Then set changeflag (ones' complement)
		if(ruleState[rule].value != ruleValue)
			ruleState[rule].value = ~ruleValue;
	}
}

// After each rule is processed, send network packets for each changed rule and reset changeflag
void sendChangedRules(void)
{
	for(ruleNum_t rule = 0; rule < ARRAY_SIZE(ruleData); rule++)
	{
		if(ruleState[rule].value != rvUnknown && ruleState[rule].value < 0)
		{
UDP_Port_t port = ruleData[rule].networkPort ? : rule;
			if(ruleData[rule].type >= 0 /*&& ruleData[rule].networkPort*/)
			{
				Packet_t sendPacket;
				if(USB_isReady() && USB_prepareTS(&sendPacket))
				{
					ruleState[rule].value = ~ruleState[rule].value;

					sendPacket.len = UDP_GenerateBroadcast(sendPacket.data, /*ruleData[rule].networkPort*/ port, sizeof(ruleValue_t));
					ruleValue_t *packetValue = (ruleValue_t *)(sendPacket.data + sendPacket.len);
					*packetValue = ruleState[rule].value;

					sendPacket.len += sizeof(ruleValue_t);
					USB_Send(sendPacket);
				}
				// else: could not send packet, try again next round
			} else {
				ruleState[rule].value = ~ruleState[rule].value;
			}
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
	ruleValue_t *packetValue = (ruleValue_t *)packet;

	if(length != sizeof(ruleValue_t) || *packetValue != rvUnknown)
		return false;

	for(ruleNum_t rule = 0; rule < ARRAY_SIZE(ruleData); rule++)
	{
		if(ruleData[rule].type < 0) continue;
		if(ruleData[rule].networkPort != destinationPort) continue;
		// Now we know the packet is for the current rule

		// Don't send an answer, if value is changed (will be send later automatically) or is unknown
		if(ruleState[destinationPort].value < 0) break;

		*packetValue = ruleState[destinationPort].value;
		return true;
	}
	return false;
}

ATTR_NON_NULL_PTR_ARG(1, 2)
void UDP_Callback_Reply(uint8_t packet[], const IP_Address_t *sourceIP, UDP_Port_t sourcePort, uint16_t length)
{
	for(ruleNum_t rule = 0; rule < ARRAY_SIZE(ruleData); rule++)
	{
		if(ruleData[rule].type >= 0) continue;
		if(ruleData[rule].networkPort != sourcePort) continue;
		if(ruleData[rule].data.IP != *sourceIP) continue;

		// Now we know the packet is for the current rule
		ruleValue_t newState;
		if(ruleData[rule].type == ptSNTP)
		{
			time_t newTime = SNTP_ProcessPacket(packet, length);
			if(!newTime) break;

			newState = true;
			ruleState[rule].timer = newTime + SNTP_TimeBetweenQueries;
		} else { // ptRemote
			ruleValue_t *packetValue = (ruleValue_t *)packet;

			if(length != sizeof(ruleValue_t) || *packetValue < 0)
				break;

			newState = *packetValue;
			ruleState[rule].timer = timerOff;
		}
		if(ruleState[rule].value != newState)
		{
			ruleState[rule].value = ~newState;
		}
		// Dont check this packet against later rules
		break;
	}
}
