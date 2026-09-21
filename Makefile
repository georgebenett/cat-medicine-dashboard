LVGL_DIR      := $(CURDIR)
LVGL_DIR_NAME := lvgl

CC  := gcc
CXX := g++

CFLAGS   := -O2 -Wall -Wno-unused-function -I$(LVGL_DIR) -I$(LVGL_DIR)/src
CXXFLAGS := $(CFLAGS) -std=c++11
LDFLAGS  := -lm -lpthread

# The seven src makefiles, not lvgl.mk: its first two lines pull in
# demos/ and examples/ - the music player, the benchmark, every widget
# example - which was 234 of 424 object files linked into a cat dashboard.
include $(LVGL_DIR)/lvgl/src/core/lv_core.mk
include $(LVGL_DIR)/lvgl/src/draw/lv_draw.mk
include $(LVGL_DIR)/lvgl/src/extra/lv_extra.mk
include $(LVGL_DIR)/lvgl/src/font/lv_font.mk
include $(LVGL_DIR)/lvgl/src/hal/lv_hal.mk
include $(LVGL_DIR)/lvgl/src/misc/lv_misc.mk
include $(LVGL_DIR)/lvgl/src/widgets/lv_widgets.mk

CSRCS   += main.c $(wildcard src/*.c)
CXXSRCS := $(wildcard src/*.cpp)

COBJS   := $(CSRCS:.c=.o)
CXXOBJS := $(CXXSRCS:.cpp=.o)
OBJS    := $(COBJS) $(CXXOBJS)

app: $(OBJS)
	$(CXX) -o $@ $(OBJS) $(LDFLAGS)

# lv_conf.h reshapes half of LVGL's structs, so everything depends on it.
# Must come AFTER the app rule: a rule's first target is make's default
# goal, and $(OBJS) starting the file made `make` build one .o and stop.
$(OBJS): lv_conf.h

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) app

# Logic checked without LVGL or a panel.
test:
	@$(CC) -Wall -Wextra -I. -o /tmp/sched_test tests/test_sched.c && /tmp/sched_test
	@$(CC) -Wall -Wextra -I. -o /tmp/touch_test tests/test_touch.c && /tmp/touch_test

.PHONY: clean test
