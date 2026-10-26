#!/usr/bin/env python3
"""Reading PDF for direct sale: front cover (cropped from the wrap) + interior.
  python3 reading_pdf.py <out-dir>"""
import json, sys
from pathlib import Path
import pypdf, yaml
from pypdf.generic import RectangleObject
HERE = Path(__file__).resolve().parent; OUT = Path(sys.argv[1]).resolve()
C = yaml.safe_load((HERE / "edition.yaml").read_text())
info = json.loads((OUT / "interior.json").read_text())
b, tw, th = C["bleed_in"], C["trim_w_in"], C["trim_h_in"]
s = info["pages"] * (0.002252 if C.get("paper", "white") == "white" else 0.0025)
# the colour interior when it was built (build.sh does), else the print one
src = OUT / "interior-color.pdf"
w = pypdf.PdfWriter(clone_from=str(src if src.exists() else OUT / "interior.pdf"))
w.insert_page(pypdf.PdfReader(str(OUT / "cover" / "cover-wrap.pdf")).pages[0], 0)
x0, H = (b + tw + s) * 72, (th + 2 * b) * 72
w.pages[0].mediabox = RectangleObject([x0, b * 72, x0 + tw * 72, H - b * 72]); w.pages[0].cropbox = w.pages[0].mediabox
w._root_object.pop("/PageLabels", None)
body = info["body_start_index"] + 1
w.set_page_label(0, 0, prefix="Cover"); w.set_page_label(1, body - 1, style="/r")
w.set_page_label(body, len(w.pages) - 1, style="/D", start=1)
w.compress_identical_objects(remove_duplicates=True, remove_unreferenced=True)
w.write(str(OUT / "the-nanosecond-budget.pdf"))
print(OUT / "the-nanosecond-budget.pdf")
