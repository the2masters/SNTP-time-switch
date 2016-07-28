enum RuleNames {
// Global
	SystemError = 0,	// This is the default dependency for all rules
	TimeOK,
	TimeTest,
	Logic,
	RelayK1,
	RelayK2
};

static const __flash ruleData_t ruleData[] = {
// Global rules
	[SystemError] = {.type = ptLogic, .data.logic = {.type = LogicForceOff }, .networkPort = 0},		// Should be ptSystemError
	[TimeOK] = {.type = ptSNTP, .data.IP = CPU_TO_BE32(0xC0A8C803), .networkPort = 123},
	[TimeTest] = {.type = ptTimeSwitch, .dependIndex = 0, .data.time = {.wdays = 0b01111111, .starthour = 1, .startmin = 0, .stophour = 1, .stopmin = 1}, .networkPort = 2},
	[Logic] = {.type = ptLogic, .dependIndex = TimeOK, .data.logic = {.type = LogicRepeat}, .networkPort = 5},
// Outputs
	[RelayK1] = {.type = ptRelay, .dependIndex = TimeTest, .data.hw = {.port = &PORTC, .bitValue = _BV(5)}},	// K1
	[RelayK2] = {.type = ptRelay, .dependIndex = 0, .data.hw = {.port = &PORTC, .bitValue = _BV(4)}},	// K2
};

