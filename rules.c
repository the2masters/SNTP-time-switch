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

#define RuleValueOffset 1
#define boolToRuleValue(val) (val + RuleValueOffset)
#define RuleValueToBool(val) (val - RuleValueOffset)

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
	ptMonoflop,
	ptSunrise,
	ptSunset,
	ptSNTP = -1,	// negative = remote dependency
	ptRemote = -2
} ruleType_t;

typedef enum {
	LogicXOR = _BV(6),
	LogicINV_E = _BV(5),
	LogicINV_D = _BV(4),
	LogicINV_C = _BV(3),
	LogicINV_B = _BV(2),
	LogicINV_A = _BV(1),
	LogicINV_Q = _BV(0),

	LogicRepeat = 0,
	LogicNOT = LogicINV_Q,
	LogicForceOn = LogicINV_E,
	LogicForceOff = LogicForceOn ^ LogicINV_Q,
	LogicOR = 0,
	LogicNOR = LogicOR ^ LogicINV_Q,
	LogicAND = LogicINV_A ^ LogicINV_B ^ LogicINV_C ^ LogicINV_D ^ LogicINV_E ^ LogicINV_Q,
	LogicNAND = LogicAND ^ LogicINV_Q,
	LogicXNOR = LogicXOR ^ LogicINV_Q,
} LogicType_t;

typedef struct {
	ruleType_t		type;
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
			bool	minmaxtime;	// false: minimum time, true: maximum time
			int8_t	hour;
			int8_t	min;
			int8_t	sec;
		} monoflop;
		struct {
			ruleNum_t second;	// Zero means no argument
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
	[0] = {.type = ptSNTP, .dependIndex = 123, .data.IP = CPU_TO_BE32(0xC0A8C801)},
	[1] = {.type = ptRemote, .dependIndex = 1, .data.IP = CPU_TO_BE32(0xC0A8C802)},
	[2] = {.type = ptTimeSwitch, .dependIndex = 0, .data.time = {.backward = false, .hour = 12, .min = 0, .wdays = 0b01111111}},
	[3] = {.type = ptRelay, .dependIndex = 3, .data.hw = {.port = &PORTC, .bitValue = _BV(5)}},
};

static ruleState_t ruleState[ARRAY_SIZE(ruleData)] = {{0}};


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

ATTR_PURE static bool checkDependency(ruleNum_t dependIndex, ruleNum_t noDependIndex, bool *value, bool *changed)
{
	if(dependIndex == noDependIndex)
		return false;

	ruleValue_t dependency = ruleState[dependIndex].value;
	if(dependency == pvUnknown)
		return true;

	if(dependency < pvUnknown)
	{
		*changed = true;
		dependency = -dependency;
	}
	*value = RuleValueToBool(dependency);

	return false;
}

void checkRules(void)
{
	time_t now = time(NULL);

	for(ruleNum_t rule = 0; rule < ARRAY_SIZE(ruleData); rule++)
	{
		bool ruleValue = false;
		bool dependChanged = false;

		// Check if dependency is in state unknown, otherwise set ruleValue to dependencyValue
		if(ruleData[rule].type >= 0 && checkDependency(ruleData[rule].dependIndex, rule, &ruleValue, &dependChanged))
			continue;

		// For ptLogic type we need to check more dependencies for unknown and changed values.
		bool ptlogicValues[4] = {0};	// Save State of input B - E
		if(ruleData[rule].type == ptLogic)
		{
			if(checkDependency(ruleData[rule].data.logic.second, 0, &ptlogicValues[0], &dependChanged))
				continue;

			if(checkDependency(ruleData[rule].data.logic.third, 0, &ptlogicValues[1], &dependChanged))
				continue;

			if(checkDependency(ruleData[rule].data.logic.fourth, 0, &ptlogicValues[2], &dependChanged))
				continue;

			// Fixed input E is always false
		}


		// If we depend on a rule which didn't change in this run and our timer has not expired, skip this rule
		if(!dependChanged && now < ruleState[rule].timer)
			continue;

		// Set few defaults for following rules
		bool resultUnknown = false;
		time_t newTimer = timerOff;

		switch(ruleData[rule].type)
		{
			case ptLogic: ;
				LogicType_t logictype = ruleData[rule].data.logic.type;

				if((logictype & LogicINV_A) == LogicINV_A)
					ruleValue = !ruleValue;
				if((logictype & LogicINV_B) == LogicINV_B)
					ptlogicValues[0] = !ptlogicValues[0];
				if((logictype & LogicINV_C) == LogicINV_C)
					ptlogicValues[1] = !ptlogicValues[1];
				if((logictype & LogicINV_D) == LogicINV_D)
					ptlogicValues[2] = !ptlogicValues[2];
				if((logictype & LogicINV_E) == LogicINV_E)
					ptlogicValues[3] = !ptlogicValues[3];

				if((logictype & LogicXOR) == LogicXOR)
					ruleValue ^= ptlogicValues[0] ^ ptlogicValues[1] ^ ptlogicValues[2] ^ ptlogicValues[3];
				else
					ruleValue |= ptlogicValues[0] | ptlogicValues[1] | ptlogicValues[2] | ptlogicValues[3];

				if((logictype & LogicINV_Q) == LogicINV_Q)
					ruleValue = !ruleValue;
				break;

			case ptRelay:
				if(ruleValue)
					*ruleData[rule].data.hw.port |= ruleData[rule].data.hw.bitValue;
				else
					*ruleData[rule].data.hw.port &= ~ruleData[rule].data.hw.bitValue;
				break;

			case ptTimeSwitch: ;
				struct tm *tm = getStructTM(now);

				if((ruleData[rule].data.time.wdays & _BV(tm->tm_wday)) == 0)
				{
					ruleValue = false;
					newTimer = getMidnight(tm);
					break;
				}

				ruleValue = ruleData[rule].data.time.backward;
				time_t switchtime = calculateTimestamp(tm, ruleData[rule].data.time.hour, ruleData[rule].data.time.min);

				if(now < switchtime)
				{
					newTimer = switchtime;
				} else {
					newTimer = getMidnight(tm);
					ruleValue = !ruleValue;
				}
				break;

			case ptMonoflop:
				if(ruleState[rule].timer == timerOff)
				{
					if(dependChanged && ruleValue)
					{
						newTimer = now + (ruleData[rule].data.monoflop.hour * (int16_t)60 + ruleData[rule].data.monoflop.min) * (int32_t)60 + ruleData[rule].data.monoflop.sec;
					}
				}
				else if(now < ruleState[rule].timer)
				{
					ruleValue = true;
				} else {
					ruleValue = false;
					newTimer = timerOff;
				}

			case ptSNTP:
			case ptRemote:
				resultUnknown = true;

				Packet_t sendPacket;
				// Check if USB Queue is free
				if(USB_isReady() && USB_prepareTS(&sendPacket))
				{
					int8_t length;

					if(ruleData[rule].type == ptSNTP)
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
		ruleState[rule].timer = newTimer;
		if(resultUnknown)
			continue;

		ruleValue_t newValue = boolToRuleValue(ruleValue);

		// Did the result of our rule change? Then set changeflag (inverted value)
		if(ruleState[rule].value != newValue)
			ruleState[rule].value = -newValue;
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
		if(ruleData[rule].type >= 0) continue;
		if(ruleData[rule].dependIndex != sourcePort) continue;
		if(ruleData[rule].data.IP != *sourceIP) continue;

		// Now we know the packet is for the current rule
		ruleValue_t newState;
		if(ruleData[rule].type == ptSNTP)
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
