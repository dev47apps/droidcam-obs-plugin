# Variables with ?= can be overridden
# Example: `ALLOW_STATIC=yes  make`

ALLOW_STATIC ?= no    # Allow static linking some deps
ENABLE_GUI   ?= no    # Enable Gui components (links with Qt)

# libimobiledevice for static linking
IMOBILEDEV_DIR ?= /opt/libimobiledevice
IMOBILEDEV_LIB ?= $(IMOBILEDEV_DIR)/lib

# libjpeg-turbo for static linking
JPEG_DIR ?= /opt/libjpeg-turbo
JPEG_LIB ?= $(JPEG_DIR)/lib$(shell getconf LONG_BIT)

# other overrides
LIBOBS_INCLUDES ?= /usr/include/obs
FFMPEG_INCLUDES ?= /usr/include/ffmpeg


#
define ADD_Lib =
	INCLUDES += $(shell pkg-config --cflags-only-I  $(1))
	LDD_LIBS += $(shell pkg-config --libs-only-l  $(1))
endef

ifeq "$(ALLOW_STATIC)" "yes"
	INCLUDES += -I$(IMOBILEDEV_DIR)/include
	INCLUDES += -I$(JPEG_DIR)/include

	STATIC += $(JPEG_LIB)/libturbojpeg.a
	STATIC += $(IMOBILEDEV_LIB)/libimobiledevice.a
	STATIC += $(IMOBILEDEV_LIB)/libusbmuxd.a
	STATIC += $(IMOBILEDEV_LIB)/libplist-2.0.a

else
$(eval	$(call ADD_Lib,libturbojpeg))
$(eval	$(call ADD_Lib,libusbmuxd))
endif

ifdef DROIDCAM_OVERRIDE
CXXFLAGS += -DDROIDCAM_OVERRIDE=1
ENABLE_GUI=yes
endif

ifeq "$(ENABLE_GUI)" "yes"
CXXFLAGS += -DENABLE_GUI=1
include linux/gui.mk
endif

INCLUDES += -I$(LIBOBS_INCLUDES)
INCLUDES += -I$(FFMPEG_INCLUDES)

LDD_LIBS += -lobs
LDD_FLAG += -shared
