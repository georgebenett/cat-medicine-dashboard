/* Pure date and schedule math - no LVGL, no globals, so it can be checked
 * on its own. See test_sched.c, run by `make test`. */
#pragma once

/* Days since 1970-01-01 from a civil date, by Howard Hinnant's algorithm.
 * Deliberately not mktime()/86400: that is timezone-dependent, and a
 * 23-hour DST day can land both noons in the same UTC day. Anchoring at
 * noon hides it in Europe but not everywhere - scanning 2026 in
 * Pacific/Auckland gives a 0-day step on 09-27 and a 2-day step on 04-05.
 * This version is pure integer math and has no timezone at all. */
static inline long sched_day_num(int y, int m, int d)
{
    y -= (m <= 2);
    long era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (unsigned)((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1);
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (long)doe - 719468;
}

/* Civil date back from a day number - the inverse of sched_day_num, same
 * algorithm run backwards. Lets the calendar and week strip label cells
 * straight from a day number instead of walking a struct tm. */
static inline void sched_civil(long z, int *y, int *m, int *d)
{
    z += 719468;
    long era = (z >= 0 ? z : z - 146096) / 146097;
    unsigned doe = (unsigned)(z - era * 146097);
    unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    unsigned mp  = (5 * doy + 2) / 153;
    unsigned dd  = doy - (153 * mp + 2) / 5 + 1;
    unsigned mm  = mp + (mp < 10 ? 3 : (unsigned)-9);
    *y = (int)((long)yoe + era * 400 + (mm <= 2));
    *m = (int)mm;
    *d = (int)dd;
}

/* Weekday, 0=Sunday. Epoch day 0 (1970-01-01) was a Thursday. The extra
 * +7 keeps it right for negative day numbers. */
static inline int sched_wday(long dn)
{
    return (int)(((dn % 7) + 11) % 7);
}

/* Day number of the Monday on or before `dn` - weeks run Mon..Sun here,
 * matching the mockup's calendar and "this week" strip. */
static inline long sched_monday(long dn)
{
    return dn - ((sched_wday(dn) + 6) % 7);
}

static inline int sched_days_in_month(int y, int m)
{
    static const int d[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) return 29;
    return d[m - 1];
}

/* Is `m` (minutes since midnight) inside [start, end)? Wraps past midnight
 * when start > end, which is what a night window needs. start == end means
 * the window is disabled, never always-on - an empty range is the safer
 * reading of "no window set".
 *
 * Shared by the night dim and the commute board so there is one piece of
 * wrap-around logic rather than two. */
static inline int sched_in_window(int m, int start, int end)
{
    if (start == end)  return 0;
    if (start < end)   return m >= start && m < end;
    return m >= start || m < end;
}

/* ISO 8601 week number. The week belongs to whichever year its Thursday
 * falls in, which is what makes 29 Dec 2025 week 1 and 1 Jan 2027 week 53. */
static inline int sched_iso_week(long dn)
{
    long thu = sched_monday(dn) + 3;
    int y, m, d;
    sched_civil(thu, &y, &m, &d);
    return (int)((thu - sched_day_num(y, 1, 1)) / 7) + 1;
}

/* Weekday (0=Sunday) of the next scheduled dose strictly after `wday`,
 * or -1 if no day is scheduled. A single scheduled day returns itself. */
static inline int sched_next_wday(int mask, int wday)
{
    if (!(mask & 0x7f)) return -1;
    for (int i = 1; i <= 7; i++) {
        int w = (wday + i) % 7;
        if (mask & (1 << w)) return w;
    }
    return -1;
}
