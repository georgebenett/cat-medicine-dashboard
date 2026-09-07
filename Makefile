LVGL_DIR      := $(CURDIR)
LVGL_DIR_NAME := lvgl

CC  := gcc
CXX := g++

CFLAGS   := -O2 -Wall -Wno-unused-function -I$(LVGL_DIR) -I$(LVGL_DIR)/src
CXXFLAGS := $(CFLAGS) -std=c++11
LDFLAGS  := -lm -lpthread

include $(LVGL_DIR)/lvgl/lvgl.mk

CSRCS   += main.c $(wildcard src/ui/*.c)
CXXSRCS := $(wildcard src/ui/*.cpp)

COBJS   := $(CSRCS:.c=.o)
CXXOBJS := $(CXXSRCS:.cpp=.o)
OBJS    := $(COBJS) $(CXXOBJS)

app: $(OBJS)
	$(CXX) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) app

.PHONY: clean
