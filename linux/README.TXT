DroidCam OBS Linux Plugin

i. Installation

To install the plugin, copy the 'droidcam-obs' directory from this package to your OBS plugins folder.
The install.sh script will try to do this automatically.

ii. USB Support

For Android USB support, make sure you have adb installed:

Debian/Ubuntu Linux users can do: sudo apt-get install adb
ARCH Linux users can do: sudo pacman -S android-tools
Fedora/SUSE based Linux users can do: sudo yum install android-tools


For iOS USB support, make sure usbmuxd is installed and running:

Debian/Ubuntu Linux users can do: sudo apt-get install usbmuxd
ARCH Linux users can do: sudo pacman -S usbmuxd
Fedora/SUSE based Linux users can do: sudo yum install usbmuxd
