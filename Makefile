LVGL_DIR      := $(CURDIR)
LVGL_DIR_NAME := lvgl

CC  := gcc
CXX := g++

CFLAGS   := -O2 -Wall -Wno-unused-function -I$(LVGL_DIR) -I$(LVGL_DIR)/src
CXXFLAGS := $(CFLAGS) -std=c++11
LDFLAGS  := -lm -lpthread

include $(LVGL_DIR)/lvgl/lvgl.mk

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
	@$(CC) -Wall -Wextra -I. -o /tmp/sched_test test_sched.c && /tmp/sched_test
	@$(CC) -Wall -Wextra -I. -o /tmp/touch_test test_touch.c && /tmp/touch_test

.PHONY: clean test
