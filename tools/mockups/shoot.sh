#!/bin/bash
# Render droidHPSDR mock-ups at the S26 Ultra's real size (3120x1440, 3.75 px/dp).
# Usage: shoot.sh OUTDIR [ID ...]   (IDs from layouts.js; default: all)
set -e
here=$(cd "$(dirname "$0")" && pwd)
out=${1:?outdir}; shift
mkdir -p "$out"
ids=("$@")
[ ${#ids[@]} -eq 0 ] && ids=($(grep -hoE '^  [LPWVXYZKTD][0-9]+:' "$here"/layouts*.js | tr -d ' :'))
for id in "${ids[@]}"; do
  if [[ $id == P* ]]; then size=384,832; else size=832,384; fi
  chromium --headless=new --disable-gpu --hide-scrollbars --force-device-scale-factor=3.75 \
    --window-size=$size --virtual-time-budget=4000 --allow-file-access-from-files \
    --screenshot="$out/$id.png" "file://$here/mock.html?l=$id" >/dev/null 2>&1
  echo "$out/$id.png"
done
