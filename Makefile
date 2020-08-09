# Copyright (C) 2020 github.com/aramg

DATA_DIR = data
BUILD_DIR = build

RM       = rm
CXX      = g++
CXXFLAGS = -std=c++11 -x c++ -Wall -fPIC
INCLUDES = -Isrc/
LDD_DIRS =
LDD_LIBS = -lobs-frontend-api
LDD_FLAG =
LIB_DLL  = $(BUILD_DIR)/droidcam-obs.so
STATIC   =
SRC      = $(shell ls src/*.c src/sys/unix/*.c)

ifeq "$(RELEASE)" "1"
	CXXFLAGS += -DRELEASE=1
endif

ifeq ($(OS),Windows_NT)
lol:
	@exit
endif

.PHONY: run clean
all: $(LIB_DLL)

UNAME := $(shell uname -s)
ifeq ($(UNAME),Linux)
## LINUX ##
	JPEG_DIR ?= /opt/libjpeg-turbo
	JPEG_LIB  = $(JPEG_DIR)/lib$(shell getconf LONG_BIT)

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
## MACOS ##
	CXXFLAGS += -dead_strip
	INCLUDES += -I/usr/local/opt/qt5/include
	INCLUDES += -I/usr/local/opt/qt5/include/QtCore
	INCLUDES += -I/usr/local/opt/qt5/include/QtWidgets
	INCLUDES += -I/usr/local/opt/libjpeg-turbo/include
	INCLUDES += -I../ffmpeg
	INCLUDES += -I../obs-studio-24.0.2/UI
	INCLUDES += -I../obs-studio-24.0.2/libobs
	LDD_DIRS += -L/Applications/OBS.app/Contents/Resources/bin
	LDD_DIRS += -L/usr/local/opt/libjpeg-turbo/lib
	LDD_LIBS += -lobs.0 -lavcodec.58 -lavformat.58 -lavutil.56
	LDD_LIBS += -lturbojpeg
	LDD_FLAG += -bundle

run:
	rm ~/Library/ApplicationSupport/obs-studio/logs/* && /Applications/OBS.app/Contents/MacOS/OBS
## MACOS ##
endif

$(LIB_DLL): $(SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(LDD_DIRS) $(LDD_LIBS) $(LDD_FLAG) $^ $(STATIC) -o $@

clean:
	$(RM) $(BUILD_DIR)/*.o $(BUILD_DIR)/*.so

adbz:
	gcc -o /tmp/adbz src/test/adbz.c

test: adbz
	g++ -DTEST -otest -Isrc/ -Isrc/test/ src/net.c src/command.c src/sys/unix/cmd.c src/test/main.c
