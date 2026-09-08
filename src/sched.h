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
