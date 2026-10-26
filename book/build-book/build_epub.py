#!/usr/bin/env python3
"""Build the store EPUB.  python3 build_epub.py <manuscript-dir> <out-dir> <cover.jpg>"""
import re, shutil, subprocess, sys, uuid, zipfile
from pathlib import Path
import yaml
import preprocess

HERE = Path(__file__).resolve().parent
MS, OUT, COVER = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve(), Path(sys.argv[3]).resolve()
C = yaml.safe_load((HERE / "edition.yaml").read_text())
tmp = OUT / "epub-src"; tmp.mkdir(parents=True, exist_ok=True)

ident = f"urn:isbn:{C['isbn_ebook'].replace('-', '')}" if C.get("isbn_ebook") else \
        f"urn:uuid:{uuid.uuid5(uuid.NAMESPACE_URL, 'nanosecond-budget/' + C['version'])}"
meta = {
    "title": [{"type": "main", "text": C["title"]}, {"type": "subtitle", "text": C["subtitle"]}],
    "creator": [{"role": "author", "text": C["author"]}],
    "identifier": [{"text": ident}],
    "publisher": C["publisher"], "lang": C["language"], "date": C["pub_date"],
    "rights": f"© {C['year']} {C['author']}. All rights reserved. Companion code: {C['code_licence']}.",
    "description": open(HERE / "description.txt").read().strip(),
    "subject": [k for k in C["keywords"]] + [b.split(" ", 1)[1] for b in C["bisac"]],
    "cover-image": str(COVER),
    "css": [str(HERE / "epub.css")],
    "toc-title": "Contents",
}
(tmp / "metadata.yaml").write_text("---\n" + yaml.safe_dump(meta, allow_unicode=True, sort_keys=False) + "---\n")
isbn = C.get("isbn_ebook") or "to be assigned"
site = f"**Code and errata:** {preprocess.code_url(C)}\n\n"
(tmp / "000-copyright.md").write_text(f"""# Copyright {{.unnumbered epub:type=copyright-page}}

::: copyright
**{C['title']}: {C['subtitle']}**

Copyright © {C['year']} {C['author']}. All rights reserved.

No part of this book may be reproduced, stored in a retrieval system or transmitted in any form or by any means, electronic, mechanical, photocopying, recording or otherwise, without the prior written permission of the author, except for brief quotations in reviews and other uses permitted by copyright law.

**Companion code.** The source code of the `llds` companion project is licensed under the {C['code_licence']}; see the `LICENSE` file in the project.

{site}**Disclaimer.** The information in this book is provided "as is", without warranty of any kind. The author shall have no liability for any loss or damage caused, or alleged to be caused, directly or indirectly by the instructions or code it contains. The medical-device and PET examples are engineering illustrations: nothing in this book is fit for clinical use, constitutes regulatory advice, or describes the products of any manufacturer. Performance figures were measured on the machines described in the text; your results will differ.

**Trademarks.** Intel and Xeon are trademarks of Intel Corporation. Arm is a trademark of Arm Limited. Linux is a registered trademark of Linus Torvalds. Other product and company names are trademarks of their respective owners and are used in an editorial fashion, with no intention of infringement.

{C['edition']}, {C['pub_date']} (version {C['version']}). ISBN (ebook): {isbn}. Published by {C['publisher']}.
:::
""")
ms = preprocess.prepare(MS, tmp / "ms", C)
files = [str(tmp / "000-copyright.md")] + sorted(str(p) for p in ms.glob("*.md"))
if C.get("author_bio"):
    (tmp / "zzz-about.md").write_text(f"# About the Author {{.unnumbered epub:type=backmatter}}\n\n{C['author_bio']}\n")
    files.append(str(tmp / "zzz-about.md"))
A11Y = """    <meta property="schema:accessMode">textual</meta>
    <meta property="schema:accessMode">visual</meta>
    <meta property="schema:accessModeSufficient">textual</meta>
    <meta property="schema:accessibilityFeature">structuralNavigation</meta>
    <meta property="schema:accessibilityFeature">tableOfContents</meta>
    <meta property="schema:accessibilityFeature">readingOrder</meta>
    <meta property="schema:accessibilityFeature">alternativeText</meta>
    <meta property="schema:accessibilityFeature">displayTransformability</meta>
    <meta property="schema:accessibilityHazard">none</meta>
    <meta property="schema:accessibilitySummary">Reflowable text with a navigable table of contents and headings for every part, chapter and section. Every figure has a text description, and code listings are real text. There are no flashing, sound or motion hazards.</meta>
    <meta property="dcterms:conformsTo">EPUB Accessibility 1.1 - WCAG 2.1 Level AA</meta>
"""

def postprocess(epub):
    """Add accessibility metadata and give every content document a real title."""
    d = OUT / "epub-x"
    shutil.rmtree(d, ignore_errors=True)
    with zipfile.ZipFile(epub) as z:
        z.extractall(d)
    opf = next(d.rglob("content.opf"))
    o = opf.read_text()
    o = o.replace("</metadata>", A11Y.format(publisher=C["publisher"]) + "  </metadata>", 1)
    if "xml:lang" not in o.split(">", 2)[1]:
        o = re.sub(r"<package ", f'<package xml:lang="{C["language"]}" ', o, count=1)
    opf.write_text(o)
    for x in d.rglob("*.xhtml"):
        t = x.read_text()
        t = t.replace('<nav epub:type="toc" id="toc">', '<nav epub:type="toc" role="doc-toc" id="toc">')
        # A front-matter chapter is an h1 with h3 sections (the print layout
        # needs that); in a standalone document, keep heading levels contiguous.
        if re.search(r"<h1[ >]", t) and "<h3" in t and not re.search(r"<h2[ >]", t):
            t = re.sub(r"<(/?)h3([ >])", r"<\1h2\2", t)
            t = re.sub(r"<(/?)h4([ >])", r"<\1h3\2", t)
        m = re.search(r"<h[12][^>]*>(.*?)</h[12]>", t, re.S)
        if m:
            title = re.sub(r"<[^>]+>", "", m.group(1))
            title = re.sub(r"\s+", " ", title).strip()
            t = re.sub(r"<title>.*?</title>", f"<title>{title}</title>", t, count=1, flags=re.S)
            x.write_text(t)
    epub.unlink()
    with zipfile.ZipFile(epub, "w") as z:
        z.write(d / "mimetype", "mimetype", compress_type=zipfile.ZIP_STORED)
        for f in sorted(d.rglob("*")):
            if f.is_file() and f.name != "mimetype":
                z.write(f, f.relative_to(d).as_posix(), compress_type=zipfile.ZIP_DEFLATED)

out = OUT / "the-nanosecond-budget.epub"
subprocess.run(["pandoc", str(tmp / "metadata.yaml"), *files, "-o", str(out), "--toc", "--toc-depth=2",
                "--split-level=2", "--lua-filter", str(HERE / "chapnum.lua"), "--epub-title-page=true",
                "--resource-path", str(HERE), "--highlight-style=monochrome"], check=True)
postprocess(out)
print(out)
