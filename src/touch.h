/* Tap latching for the evdev touch driver.
 *
 * touch_read() drains every pending event before reporting, but LVGL only
 * polls every LV_INDEV_DEF_READ_PERIOD (30ms). A tap fast enough that its
 * press AND release both land inside one poll window would otherwise be
 * reported as "released" only - LVGL never sees a press, so LV_EVENT_CLICKED
 * never fires and the button silently ignores the tap.
 *
 * So: if the drain saw a press but the finger is already up, report the
 * press now and hold the release for the next poll. LVGL then sees a proper
 * press/release pair, 30ms late but intact.
 */
#ifndef TOUCH_H
#define TOUCH_H

/* saw_press:   a BTN_TOUCH=1 arrived during this drain
 * hw_pressed:  finger state after the drain
 * deferred:    carried between calls; caller owns the storage
 * returns:     1 to report PRESSED this poll, 0 for RELEASED
 *
 * ponytail: one tap of memory. Two complete taps inside a single 30ms
 * window collapse to one; add a small counter if that ever shows up. */
static inline int touch_latch(int saw_press, int hw_pressed, int *deferred)
{
    if (*deferred) { *deferred = 0; return 0; }   /* release the latched tap */
    if (saw_press && !hw_pressed) {               /* whole tap inside one window */
        *deferred = 1;
        return 1;
    }
    return hw_pressed;
}

#endif /* TOUCH_H */
