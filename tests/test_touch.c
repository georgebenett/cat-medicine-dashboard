/* Self-check for tap latching: `make test`. */
#include <assert.h>
#include <stdio.h>
#include "src/touch.h"

int main(void)
{
    int d = 0;

    /* Finger down, still down: plain press. */
    assert(touch_latch(1, 1, &d) == 1);  assert(d == 0);
    /* Held across polls, no new edge. */
    assert(touch_latch(0, 1, &d) == 1);  assert(d == 0);
    /* Lifted. */
    assert(touch_latch(0, 0, &d) == 0);  assert(d == 0);
    /* Idle stays idle. */
    assert(touch_latch(0, 0, &d) == 0);  assert(d == 0);

    /* The bug this exists for: press and release inside one poll window.
     * Must report PRESSED now and RELEASED next, not swallow the tap. */
    assert(touch_latch(1, 0, &d) == 1);  assert(d == 1);
    assert(touch_latch(0, 0, &d) == 0);  assert(d == 0);

    /* A new touch arriving while a release is still owed: the release wins
     * this poll, the new press lands on the next one. */
    d = 1;
    assert(touch_latch(1, 1, &d) == 0);  assert(d == 0);
    assert(touch_latch(0, 1, &d) == 1);  assert(d == 0);

    printf("touch latch: all assertions passed\n");
    return 0;
}
