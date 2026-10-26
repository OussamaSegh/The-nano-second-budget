#!/bin/sh
# Build the whole edition: print interior (B&W), colour interior for the reading PDF, cover (spine from the page count), EPUB, reading PDF.
#   build-book/build.sh <manuscript-dir> <out-dir>
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
MS=$1; OUT=$2
python3 "$HERE/build_print.py" "$MS" "$OUT"
python3 "$HERE/build_print.py" "$MS" "$OUT" --color
PAGES=$(python3 -c "import json,sys;print(json.load(open(sys.argv[1]))['pages'])" "$OUT/interior.json")
python3 "$HERE/cover.py" "$OUT/cover" "$PAGES"
python3 "$HERE/build_epub.py" "$MS" "$OUT" "$OUT/cover/cover-ebook.jpg"
python3 "$HERE/reading_pdf.py" "$OUT"
