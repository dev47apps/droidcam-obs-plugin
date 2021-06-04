#!/bin/bash
set -e

if [ -d ~/snap/obs-studio/current/.config ]; then
	set -x
	mkdir -p           ~/snap/obs-studio/current/.config/obs-studio/plugins/
	cp -R droidcam-obs ~/snap/obs-studio/current/.config/obs-studio/plugins/
	exit
fi

if [ -d ~/.config/obs-studio ]; then
	set -x
	mkdir -p ~/.config/obs-studio/plugins/
	cp -R droidcam-obs ~/.config/obs-studio/plugins/
	exit
fi

if [ -d ~/.var/app/com.obsproject.Studio/config/obs-studio ]; then
	set -x
	mkdir -p ~/.var/app/com.obsproject.Studio/config/obs-studio/plugins/
	cp -R droidcam-obs ~/.var/app/com.obsproject.Studio/config/obs-studio/plugins/
	exit
fi

echo "OBS Studio config folder not found."
echo "Checked ~/.config/obs-studio, ~/snap/obs-studio/current/.config/obs-studio, and ~/.var/app/com.obsproject.Studio/config/obs-studio"
exit 1
