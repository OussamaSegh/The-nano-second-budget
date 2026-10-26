#!/usr/bin/env python3
"""Cover artwork for the ebook (1600x2560 JPEG) and the print wrap (PDF with bleed).

  python3 cover.py <out-dir> <interior-page-count>

The art is a PET detector ring drawn as cache-line-sized blocks: a run of
occupied slots (the ring buffer between tail and head) and lines of response
crossing the bore (coincidences found inside the nanosecond window).
"""
import html, math, random, subprocess, sys
from pathlib import Path
import yaml
import preprocess

HERE = Path(__file__).resolve().parent
OUT = Path(sys.argv[1]).resolve(); OUT.mkdir(parents=True, exist_ok=True)
PAGES = int(sys.argv[2])
C = yaml.safe_load((HERE / "edition.yaml").read_text())
E = html.escape

INK, INK2 = "#070c1a", "#13213f"
AMBER, AMBER2, CYAN, SLATE, PALE = "#ffb347", "#ff7a2f", "#7fd3ff", "#3a5a8c", "#c9d6ee"

def ring_svg(cx, cy, R, seed=7, n_blocks=40, scale=1.0, flip=False):
    """Detector ring centred at (cx, cy) with outer radius R, in user units."""
    rnd = random.Random(seed)
    depth = R * 0.16            # radial depth of a block
    rin = R - depth
    parts = []
    occupied = range(28, 38)    # tail .. head-1
    hits = (4, 27)              # the coincidence pair
    cols, rows = 4, 3           # crystals per block (tangential x radial)
    for b in range(n_blocks):
        a0 = 2 * math.pi * b / n_blocks - math.pi / 2
        span = 2 * math.pi / n_blocks
        for ci in range(cols):
            for ri in range(rows):
                ta = a0 + span * (0.06 + 0.88 * (ci + 0.5) / cols)
                rr = rin + depth * (ri + 0.5) / rows
                size_t = span * rr * 0.88 / cols * 0.8
                size_r = depth / rows * 0.78
                if flip:
                    ta = math.pi - ta
                x, y = cx + rr * math.cos(ta), cy + rr * math.sin(ta)
                rot = math.degrees(ta) + 90
                if b in hits:
                    fill, op = "#ffffff", 1.0
                elif b in occupied:
                    t = (b - occupied.start) / (len(occupied) - 1)
                    fill = AMBER if t < 0.55 else AMBER2
                    op = 0.55 + 0.4 * rnd.random()
                else:
                    fill, op = SLATE, 0.22 + 0.25 * rnd.random()
                parts.append(f'<rect x="{x - size_t/2:.2f}" y="{y - size_r/2:.2f}" width="{size_t:.2f}" height="{size_r:.2f}" '
                             f'rx="{size_r*0.12:.2f}" fill="{fill}" fill-opacity="{op:.2f}" transform="rotate({rot:.2f} {x:.2f} {y:.2f})"/>')
    def pt(b, r):
        a = 2 * math.pi * (b + 0.5) / n_blocks - math.pi / 2
        if flip:
            a = math.pi - a
        return cx + r * math.cos(a), cy + r * math.sin(a)
    lors = []
    for i, (p, q) in enumerate([(2, 23), (14, 36), (27, 5), (19, 38), (33, 16), (9, 26)]):
        (x1, y1), (x2, y2) = pt(p, rin * 0.98), pt(q, rin * 0.98)
        lors.append(f'<line x1="{x1:.1f}" y1="{y1:.1f}" x2="{x2:.1f}" y2="{y2:.1f}" stroke="{CYAN}" stroke-opacity="{0.10 + 0.05*i:.2f}" stroke-width="{R*0.004:.2f}"/>')
    (x1, y1), (x2, y2) = pt(hits[0], rin * 0.98), pt(hits[1], rin * 0.98)
    main = (f'<line x1="{x1:.1f}" y1="{y1:.1f}" x2="{x2:.1f}" y2="{y2:.1f}" stroke="{AMBER}" stroke-width="{R*0.03:.2f}" stroke-opacity="0.25" filter="url(#glow)"/>'
            f'<line x1="{x1:.1f}" y1="{y1:.1f}" x2="{x2:.1f}" y2="{y2:.1f}" stroke="#fff3df" stroke-width="{R*0.0065:.2f}"/>')
    # annihilation point and the window label
    mx, my = x1 + (x2 - x1) * 0.42, y1 + (y2 - y1) * 0.42
    dot = (f'<circle cx="{mx:.1f}" cy="{my:.1f}" r="{R*0.05:.1f}" fill="{AMBER}" fill-opacity="0.35" filter="url(#glow)"/>'
           f'<circle cx="{mx:.1f}" cy="{my:.1f}" r="{R*0.012:.1f}" fill="#fff"/>')
    fs = R * 0.068 * scale
    label = (f'<text x="{mx + (R*0.05 if not flip else -R*0.05):.1f}" text-anchor="{'end' if flip else 'start'}" y="{my + R*0.13:.1f}" font-family="JetBrains Mono" font-size="{fs:.1f}" '
             f'fill="{PALE}" fill-opacity="0.85">|Δt| ≤ 4 ns</text>')
    # head / tail markers outside the ring
    def tick(b, text, col):
        (ax, ay), (bx, by) = pt(b - 0.5, R * 1.03), pt(b - 0.5, R * 1.13)
        (lx, ly) = pt(b - 0.5, R * 1.2)
        return (f'<line x1="{ax:.1f}" y1="{ay:.1f}" x2="{bx:.1f}" y2="{by:.1f}" stroke="{col}" stroke-width="{R*0.008:.1f}"/>'
                f'<text x="{lx:.1f}" y="{ly + fs*0.35:.1f}" text-anchor="middle" font-family="JetBrains Mono" font-size="{fs:.1f}" fill="{col}">{text}</text>')
    ticks = tick(occupied.start, "tail", PALE) + tick(occupied.stop, "head", AMBER)
    halo = f'<circle cx="{cx}" cy="{cy}" r="{R*0.9:.1f}" fill="url(#halo)"/>'
    return halo + "".join(lors) + main + "".join(parts) + dot + label

def defs():
    return f"""<defs>
<linearGradient id="bg" x1="0" y1="0" x2="0.35" y2="1"><stop offset="0" stop-color="{INK}"/><stop offset="1" stop-color="{INK2}"/></linearGradient>
<radialGradient id="halo"><stop offset="0" stop-color="#1d3a6e" stop-opacity="0.55"/><stop offset="1" stop-color="#1d3a6e" stop-opacity="0"/></radialGradient>
<filter id="glow" x="-50%" y="-50%" width="200%" height="200%"><feGaussianBlur stdDeviation="14"/></filter>
<pattern id="grid" width="22" height="10" patternUnits="userSpaceOnUse"><rect x="1" y="1" width="18" height="6" fill="#8fb4ff" fill-opacity="0.035"/></pattern>
</defs>"""

def front_svg(W, H, ebook, x0=0, bleed=0):
    """Front cover in a W x H box (user units = px at 1600 wide for ebook, points for print)."""
    u = W / 100.0                       # 1u = 1% of the front width
    m = 9 * u                           # side margin
    if ebook:
        ring = ring_svg(x0 + W * 0.64, H * 0.73, W * 0.42, scale=1.0)
        ty = H * 0.075
    else:
        ring = ring_svg(x0 + W * 0.65, bleed + H * 0.81, W * 0.32, scale=1.0, flip=True)
        ty = bleed + H * 0.085
    words = C["title"].split()
    # "The" small, then the remaining words one per line, sized so the longest fills the measure
    from PIL import ImageFont
    f = ImageFont.truetype("/usr/share/fonts/opentype/inter/Inter-Black.otf", 1000)
    lines = words[1:] if words[0].lower() == "the" else words
    longest = max(f.getlength(w) * 0.97 for w in lines) / 1000
    tsize = min(16 * u if ebook else 11.5 * u, (W - 2 * m) / longest)
    t = [ring,
         f'<text x="{x0+m:.1f}" y="{ty:.1f}" font-family="JetBrains Mono" font-size="{2.5*u:.1f}" letter-spacing="{0.25*u:.1f}" fill="{CYAN}" fill-opacity="0.9">C++20 · LOCK-FREE · REAL-TIME</text>']
    y = ty + 5.5 * u
    if lines is not words:
        y += tsize * 0.42
        t.append(f'<text x="{x0+m - 0.2*u:.1f}" y="{y:.1f}" font-family="InterT" font-weight="800" font-size="{tsize*0.42:.1f}" fill="#ffffff">{E(words[0])}</text>')
    for i, w in enumerate(lines):
        y += tsize * (0.86 if i == 0 else 0.9)
        col = AMBER if i == len(lines) - 1 else "#ffffff"
        t.append(f'<text x="{x0+m - 0.5*u:.1f}" y="{y:.1f}" font-family="InterT" font-weight="900" font-size="{tsize:.1f}" letter-spacing="{-0.03*tsize:.1f}" fill="{col}">{E(w)}</text>')
    t.append(f'<rect x="{x0+m:.1f}" y="{y + 3.2*u:.1f}" width="{11*u:.1f}" height="{0.75*u:.1f}" fill="{AMBER}"/>')
    sy = y + 9.5 * u
    sub = C["subtitle"]
    # split subtitle into two balanced lines
    sw = sub.split(); best = min(range(1, len(sw)), key=lambda k: abs(len(" ".join(sw[:k])) - len(" ".join(sw[k:]))))
    for i, line in enumerate([" ".join(sw[:best]), " ".join(sw[best:])]):
        t.append(f'<text x="{x0+m:.1f}" y="{sy + i*5.6*u:.1f}" font-family="InterT" font-weight="500" font-size="{4.5*u:.1f}" fill="{PALE}">{E(line)}</text>')
    t.append(f'<text x="{x0+m:.1f}" y="{sy + 13.3*u:.1f}" font-family="&quot;Source Serif 4&quot;" font-style="italic" font-size="{3.6*u:.1f}" fill="{PALE}" fill-opacity="0.75">{E(C["tagline"])}</text>')
    ay = (H - 6.5 * u) if ebook else (bleed + H - 7.5 * u)
    t.append(f'<text x="{x0+m:.1f}" y="{ay:.1f}" font-family="InterT" font-weight="700" font-size="{5.0*u:.1f}" letter-spacing="{0.02*u:.1f}" fill="#ffffff">{E(C["author"])}</text>')
    return "".join(t)

FONTFACE = """<style>@font-face { font-family: "Source Serif 4"; font-style: italic; font-weight: 400; src: url(fonts/SourceSerif4-It.ttf); }
@font-face { font-family: "Source Serif 4"; font-style: normal; font-weight: 400; src: url(fonts/SourceSerif4-Regular.ttf); }
@font-face { font-family: "Source Serif 4"; font-style: normal; font-weight: 600; src: url(fonts/SourceSerif4-Semibold.ttf); }
@font-face { font-family: "InterT"; font-weight: 400; font-style: normal; src: url(fonts/inter-latin-400-normal.woff2); } @font-face { font-family: "InterT"; font-weight: 500; font-style: normal; src: url(fonts/inter-latin-500-normal.woff2); } @font-face { font-family: "InterT"; font-weight: 600; font-style: normal; src: url(fonts/inter-latin-600-normal.woff2); } @font-face { font-family: "InterT"; font-weight: 700; font-style: normal; src: url(fonts/inter-latin-700-normal.woff2); } @font-face { font-family: "InterT"; font-weight: 800; font-style: normal; src: url(fonts/inter-latin-800-normal.woff2); } @font-face { font-family: "InterT"; font-weight: 900; font-style: normal; src: url(fonts/inter-latin-900-normal.woff2); }
html, body { margin: 0; padding: 0; background: #070c1a; }</style>"""

PRELOAD = """<div style="position:absolute;opacity:0;font-family:'Source Serif 4';font-style:italic">x<b style="font-family:InterT;font-weight:900">x</b></div>"""

def ebook_html():
    W, H = 1600, 2560
    svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">{defs()}'
           f'<rect width="{W}" height="{H}" fill="url(#bg)"/><rect width="{W}" height="{H}" fill="url(#grid)"/>'
           f'{front_svg(W, H, True)}</svg>')
    return f"<!DOCTYPE html><html><head><meta charset='utf-8'>{FONTFACE}</head><body>{PRELOAD}{svg}</body></html>", W, H

def spine_width_in():
    per_page = 0.002252 if C.get("paper", "white") == "white" else 0.0025   # KDP black-and-white interior
    return PAGES * per_page

BACK_BLURB = [
    "A trading gateway has a few microseconds from packet to order. A bedside monitor must raise its alarm within a bounded time, every time, for years. A PET scanner streams millions of detector events a second and may not lose one without saying so. A mutex and a standard container will not get you there. Knowing what the cache, the memory model and the scheduler really do will.",
    "<b>The Nanosecond Budget</b> builds every structure from scratch in C++20, explains it line by line, tests it under ThreadSanitizer and measures it with a harness that tells the truth about tail latency.",
]
BACK_BULLETS = [
    "read latency as a distribution, and measure p99.9 without fooling yourself",
    "use the C++ memory model, atomics and release/acquire with confidence",
    "build spinlocks, seqlocks, SPSC rings, MPMC queues and a Disruptor-style broadcast ring",
    "take malloc off the hot path, and reclaim memory safely with hazard pointers and epochs",
    "design concurrent hash maps, skip lists and order books",
    "tune Linux for low jitter, and verify concurrent code with sanitizers and model checking",
    "apply it all to a real-time PET data-acquisition pipeline with coincidence sorting",
]
BACK_CLOSE = "A compact Foundations part refreshes the modern C++ and threading the rest of the book relies on. Every chapter ends with exercises, all solved in full, and the book comes with <code>llds</code>, a complete header-only companion library with tests and benchmarks."

def wrap_html():
    b = C["bleed_in"]; tw, th = C["trim_w_in"], C["trim_h_in"]; s = spine_width_in()
    Win, Hin = 2 * b + 2 * tw + s, th + 2 * b
    pt = 72.0
    W, H = Win * pt, Hin * pt
    fx = (b + tw + s) * pt                 # front panel left edge (trim)
    front = front_svg(tw * pt + b * pt, th * pt, False, x0=fx, bleed=b * pt)
    sx, sw = (b + tw) * pt, s * pt
    # spine: title and author running top to bottom
    words = C["title"]
    spine = (f'<g transform="translate({sx + sw/2:.2f},{b*pt + 0.45*pt:.2f}) rotate(90)">'
             f'<text x="0" y="{sw*0.17:.2f}" font-family="InterT" font-weight="900" font-size="{min(sw*0.5, 22):.2f}" fill="#ffffff">{E(C["title"].split()[0])} {E(" ".join(C["title"].split()[1:-1]))} <tspan fill="{AMBER}">{E(C["title"].split()[-1])}</tspan></text>'
             f'<text x="{th*pt - 0.9*pt:.2f}" y="{sw*0.14:.2f}" text-anchor="end" font-family="InterT" font-weight="600" font-size="{min(sw*0.36, 13):.2f}" fill="{PALE}">{E(C["author"])}</text>'
             f'</g>') if s >= 0.25 else ""
    # back panel: HTML block positioned over the SVG
    bx = b * pt + 0.6 * pt; by = b * pt + 0.7 * pt; bw = tw * pt - 1.2 * pt
    bullets = "".join(f"<li>{x}</li>" for x in BACK_BULLETS)
    bio = f'<p class="bio"><b>{E(C["author"])}</b> {E(C["author_bio"])}</p>' if C.get("author_bio") else ""
    back = f"""<div class="back" style="left:{bx:.1f}pt;top:{by:.1f}pt;width:{bw:.1f}pt">
<div class="kick">C++20 · CONCURRENCY · LOW LATENCY</div>
<h2>Fast is easy. Fast <em>and</em> correct, every time, is engineering.</h2>
<p>{BACK_BLURB[0]}</p><p>{BACK_BLURB[1]}</p>
<p class="lead">You will learn to</p><ul>{bullets}</ul>
<p>{BACK_CLOSE}</p>{bio}
<p class="url">Code and errata: {E(preprocess.code_url(C))}</p>
</div>
<div class="barcode" style="left:{(b + tw - 0.25 - 2.0)*pt:.1f}pt;top:{(b + th - 0.25 - 1.2)*pt:.1f}pt;width:{2.0*pt:.1f}pt;height:{1.2*pt:.1f}pt"></div>
<div class="cat" style="left:{bx:.1f}pt;top:{(b + th - 0.62)*pt:.1f}pt">PROGRAMMING / C++</div>"""
    ring_small = ""
    svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W:.2f}pt" height="{H:.2f}pt" viewBox="0 0 {W:.2f} {H:.2f}" style="position:absolute;left:0;top:0">{defs().replace("stdDeviation=\"14\"", "stdDeviation=\"6\"")}'
           f'<rect width="{W:.2f}" height="{H:.2f}" fill="url(#bg)"/><rect width="{W:.2f}" height="{H:.2f}" fill="url(#grid)"/>'
           f'<rect x="{sx:.2f}" y="0" width="{sw:.2f}" height="{H:.2f}" fill="#050913"/>'
           f'{front}{spine}{ring_small}</svg>')
    css = f"""@page {{ size: {Win:.4f}in {Hin:.4f}in; margin: 0; }}
body {{ width: {W:.2f}pt; height: {H:.2f}pt; position: relative; overflow: hidden; }}
.back {{ position: absolute; color: {PALE}; font-family: "Source Serif 4", serif; font-size: 10.4pt; line-height: 1.45; }}
.back .kick {{ font-family: "JetBrains Mono"; font-size: 7pt; letter-spacing: 0.18em; color: {CYAN}; margin-bottom: 9pt; }}
.back h2 {{ font-family: InterT, sans-serif; font-weight: 800; font-size: 19pt; line-height: 1.15; color: #fff; margin: 0 0 12pt; letter-spacing: -0.01em; }}
.back h2 em {{ color: {AMBER}; font-style: normal; }}
.back p {{ margin: 0 0 8pt; }} .back b {{ color: #fff; }}
.back .lead {{ font-family: InterT, sans-serif; font-weight: 700; color: #fff; font-size: 9pt; margin: 11pt 0 4pt; text-transform: uppercase; letter-spacing: 0.12em; }}
.back ul {{ margin: 0 0 10pt; padding-left: 12pt; }} .back li {{ margin: 0 0 2.5pt; }} .back li::marker {{ color: {AMBER}; }}
.back code {{ font-family: "JetBrains Mono"; font-size: 0.88em; color: #fff; }}
.back .url {{ font-family: "JetBrains Mono"; font-size: 8.6pt; color: #fff; margin-top: 10pt; }}
.back .bio {{ font-size: 8.6pt; margin-top: 10pt; }}
.barcode {{ position: absolute; background: #fff; }}
.cat {{ position: absolute; font-family: InterT, sans-serif; font-weight: 600; font-size: 7.5pt; letter-spacing: 0.16em; color: {PALE}; }}"""
    doc = f"<!DOCTYPE html><html><head><meta charset='utf-8'>{FONTFACE}<style>{css}</style></head><body>{PRELOAD}{svg}{back}</body></html>"
    return doc, Win, Hin, s

def main():
    import shutil
    shutil.copytree(HERE / "fonts", OUT / "fonts", dirs_exist_ok=True)
    doc, W, H = ebook_html()
    (OUT / "cover-ebook.html").write_text(doc)
    doc, Win, Hin, s = wrap_html()
    (OUT / "cover-wrap.html").write_text(doc)
    js = f"""const {{ chromium }} = require('playwright');
(async () => {{
  const b = await chromium.launch();
  let p = await b.newPage({{ viewport: {{ width: {W}, height: {H} }} }});
  await p.goto('file://{OUT}/cover-ebook.html'); await p.evaluate(() => document.fonts.ready);
  await p.screenshot({{ path: '{OUT}/cover-ebook.png', clip: {{ x: 0, y: 0, width: {W}, height: {H} }} }});
  p = await b.newPage({{ viewport: {{ width: {round(Win*96)}, height: {round(Hin*96)} }}, deviceScaleFactor: 3.125 }});
  await p.goto('file://{OUT}/cover-wrap.html'); await p.evaluate(() => document.fonts.ready);
  await p.pdf({{ path: '{OUT}/cover-wrap.pdf', preferCSSPageSize: true, printBackground: true }});
  await p.screenshot({{ path: '{OUT}/cover-wrap.png', clip: {{ x: 0, y: 0, width: {round(Win*96)}, height: {round(Hin*96)} }} }});
  await b.close();
}})();"""
    (OUT / "cover-render.js").write_text(js)
    subprocess.run(["node", str(OUT / "cover-render.js")], check=True)
    from PIL import Image
    Image.open(OUT / "cover-ebook.png").convert("RGB").save(OUT / "cover-ebook.jpg", quality=93, optimize=True)
    print(f"spine {s:.3f} in, wrap {Win:.3f} x {Hin:.3f} in")

if __name__ == "__main__":
    main()
