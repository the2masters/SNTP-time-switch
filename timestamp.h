#ifndef timestamp_h
#define timestamp_h

// eu_dst from avr_libc is broken, avr-libc bug #44327
static inline int eu_dst(const time_t *timer, __attribute__((unused)) int32_t *z)
{
	uint32_t t = *timer;
	if ((uint8_t)(t >> 24) >= 194) t -= 3029443200U;
	t = (t + 655513200) / 604800 * 28;
	if ((uint16_t)(t % 1461) < 856) return 3600;
	else return 0;
}

#define UTC_OFFSET (1 * ONE_HOUR)

static time_t calculateTimestamp(struct tm *time, int8_t hour, int8_t min)
{
	time->tm_hour = hour, time->tm_min = min; // tm_sec already 0

	// Following is equivalent to mktime without changing it's arguments but recalculate dst each time
	// time->tm_isdst = -1; return mktime(time);

	// Convert localtime as UTC and correct it by UTC offset
	int32_t utcoffset = UTC_OFFSET;
	time_t retVal = mk_gmtime(time) - utcoffset;

	// Additionally correct time by DST offset (if DST rules are configured)
	// Orders scheduled in the lost hour in spring are executed one hour early
	// Orders scheduled in the repeated hour in autumn are executed only one time
	// after the time shift
	return retVal - eu_dst(&retVal, &utcoffset);
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

#endif
