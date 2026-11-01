#!/bin/sh
# Run inside the MSYS2 UCRT64 shell after building. Creates lms-<version>-windows.zip containing
# lms.exe, lms_seed.exe and the DLLs they need, so the school computer needs no MSYS2.
set -e
VERSION=${1:-1.1.0}
OUT=dist/lms-$VERSION
rm -rf dist && mkdir -p "$OUT"
cp build/lms.exe build/lms_seed.exe "$OUT"/
for exe in build/lms.exe build/lms_seed.exe; do
  ldd "$exe" | awk '/ucrt64/ {print $3}' | sort -u | while read dll; do cp -n "$dll" "$OUT"/; done
done
cp README.md docs/RELEASE_NOTES.md "$OUT"/
cat > "$OUT"/start-lms.bat <<'BAT'
@echo off
cd /d "%~dp0"
lms.exe --db "%~dp0data\lms.db" --storage "%~dp0data\submissions" --log "%~dp0data\lms.log"
pause
BAT
mkdir -p "$OUT"/data
(cd dist && zip -qr "lms-$VERSION-windows.zip" "lms-$VERSION")
echo "Created dist/lms-$VERSION-windows.zip"
