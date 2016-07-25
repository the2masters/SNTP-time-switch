enum RuleNames {
// Global
	SystemError = 0,	// This is the default dependency for all rules
	TimeOK,
	Time1,
	Time2,
	LogicOr,
	RelayK1,
	RelayK2
};

static const __flash ruleData_t ruleData[] = {
// Global rules
	[SystemError] = {.type = ptLogic, .data.logic = {.type = LogicForceOff }, .networkPort = 0},		// Should be ptSystemError
	[TimeOK] = {.type = ptSNTP, .data.IP = CPU_TO_BE32(0xC0A8C803), .networkPort = 123},
// Zeiten
	[Time1] = {.type = ptTimeSwitch, .dependIndex = 0, .data.time = {.wdays = 0b01111111, .starthour = 12, .startmin = 0, .stophour = 13, .stopmin = 0}},
	[Time2] = {.type = ptTimeSwitch, .dependIndex = 0, .data.time = {.wdays = 0b01111111, .starthour = 17, .startmin = 30, .stophour = 18, .stopmin = 30}},
	[LogicOr] = {.type = ptLogic, .dependIndex = Time1, .data.logic = {.second = Time2, .type = LogicORAB}}, // (Time1 v Time2)
// Outputs
	[RelayK1] = {.type = ptRelay, .dependIndex = LogicOr, .data.hw = {.port = &PORTC, .bitValue = _BV(5)}},	// K1
	[RelayK2] = {.type = ptRelay, .dependIndex = 0, .data.hw = {.port = &PORTC, .bitValue = _BV(4)}},	// K2
};

