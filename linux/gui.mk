
ifdef DROIDCAM_OVERRIDE
MOC      ?= /usr/lib/qt6/moc
UIC      ?= /usr/lib/qt6/uic
INCLUDES += -Isrc/ui
SRC      += src/ui/AddDevice.cpp
SRC      += src/ui/moc_AddDevice.cpp
SRC      += src/ui/uic_AddDevice.h

src/ui/uic_AddDevice.h: src/ui/AddDevice.ui
	$(UIC) -o $@ $^

src/ui/moc_AddDevice.cpp: src/ui/AddDevice.h
	$(MOC) -o $@ $^

.PHONY: install
install: /opt/droidcam-obs-client/bin/64bit/droidcam
	sudo cp $(LIB_DLL) /opt/droidcam-obs-client/obs-plugins/64bit/
	sudo cp -R data/*  /opt/droidcam-obs-client/data/obs-plugins/droidcam-obs/

endif

$(eval $(call ADD_Lib,Qt6Core))
$(eval $(call ADD_Lib,Qt6Gui))
$(eval $(call ADD_Lib,Qt6Svg))
$(eval $(call ADD_Lib,Qt6SvgWidgets))
$(eval $(call ADD_Lib,Qt6Widgets))

