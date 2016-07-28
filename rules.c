#include <util/delay.h>
#include <stdint.h>
#include <stdbool.h>
#include "rules.h"
#include "resources.h"
#include "helper.h"
#include "timestamp.h"
#include "USB.h"
#include "Lib/Ethernet.h"
#include "Lib/SNTP.h"
#include "Lib/UDP.h"

typedef uint32_t ruleValue_t;
typedef uint8_t ruleNum_t;

typedef enum {
	ruleUnknown = 0,
	ruleOK = 1,
	ruleChanged = 2,
	ruleSendLater = 3
} ruleOK_t;

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
	LogicINV_A = _BV(0),
	LogicINV_B = _BV(1),
	LogicINV_C = _BV(2),
	LogicINV_D = _BV(3),
	LogicINV_Q1 = _BV(4),
	LogicINV_Q2 = _BV(5),
	LogicINV_Q = _BV(6),
	LogicXOR = _BV(7),

	LogicORAB = 0,
	LogicORCD = 0,
	LogicORQ12 = 0,
	LogicANDAB = LogicORAB ^ LogicINV_A ^ LogicINV_B ^ LogicINV_Q1,
	LogicANDCD = LogicORCD ^ LogicINV_C ^ LogicINV_D ^ LogicINV_Q2,
	LogicANDQ12 = LogicORQ12 ^ LogicINV_Q1 ^ LogicINV_Q2 ^ LogicINV_Q,
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
	MathAddition = _BV(0),
	MathSubtraction = _BV(1),
	MathMultiplication = _BV(2),
	MathDivision = _BV(3),
	MathSet = _BV(7),
} MathType_t;

typedef enum {
	TriggerRising = _BV(0),
	TriggerFalling = _BV(1),
	TriggerMonoflopRetrigger = _BV(2),

	TriggerBoth = TriggerRising | TriggerFalling,
} TriggerType_t;

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
_Static_assert(sizeof(((ruleData_t *)0)->data) == 4, "data field grew over 4 bytes");

typedef struct {
	time_t		timer;
	ruleValue_t	value;
	ruleOK_t	ok;
} ruleState_t;
#define timerOff UINT32_MAX

#include STRINGIFY_EXPANDED(CONFIG)

static ruleState_t ruleState[] = {[0 ... ARRAY_SIZE(ruleData)-1] = {.timer = 5}};

static bool checkDependency(ruleNum_t dependIndex, bool *changed)
{
	if(ruleState[dependIndex].ok == ruleUnknown) return true;
	if(ruleState[dependIndex].ok == ruleChanged) *changed = true;
	return false;
}

void checkRules(void)
{
	time_t now = time(NULL);

	for(ruleNum_t rule = 0; rule < ARRAY_SIZE(ruleData); rule++)
	{
		bool dependChanged = false;

		// Check if dependency is in state unknown or changed its value
		if(checkDependency(ruleData[rule].dependIndex, &dependChanged))
 		{
			// If the dependency is unknown, we can't proceed
			ruleState[rule].ok = ruleUnknown;
			continue;
		}

		// For ptLogic type we need to check more dependencies for unknown and changed values.
		if(ruleData[rule].type == ptLogic)
		{
			if(checkDependency(ruleData[rule].data.logic.second, &dependChanged) ||
			   checkDependency(ruleData[rule].data.logic.third, &dependChanged) ||
			   checkDependency(ruleData[rule].data.logic.fourth, &dependChanged))
			{
				// If a dependency is unknown, we can't proceed
				ruleState[rule].ok = ruleUnknown;
				continue;
	                }
		}

		// If we depend on a rule which didn't change in this run and our timer has not expired, skip this rule
		if(!dependChanged && now < ruleState[rule].timer)
			continue;

		// Start with the dependency rule value as resulting rule value
		ruleValue_t ruleValue = ruleState[ruleData[rule].dependIndex].value;
		// Reset State of this rule, but keep Flag ruleSendLater
		if(ruleState[rule].ok != ruleSendLater)
			ruleState[rule].ok = ruleOK;

		switch(ruleData[rule].type)
		{
			case ptLogic:
			{
				LogicType_t logictype = ruleData[rule].data.logic.type;
				ruleValue_t dependValueB = ruleState[ruleData[rule].data.logic.second].value;
				ruleValue_t dependValueC = ruleState[ruleData[rule].data.logic.third].value;
				ruleValue_t dependValueD = ruleState[ruleData[rule].data.logic.fourth].value;

				if((logictype & LogicINV_A) == LogicINV_A)
					ruleValue = ~ruleValue;
				if((logictype & LogicINV_B) == LogicINV_B)
					dependValueB = ~dependValueB;
				if((logictype & LogicINV_C) == LogicINV_C)
					dependValueC = ~dependValueC;
				if((logictype & LogicINV_D) == LogicINV_D)
					dependValueD = ~dependValueD;

				ruleValue_t Q1, Q2;
				if((logictype & LogicXOR) == LogicXOR)
				{
					Q1 = ruleValue ^ dependValueB;
					Q2 = dependValueC ^ dependValueD;
				} else {
					Q1 = ruleValue | dependValueB;
					Q2 = dependValueC | dependValueD;
				}
				if((logictype & LogicINV_Q1) == LogicINV_Q1)
					Q1 = ~Q1;
				if((logictype & LogicINV_Q2) == LogicINV_Q2)
					Q2 = ~Q2;
				if((logictype & LogicXOR) == LogicXOR)
					ruleValue = Q1 ^ Q2;
				else
					ruleValue = Q1 | Q2;

				if((logictype & LogicINV_Q) == LogicINV_Q)
					ruleValue = ~ruleValue;

				ruleState[rule].timer = timerOff;
			} break;

			case ptRelay:
			{
//TODO: Irgendwo muss DDR auch gesetzt werden
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
					ruleValue = 0;
					ruleState[rule].timer = getMidnight(tm);
					break;
				}

				time_t starttime = calculateTimestamp(tm, ruleData[rule].data.time.starthour, ruleData[rule].data.time.startmin);
				if(now < starttime)
				{
					ruleValue = 0;
					ruleState[rule].timer = starttime;
				} else {
					// Optimization: Remove countdown or calculate only one time (not every second)
	                                time_t stoptime = calculateTimestamp(tm, ruleData[rule].data.time.stophour, ruleData[rule].data.time.stopmin);
					if(now < stoptime)
					{
						ruleValue = stoptime - now;
					//TODO: Make Update-Interval configurable
						ruleState[rule].timer = now + 1;
					} else {
						ruleValue = 0;
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
						//TODO: Implement countdown?
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
					} else { // ptRemote: Send Packet with empty body
						length = UDP_GenerateUnicast(sendPacket.data, &ipCopy, ruleData[rule].networkPort, sizeof(ruleValue_t));
						if(length > 0)
						{
							ruleValue_t *packetValue = (ruleValue_t *)(sendPacket.data + length);
							*packetValue = 0;
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
				ruleState[rule].ok = ruleUnknown;
			} break;

			default:
			{
				ruleState[rule].ok = ruleUnknown;
				ruleState[rule].timer = timerOff;
			} break;
		}

		if(ruleState[rule].ok == ruleUnknown)
			continue;

		// Did the result of our rule change? Then set changeflag
		if(ruleState[rule].value != ruleValue)
			ruleState[rule].ok = ruleChanged;
	}
}

// After each rule is processed, send network packets for each changed rule and reset changeflag
void sendChangedRules(void)
{
	for(ruleNum_t rule = 0; rule < ARRAY_SIZE(ruleData); rule++)
	{
		if(ruleState[rule].ok >= ruleChanged)
		{
UDP_Port_t port = ruleData[rule].networkPort ? : rule;
			if(ruleData[rule].type >= 0 /*&& ruleData[rule].networkPort*/)
			{
				Packet_t sendPacket;
				if(USB_isReady() && USB_prepareTS(&sendPacket))
				{
					sendPacket.len = UDP_GenerateBroadcast(sendPacket.data, /*ruleData[rule].networkPort*/ port, sizeof(ruleValue_t));
					ruleValue_t *packetValue = (ruleValue_t *)(sendPacket.data + sendPacket.len);
					*packetValue = ruleState[rule].value;

					sendPacket.len += sizeof(ruleValue_t);
					USB_Send(sendPacket);

					ruleState[rule].ok = ruleOK;
				}
				// else: could not send packet, try again next round
			} else {
				ruleState[rule].ok = ruleSendLater;
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

	if(length != sizeof(ruleValue_t) && *packetValue != 0)
		return false;

	for(ruleNum_t rule = 0; rule < ARRAY_SIZE(ruleData); rule++)
	{
		if(ruleData[rule].type < 0) continue;
		if(ruleData[rule].networkPort != destinationPort) continue;
		// Now we know the packet is for the current rule

		// Don't send an answer, if value is unknown
		if(ruleState[destinationPort].ok == ruleUnknown) break;

		*packetValue = ruleState[destinationPort].value;
		ruleState[destinationPort].ok = ruleOK; // If the rule was in state ruleChanged or ruleSendLater, this is now fixed
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
		ruleValue_t ruleValue;
		if(ruleData[rule].type == ptSNTP)
		{
			time_t newTime = SNTP_ProcessPacket(packet, length);
			if(!newTime) break;

			ruleValue = (ruleValue_t)newTime;
			ruleState[rule].timer = newTime + SNTP_TimeBetweenQueries;
		} else { // ptRemote
			if(length != sizeof(ruleValue_t))
				break;

			ruleValue = *(ruleValue_t *)packet;
			ruleState[rule].timer = timerOff;
		}
		if(ruleState[rule].value == ruleValue)
			ruleState[rule].ok = ruleOK;
		else
			ruleState[rule].ok = ruleChanged;

		// Don't check this packet against later rules
		break;
	}
}
