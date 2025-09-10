# Variables with ?= can be overridden
# Example: `ARCH=arm64 make `
ARCH       ?= x86_64
DEPS_DIR   ?= ../deps/32
OBS_DIR    ?= $(DEPS_DIR)/src
FFMPEG_DIR ?= $(DEPS_DIR)
QT_DIR     ?= /usr/local/opt/qt6
JPEG_DIR   ?= /usr/local/opt/libjpeg-turbo

CXXFLAGS += -dead_strip
CXXFLAGS += -target $(ARCH)-apple-darwin
CXXFLAGS += -DENABLE_GUI -DQT_NO_VERSION_TAGGING

INCLUDES += -I$(QT_DIR)/include
INCLUDES += -I$(QT_DIR)/include/QtCore
INCLUDES += -I$(QT_DIR)/include/QtGui
INCLUDES += -I$(QT_DIR)/include/QtSvg
INCLUDES += -I$(QT_DIR)/include/QtWidgets
INCLUDES += -I$(QT_DIR)/include/SvgWidgets

INCLUDES += -I$(JPEG_DIR)/include
INCLUDES += -I$(FFMPEG_DIR)/include
INCLUDES += -I$(OBS_DIR)/frontend
INCLUDES += -I$(OBS_DIR)/frontend/api
INCLUDES += -I$(OBS_DIR)/libobs

LDD_DIRS += -L$(JPEG_DIR)/lib
LDD_DIRS += -L$(FFMPEG_DIR)/lib
LDD_DIRS += -F$(DEPS_DIR)/lib

LDD_LIBS += -framework Foundation
LDD_LIBS += -framework QtCore
LDD_LIBS += -framework QtGui
LDD_LIBS += -framework QtSvg
LDD_LIBS += -framework QtXml
LDD_LIBS += -framework QtWidgets
LDD_LIBS += -framework libobs
LDD_LIBS += -lobs-frontend-api.1
LDD_LIBS += -lavcodec -lavformat -lavutil
LDD_LIBS += -lturbojpeg

LDD_FLAG += -bundle
