#!/bin/bash
# Packs a Release build into dist/TSS-1-<version>-Linux-x86_64.tar.gz (with an install script)
set -euo pipefail
cd "$(dirname "$0")/.."

VERSION=$(sed -nE 's/^project\(TSS1 VERSION ([0-9.]+)\).*/\1/p' CMakeLists.txt)
ART="build/TSS1_artefacts/Release"
NAME="TSS-1-$VERSION-Linux-$(uname -m)"
STAGE="dist/$NAME"

rm -rf "$STAGE" && mkdir -p "$STAGE"
cp -R "$ART/VST3/TSS-1.vst3"   "$STAGE/"
cp -R "$ART/LV2/TSS-1.lv2"     "$STAGE/"
cp    "$ART/Standalone/TSS-1"  "$STAGE/"

cat > "$STAGE/install.sh" <<'EOS'
#!/bin/bash
# Installs TSS-1 for the current user (no root needed).
set -e
cd "$(dirname "$0")"
mkdir -p ~/.vst3 ~/.lv2 ~/.local/bin
rm -rf ~/.vst3/"TSS-1.vst3" ~/.lv2/"TSS-1.lv2"
cp -R "TSS-1.vst3" ~/.vst3/
cp -R "TSS-1.lv2"  ~/.lv2/
cp "TSS-1" ~/.local/bin/TSS-1 && chmod +x ~/.local/bin/TSS-1
echo "Installed: ~/.vst3/TSS-1.vst3, ~/.lv2/TSS-1.lv2, ~/.local/bin/TSS-1"
echo "Rescan plug-ins in your DAW (Bitwig, Reaper, Ardour...)."
EOS

cat > "$STAGE/uninstall.sh" <<'EOS'
#!/bin/bash
rm -rf ~/.vst3/"TSS-1.vst3" ~/.lv2/"TSS-1.lv2" ~/.local/bin/TSS-1
echo "TSS-1 removed."
EOS
chmod +x "$STAGE/install.sh" "$STAGE/uninstall.sh"

cat > "$STAGE/README.txt" <<EOS
TSS-1 $VERSION - Transistor Speech System - Transistor Labs 2026

Install:    ./install.sh      (VST3 -> ~/.vst3, LV2 -> ~/.lv2, app -> ~/.local/bin/TSS-1)
Uninstall:  ./uninstall.sh
Needs: ALSA or JACK, X11, FreeType (standard on desktop distributions).
EOS

tar -C dist -czf "dist/$NAME.tar.gz" "$NAME"
rm -rf "$STAGE"
echo "==> Done: dist/$NAME.tar.gz"
