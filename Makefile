# Copyright (C) 2021 DEV47APPS, github.com/dev47apps

DATA_DIR = data
BUILD_DIR = build

RM       = rm -f
CXX      = g++
CXXFLAGS = -std=c++11 -x c++ -Wall -fPIC
INCLUDES = -Isrc/
LDD_DIRS =
LDD_LIBS =
LDD_FLAG =
LIB_DLL  = $(BUILD_DIR)/droidcam-obs.so
STATIC   =
SRC      = $(shell ls src/*.c src/sys/unix/*.c)

ifeq ($(OS),Windows_NT)
all:
	@exit
endif

.PHONY: run clean
all: $(LIB_DLL)
debug: CXXFLAGS += -DDEBUG
debug: all

UNAME := $(shell uname -s)
ifeq ($(UNAME),Linux)
## LINUX ##
	JPEG_DIR ?= /opt/libjpeg-turbo
	JPEG_LIB ?= $(JPEG_DIR)/lib$(shell getconf LONG_BIT)

	STATIC   += $(JPEG_LIB)/libturbojpeg.a
	INCLUDES += -I$(JPEG_DIR)/include
	INCLUDES += -I/usr/include/obs
	LDD_LIBS += -lobs
	LDD_LIBS += -lusbmuxd
	LDD_FLAG += -shared

run:
	rm ~/.config/obs-studio/logs/* && obs
## LINUX ##
endif

ifeq ($(UNAME),Darwin)
# macOS
# Variables with ?= can be overridden
# Example: `OBS_DIR=/tmp/obs-26.0 ARCH=arm64 make `

QT_DIR     ?= /usr/local/opt/qt5
FFMPEG_DIR ?= /usr/local/opt/ffmpeg
JPEG_DIR   ?= /usr/local/opt/libjpeg-turbo
OBS_DIR    ?= ../obs-studio-25.0.8
ARCH       ?= x86_64

CXXFLAGS += -dead_strip
CXXFLAGS += -target $(ARCH)-apple-darwin

# INCLUDES += -I$(QT_DIR)/include
# INCLUDES += -I$(QT_DIR)/include/QtCore
# INCLUDES += -I$(QT_DIR)/include/QtWidgets
INCLUDES += -I$(JPEG_DIR)/include
INCLUDES += -I$(FFMPEG_DIR)/include
INCLUDES += -I$(OBS_DIR)/UI -I$(OBS_DIR)/libobs

LDD_DIRS += -L$(JPEG_DIR)/lib
LDD_DIRS += -L/Applications/OBS.app/Contents/Frameworks

LDD_LIBS += -lobs.0
LDD_LIBS += -lavcodec.58 -lavformat.58 -lavutil.56
LDD_LIBS += -lturbojpeg
LDD_FLAG += -bundle

run:
	rm ~/Library/ApplicationSupport/obs-studio/logs/* && /Applications/OBS.app/Contents/MacOS/OBS
endif

$(LIB_DLL): $(SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(LDD_DIRS) $(LDD_LIBS) $(LDD_FLAG) $^ $(STATIC) -o $@

clean:
	$(RM) $(BUILD_DIR)/*.o $(BUILD_DIR)/*.so
	$(RM) test adbz

adbz:
	$(CXX) $(CXXFLAGS) -o adbz src/test/adbz.c

test: adbz
	$(CXX) $(CXXFLAGS) -DDEBUG -DTEST -otest -Isrc/ -Isrc/test/ \
		src/net.c src/command.c src/sys/unix/cmd.c \
		src/test/main.c
