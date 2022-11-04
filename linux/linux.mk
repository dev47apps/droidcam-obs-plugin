# Variables with ?= can be overridden
# Example: `ALLOW_STATIC=no  make`

# Allow static linking some deps
ALLOW_STATIC ?= yes

# libimobiledevice
IMOBILEDEV_DIR ?= /opt/libimobiledevice
IMOBILEDEV_LIB ?= $(IMOBILEDEV_DIR)/lib

# libjpeg-turbo
JPEG_DIR ?= /opt/libjpeg-turbo
JPEG_LIB ?= $(JPEG_DIR)/lib$(shell getconf LONG_BIT)

ifeq "$(ALLOW_STATIC)" "yes"
	STATIC += $(JPEG_LIB)/libturbojpeg.a
	STATIC += $(IMOBILEDEV_LIB)/libimobiledevice.a
	STATIC += $(IMOBILEDEV_LIB)/libusbmuxd.a
	STATIC += $(IMOBILEDEV_LIB)/libplist-2.0.a

else
	LDD_DIRS += -L$(JPEG_LIB)
	LDD_DIRS += -L$(IMOBILEDEV_LIB)

	LDD_LIBS += -lturbojpeg
	LDD_LIBS += -limobiledevice
endif

INCLUDES += -I$(IMOBILEDEV_DIR)/include
INCLUDES += -I$(JPEG_DIR)/include
INCLUDES += -I/usr/include/ffmpeg
INCLUDES += -I/usr/include/obs

LDD_LIBS += -lobs

LDD_FLAG += -shared