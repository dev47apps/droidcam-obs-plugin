# Linux packaging (.deb / .tar.xz)

Repackages the official DroidCam OBS Linux zip into a Debian package and a
generic .tar.xz, without compiling anything.

## How it works

The workflow triggers when a GitHub Release is published (or manually, with a
version input), downloads the official Linux zip from the release assets,
unpacks it, and runs `packaging/build-linux.sh`:

- `droidcam-obs_<version>_amd64.deb` installs the plugin into the system OBS
  plugin directories (binary in `/usr/lib/x86_64-linux-gnu/obs-plugins`, data
  in `/usr/share/obs/obs-plugins/droidcam-obs`).
- `droidcam-obs-<version>-linux-x86_64.tar.xz` mirrors the zip layout plus
  `install.sh`, `README.TXT` and `LICENSE` when present.

Because the artifacts are built from the official release binary, there are no
compiler dependencies, no library name pinning, and no risk of the built
plugin silently diverging from what the maintainers tested. If upstream ever
renames or drops the Linux zip, the workflow fails loudly at the download
step.

## Building locally

No build dependencies are required. Download the official zip, unpack it, and
run the script from the repo root:

    curl -LO https://github.com/dev47apps/droidcam-obs-plugin/releases/download/2.5.1/droidcam_obs_2.5.1_linux_x86_64.zip
    unzip -o droidcam_obs_2.5.1_linux_x86_64.zip -d unpacked
    packaging/build-linux.sh unpacked 2.5.1

Artifacts are written to `dist/`.

## Notes

- The plugin is a library that OBS loads from its plugins directory, not a
  standalone application, so AppImage is not a natural format for it.
- Fedora ships its own RPM (`obs-studio-plugin-droidcam`) and Arch has AUR
  packages, so this packaging targets Debian/Ubuntu plus a manual fallback.
