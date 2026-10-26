#!/usr/bin/env python3
"""Generate the book's figures and charts as SVG.

  python3 figures.py <out-dir> [--gray]

Figures are drawn in colour (the cover's navy and amber, plus a few accents).
With --gray every colour is mapped to a gray chosen to keep neighbouring
series and fills apart, for the black-and-white print interior; charts also
differ by marker shape and dash pattern, so they read without colour.

Every figure is drawn in a 640-unit-wide coordinate system and scaled to
the text width. Fonts are named by family only; the print build inlines the
SVG so the page fonts apply, and EPUB readers fall back to their own sans.
"""
import math, re, sys
from pathlib import Path

OUT = Path(sys.argv[1]); OUT.mkdir(parents=True, exist_ok=True)
GRAY = "--gray" in sys.argv[2:]
SANS = "InterT, Inter, Helvetica, Arial, sans-serif"
MONO = "'JetBrains Mono', 'DejaVu Sans Mono', monospace"
SERIF_FAM = "'Source Serif 4', serif"

# ---------------------------------------------------------------- palette
INK, MID, LIGHT, GRID = "#16202c", "#566173", "#b9c1cc", "#e3e7ed"
PALE = "#eef2f7"
NAVY, BLUE, SKY, SKY2 = "#1b3a5c", "#2f6db5", "#d8e7f7", "#a9c8ea"
AMBER, AMBER_D, CREAM = "#e8a33d", "#a0600a", "#fcefd6"
TEAL, TEAL_P = "#1d8a7c", "#d3eee9"
RED, RED_P = "#c0392b", "#f8dbd6"
GREEN, GREEN_P = "#2c8a4c", "#d9f0e0"
PURPLE = "#6f52a3"
# chart series, in order of use; each has its own gray
S = ["#2f6db6", "#e8a33e", "#1d8a7d", "#d9634f", "#7b5ea8", "#5d9c3c"]
GRAYS = {
    INK: "#111111", MID: "#5e5e5e", LIGHT: "#bbbbbb", GRID: "#e4e4e4", PALE: "#f0f0f0",
    NAVY: "#2a2a2a", BLUE: "#3d3d3d", SKY: "#eeeeee", SKY2: "#d0d0d0",
    AMBER: "#9a9a9a", AMBER_D: "#4a4a4a", CREAM: "#f4f4f4",
    TEAL: "#666666", TEAL_P: "#e2e2e2", RED: "#333333", RED_P: "#dddddd",
    GREEN: "#444444", GREEN_P: "#e8e8e8", PURPLE: "#555555",
    S[0]: "#333333", S[1]: "#b4b4b4", S[2]: "#737373", S[3]: "#d6d6d6", S[4]: "#909090", S[5]: "#555555",
}

def to_gray(svg):
    def g(m):
        h = m.group(0).lower()
        if len(h) == 4:
            h = "#" + "".join(c * 2 for c in h[1:])
        if h in GRAYS:
            return GRAYS[h]
        r, gg, b = (int(h[i:i + 2], 16) for i in (1, 3, 5))
        y = round(0.299 * r + 0.587 * gg + 0.114 * b)
        return f"#{y:02x}{y:02x}{y:02x}"
    return re.sub(r"#[0-9a-fA-F]{6}\b|#[0-9a-fA-F]{3}\b", g, svg)

def esc(s):
    s = s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
    # The Inter subset lacks these glyphs; take them from the serif face.
    for ch in "→₂":
        s = s.replace(ch, f'<tspan font-family="{SERIF_FAM}">{ch}</tspan>')
    return s

class Fig:
    def __init__(self, w, h, title):
        self.w, self.h, self.title, self.parts, self.markers = w, h, title, [], set()
    def add(self, s): self.parts.append(s); return self
    def _arrow(self, stroke):
        self.markers.add(stroke)
        return f' marker-end="url(#arr{stroke[1:]})"'
    def rect(self, x, y, w, h, fill="#fff", stroke=INK, sw=1.2, rx=3, dash=None):
        d = f' stroke-dasharray="{dash}"' if dash else ""
        return self.add(f'<rect x="{x:.1f}" y="{y:.1f}" width="{w:.1f}" height="{h:.1f}" rx="{rx}" fill="{fill}" stroke="{stroke}" stroke-width="{sw}"{d}/>')
    def text(self, x, y, s, size=12, anchor="middle", weight=400, fill=INK, mono=False, italic=False):
        fam = MONO if mono else SANS
        st = ' font-style="italic"' if italic else ""
        return self.add(f'<text x="{x:.1f}" y="{y:.1f}" font-family="{fam}" font-size="{size}" font-weight="{weight}" text-anchor="{anchor}" fill="{fill}"{st}>{esc(s)}</text>')
    def line(self, x1, y1, x2, y2, stroke=INK, sw=1.2, arrow=False, dash=None):
        a = self._arrow(stroke) if arrow else ""
        d = f' stroke-dasharray="{dash}"' if dash else ""
        return self.add(f'<line x1="{x1:.1f}" y1="{y1:.1f}" x2="{x2:.1f}" y2="{y2:.1f}" stroke="{stroke}" stroke-width="{sw}"{a}{d}/>')
    def path(self, d, stroke=INK, sw=1.2, fill="none", arrow=False, dash=None):
        a = self._arrow(stroke) if arrow else ""
        ds = f' stroke-dasharray="{dash}"' if dash else ""
        return self.add(f'<path d="{d}" stroke="{stroke}" stroke-width="{sw}" fill="{fill}"{a}{ds}/>')
    def circle(self, x, y, r, fill="#fff", stroke=INK, sw=1.2):
        return self.add(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{r}" fill="{fill}" stroke="{stroke}" stroke-width="{sw}"/>')
    def box(self, x, y, w, h, label, sub=None, fill="#fff", size=12, mono=False, weight=600, stroke=INK, color=INK):
        self.rect(x, y, w, h, fill=fill, stroke=stroke)
        if sub:
            self.text(x + w / 2, y + h / 2 - 2, label, size, weight=weight, mono=mono, fill=color)
            self.text(x + w / 2, y + h / 2 + 13, sub, size - 2, fill=MID)
        else:
            self.text(x + w / 2, y + h / 2 + size * 0.36, label, size, weight=weight, mono=mono, fill=color)
        return self
    # -------------------------------------------------- chart helpers
    def marker(self, kind, x, y, color, r=3.6):
        if kind == "circle":
            return self.circle(x, y, r, fill=color, stroke="#fff", sw=0.8)
        if kind == "ring":
            return self.circle(x, y, r, fill="#fff", stroke=color, sw=1.6)
        if kind == "square":
            return self.rect(x - r, y - r, 2 * r, 2 * r, fill=color, stroke="#fff", sw=0.8, rx=0)
        if kind == "box":
            return self.rect(x - r, y - r, 2 * r, 2 * r, fill="#fff", stroke=color, sw=1.6, rx=0)
        if kind == "tri":
            return self.path(f"M{x:.1f},{y - r - 1:.1f} L{x + r + 0.5:.1f},{y + r:.1f} L{x - r - 0.5:.1f},{y + r:.1f} Z", stroke="#fff", sw=0.8, fill=color)
        if kind == "diamond":
            r += 0.8
            return self.path(f"M{x:.1f},{y - r:.1f} L{x + r:.1f},{y:.1f} L{x:.1f},{y + r:.1f} L{x - r:.1f},{y:.1f} Z", stroke="#fff", sw=0.8, fill=color)
    def series(self, pts, color, kind="circle", dash=None, sw=2):
        d = "M" + " L".join(f"{x:.1f},{y:.1f}" for x, y in pts)
        self.path(d, stroke=color, sw=sw, dash=dash)
        for x, y in pts:
            self.marker(kind, x, y, color)
        return self
    def frame(self, x0, y0, x1, y1):
        return self.line(x0, y1, x1, y1, stroke=INK, sw=1).line(x0, y0, x0, y1, stroke=INK, sw=1)
    def ygrid(self, x0, x1, ticks, ymap, fmt, size=9):
        for v in ticks:
            y = ymap(v)
            self.line(x0, y, x1, y, stroke=GRID, sw=0.8).text(x0 - 5, y + 3, fmt(v), size, anchor="end", fill=MID)
        return self
    def xgrid(self, y0, y1, ticks, xmap, fmt, size=9):
        for v in ticks:
            x = xmap(v)
            self.line(x, y0, x, y1, stroke=GRID, sw=0.8).text(x, y1 + 13, fmt(v), size, fill=MID)
        return self
    def legend(self, x, y, items, dx=None, dy=16):
        """items: (label, color, kind, dash) - kind 'bar' for a swatch."""
        for i, it in enumerate(items):
            label, color, kind = it[0], it[1], it[2]
            dash = it[3] if len(it) > 3 else None
            xx, yy = (x + i * dx, y) if dx else (x, y + i * dy)
            if kind == "bar":
                self.rect(xx, yy - 8, 14, 10, fill=color, stroke=INK, sw=0.6, rx=1)
            else:
                self.line(xx - 2, yy - 3, xx + 18, yy - 3, stroke=color, sw=2, dash=dash)
                self.marker(kind, xx + 8, yy - 3, color, r=3.2)
            self.text(xx + 22, yy, label, 10, anchor="start")
        return self
    def save(self, name):
        defs = "".join(
            f'<marker id="arr{c[1:]}" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse">'
            f'<path d="M0,0 L10,5 L0,10 z" fill="{c}"/></marker>' for c in sorted(self.markers))
        svg = (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {self.w} {self.h}" role="img" aria-label="{self.title}">'
               f'<title>{self.title}</title><defs>{defs}</defs>'
               + "".join(self.parts) + "</svg>")
        (OUT / f"{name}.svg").write_text(to_gray(svg) if GRAY else svg)

def logmap(lo, hi, a, b):
    la, lb = math.log10(lo), math.log10(hi)
    return lambda v: a + (math.log10(v) - la) / (lb - la) * (b - a)

def linmap(lo, hi, a, b):
    return lambda v: a + (v - lo) / (hi - lo) * (b - a)

def fmt_ns(v):
    for unit, k in (("s", 1e9), ("ms", 1e6), ("µs", 1e3)):
        if v >= k:
            q = v / k
            return f"{q:g} {unit}"
    return f"{v:g} ns"

# ================================================================ diagrams

# ------------------------------------------------------------- Ch1: latency tail
def latency_tail():
    f = Fig(640, 300, "Histogram of request latency on a logarithmic axis, with a sharp floor, a peak near the median and a long tail to the right, with p50, p99, p99.9 and the maximum marked")
    x0, x1, yb = 60, 610, 240
    def X(ns): return x0 + (math.log10(ns) - 2) * (x1 - x0) / 5
    f.rect(X(10 ** 3.3), 30, x1 - X(10 ** 3.3), yb - 30, fill=RED_P, stroke="none", rx=0)
    f.text(x1 - 6, 44, "the tail", 11, anchor="end", weight=700, fill=RED)
    f.line(x0, yb, x1, yb).line(x0, yb, x0, 30)
    decades = ["100 ns", "1 µs", "10 µs", "100 µs", "1 ms", "10 ms"]
    for i, d in enumerate(decades):
        x = x0 + i * (x1 - x0) / 5
        f.line(x, yb, x, yb + 5).text(x, yb + 20, d, 11, fill=MID)
    f.text((x0 + x1) / 2, yb + 42, "latency (log scale)", 11, fill=MID)
    f.text(x0 - 10, 22, "requests (log scale)", 11, anchor="start", fill=MID)
    def H(ns):  # illustrative shape: steep floor, body, slow tail
        lg = math.log10(ns)
        if lg < 2.35: return 0
        body = math.exp(-((lg - 2.55) / 0.12) ** 2) * 190
        tail = 120 * math.exp(-(lg - 2.4) * 1.55)
        return max(body, tail if lg > 2.6 else 0)
    ns = 10 ** 2.3
    while ns < 10 ** 6.6:
        nxt = ns * 10 ** 0.05
        h = H(math.sqrt(ns * nxt))
        if h > 0.6:
            tail = ns > 10 ** 3.3
            f.rect(X(ns), yb - h, X(nxt) - X(ns) - 0.6, h, fill=S[3] if tail else BLUE, stroke="#fff", sw=0.4, rx=0)
        ns = nxt
    for lab, v, y, c in [("p50", 360, 40, NAVY), ("p99", 1500, 70, NAVY), ("p99.9", 9000, 100, RED), ("max", 2.5e6, 130, RED)]:
        x = X(v)
        f.line(x, yb, x, y + 6, dash="3 3", stroke=c).text(x, y, lab, 11, weight=700, fill=c)
    mean = X(2400)
    f.line(mean, yb, mean, 165, stroke=AMBER_D, dash="1 3", sw=1.6).text(mean + 6, 176, "mean", 11, anchor="start", fill=AMBER_D, italic=True, weight=600)
    f.text(X(10 ** 2.33) - 4, yb - 6, "floor", 10, anchor="end", fill=MID, italic=True)
    f.save("latency-tail")

# ------------------------------------------------------------- Ch2: MESI
MESI = {"M": (RED, "#fff"), "E": (AMBER, INK), "S": (SKY2, INK), "I": ("#fff", MID)}
def mesi():
    f = Fig(640, 300, "Four steps of the MESI protocol for one cache line shared by core 0 and core 1, showing each core's copy as Exclusive, Shared, Invalid or Modified")
    steps = [("core 0 reads x", "E", "I", "only copy, clean"),
             ("core 1 reads x", "S", "S", "both copies clean"),
             ("core 1 writes x", "I", "M", "invalidate sent to core 0"),
             ("core 0 reads x", "S", "S", "core 1 supplies the line")]
    f.text(84, 64, "core 0", 12, weight=700, fill=NAVY).text(84, 134, "core 1", 12, weight=700, fill=NAVY)
    for i, (what, a, b, note) in enumerate(steps):
        x = 150 + i * 122
        f.text(x + 50, 28, f"{i+1}. {what}", 11, weight=600)
        for row, st in ((0, a), (1, b)):
            y = 40 + row * 70
            fill, tc = MESI[st]
            f.rect(x, y, 100, 40, fill=fill, dash="4 3" if st == "I" else None, stroke=MID if st == "I" else INK)
            f.text(x + 50, y + 26, st, 18, weight=800, fill=tc)
        f.text(x + 50, 176, note, 10, fill=MID)
        if i < 3:
            f.line(x + 104, 95, x + 118, 95, arrow=True, stroke=MID)
    x = 150 + 2 * 122
    f.path(f"M{x+50},110 C{x+20},100 {x+20},90 {x+50},82", arrow=True, dash="3 2", stroke=RED, sw=1.5)
    ly = 220
    for j, (st, name) in enumerate([("M", "Modified: dirty, only copy"), ("E", "Exclusive: clean, only copy"),
                                     ("S", "Shared: clean, maybe other copies"), ("I", "Invalid: must fetch")]):
        x = 40 + (j % 2) * 300; y = ly + (j // 2) * 32
        fill, tc = MESI[st]
        f.rect(x, y, 22, 22, fill=fill, dash="4 3" if st == "I" else None, stroke=MID if st == "I" else INK)
        f.text(x + 11, y + 16, st, 12, weight=800, fill=tc)
        f.text(x + 32, y + 16, name, 11, anchor="start")
    f.save("mesi")

# ------------------------------------------------------------- Ch3: store buffer
def store_buffer():
    f = Fig(640, 290, "Store-buffering: each core puts its store in a private store buffer and then loads the other variable from memory, so both loads can return 0")
    for i, (core, st, ld, res) in enumerate([("core 1", "x = 1", "r1 = y", "reads 0"), ("core 2", "y = 1", "r2 = x", "reads 0")]):
        x = 40 + i * 340
        f.box(x, 20, 220, 44, core, f"{st};  then  {ld};", fill=SKY, stroke=NAVY, color=NAVY)
        f.rect(x + 20, 96, 180, 40, fill=CREAM, stroke=AMBER_D)
        f.text(x + 30, 112, "store buffer", 10, anchor="start", fill=AMBER_D, weight=600)
        f.rect(x + 120, 104, 70, 24, fill=AMBER, rx=2, stroke=AMBER_D).text(x + 155, 121, st, 11, mono=True, weight=700)
        f.line(x + 110, 64, x + 140, 102, arrow=True, stroke=AMBER_D)
        f.text(x + 100, 160, f"{ld}: {res}", 11, weight=700, fill=RED)
    f.rect(140, 200, 360, 56, fill="#fff", sw=1.6, stroke=NAVY)
    f.text(320, 222, "memory (caches)", 11, fill=MID)
    f.text(320, 244, "x = 0    y = 0", 13, mono=True, weight=700, fill=NAVY)
    f.path("M 46 64 L 46 228 L 138 228", arrow=True, dash="4 3", stroke=RED, sw=1.4)
    f.path("M 594 64 L 594 228 L 502 228", arrow=True, dash="4 3", stroke=RED, sw=1.4)
    f.text(52, 190, "load y", 10, anchor="start", fill=RED).text(588, 190, "load x", 10, anchor="end", fill=RED)
    f.text(320, 282, "The stores reach memory only later, so r1 = r2 = 0 is possible.", 11, italic=True, fill=MID)
    f.save("store-buffer")

# ------------------------------------------------------------- Ch6: ABA
def aba():
    f = Fig(640, 250, "The ABA problem in three steps: thread 1 reads top A and next B, thread 2 pops A and B and pushes A back, and thread 1's compare-and-swap then succeeds and installs the freed node B as top")
    cols = [("1. Thread 1 reads", "top = A, next = B", ["A", "B", "C"], None, NAVY),
            ("2. Thread 2: pop A, pop B, push A", "B is freed", ["A", "C"], "B", TEAL),
            ("3. Thread 1: CAS(top, A → B)", "succeeds: top = freed B", ["B?", "C"], None, RED)]
    for i, (title, sub, nodes, freed, c) in enumerate(cols):
        x = 20 + i * 212
        f.text(x + 95, 22, title, 11, weight=700, fill=c)
        f.text(x + 95, 40, sub, 10, fill=RED if i == 2 else MID, weight=600 if i == 2 else 400)
        f.text(x + 4, 76, "top", 11, anchor="start", weight=600)
        f.line(x + 28, 72, x + 46, 72, arrow=True)
        y = 56
        for j, n in enumerate(nodes):
            nx = x + 50 + j * 52
            dangling = n.endswith("?")
            f.rect(nx, y, 36, 32, fill=RED_P if dangling else SKY2, dash="4 3" if dangling else None, stroke=RED if dangling else NAVY)
            f.text(nx + 18, y + 21, n.rstrip("?"), 13, weight=700, fill=RED if dangling else NAVY)
            if j < len(nodes) - 1:
                f.line(nx + 36, y + 16, nx + 50, y + 16, arrow=True)
        if freed:
            f.rect(x + 50, 120, 36, 32, fill=RED_P, dash="4 3", stroke=RED).text(x + 68, 141, freed, 13, weight=700, fill=RED)
            f.text(x + 96, 141, "freed", 10, anchor="start", fill=RED, italic=True)
    f.text(320, 200, "The value of top is A again, so a CAS that compares only the pointer cannot tell", 11, fill=MID)
    f.text(320, 216, "that the stack changed underneath it. A version tag packed with the pointer can.", 11, fill=MID)
    f.save("aba")

# ------------------------------------------------------------- Ch7: seqlock timeline
def seqlock():
    f = Fig(640, 220, "A sequence lock over time: the counter is odd while the writer writes; reader 1 sees the same even value before and after and succeeds, reader 2 overlaps a write and retries")
    x0, x1 = 70, 610
    f.text(20, 52, "writer", 11, anchor="start", weight=700, fill=NAVY)
    segs = [(0, 120, "0", False), (120, 190, "1", True), (190, 330, "2", False), (330, 400, "3", True), (400, 540, "4", False)]
    for a, b, v, odd in segs:
        f.rect(x0 + a, 36, b - a, 26, fill=AMBER if odd else SKY, rx=0, stroke=NAVY)
        f.text(x0 + (a + b) / 2, 54, f"seq = {v}", 11, weight=700, fill=INK, mono=True)
    f.text(x0 + 155, 80, "writing", 10, fill=AMBER_D, italic=True, weight=600).text(x0 + 365, 80, "writing", 10, fill=AMBER_D, italic=True, weight=600)
    def reader(y, a, b, label, c):
        f.text(20, y + 5, label, 11, anchor="start", weight=700, fill=c)
        f.line(x0 + a, y, x0 + b, y, sw=3, stroke=c)
        f.line(x0 + a, y - 7, x0 + a, y + 7, stroke=c, sw=1.6).line(x0 + b, y - 7, x0 + b, y + 7, stroke=c, sw=1.6)
    reader(120, 210, 300, "reader 1", GREEN)
    f.text(x0 + 210, 140, "reads 2", 10, fill=MID, mono=True).text(x0 + 300, 140, "reads 2", 10, fill=MID, mono=True)
    f.text(x0 + 255, 108, "same even value: success", 10, weight=700, fill=GREEN)
    reader(178, 290, 370, "reader 2", RED)
    f.text(x0 + 290, 198, "reads 2", 10, fill=MID, mono=True).text(x0 + 370, 198, "reads 3", 10, fill=MID, mono=True)
    f.text(x0 + 330, 166, "changed: retry", 10, weight=700, fill=RED)
    f.line(x0, 92, x1, 92, stroke=LIGHT, arrow=True)
    f.text(x1, 108, "time", 10, anchor="end", fill=MID)
    f.save("seqlock")

# ------------------------------------------------------------- Ch8: SPSC ring states
def spsc():
    f = Fig(640, 360, "Four states of a ring buffer of capacity 8 with free-running read and write indices: empty, partly full, full, and wrapped past the end of the array")
    rows = [("empty", 0, 0), ("3 elements", 0, 3), ("full", 0, 8), ("after wrapping", 13, 18)]
    for r, (name, rd, wr) in enumerate(rows):
        y = 22 + r * 82
        f.text(20, y + 30, name, 11, anchor="start", weight=700, fill=NAVY)
        f.text(20, y + 46, f"read = {rd}, write = {wr}", 10, anchor="start", fill=MID, mono=True)
        for i in range(8):
            x = 230 + i * 46
            full = any((k & 7) == i for k in range(rd, wr))
            f.rect(x, y + 16, 42, 30, fill=SKY2 if full else "#fff", rx=1, stroke=NAVY if full else LIGHT)
            f.text(x + 5, y + 27, str(i), 8, anchor="start", fill=NAVY if full else MID, mono=True)
        x = 230 + (rd & 7) * 46 + 21
        f.line(x, y - 2, x, y + 14, arrow=True, stroke=TEAL, sw=1.6).text(x + 5, y + 4, "read & 7", 9, anchor="start", mono=True, fill=TEAL, weight=700)
        if wr != rd:
            x = 230 + (wr & 7) * 46 + 21
            f.line(x, y + 64, x, y + 48, arrow=True, stroke=AMBER_D, sw=1.6).text(x + 5, y + 66, "write & 7", 9, anchor="start", mono=True, fill=AMBER_D, weight=700)
    f.text(320, 352, "size = write − read; empty when equal, full when size == capacity; slot = index & (capacity − 1)", 10, fill=MID)
    f.save("spsc-ring")

# ------------------------------------------------------------- Ch9: Michael-Scott lagging tail
def msqueue():
    f = Fig(640, 190, "A Michael-Scott queue with a dummy node: head points to the dummy, a producer has linked node C but tail still points to B, so the next operation first swings tail forward")
    names = [("dummy", PALE, MID), ("A", SKY2, NAVY), ("B", SKY2, NAVY), ("C", AMBER, INK)]
    for i, (n, fill, c) in enumerate(names):
        x = 60 + i * 140
        f.box(x, 70, 90, 40, n, fill=fill, size=13, stroke=NAVY, color=c)
        if i < 3:
            f.line(x + 90, 90, x + 136, 90, arrow=True)
            f.text(x + 113, 84, "next", 9, fill=MID)
    f.text(105, 30, "head", 12, weight=700, fill=TEAL).line(105, 36, 105, 66, arrow=True, stroke=TEAL, sw=1.6)
    f.text(385, 30, "tail (lagging)", 12, weight=700, fill=AMBER_D).line(385, 36, 385, 66, arrow=True, stroke=AMBER_D, sw=1.6)
    f.path("M 400 36 C 470 20 520 30 525 64", dash="4 3", arrow=True, stroke=AMBER_D, sw=1.6)
    f.text(510, 22, "CAS tail → C", 10, fill=AMBER_D, italic=True, weight=600)
    f.text(320, 150, "Enqueue is two CAS operations: link C after the last node, then swing tail.", 11, fill=MID)
    f.text(320, 168, "Any thread that finds tail.next non-null helps by swinging tail before it continues.", 11, fill=MID)
    f.save("msqueue")

# ------------------------------------------------------------- Ch10: Disruptor graph
def disruptor():
    f = Fig(640, 250, "A Disruptor pipeline: the producer claims sequences in the ring, parse and journal read each entry in parallel, process waits for parse, and the producer may reuse a slot only when both process and journal have passed it")
    f.box(20, 90, 100, 46, "producer", "cursor", fill=SKY, stroke=NAVY, color=NAVY)
    for i in range(8):
        f.rect(150 + i * 26, 98, 24, 30, fill=SKY2 if i < 5 else "#fff", rx=1, stroke=NAVY if i < 5 else LIGHT)
    f.text(254, 90, "ring (sequences)", 10, fill=MID)
    f.line(120, 113, 148, 113, arrow=True)
    f.box(410, 30, 90, 40, "parse", fill=TEAL_P, stroke=TEAL)
    f.box(530, 30, 90, 40, "process", fill=TEAL_P, stroke=TEAL)
    f.box(410, 156, 90, 40, "journal", fill=CREAM, stroke=AMBER_D)
    f.line(360, 108, 408, 56, arrow=True, stroke=TEAL).line(360, 118, 408, 170, arrow=True, stroke=AMBER_D)
    f.line(500, 50, 528, 50, arrow=True, stroke=TEAL)
    f.text(514, 88, "waits for parse", 9, fill=TEAL, italic=True)
    f.path("M 575 72 L 575 216 L 70 216 L 70 140", dash="4 3", arrow=True, stroke=RED, sw=1.4)
    f.path("M 455 196 L 455 216", dash="4 3", stroke=RED, sw=1.4)
    f.text(320, 240, "gating: producer waits for min(process, journal) before reusing a slot", 10, fill=RED, italic=True)
    f.save("disruptor")

# ------------------------------------------------------------- Ch12: hazard pointer ordering
def hazard():
    f = Fig(640, 250, "Hazard pointer protocol as two timelines: the reader announces p in its slot and then re-reads src, both seq_cst; the writer unlinks the node, executes a seq_cst fence and then scans the slots. In the single total order, either the scan sees the hazard or the re-read sees the unlink")
    f.text(20, 56, "reader", 12, anchor="start", weight=700, fill=NAVY).text(20, 156, "writer", 12, anchor="start", weight=700, fill=AMBER_D)
    f.line(90, 50, 620, 50, stroke=LIGHT).line(90, 150, 620, 150, stroke=LIGHT)
    for x, lab, sub in [(100, "p = src", "relaxed"), (250, "slot = p", "seq_cst store"), (400, "src == p ?", "seq_cst load")]:
        f.box(x, 30, 120, 40, lab, sub, fill=SKY, stroke=NAVY, color=NAVY)
    for x, lab, sub in [(160, "src = next", "unlink"), (310, "fence", "seq_cst"), (460, "read all slots", "scan")]:
        f.box(x, 130, 120, 40, lab, sub, fill=CREAM, stroke=AMBER_D, color=AMBER_D)
    f.line(370, 72, 370, 128, dash="3 3", stroke=PURPLE, sw=1.6)
    f.text(380, 104, "single total order of seq_cst operations", 10, anchor="start", fill=PURPLE, italic=True, weight=600)
    f.text(320, 206, "If the scan comes later, it sees the hazard and keeps the node.", 11)
    f.text(320, 224, "If the re-read comes later, it sees the unlink and the reader retries.", 11)
    f.text(320, 242, "With an acquire re-read, ARM and POWER may let the load pass the store, and neither happens.", 10, fill=RED, italic=True)
    f.save("hazard-order")

# ------------------------------------------------------------- Ch12: epochs
def epochs():
    f = Fig(640, 230, "Epoch-based reclamation: a node retired in epoch e is freed only after the global epoch reaches e plus 2, when every reader region that could have seen it has ended")
    x0 = 60
    for i, lab in enumerate(["epoch e", "e + 1", "e + 2"]):
        x = x0 + i * 180
        f.rect(x, 24, 176, 26, fill=[SKY, SKY2, NAVY][i], rx=0, stroke=NAVY)
        f.text(x + 88, 42, lab, 12, weight=700, fill="#fff" if i == 2 else NAVY)
    regions = [(40, 120, 74, "reader A"), (100, 260, 104, "reader B"), (200, 330, 134, "reader C"), (390, 470, 164, "reader D")]
    for a, b, y, lab in regions:
        f.rect(x0 + a, y - 9, b - a, 18, fill=TEAL_P, rx=9, stroke=TEAL)
        f.text(x0 + a + 8, y + 4, lab, 10, anchor="start", fill=INK)
    rx = x0 + 70
    f.line(rx, 52, rx, 196, dash="3 3", stroke=AMBER_D, sw=1.6).text(rx, 210, "retire(n)", 11, weight=700, mono=True, fill=AMBER_D)
    fx = x0 + 362
    f.line(fx, 52, fx, 196, dash="3 3", stroke=GREEN, sw=1.6).text(fx, 210, "free(n)", 11, weight=700, mono=True, fill=GREEN)
    f.text(x0 + 216, 226, "the epoch advances only when every active reader has announced the current one", 10, fill=MID, italic=True)
    f.save("epochs")

# ------------------------------------------------------------- Ch14: skip list
def skiplist():
    f = Fig(640, 210, "A skip list with four levels: every node is on level 0, and each higher level holds about half the nodes of the level below, so a search drops down a level at a time")
    keys = [5, 10, 15, 20, 25, 35, 40, 50, 60, 70, 80]
    heights = [1, 2, 1, 3, 1, 2, 1, 4, 1, 2, 1]
    xs = [108 + i * 45 for i in range(len(keys))]
    lvl_fill = [SKY2, SKY, TEAL_P, CREAM]
    lvl_stroke = [NAVY, BLUE, TEAL, AMBER_D]
    for lvl in range(4):
        y = 170 - lvl * 40
        f.text(20, y + 5, f"level {lvl}", 10, anchor="start", fill=lvl_stroke[lvl], weight=600)
        f.rect(70, y - 12, 26, 24, fill=PALE, rx=1, stroke=MID)
        prev = 96
        for x, h in zip(xs, heights):
            if h > lvl:
                f.line(prev, y, x - 2, y, arrow=True, stroke=lvl_stroke[lvl])
                prev = x + 28
        f.line(prev, y, 598, y, arrow=True, stroke=lvl_stroke[lvl])
        f.text(604, y + 4, "null", 9, anchor="start", mono=True, fill=MID)
    for x, k, h in zip(xs, keys, heights):
        for lvl in range(h):
            y = 170 - lvl * 40
            f.rect(x, y - 12, 28, 24, fill=lvl_fill[lvl], rx=1, stroke=lvl_stroke[lvl])
        f.text(x + 14, 174, str(k), 11, weight=700, fill=NAVY)
    f.text(83, 30, "head", 10, weight=600)
    f.save("skiplist")

# ------------------------------------------------------------- Ch19: detector ring
def pet_ring():
    f = Fig(640, 330, "A PET detector ring seen end on: two 511 keV photons from one annihilation travel in opposite directions and hit two detector blocks; the line between the blocks is the line of response")
    cx, cy, R = 250, 165, 140
    n = 32
    for b in range(n):
        a = 2 * math.pi * b / n
        x, y = cx + R * math.cos(a), cy + R * math.sin(a)
        rot = math.degrees(a) + 90
        hit = b in (3, 19)
        f.add(f'<rect x="{x-11:.1f}" y="{y-9:.1f}" width="22" height="18" fill="{AMBER if hit else SKY2}" stroke="{NAVY}" stroke-width="0.8" transform="rotate({rot:.1f} {x:.1f} {y:.1f})"/>')
    def pt(b): a = 2 * math.pi * b / n; return cx + (R - 12) * math.cos(a), cy + (R - 12) * math.sin(a)
    (x1, y1), (x2, y2) = pt(3), pt(19)
    f.add(f'<circle cx="{cx}" cy="{cy}" r="58" fill="{TEAL_P}" stroke="{TEAL}" stroke-dasharray="4 3"/>')
    f.line(x1, y1, x2, y2, sw=2.2, stroke=RED)
    mx, my = x1 + (x2 - x1) * 0.4, y1 + (y2 - y1) * 0.4
    f.add(f'<circle cx="{mx:.1f}" cy="{my:.1f}" r="5.5" fill="{AMBER}" stroke="{INK}" stroke-width="1.5"/>')
    f.text(cx, cy + 76, "patient", 10, fill=TEAL, italic=True, weight=600)
    lx = 430
    f.text(lx, 70, "detector block", 11, anchor="start", weight=700, fill=NAVY)
    f.text(lx, 86, "crystals + photodetectors;", 10, anchor="start", fill=MID)
    f.text(lx, 100, "each detected photon is a hit", 10, anchor="start", fill=MID)
    bx, by = cx + R * math.cos(2 * math.pi * 3 / n), cy + R * math.sin(2 * math.pi * 3 / n)
    f.line(lx - 6, 74, bx + 14, by - 4, stroke=MID, arrow=True)
    f.text(lx, 170, "annihilation", 11, anchor="start", weight=700, fill=AMBER_D)
    f.text(lx, 186, "two 511 keV photons,", 10, anchor="start", fill=MID)
    f.text(lx, 200, "opposite directions", 10, anchor="start", fill=MID)
    f.line(lx - 6, 174, mx + 8, my, stroke=MID, arrow=True)
    f.text(lx, 250, "line of response", 11, anchor="start", weight=700, fill=RED)
    f.text(lx, 266, "the pair of blocks hit within", 10, anchor="start", fill=MID)
    f.text(lx, 280, "the coincidence window", 10, anchor="start", fill=MID)
    f.line(lx - 6, 254, x2 + (x1 - x2) * 0.2, y2 + (y1 - y2) * 0.2 + 4, stroke=MID, arrow=True)
    f.save("pet-ring")

# ------------------------------------------------------------- Ch19: pipeline
def pet_pipeline():
    f = Fig(640, 250, "The simulated acquisition pipeline: two sources each push wire batches into their own SPSC ring; the sorter thread decodes, checks sequence numbers, merges by watermark and sorts coincidences; an SPSC ring of coincidences feeds the analysis thread")
    f.box(10, 30, 130, 50, "source 0", "modules 0–3", fill=SKY, stroke=NAVY, color=NAVY)
    f.box(10, 150, 130, 50, "source 1", "modules 4–7", fill=SKY, stroke=NAVY, color=NAVY)
    f.line(140, 55, 230, 100, arrow=True, stroke=NAVY).line(140, 175, 230, 130, arrow=True, stroke=NAVY)
    f.text(176, 58, "SpscRing<WireBatch>", 9, mono=True, fill=MID, anchor="start")
    f.text(176, 184, "SpscRing<WireBatch>", 9, mono=True, fill=MID, anchor="start")
    f.rect(232, 60, 180, 110, fill=TEAL_P, stroke=TEAL)
    f.text(322, 82, "sorter thread", 12, weight=700, fill=TEAL)
    for i, s in enumerate(["decode, sequence check", "watermark merge (Ch. 20)", "coincidence sort (Ch. 20)"]):
        f.text(322, 104 + i * 18, s, 10)
    f.line(412, 115, 488, 115, arrow=True, stroke=TEAL)
    f.text(450, 96, "SpscRing", 9, mono=True, fill=MID).text(450, 107, "<Coincidence>", 9, mono=True, fill=MID)
    f.rect(490, 70, 145, 90, fill=CREAM, stroke=AMBER_D)
    f.text(562, 94, "analysis thread", 12, weight=700, fill=AMBER_D)
    for i, s in enumerate(["calibrate", "energy window", "histograms (Ch. 21)"]):
        f.text(562, 114 + i * 16, s, 10)
    f.text(320, 236, "One owner per stage; data moves only through bounded SPSC rings.", 11, fill=MID, italic=True)
    f.save("pet-pipeline")

# ------------------------------------------------------------- Ch18: patient monitor
def monitor():
    f = Fig(640, 240, "Patient-monitor architecture: heart-rate and SpO2 sensor threads feed one evaluator thread through SPSC rings; the evaluator publishes a VitalsView through a seqlock to the display and alarm events through a Disruptor to the audit logger and the annunciator")
    f.box(10, 30, 130, 50, "heart rate", "500 Hz sensor", fill=SKY, stroke=NAVY, color=NAVY)
    f.box(10, 140, 130, 50, "SpO₂", "250 Hz sensor", fill=SKY, stroke=NAVY, color=NAVY)
    f.line(140, 55, 236, 95, arrow=True, stroke=NAVY).line(140, 165, 236, 125, arrow=True, stroke=NAVY)
    f.text(186, 64, "SPSC", 10, fill=MID).text(186, 166, "SPSC", 10, fill=MID)
    f.rect(238, 60, 150, 100, fill=TEAL_P, stroke=TEAL)
    f.text(313, 92, "evaluator", 12, weight=700, fill=TEAL)
    f.text(313, 110, "ThresholdAlarm × 2", 10)
    f.text(313, 126, "owns all alarm state", 10, fill=MID)
    f.box(480, 14, 150, 40, "display", "60 Hz", fill="#fff", stroke=NAVY)
    f.box(480, 96, 150, 40, "audit logger", fill=CREAM, stroke=AMBER_D)
    f.box(480, 170, 150, 40, "annunciator", fill=RED_P, stroke=RED, color=RED)
    f.line(388, 80, 478, 36, arrow=True, stroke=NAVY).text(430, 46, "SeqLock", 10, fill=NAVY, weight=600)
    f.line(388, 115, 478, 116, arrow=True, stroke=AMBER_D).line(388, 130, 478, 188, arrow=True, stroke=AMBER_D)
    f.text(432, 108, "Disruptor", 10, fill=AMBER_D, weight=600)
    f.save("monitor")

# ------------------------------------------------------------- Ch20: coincidence window
def window():
    f = Fig(640, 230, "Hits on a time axis labelled by detector module; a coincidence window opens at each hit, pairs hits on different modules that fall inside it, and a delayed window shifted far in time estimates random coincidences")
    x0, x1, y = 40, 620, 110
    f.line(x0, y, x1, y, arrow=True).text(x1, y + 24, "time", 10, anchor="end", fill=MID)
    for a, w, lab, fill, st in [(70, 40, "pair (m2, m6)", TEAL_P, TEAL), (300, 40, "multiple: 3 hits in one window", CREAM, AMBER_D)]:
        f.rect(a, y - 34, w, 18, fill=fill, rx=1, stroke=st)
        f.text(a + w / 2 + (40 if a > 200 else 0), y - 42, lab, 10, weight=700, fill=st)
    f.rect(190, y - 34, 40, 18, fill="#fff", rx=1, dash="3 2", stroke=MID).text(210, y - 42, "single", 10, fill=MID)
    hits = [(70, "m2", TEAL), (96, "m6", TEAL), (190, "m1", MID), (300, "m3", AMBER_D), (318, "m7", AMBER_D), (334, "m0", AMBER_D), (470, "m5", NAVY), (560, "m4", NAVY)]
    for x, m, c in hits:
        f.line(x, y - 14, x, y + 14, sw=2.4, stroke=c).text(x, y + 30, m, 10, mono=True, fill=c, weight=600)
    f.line(70, 170, 110, 170, stroke=TEAL).line(70, 164, 70, 176, stroke=TEAL).line(110, 164, 110, 176, stroke=TEAL)
    f.text(90, 190, "window, a few ns", 10, fill=TEAL)
    f.rect(440, 150, 40, 18, fill="#fff", rx=1, dash="5 3", stroke=PURPLE)
    f.text(460, 186, "delayed window", 10, fill=PURPLE, weight=600)
    f.text(460, 200, "(same width, shifted)", 10, fill=MID)
    f.save("coincidence-window")

# ================================================================ charts of the book's measurements

# ------------------------------------------------------------- Ch1: fan-out
def fanout():
    f = Fig(640, 270, "Probability that a request waiting on n back-end calls is slow, if each call is slow with probability 1 percent or 0.1 percent: with 100 calls it is 63 percent and 9.5 percent")
    x0, x1, y0, y1 = 70, 600, 24, 220
    X, Y = linmap(0, 200, x0, x1), linmap(0, 100, y1, y0)
    f.ygrid(x0, x1, [0, 25, 50, 75, 100], Y, lambda v: f"{v}%")
    f.xgrid(y0, y1, [1, 50, 100, 150, 200], X, lambda v: f"{v}")
    f.frame(x0, y0, x1, y1)
    for p, c, lab, kind in [(0.01, S[3], "each call slow 1% of the time (its p99)", "circle"), (0.001, S[0], "each call slow 0.1% of the time (its p99.9)", "square")]:
        pts = [(X(n), Y(100 * (1 - (1 - p) ** n))) for n in range(1, 201, 3)]
        d = "M" + " L".join(f"{x:.1f},{y:.1f}" for x, y in pts)
        f.path(d, stroke=c, sw=2.6)
        v = 100 * (1 - (1 - p) ** 100)
        f.marker(kind, X(100), Y(v), c, r=4.5)
        f.text(X(100) + 9, Y(v) + 16, f"{v:.1f}% at n = 100", 10, anchor="start", weight=700, fill=c)
    f.text((x0 + x1) / 2, y1 + 34, "back-end calls per request (n)", 10, fill=MID)
    f.text(x0 - 46, y0 - 8, "requests that are slow", 10, anchor="start", fill=MID)
    f.legend(x0 + 260, y1 - 54, [("each call slow 1% of the time", S[3], "circle"), ("each call slow 0.1% of the time", S[0], "square")])
    f.save("chart-fanout")

# ------------------------------------------------------------- Ch2: pointer chase
def chase():
    f = Fig(640, 290, "Measured latency of a dependent load against working-set size on the reference machine: plateaus at 1.8 ns in L1, about 6 ns in L2, 25 to 40 ns in L3, a jump to 115 ns at 16 MiB from TLB misses, and 185 ns in DRAM at 512 MiB")
    data = [(4, 1.80), (16, 1.78), (32, 1.80), (64, 5.68), (256, 5.80), (512, 6.47), (1024, 8.96), (2048, 25.51),
            (4096, 37.06), (8192, 38.84), (16384, 114.5), (65536, 145.17), (262144, 168.33), (524288, 184.62)]
    x0, x1, y0, y1 = 70, 615, 40, 230
    X, Y = logmap(4, 524288, x0, x1), logmap(1, 300, y1, y0)
    bands = [(4, 48, "L1", SKY), (48, 2048, "L2", TEAL_P), (2048, 8192, "L3", CREAM), (8192, 266240, "L3 + TLB misses", RED_P), (266240, 524288, "DRAM", "#e8e2f3")]
    for a, b, lab, c in bands:
        f.rect(X(a), y0, X(b) - X(a), y1 - y0, fill=c, stroke="none", rx=0)
        f.text((X(a) + X(b)) / 2, y0 - 6, lab, 10, weight=700, fill=MID if lab != "L3 + TLB misses" else RED)
    f.ygrid(x0, x1, [1, 3, 10, 30, 100, 300], Y, lambda v: f"{v:g}")
    ticks = [4, 32, 256, 2048, 16384, 131072, 524288]
    lab = lambda k: f"{k} KiB" if k < 1024 else f"{k // 1024} MiB"
    f.xgrid(y0, y1, ticks, X, lab)
    f.frame(x0, y0, x1, y1)
    f.series([(X(k), Y(v)) for k, v in data], NAVY, "circle", sw=2.4)
    for k, v, t in [(32, 1.80, "1.8 ns"), (256, 5.80, "5.8 ns"), (8192, 38.84, "39 ns"), (16384, 114.5, "115 ns"), (524288, 184.62, "185 ns")]:
        dy = 16 if k in (8192, 524288) else -9
        f.text(X(k) - 4 if k == 524288 else X(k) + (8 if k == 8192 else 0), Y(v) + dy, t, 10, weight=700, fill=NAVY, anchor="end" if k == 524288 else "middle")
    f.text((x0 + x1) / 2, y1 + 32, "working set (log scale)", 10, fill=MID)
    f.text(x0 - 50, y0 - 22, "ns per load (log)", 10, anchor="start", fill=MID)
    f.save("chart-chase")

# ------------------------------------------------------------- Ch2: false sharing
def false_sharing():
    f = Fig(640, 130, "Measured cost of an increment when two threads' counters share a cache line, 34.15 ns, against 6.77 ns when they are on separate lines: five times slower")
    x0, x1 = 150, 560
    X = linmap(0, 40, x0, x1)
    f.xgrid(16, 92, [0, 10, 20, 30, 40], X, lambda v: f"{v} ns")
    for i, (lab, v, c) in enumerate([("same cache line", 34.15, S[3]), ("separate lines", 6.77, S[2])]):
        y = 24 + i * 36
        f.text(x0 - 8, y + 16, lab, 11, anchor="end", weight=600)
        f.rect(x0, y, X(v) - x0, 24, fill=c, stroke=INK, sw=0.6, rx=2)
        f.text(X(v) + 6, y + 16, f"{v:.2f} ns", 11, anchor="start", weight=700)
    f.text(X(34.15) + 70, 66, "5.0×", 16, weight=800, fill=RED, anchor="start")
    f.text((x0 + x1) / 2, 124, "ns per increment, two threads", 10, fill=MID)
    f.save("chart-false-sharing")

# ------------------------------------------------------------- Ch4: coordinated omission
def omission():
    f = Fig(640, 270, "Percentiles of the simulated service from Chapter 4 measured naively and corrected for coordinated omission: the naive p99, p99.9 and p99.99 stay at 2 microseconds, while the corrected values are 10 to 20 milliseconds")
    pcts = ["p50", "p99", "p99.9", "p99.99", "max"]
    naive = [2015, 2015, 2015, 2015, 2e7]
    corr = [2015, 10354687, 19136511, 19922943, 2e7]
    x0, x1, y0, y1 = 80, 600, 24, 214
    X = lambda i: x0 + 30 + i * (x1 - x0 - 60) / 4
    Y = logmap(1e3, 1e8, y1, y0)
    f.ygrid(x0, x1, [1e3, 1e4, 1e5, 1e6, 1e7, 1e8], Y, fmt_ns)
    for i, p in enumerate(pcts):
        f.text(X(i), y1 + 15, p, 10, fill=MID, weight=600)
    f.frame(x0, y0, x1, y1)
    poly = " ".join(f"{X(i):.1f},{Y(v):.1f}" for i, v in enumerate(corr)) + " " + " ".join(f"{X(i):.1f},{Y(v):.1f}" for i, v in reversed(list(enumerate(naive))))
    f.add(f'<polygon points="{poly}" fill="{RED_P}" stroke="none"/>')
    f.series([(X(i), Y(v)) for i, v in enumerate(corr)], RED, "square", sw=2.4)
    f.series([(X(i), Y(v)) for i, v in enumerate(naive)], S[0], "circle", sw=2.4, dash="6 3")
    f.text(X(2), Y(2e5), "9,500× too optimistic", 12, weight=800, fill=RED)
    f.legend(x0 + 14, y0 + 18, [("corrected (measured from intended send time)", RED, "square"), ("naive (service time only)", S[0], "circle", "6 3")])
    f.text((x0 + x1) / 2, y1 + 34, "percentile", 10, fill=MID)
    f.save("chart-omission")

# ------------------------------------------------------------- Ch5: locks
LOCKS = [("std::mutex", S[0], "circle", None, (15.16, 4.60, 5.38), 27647),
         ("TAS", S[3], "square", "5 3", (19.12, 7.83, 3.96), 172031),
         ("TTAS", S[4], "tri", "2 2", (19.52, 6.92, 5.10), 61439),
         ("TTAS + backoff", S[1], "diamond", None, (17.53, 16.95, 15.65), 57343),
         ("ticket", S[2], "ring", "6 2 2 2", (17.89, 1.97, 1.86), 29439),
         ("MCS", S[5], "box", None, (15.17, 3.50, 4.31), 17919)]
def locks():
    f = Fig(640, 300, "Measured lock throughput for one, two and four threads, and the p99.9 acquisition latency at four threads: backoff keeps the highest throughput, about 16 million acquisitions a second, while MCS has the best tail at 18 microseconds and TAS the worst at 172 microseconds")
    x0, x1, y0, y1 = 52, 300, 30, 232
    X = lambda i: x0 + 26 + i * (x1 - x0 - 52) / 2
    Y = linmap(0, 20, y1, y0)
    f.text(x0 - 40, 14, "throughput, M acquisitions/s", 11, anchor="start", weight=700, fill=NAVY)
    f.ygrid(x0, x1, [0, 5, 10, 15, 20], Y, lambda v: f"{v}")
    for i, t in enumerate(["T = 1", "T = 2", "T = 4"]):
        f.text(X(i), y1 + 15, t, 10, fill=MID, weight=600)
    f.frame(x0, y0, x1, y1)
    for name, c, kind, dash, vals, _ in LOCKS:
        f.series([(X(i), Y(v)) for i, v in enumerate(vals)], c, kind, dash=dash, sw=2.2 if name != "TTAS + backoff" else 3)
    f.legend(x0 - 30, y1 + 36, [(n, c, k, d) for n, c, k, d, _, _ in LOCKS[:3]], dx=112)
    f.legend(x0 - 30, y1 + 54, [(n, c, k, d) for n, c, k, d, _, _ in LOCKS[3:]], dx=112)
    # right: p99.9 at T = 4, horizontal log bars
    bx0, bx1 = 430, 615
    B = logmap(1e4, 6e5, bx0, bx1)
    f.text(bx0 - 90, 14, "p99.9 acquire latency at T = 4", 11, anchor="start", weight=700, fill=RED)
    f.xgrid(y0, y1, [1e4, 3e4, 1e5, 3e5], B, fmt_ns)
    for j, (name, c, *_rest, p999) in enumerate(sorted(LOCKS, key=lambda l: l[5])):
        y = y0 + 8 + j * 32
        f.text(bx0 - 6, y + 13, name, 10, anchor="end", weight=600)
        f.rect(bx0, y, B(p999) - bx0, 18, fill=c, stroke=INK, sw=0.6, rx=1)
        f.text(B(p999) + 4, y + 13, fmt_ns(round(p999, -2)), 9, anchor="start", weight=700)
    f.line(bx0, y0, bx0, y1, stroke=INK, sw=1)
    f.save("chart-locks")

# ------------------------------------------------------------- Ch6: atomics
def atomics():
    f = Fig(640, 250, "Measured cost of atomic operations on the reference machine: plain loads and stores under 1 ns, a seq_cst store and fetch_add about 7 ns, compare_exchange 12 ns; under contention, fetch_add rises to 29 ns and a CAS loop to 51 ns per increment at four threads")
    ops = [("plain increment", 0.18), ("load(relaxed)", 0.38), ("load(seq_cst)", 0.73), ("store(release)", 0.36),
           ("store(seq_cst)", 6.66), ("fetch_add(relaxed)", 6.97), ("compare_exchange", 11.81)]
    x0, x1, y0 = 140, 350, 30
    X = linmap(0, 14, x0, x1)
    f.text(10, 14, "one thread, ns per operation", 11, anchor="start", weight=700, fill=NAVY)
    f.xgrid(y0 - 4, y0 + 7 * 26, [0, 4, 8, 12], X, lambda v: f"{v}")
    for i, (op, v) in enumerate(ops):
        y = y0 + i * 26
        rmw = v > 5
        f.text(x0 - 6, y + 13, op, 10, anchor="end", mono=True)
        f.rect(x0, y + 1, max(X(v) - x0, 1.5), 17, fill=S[1] if rmw else S[0], stroke=INK, sw=0.5, rx=1)
        f.text(X(v) + 4, y + 13, f"{v:g}", 9, anchor="start", weight=700)
    f.text(x0 + 110, y0 + 7 * 26 + 30, "orange: a full barrier (lock prefix or xchg on x86)", 9, fill=AMBER_D, italic=True)
    # right: contention
    cx0, cx1, cy0, cy1 = 430, 615, 30, 200
    CX = lambda i: cx0 + 20 + i * (cx1 - cx0 - 40) / 2
    CY = linmap(0, 60, cy1, cy0)
    f.text(cx0 - 26, 14, "one shared counter, ns per increment", 11, anchor="start", weight=700, fill=RED)
    f.ygrid(cx0, cx1, [0, 20, 40, 60], CY, lambda v: f"{v}")
    for i, t in enumerate(["T = 1", "T = 2", "T = 4"]):
        f.text(CX(i), cy1 + 15, t, 10, fill=MID, weight=600)
    f.frame(cx0, cy0, cx1, cy1)
    f.series([(CX(i), CY(v)) for i, v in enumerate((9.63, 22.73, 28.81))], S[0], "circle")
    f.series([(CX(i), CY(v)) for i, v in enumerate((11.74, 40.39, 51.30))], S[3], "square", dash="5 3")
    f.legend(cx0 + 10, cy1 + 36, [("fetch_add", S[0], "circle"), ("CAS loop", S[3], "square", "5 3")], dx=90)
    f.save("chart-atomics")

# ------------------------------------------------------------- Ch7: seqlock vs locks
def seqlock_bench():
    f = Fig(640, 260, "Measured read throughput and writer p99 latency for a 48-byte record shared by one writer and one or three readers: the seqlock reads 106 and 213 million times a second against 47 and 21 million for a mutex, and its writer p99 is 195 ns against 20 microseconds for the mutex and 87 microseconds for shared_mutex")
    kinds = [("std::mutex", S[0]), ("std::shared_mutex", S[4]), ("SeqLock", S[1])]
    reads = {1: (46.60, 1.12, 105.93), 3: (21.02, 11.59, 212.90)}
    wp99 = (20223, 87039, 195)
    x0, x1, y0, y1 = 50, 320, 30, 200
    Y = linmap(0, 240, y1, y0)
    f.text(x0 - 40, 14, "reads per second (millions)", 11, anchor="start", weight=700, fill=NAVY)
    f.ygrid(x0, x1, [0, 60, 120, 180, 240], Y, lambda v: f"{v}")
    for g, r in enumerate((1, 3)):
        gx = x0 + 30 + g * 130
        for k, ((name, c), v) in enumerate(zip(kinds, reads[r])):
            x = gx + k * 30
            f.rect(x, Y(v), 26, y1 - Y(v), fill=c, stroke=INK, sw=0.5, rx=1)
            f.text(x + 13, Y(v) - 4, f"{v:.0f}" if v >= 10 else f"{v:.1f}", 9, weight=700)
        f.text(gx + 43, y1 + 15, f"{r} reader{'s' if r > 1 else ''}", 10, fill=MID, weight=600)
    f.frame(x0, y0, x1, y1)
    f.legend(x0 - 20, y1 + 38, [(n, c, "bar") for n, c in kinds], dx=125)
    bx0, bx1 = 460, 615
    B = logmap(100, 3e5, bx0, bx1)
    f.text(bx0 - 100, 14, "writer p99 with 3 readers", 11, anchor="start", weight=700, fill=RED)
    f.xgrid(y0, y0 + 110, [100, 1e3, 1e4, 1e5], B, fmt_ns)
    for j, ((name, c), v) in enumerate(zip(kinds, wp99)):
        y = y0 + 12 + j * 32
        f.text(bx0 - 6, y + 13, name, 10, anchor="end", weight=600, mono=True)
        f.rect(bx0, y, B(v) - bx0, 18, fill=c, stroke=INK, sw=0.5, rx=1)
        f.text(B(v) + 4, y + 13, fmt_ns(v if v < 1000 else round(v, -3)), 9, anchor="start", weight=700)
    f.line(bx0, y0, bx0, y0 + 110, stroke=INK, sw=1)
    f.text(bx0 + 30, y0 + 150, "readers never slow the writer", 10, fill=AMBER_D, italic=True, weight=600)
    f.save("chart-seqlock")

# ------------------------------------------------------------- Ch8: SPSC steps
def spsc_steps():
    f = Fig(640, 280, "Measured throughput of the five SPSC queue versions in Chapter 8, with the range over runs: mutex and deque 2.5 million items a second, atomic indices 7.5 million, power-of-two masks and acquire-release 53 million, padding 86 million, cached indices 139 million, 55 times the first version")
    steps = [("v1", "mutex + deque", 2.5, 2.4, 2.6, S[3]), ("v2", "atomic indices", 7.5, 6.5, 8.3, SKY2),
             ("v3", "mask, acq/rel", 53.1, 45.7, 69.8, "#6f9fd8"), ("v4", "+ padding", 86.0, 65.5, 99.8, BLUE), ("v5", "+ cached indices", 138.8, 112.2, 160.5, NAVY)]
    x0, x1, y0, y1 = 60, 610, 24, 220
    Y = linmap(0, 170, y1, y0)
    f.ygrid(x0, x1, [0, 40, 80, 120, 160], Y, lambda v: f"{v}")
    f.text(x0 - 44, 14, "million items per second", 10, anchor="start", fill=MID)
    w = 62
    for i, (v, lab, m, lo, hi, c) in enumerate(steps):
        cx = x0 + 60 + i * 110
        f.rect(cx - w / 2, Y(m), w, y1 - Y(m), fill=c, stroke=INK, sw=0.6, rx=2)
        f.line(cx, Y(hi), cx, Y(lo), stroke=INK, sw=1.2).line(cx - 7, Y(hi), cx + 7, Y(hi), stroke=INK).line(cx - 7, Y(lo), cx + 7, Y(lo), stroke=INK)
        f.text(cx, Y(hi) - 6, f"{m:g}", 11, weight=800, fill=NAVY)
        f.text(cx, y1 + 16, v, 11, weight=800, fill=NAVY)
        f.text(cx, y1 + 30, lab, 9, fill=MID)
        if i:
            prev = steps[i - 1][2]
            f.text(cx - 55, Y(max(m, prev)) - 26 if i > 1 else Y(30), f"×{m / prev:.1f}", 10, weight=700, fill=AMBER_D)
    f.frame(x0, y0, x1, y1)
    f.text(x0 + 16, y0 + 14, "55× from v1 to v5", 13, anchor="start", weight=800, fill=AMBER_D)
    f.text(x0 + 16, y0 + 30, "whiskers: range over runs", 9, anchor="start", fill=MID)
    f.save("chart-spsc")

# ------------------------------------------------------------- Ch9: MPMC queues
def mpmc():
    f = Fig(640, 270, "Measured capacity and paced median hand-off latency of three MPMC queues for four producer and consumer mixes: Vyukov's queue moves 5.7 to 6.5 million items a second in every mix, and its median latency stays between 207 and 487 ns, while the mutex queue's median reaches 4.8 ms with three producers")
    names = [("mutex + deque", S[3]), ("Michael-Scott", S[2]), ("Vyukov", S[0])]
    cfg = ["1P 1C", "2P 2C", "3P 1C", "1P 3C"]
    flood = [(1.67, 2.03, 6.52), (2.14, 2.84, 5.72), (0.93, 3.27, 6.39), (1.15, 1.49, 5.84)]
    p50 = [(1055, 427, 207), (4479, 647, 487), (4784127, 791, 343), (1663, 503, 315)]
    def panel(px0, px1, title, tc, Y, ticks, fmt, vals, lab):
        y0, y1 = 30, 200
        f.text(px0 - 36, 14, title, 11, anchor="start", weight=700, fill=tc)
        f.ygrid(px0, px1, ticks, Y, fmt)
        gw = (px1 - px0) / 4
        for g, row in enumerate(vals):
            gx = px0 + g * gw + 8
            for k, v in enumerate(row):
                bw = (gw - 16) / 3
                x = gx + k * bw
                f.rect(x, Y(v), bw - 2, y1 - Y(v), fill=names[k][1], stroke=INK, sw=0.5, rx=1)
            f.text(px0 + g * gw + gw / 2, y1 + 15, cfg[g], 9, fill=MID, weight=600)
        f.frame(px0, y0, px1, y1)
        for g, k, txt in lab:
            gx = px0 + g * gw + 8; bw = (gw - 16) / 3
            f.text(gx + k * bw + bw / 2 - 1, Y(vals[g][k]) - 4, txt, 8, weight=700)
    panel(50, 300, "capacity, flooded (M items/s)", NAVY, linmap(0, 7, 200, 30), [0, 2, 4, 6], lambda v: f"{v}", flood,
          [(g, 2, f"{flood[g][2]:.1f}") for g in range(4)])
    panel(380, 615, "paced median hand-off latency", RED, logmap(100, 1e7, 200, 30), [100, 1e3, 1e4, 1e5, 1e6, 1e7], fmt_ns, p50,
          [(2, 0, "4.8 ms"), (0, 2, "207"), (1, 2, "487")])
    f.legend(150, 246, [(n, c, "bar") for n, c in names], dx=130)
    f.save("chart-mpmc")

# ------------------------------------------------------------- Ch10: Disruptor vs chained rings
def pipeline_bench():
    f = Fig(640, 210, "Measured median end-to-end latency of the three-stage pipeline built from chained SPSC rings and from a Disruptor: flooded, 270 microseconds against 5.4 microseconds; paced at one event per microsecond, 499 ns against 455 ns")
    names = [("chained SPSC rings", S[3]), ("Disruptor", S[0])]
    vals = [("flooded (capacity)", 270335, 5439), ("paced, 1 event/µs", 499, 455)]
    x0, x1 = 170, 600
    B = logmap(100, 1e6, x0, x1)
    f.xgrid(16, 150, [100, 1e3, 1e4, 1e5, 1e6], B, fmt_ns)
    for g, (lab, a, b) in enumerate(vals):
        y = 26 + g * 64
        f.text(x0 - 8, y + 26, lab, 11, anchor="end", weight=700)
        for k, v in enumerate((a, b)):
            yy = y + k * 24
            f.rect(x0, yy, B(v) - x0, 20, fill=names[k][1], stroke=INK, sw=0.5, rx=1)
            f.text(B(v) + 5, yy + 14, fmt_ns(round(v, -3) if v > 1e4 else (round(v, -2) if v > 1e3 else v)), 10, anchor="start", weight=700)
    f.line(x0, 16, x0, 150, stroke=INK, sw=1)
    f.text((x0 + x1) / 2, 186, "median end-to-end latency (log scale)", 10, fill=MID)
    f.legend(x0 + 40, 204, [(n, c, "bar") for n, c in names], dx=170)
    f.save("chart-pipeline")

# ------------------------------------------------------------- Ch11: allocators across threads
def alloc_bench():
    f = Fig(640, 250, "Measured allocation latency when the consumer frees what the producer allocated: new and delete have a 106 ns median and a 3.6 microsecond p99.9, the lock-free pool 299 ns and 3.6 microseconds, a private free list with a return ring 27 ns and 251 ns")
    names = [("new / delete", S[3]), ("LockFreePool", S[4]), ("private list + return ring", S[2])]
    pct = ["p50", "p90", "p99", "p99.9"]
    vals = [(106, 1087, 1663, 3615), (299, 503, 807, 3647), (27, 116, 201, 251)]
    x0, x1, y0, y1 = 70, 610, 24, 196
    Y = logmap(10, 1e4, y1, y0)
    f.ygrid(x0, x1, [10, 100, 1e3, 1e4], Y, fmt_ns)
    gw = (x1 - x0) / 4
    for g, p in enumerate(pct):
        for k in range(3):
            bw = (gw - 40) / 3
            x = x0 + g * gw + 20 + k * bw
            v = vals[k][g]
            f.rect(x, Y(v), bw - 3, y1 - Y(v), fill=names[k][1], stroke=INK, sw=0.5, rx=1)
            f.text(x + (bw - 3) / 2, Y(v) - 4, f"{v:,}", 9, weight=700)
        f.text(x0 + g * gw + gw / 2, y1 + 15, p, 10, fill=MID, weight=600)
    f.frame(x0, y0, x1, y1)
    f.legend(x0 + 20, y1 + 40, [(n, c, "bar") for n, c in names], dx=150)
    f.save("chart-alloc")

# ------------------------------------------------------------- Ch14: ordered maps
def maps_bench():
    f = Fig(640, 230, "Measured throughput of three ordered maps on one and four threads: the mutex map falls from 4.3 to 1.3 million operations a second, the reader-writer map from 3.8 to 0.4 million, while the lazy skip list rises from 2.1 to 7.0 million")
    names = [("std::map + mutex", S[3]), ("std::map + shared_mutex", S[4]), ("lazy skip list", S[0])]
    vals = [(4.29, 1.32), (3.83, 0.39), (2.12, 7.03)]
    x0, x1, y0, y1 = 60, 600, 24, 180
    Y = linmap(0, 8, y1, y0)
    f.ygrid(x0, x1, [0, 2, 4, 6, 8], Y, lambda v: f"{v}")
    f.text(x0 - 44, 14, "million operations per second", 10, anchor="start", fill=MID)
    gw = (x1 - x0) / 2
    for g, t in enumerate(["1 thread", "4 threads"]):
        for k in range(3):
            bw = 60
            x = x0 + g * gw + (gw - 3 * bw) / 2 + k * bw
            v = vals[k][g]
            f.rect(x + 4, Y(v), bw - 8, y1 - Y(v), fill=names[k][1], stroke=INK, sw=0.5, rx=1)
            f.text(x + bw / 2, Y(v) - 5, f"{v:.2f}", 10, weight=700)
        f.text(x0 + g * gw + gw / 2, y1 + 16, t, 11, fill=MID, weight=600)
    f.frame(x0, y0, x1, y1)
    f.legend(x0 + 10, y1 + 42, [(n, c, "bar") for n, c in names], dx=180)
    f.save("chart-maps")

FIGS = [latency_tail, mesi, store_buffer, aba, seqlock, spsc, msqueue, disruptor, hazard, epochs, skiplist, pet_ring, pet_pipeline, monitor, window,
        fanout, chase, false_sharing, omission, locks, atomics, seqlock_bench, spsc_steps, mpmc, pipeline_bench, alloc_bench, maps_bench]
for g in FIGS:
    g()
print(f"{len(FIGS)} figures in {OUT}{' (gray)' if GRAY else ''}")
