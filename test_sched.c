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

    printf("sched: all checks passed\n");
    return 0;
}
