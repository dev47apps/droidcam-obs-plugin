#!/bin/bash
set -e
OK=0
DIRS="
$HOME/.config/obs-studio
$HOME/snap/obs-studio/current/.config/obs-studio
$HOME/.var/app/com.obsproject.Studio/config/obs-studio
"

for dir in $DIRS; do
	if [ -d $dir ]; then
		set -x
		mkdir -p "${dir}/plugins/"
		cp -R droidcam-obs "${dir}/plugins/"
		set +x
		OK=1
	fi
done

if [ $OK == 0 ]; then
	echo "OBS Studio config folder not found!"
	echo "Checked:${DIRS}"
	exit 1
fi
echo "Done"
