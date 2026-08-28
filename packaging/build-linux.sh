#!/usr/bin/env bash
#
# Repackages the official DroidCam OBS Linux zip into a Debian package (.deb)
# and a generic .tar.xz. No compilation is involved.
#
# Usage: packaging/build-linux.sh <extracted-zip-dir> <version>
#   <extracted-zip-dir>  directory that contains droidcam-obs/ with
#                        bin/64bit/droidcam-obs.so and data/
#   <version>            release version, e.g. 2.5.1
#
# Outputs are written to ./dist/ (relative to the current directory):
#   droidcam-obs_<version>_amd64.deb
#   droidcam-obs-<version>-linux-x86_64.tar.xz

set -euo pipefail

ZIP_DIR="${1:?usage: build-linux.sh <extracted-zip-dir> <version>}"
VERSION="${2:?usage: build-linux.sh <extracted-zip-dir> <version>}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PKG_NAME="droidcam-obs"
ARCH="amd64"
PLUGIN_DIR="$ZIP_DIR/droidcam-obs"
PLUGIN_SO="$PLUGIN_DIR/bin/64bit/droidcam-obs.so"
DATA_DIR="$PLUGIN_DIR/data"
LIB_DIR="/usr/lib/x86_64-linux-gnu/obs-plugins"
DATA_INSTALL_DIR="/usr/share/obs/obs-plugins/$PKG_NAME"
DOC_DIR="/usr/share/doc/$PKG_NAME"

if [[ ! -f "$PLUGIN_SO" ]]; then
  echo "error: $PLUGIN_SO not found (did you unpack the official Linux zip?)" >&2
  exit 1
fi
if [[ ! -d "$DATA_DIR" ]]; then
  echo "error: $DATA_DIR not found" >&2
  exit 1
fi

OUT_DIR="$PWD/dist"
STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT

DEB_ROOT="$STAGE/deb"
TXZ_ROOT="$STAGE/txz/droidcam-obs-$VERSION"

mkdir -p   "$DEB_ROOT/DEBIAN"   "$DEB_ROOT$LIB_DIR"   "$DEB_ROOT$DATA_INSTALL_DIR/locale"   "$DEB_ROOT$DOC_DIR"   "$TXZ_ROOT/droidcam-obs/bin/64bit"   "$TXZ_ROOT/droidcam-obs/data"

# Binary plugin
cp "$PLUGIN_SO" "$DEB_ROOT$LIB_DIR/droidcam-obs.so"
cp "$PLUGIN_SO" "$TXZ_ROOT/droidcam-obs/bin/64bit/droidcam-obs.so"

# Plugin data (icons, locale)
cp -R "$DATA_DIR/." "$DEB_ROOT$DATA_INSTALL_DIR/"
cp -R "$DATA_DIR/." "$TXZ_ROOT/droidcam-obs/data/"

# Keep the generic tarball self-contained, mirroring the official zip
for f in install.sh README.TXT LICENSE; do
  if [[ -f "$ZIP_DIR/$f" ]]; then
    cp "$ZIP_DIR/$f" "$TXZ_ROOT/"
  fi
done

# Debian metadata
{
  echo "droidcam-obs ($VERSION) unstable; urgency=medium"
  echo ""
  echo "  * $VERSION release package."
  echo "    - Installs droidcam-obs.so into the system OBS plugins directory"
  echo "    - Installs plugin data (icons, locale) into /usr/share/obs/obs-plugins"
  echo ""
  echo " -- DroidCam OBS Plugin contributors <support@dev47apps.com>  $(date -R)"
} > "$DEB_ROOT$DOC_DIR/changelog"
gzip -9n "$DEB_ROOT$DOC_DIR/changelog"
cp "$SCRIPT_DIR/debian/copyright" "$DEB_ROOT$DOC_DIR/copyright"
sed "s/@VERSION@/$VERSION/" "$SCRIPT_DIR/debian/control.in" > "$DEB_ROOT/DEBIAN/control"

# Normalize package permissions (dpkg-deb otherwise preserves the build umask)
find "$DEB_ROOT" -type d -exec chmod 755 {} +
find "$DEB_ROOT" -type f ! -name 'droidcam-obs.so' -exec chmod 644 {} +
chmod 755 "$DEB_ROOT$LIB_DIR/droidcam-obs.so"

mkdir -p "$OUT_DIR"
dpkg-deb --build --root-owner-group   "$DEB_ROOT" "$OUT_DIR/${PKG_NAME}_${VERSION}_${ARCH}.deb" >/dev/null
tar -C "$STAGE/txz" -cJf   "$OUT_DIR/${PKG_NAME}-${VERSION}-linux-x86_64.tar.xz" "droidcam-obs-$VERSION"

echo "Built:"
ls -lh "$OUT_DIR"
