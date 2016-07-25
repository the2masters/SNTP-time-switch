#ifndef timestamp_h
#define timestamp_h

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

#endif
