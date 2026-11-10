#!/usr/bin/env python3
"""Draws every figure in figures/ from the files in data/. Nothing is typed in by hand.

    python3 scripts/make_figures.py

Needs matplotlib and Pillow. Before a pattern is drawn it is checked again here, with an
independent one-generation simulation, to be a still life under its rule: a figure cannot
show something the data does not contain.
"""
from collections import Counter
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
from PIL import Image, ImageDraw, ImageFont  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
DATA, FIG = ROOT / "data", ROOT / "figures"

# Categorical slots in fixed order (validated for colour-vision deficiencies). Two of them sit
# below 3:1 contrast on the surface, so every series also gets its own marker and a direct label.
SURFACE, INK, INK_SOFT, GRID = "#fcfcfb", "#0b0b0b", "#52514e", "#e4e3df"
SEQUENCES = [  # (A-number, rule name, rule, terms the entry had before this work, colour, marker)
    ("A175001", "Move", "B368/S245", 10, "#2a78d6", "o"),
    ("A175000", "HighLife", "B36/S23", 13, "#eb6834", "s"),
    ("A166476", "2x2", "B36/S125", 10, "#1baf7a", "D"),
]
RULES = {"B368/S245": ({3, 6, 8}, {2, 4, 5}), "B36/S23": ({3, 6}, {2, 3}), "B36/S125": ({3, 6}, {1, 2, 5})}


def bfile(a):
    pares = [l.split() for l in (DATA / f"b{a[1:]}.txt").read_text().splitlines() if l.strip()]
    return [(int(n), int(v)) for n, v in pares]


def patterns(a, n):
    """Patterns listed by `DUMP=1 build/crosscheck RULE n` (one per line: 'WxH: row row ...')."""
    f = DATA / "patterns" / f"{a}_n{n}.txt"
    out = []
    for line in f.read_text().splitlines():
        if not line.strip():
            continue
        rows = line.split(":", 1)[1].split()
        out.append({(x, y) for y, r in enumerate(rows) for x, c in enumerate(r) if c == "O"})
    return out


def is_still(cells, rule):
    birth, survive = RULES[rule]
    near = Counter((x + dx, y + dy) for x, y in cells for dx in (-1, 0, 1) for dy in (-1, 0, 1) if dx or dy)
    nxt = {c for c, k in near.items() if (k in survive if c in cells else k in birth)}
    return nxt == cells


def islands(cells):
    left, comps = set(cells), 0
    while left:
        comps += 1
        stack = [left.pop()]
        while stack:
            x, y = stack.pop()
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    if (x + dx, y + dy) in left:
                        left.remove((x + dx, y + dy))
                        stack.append((x + dx, y + dy))
    return comps


def style(ax):
    ax.set_facecolor(SURFACE)
    for s in ("top", "right"):
        ax.spines[s].set_visible(False)
    for s in ("left", "bottom"):
        ax.spines[s].set_color(GRID)
    ax.tick_params(colors=INK_SOFT, labelsize=10)
    ax.grid(True, color=GRID, linewidth=0.8)
    ax.set_axisbelow(True)


def figure_terms():
    fig, ax = plt.subplots(figsize=(9, 5.2), dpi=150)
    fig.patch.set_facecolor(SURFACE)
    style(ax)
    for a, name, rule, before, colour, marker in SEQUENCES:
        pts = [(n, v) for n, v in bfile(a) if v > 0]
        ax.plot([n for n, _ in pts], [v for _, v in pts], color=colour, linewidth=2, zorder=2)
        old = [(n, v) for n, v in pts if n <= before]
        new = [(n, v) for n, v in pts if n > before]
        ax.scatter([n for n, _ in old], [v for _, v in old], s=46, marker=marker, facecolor=SURFACE,
                   edgecolor=colour, linewidth=1.6, zorder=3)
        ax.scatter([n for n, _ in new], [v for _, v in new], s=58, marker=marker, color=colour,
                   edgecolor=SURFACE, linewidth=1.2, zorder=4)
        n_end, v_end = pts[-1]
        ax.annotate(f"{a}  {name}\na({n_end}) = {v_end:,}", (n_end, v_end), xytext=(10, 0),
                    textcoords="offset points", va="center", fontsize=9.5, color=INK)
    ax.set_yscale("log")
    n_max = max(n for a, *_ in SEQUENCES for n, _ in bfile(a))   # the axis follows the b-files, not a typed limit
    ax.set_xlim(0.5, n_max + 3.8)
    ax.set_xticks(range(1, n_max + 1))
    ax.set_xlabel("n (live cells)", color=INK_SOFT, fontsize=10.5)
    ax.set_ylabel("strict still lifes with n cells", color=INK_SOFT, fontsize=10.5)
    hollow = plt.Line2D([], [], marker="o", linestyle="", markerfacecolor=SURFACE, markeredgecolor=INK_SOFT, markersize=7, label="already in the OEIS")
    solid = plt.Line2D([], [], marker="o", linestyle="", color=INK_SOFT, markersize=7, label="added in November 2026")
    ax.legend(handles=[hollow, solid], loc="upper left", frameon=False, fontsize=10, labelcolor=INK)
    ax.set_title("Three OEIS sequences, extended", loc="left", color=INK, fontsize=13, fontweight="bold")
    fig.tight_layout()
    fig.savefig(FIG / "terms.png", facecolor=SURFACE)
    plt.close(fig)


def figure_boxes():
    """Where the 376 still lifes of A175001 n=17 live, box by box (both programs agree)."""
    rows = [tuple(map(int, l.split())) for l in (DATA / "boxes" / "A175001_n17_stilllife.txt").read_text().splitlines() if l.strip()]
    hit = [(w, h, c) for w, h, c in rows if c]
    w0, w1 = min(w for w, _, _ in hit), max(w for w, _, _ in hit)
    h0, h1 = min(h for _, h, _ in hit), max(h for _, h, _ in hit)
    grid = [[0] * (h1 - h0 + 1) for _ in range(w1 - w0 + 1)]
    for w, h, c in hit:
        grid[w - w0][h - h0] = c
    fig, ax = plt.subplots(figsize=(7, 5), dpi=150)
    fig.patch.set_facecolor(SURFACE)
    ax.set_facecolor(SURFACE)
    import numpy as np
    m = np.array(grid, dtype=float)
    m[m == 0] = np.nan
    from matplotlib.colors import LinearSegmentedColormap
    ramp = LinearSegmentedColormap.from_list("blue", ["#cde2fb", "#86b6ef", "#3987e5", "#1c5cab", "#0d366b"])
    im = ax.imshow(m, origin="lower", cmap=ramp, aspect="equal")
    ax.set_xticks(range(h1 - h0 + 1), [str(h) for h in range(h0, h1 + 1)])
    ax.set_yticks(range(w1 - w0 + 1), [str(w) for w in range(w0, w1 + 1)])
    for w, h, c in hit:
        ax.text(h - h0, w - w0, str(c), ha="center", va="center", fontsize=9,
                color="#ffffff" if c >= 0.45 * max(x[2] for x in hit) else INK)
    ax.set_xlabel("box height h", color=INK_SOFT)
    ax.set_ylabel("box width w (w <= h)", color=INK_SOFT)
    ax.tick_params(colors=INK_SOFT, labelsize=9)
    for s in ax.spines.values():
        s.set_visible(False)
    cb = fig.colorbar(im, ax=ax, shrink=0.8)
    cb.ax.tick_params(colors=INK_SOFT, labelsize=8)
    cb.outline.set_visible(False)
    ax.set_title(f"A175001, n = 17: {sum(c for _, _, c in hit)} still lifes in {len(hit)} box sizes",
                 loc="left", color=INK, fontsize=12, fontweight="bold")
    fig.tight_layout()
    fig.savefig(FIG / "boxes-A175001-n17.png", facecolor=SURFACE)
    plt.close(fig)


def tile(cells, cell, pad, colour):
    w = max(x for x, _ in cells) + 1
    h = max(y for _, y in cells) + 1
    im = Image.new("RGB", (w * cell + 2 * pad, h * cell + 2 * pad), SURFACE)
    d = ImageDraw.Draw(im)
    for x, y in cells:
        x0, y0 = pad + x * cell, pad + y * cell
        d.rounded_rectangle([x0 + 1, y0 + 1, x0 + cell - 2, y0 + cell - 2], radius=2, fill=colour)
    return im


def mosaic(pats, colour, width, cell=7, gap=10, title=None, font=None):
    tiles = [tile(p, cell, 4, colour) for p in pats]
    rows, row, x = [], [], 0
    for t in tiles:
        if row and x + t.width > width:
            rows.append(row)
            row, x = [], 0
        row.append(t)
        x += t.width + gap
    if row:
        rows.append(row)
    top = 44 if title else 0
    height = top + sum(max(t.height for t in r) + gap for r in rows) + gap
    im = Image.new("RGB", (width + 2 * gap, height), SURFACE)
    d = ImageDraw.Draw(im)
    if title:
        d.text((gap, 12), title, fill=INK, font=font)
    y = top + gap
    for r in rows:
        x = gap
        for t in r:
            im.paste(t, (x, y))
            x += t.width + gap
        y += max(t.height for t in r) + gap
    return im


def font(size):
    for name in ("DejaVuSans.ttf", "Arial.ttf", "/System/Library/Fonts/Helvetica.ttc"):
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            continue
    return ImageFont.load_default()


def figure_galleries():
    f = font(18)
    frames = []
    for n in range(11, 16):
        pats = patterns("A175001", n)
        assert all(is_still(p, "B368/S245") for p in pats), n
        # Fewer patterns, bigger cells: every frame fills about the same area.
        frames.append(mosaic(pats, "#2a78d6", 960, cell={11: 16, 12: 14, 13: 12, 14: 8, 15: 6}[n],
                             title=f"A175001 (Move, B368/S245): all {len(pats)} strict still lifes with {n} cells", font=f))
    w = max(fr.width for fr in frames)
    h = max(fr.height for fr in frames)
    canvas = []
    for fr in frames:
        c = Image.new("RGB", (w, h), SURFACE)
        c.paste(fr, ((w - fr.width) // 2, (h - fr.height) // 2))
        canvas.append(c.convert("P", palette=Image.ADAPTIVE, colors=16))
    canvas[0].save(FIG / "gallery-move.gif", save_all=True, append_images=canvas[1:], duration=1800, loop=0, optimize=True)

    pats = patterns("A175000", 14)
    assert all(is_still(p, "B36/S23") for p in pats)
    mosaic(pats, "#eb6834", 1200, cell=5, gap=8,
           title=f"A175000 (HighLife, B36/S23): all {len(pats)} strict still lifes with 14 cells", font=f).save(FIG / "gallery-highlife-n14.png")


def figure_strict_vs_pseudo():
    """Why 'strict' matters: two islands that need each other count; two that do not, do not."""
    multi = [p for p in patterns("A175001", 15) if islands(p) >= 2]
    assert multi, "no strict still life with two islands among A175001 n=15"
    strict = min(multi, key=lambda p: (max(x for x, _ in p), sorted(p)))
    # Pseudo example in the same rule: the strict n=11 still life twice, far enough apart not to interact.
    one = patterns("A175001", 11)[0]
    shift = max(x for x, _ in one) + 3
    pseudo = one | {(x + shift, y) for x, y in one}
    assert is_still(pseudo, "B368/S245") and islands(pseudo) == 2 * islands(one)
    f, g = font(17), font(13)
    a = tile(strict, 18, 14, "#2a78d6")
    b = tile(pseudo, 18, 14, "#a7a6a1")
    textos = [("Counted", f"{islands(strict)} islands that are not stable apart",
               "a strict still life of A175001 (Move), 15 cells"),
              ("Not counted", "stable piece by piece",
               "two copies of an 11-cell still life: a pseudo still life")]
    col = [max(a.width, 330), max(b.width, 330)]
    gap, top = 70, 86
    im = Image.new("RGB", (col[0] + col[1] + gap + 60, top + max(a.height, b.height) + 60), SURFACE)
    d = ImageDraw.Draw(im)
    x = 30
    for k, (img, (titulo, sub, pie)) in enumerate(zip((a, b), textos)):
        d.text((x, 16), titulo, fill=INK, font=f)
        d.text((x, 44), sub, fill=INK_SOFT, font=g)
        im.paste(img, (x, top))
        d.text((x, top + img.height + 14), pie, fill=INK_SOFT, font=g)
        x += col[k] + gap
    im.save(FIG / "strict-vs-pseudo.png")


if __name__ == "__main__":
    FIG.mkdir(exist_ok=True)
    figure_terms()
    figure_boxes()
    figure_galleries()
    figure_strict_vs_pseudo()
    print("figures written to", FIG)
