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

