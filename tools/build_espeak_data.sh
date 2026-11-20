#!/bin/bash
# Rebuilds assets/espeak-ng-data.zip: the eSpeak-NG voice data built into the plug-in
# (English only: phoneme tables, intonations, the English dictionary, the en / en-US /
# en-GB-x-rp voices and the voice variants). Needs git, cmake and a C compiler.
#
#   ./tools/build_espeak_data.sh
#
# Keep ESPEAK_TAG the same as in CMakeLists.txt, and change kDataVersion in
# Source/SpeechEngine.cpp whenever the data changes.
set -euo pipefail
cd "$(dirname "$0")/.."
ESPEAK_TAG=1.52.0
WORK=$(mktemp -d)
git clone --depth 1 --branch "$ESPEAK_TAG" https://github.com/espeak-ng/espeak-ng "$WORK/src"
cmake -S "$WORK/src" -B "$WORK/build" -DCMAKE_BUILD_TYPE=Release -DUSE_MBROLA=OFF -DUSE_LIBSONIC=OFF \
      -DUSE_LIBPCAUDIO=OFF -DUSE_ASYNC=OFF -DUSE_SPEECHPLAYER=OFF
cmake --build "$WORK/build" --target data --parallel 4

D="$WORK/build/espeak-ng-data"
OUT="$WORK/out/espeak-ng-data"
mkdir -p "$OUT/lang/gmw" "$OUT/voices"
cp "$D/phondata" "$D/phonindex" "$D/phontab" "$D/intonations" "$D/en_dict" "$OUT/"
cp "$D/lang/gmw/en" "$D/lang/gmw/en-US" "$D/lang/gmw/en-GB-x-rp" "$OUT/lang/gmw/"
cp -R "$D/voices/!v" "$OUT/voices/"
rm -f assets/espeak-ng-data.zip
(cd "$WORK/out" && zip -qr -9 -X "$OLDPWD/assets/espeak-ng-data.zip" espeak-ng-data)
rm -rf "$WORK"
echo "done: assets/espeak-ng-data.zip"
