# Draws chart.png from the benchmark output:  ./bench | python3 chart.py
import re
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

SURFACE = "#fcfcfb"
INK = "#0b0b0b"
MUTED = "#52514e"
SERIES = ["#2a78d6", "#eb6834", "#1baf7a"]
NAMES = ["poolAllocator", "new/delete", "std::allocator"]

rows = []
source = open(sys.argv[1]) if len(sys.argv) > 1 else sys.stdin
for line in source:
    m = re.match(r"\|\s*(.+?)\s*\|\s*([\d.]+)\s*\|\s*([\d.]+)\s*\|\s*([\d.]+)\s*\|", line)
    if m:
        rows.append((m.group(1), [float(v) for v in m.groups()[1:]]))

if not rows:
    sys.exit("no result rows found in the benchmark output")

columns = 2 if len(rows) > 1 else 1
panelRows = (len(rows) + columns - 1) // columns
height = 0.8 + 3.1 * panelRows

fig, axes = plt.subplots(panelRows, columns, figsize=(10, height),
                         facecolor=SURFACE, squeeze=False)
fig.suptitle("One million 32-byte objects, lower is better",
             x=0.06, y=1 - 0.3 / height, ha="left", fontsize=13, color=INK,
             fontweight="bold")

for ax, (label, values) in zip(axes.flat, rows):
    scenario, unit = label.split(" (")
    unit = unit.rstrip(")")
    ratio = values[1] / values[0]
    note = "%.1fx faster than new/delete" % ratio
    if unit.startswith("bytes"):
        note = "%.1fx less memory than new/delete" % ratio

    ax.set_facecolor(SURFACE)
    ax.barh([2, 1, 0], values, color=SERIES, height=0.5)
    ax.set_yticks([2, 1, 0], NAMES, color=INK, fontsize=10)
    ax.set_xticks([])
    ax.set_xlim(0, max(values) * 1.25)
    for spine in ax.spines.values():
        spine.set_visible(False)
    ax.tick_params(length=0)

    digits = 0 if unit.startswith("bytes") else 2
    for y, v in zip([2, 1, 0], values):
        ax.text(v + max(values) * 0.03, y, "%.*f" % (digits, v),
                va="center", fontsize=10, color=INK)

    ax.set_title(scenario, loc="left", fontsize=12, color=INK, pad=18)
    ax.text(0, 1.03, "%s   %s" % (unit, note), transform=ax.transAxes,
            fontsize=9, color=MUTED)

for unused in list(axes.flat)[len(rows):]:
    unused.set_visible(False)

fig.tight_layout(rect=(0.0, 0.0, 1.0, 1 - 0.75 / height), h_pad=5.0, w_pad=3.0)
fig.savefig("chart.png", dpi=150, facecolor=SURFACE)
