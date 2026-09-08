/* Self-check for the date and schedule math: `make test`. */
#include <assert.h>
#include <stdio.h>
#include "src/sched.h"

int main(void)
{
    /* Consecutive days are exactly one apart, across both European DST
     * switches, month, year and leap-day boundaries. */
    assert(sched_day_num(2026, 9, 8)  - sched_day_num(2026, 9, 7)   == 1);
    assert(sched_day_num(2026, 3, 29) - sched_day_num(2026, 3, 28)  == 1);  /* clocks forward */
    assert(sched_day_num(2026, 10, 25) - sched_day_num(2026, 10, 24) == 1); /* clocks back */
    assert(sched_day_num(2026, 10, 1) - sched_day_num(2026, 9, 30)  == 1);  /* month */
    assert(sched_day_num(2027, 1, 1)  - sched_day_num(2026, 12, 31) == 1);  /* year */
    assert(sched_day_num(2028, 3, 1)  - sched_day_num(2028, 2, 29)  == 1);  /* leap day */
    assert(sched_day_num(1970, 1, 1) == 0);
    assert(sched_day_num(2026, 9, 8) - sched_day_num(2026, 9, 1) == 7);     /* the 7-day window */

    /* Mon/Wed/Fri, the default schedule. */
    const int mwf = (1 << 1) | (1 << 3) | (1 << 5);
    assert(sched_next_wday(mwf, 1) == 3);   /* Mon -> Wed */
    assert(sched_next_wday(mwf, 3) == 5);   /* Wed -> Fri */
    assert(sched_next_wday(mwf, 5) == 1);   /* Fri -> Mon, wrapping the week */
    assert(sched_next_wday(mwf, 6) == 1);   /* Sat -> Mon */
    assert(sched_next_wday(mwf, 0) == 1);   /* Sun -> Mon */
    assert(sched_next_wday(1 << 2, 2) == 2);/* one day a week: itself, next week */
    assert(sched_next_wday(0, 3) == -1);    /* nothing scheduled */

    /* Weekday. 2026-09-08 is a Tuesday; 1970-01-01 was a Thursday. */
    assert(sched_wday(sched_day_num(2026, 9, 8)) == 2);
    assert(sched_wday(sched_day_num(1970, 1, 1)) == 4);
    assert(sched_wday(sched_day_num(2026, 9, 6)) == 0);   /* Sunday */
    for (int i = 0; i < 14; i++)                          /* never out of range */
        assert(sched_wday(sched_day_num(2026, 9, 1) + i) >= 0 &&
               sched_wday(sched_day_num(2026, 9, 1) + i) <= 6);
    assert(sched_wday(-1) == 3);                          /* 1969-12-31, a Wednesday */

    /* Weeks run Monday..Sunday. */
    long mon = sched_day_num(2026, 9, 7);                 /* a Monday */
    assert(sched_monday(mon) == mon);                     /* Monday maps to itself */
    assert(sched_monday(sched_day_num(2026, 9, 8)) == mon);
    assert(sched_monday(sched_day_num(2026, 9, 13)) == mon); /* Sunday belongs to it */
    assert(sched_monday(sched_day_num(2026, 9, 14)) == mon + 7);

    assert(sched_days_in_month(2026, 2) == 28);
    assert(sched_days_in_month(2028, 2) == 29);           /* leap */
    assert(sched_days_in_month(2000, 2) == 29);           /* century leap */
    assert(sched_days_in_month(1900, 2) == 28);           /* century non-leap */
    assert(sched_days_in_month(2026, 9) == 30);

    /* sched_civil is the exact inverse of sched_day_num, every day for
     * eight years across leap years and month ends. */
    for (long dn = sched_day_num(2024, 1, 1); dn <= sched_day_num(2032, 1, 1); dn++) {
        int y, m, d;
        sched_civil(dn, &y, &m, &d);
        assert(sched_day_num(y, m, d) == dn);
        assert(m >= 1 && m <= 12 && d >= 1 && d <= sched_days_in_month(y, m));
    }
    { int y, m, d; sched_civil(0, &y, &m, &d); assert(y == 1970 && m == 1 && d == 1); }
    { int y, m, d; sched_civil(-1, &y, &m, &d); assert(y == 1969 && m == 12 && d == 31); }

    printf("sched: all checks passed\n");
    return 0;
}
