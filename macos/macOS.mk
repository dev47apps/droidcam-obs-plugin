# Variables with ?= can be overridden
# Example: `ARCH=arm64 make `
ARCH       ?= x86_64
DEPS_DIR   ?= ../deps/28
OBS_DIR    ?= $(DEPS_DIR)/src
FFMPEG_DIR ?= $(DEPS_DIR)
QT_DIR     ?= /usr/local/opt/qt6
JPEG_DIR   ?= /usr/local/opt/libjpeg-turbo


CXXFLAGS += -dead_strip
CXXFLAGS += -target $(ARCH)-apple-darwin

INCLUDES += -I$(QT_DIR)/include
INCLUDES += -I$(QT_DIR)/include/QtCore
INCLUDES += -I$(QT_DIR)/include/QtGui
INCLUDES += -I$(QT_DIR)/include/QtSvg
INCLUDES += -I$(QT_DIR)/include/QtWidgets
INCLUDES += -I$(QT_DIR)/include/SvgWidgets

INCLUDES += -I$(JPEG_DIR)/include
INCLUDES += -I$(FFMPEG_DIR)/include
INCLUDES += -I$(OBS_DIR)/UI
INCLUDES += -I$(OBS_DIR)/UI/obs-frontend-api
INCLUDES += -I$(OBS_DIR)/libobs

LDD_DIRS += -L$(JPEG_DIR)/lib
LDD_DIRS += -L$(FFMPEG_DIR)/lib
LDD_DIRS += -L$(DEPS_DIR)/lib

LDD_LIBS += -lobs.0
LDD_LIBS += -lobs-frontend-api
LDD_LIBS += -lavcodec.59 -lavformat.59 -lavutil.57
LDD_LIBS += -lturbojpeg

LDD_LIBS += $(DEPS_DIR)/lib/QtCore.framework/QtCore
LDD_LIBS += $(DEPS_DIR)/lib/QtGui.framework/QtGui
LDD_LIBS += $(DEPS_DIR)/lib/QtSvg.framework/QtSvg
LDD_LIBS += $(DEPS_DIR)/lib/QtXml.framework/QtXml
LDD_LIBS += $(DEPS_DIR)/lib/QtWidgets.framework/QtWidgets

LDD_FLAG += -bundle