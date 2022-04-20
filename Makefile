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

# Linux
ifeq ($(UNAME),Linux)

# Variables with ?= can be overridden
# Example: `JPEG_LIB=/usr/lib/aarch64-linux-gnu  ALLOW_STATIC=no  make `

# Allow static linking some deps
ALLOW_STATIC ?= yes

# libimobiledevice
IMOBILEDEV_DIR ?= /opt/libimobiledevice

# libjpeg-turbo
JPEG_DIR ?= /opt/libjpeg-turbo
JPEG_LIB ?= $(JPEG_DIR)/lib$(shell getconf LONG_BIT)

ifeq "$(ALLOW_STATIC)" "yes"
	STATIC += $(JPEG_LIB)/libturbojpeg.a
	STATIC += $(IMOBILEDEV_DIR)/lib/libimobiledevice.a
	STATIC += $(IMOBILEDEV_DIR)/lib/libusbmuxd.a
	STATIC += $(IMOBILEDEV_DIR)/lib/libplist-2.0.a

else
	LDD_DIRS += -L$(JPEG_LIB)
	LDD_DIRS += -L$(IMOBILEDEV_DIR)/lib

	LDD_LIBS += -lturbojpeg
	LDD_LIBS += -limobiledevice
endif

INCLUDES += -I$(IMOBILEDEV_DIR)/include
INCLUDES += -I$(JPEG_DIR)/include
INCLUDES += -I/usr/include/obs

LDD_LIBS += -lobs
LDD_FLAG += -shared

run: debug
	rm ~/.config/obs-studio/logs/* && obs
endif


# macOS
ifeq ($(UNAME),Darwin)

# Variables with ?= can be overridden
# Example: `OBS_DIR=/tmp/obs-26.0 ARCH=arm64 make `

ARCH       ?= x86_64
DEPS_DIR   ?= ../obs-deps
OBS_DIR    ?= ../obs-studio-25.0.8
FFMPEG_DIR ?= $(DEPS_DIR)
QT_DIR     ?= /usr/local/opt/qt5
JPEG_DIR   ?= /usr/local/opt/libjpeg-turbo


CXXFLAGS += -dead_strip
CXXFLAGS += -target $(ARCH)-apple-darwin

# INCLUDES += -I$(QT_DIR)/include
# INCLUDES += -I$(QT_DIR)/include/QtCore
# INCLUDES += -I$(QT_DIR)/include/QtWidgets
INCLUDES += -I$(JPEG_DIR)/include
INCLUDES += -I$(FFMPEG_DIR)/include
INCLUDES += -I$(OBS_DIR)/UI -I$(OBS_DIR)/libobs

LDD_DIRS += -L$(DEPS_DIR)/lib
LDD_DIRS += -L$(JPEG_DIR)/lib
LDD_DIRS += -L$(FFMPEG_DIR)/lib
LDD_DIRS += -L/Applications/OBS.app/Contents/Frameworks

LDD_LIBS += -lobs.0
LDD_LIBS += -lavcodec.58 -lavformat.58 -lavutil.56
LDD_LIBS += -lturbojpeg
LDD_FLAG += -bundle

run: debug
	rm ~/Library/ApplicationSupport/obs-studio/logs/* && /Applications/OBS.app/Contents/MacOS/OBS
endif


$(LIB_DLL): $(SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(LDD_DIRS) $(LDD_LIBS) $(LDD_FLAG) $^ $(STATIC) -o $@

clean:
	$(RM) $(BUILD_DIR)/*.o $(BUILD_DIR)/*.so $(BUILD_DIR)/*.exe

adbz:
	$(CXX) $(CXXFLAGS) -o$(BUILD_DIR)/adbz.exe src/test/adbz.c

test: adbz
	$(CXX) $(CXXFLAGS) -o$(BUILD_DIR)/test.exe -DDEBUG -DTEST -Isrc/test/ $(INCLUDES) \
		src/net.c src/command.c src/sys/unix/cmd.c \
		src/test/main.c
	$(BUILD_DIR)/test.exe
