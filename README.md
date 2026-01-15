# DroidCam OBS Plugin for Linux

[![License: GPL v2](https://img.shields.io/badge/License-GPL%20v2-blue.svg)](LICENSE)
[![Platform: Linux](https://img.shields.io/badge/Platform-Linux-orange.svg)]()

A high-quality plugin for OBS Studio that allows you to use your Android or iOS device as a camera source. This repository focuses on providing reliable installation paths for **Arch Linux** and **Arch-based distributions** (like CachyOS, Manjaro, or EndeavourOS).

## 🚀 Features
- **High Resolution**: Supports up to 1080p video.
- **Low Latency**: Optimized for minimal delay via WiFi or USB.
- **Hardware Acceleration**: Works seamlessly with NVIDIA (NVENC) and VAAPI.
- **Multi-Device**: Connect multiple phones as separate sources.

---

## 🛠 Installation (Arch Linux)

For Arch-based systems, it is **highly recommended** to use the AUR (Arch User Repository). This ensures the plugin is compiled against your specific system libraries, preventing the common `libavcodec` version mismatch errors.

### Method 1: AUR (Recommended)
This is the easiest method and handles all dependencies automatically.

```bash
# Using paru (recommended for CachyOS)
paru -S droidcam-obs-plugin

# OR using yay
yay -S droidcam-obs-plugin
