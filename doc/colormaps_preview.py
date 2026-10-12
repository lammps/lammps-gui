#!/usr/bin/env python3
"""Render the dump-image color-map preview used in the User's Guide.

The color maps are defined once, in ``src/colormaps.cpp`` (see the
``ColorMapDef`` table).  This script parses that file so the documentation
image stays in sync with the source: add or change a map there, re-run this
script, and commit the regenerated PNG.

Usage (from anywhere)::

    python3 doc/colormaps_preview.py

Requires matplotlib.  Output: ``doc/JPG/lammps-gui-colormaps.png``.
"""

import os
import re

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
import numpy as np  # noqa: E402
from matplotlib.colors import (  # noqa: E402
    LinearSegmentedColormap,
    ListedColormap,
    to_rgb,
)

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, os.pardir, "src", "colormaps.cpp")
OUT = os.path.join(HERE, "JPG", "lammps-gui-colormaps.png")


def parse(src):
    """Return (ordered names, {name: (continuous, [(pos, (r, g, b)), ...])})."""
    text = open(src).read()

    # display order from the names list inside colorMapNames(); anchor on the
    # declaration so a preceding explanatory comment (which may quote map names)
    # is not mistaken for list entries
    names_block = re.search(
        r"QStringList\s+names\s*=\s*\{(.*?)\};", text, re.DOTALL
    ).group(1)
    order = re.findall(r'"(\w+)"', names_block)

    maps = {}
    for m in re.finditer(
        r'\{"(\w+)",\s*\{(true|false),\s*\{([^}]*)\}\}\}', text, re.DOTALL
    ):
        name, continuous, body = m.group(1), m.group(2) == "true", m.group(3)
        stops = []
        for tok in re.finditer(r"(rc|nm)\(([^)]*)\)", body):
            kind = tok.group(1)
            args = [a.strip() for a in tok.group(2).split(",")]
            pos = float(args[0])
            if kind == "rc":
                rgb = (float(args[1]), float(args[2]), float(args[3]))
            else:
                rgb = to_rgb(args[1].strip().strip('"'))
            stops.append((pos, rgb))
        maps[name] = (continuous, stops)

    return [n for n in order if n in maps], maps


def main():
    order, maps = parse(SRC)

    gradient = np.linspace(0.0, 1.0, 256).reshape(1, -1)
    fig, axes = plt.subplots(nrows=len(order), figsize=(6.0, 0.30 * len(order)))
    for ax, name in zip(axes, order):
        continuous, stops = maps[name]
        if continuous:
            cmap = LinearSegmentedColormap.from_list(name, stops)
        else:
            cmap = ListedColormap([rgb for _, rgb in stops])
        ax.imshow(gradient, aspect="auto", cmap=cmap)
        ax.set_axis_off()
        ax.text(
            -0.01,
            0.5,
            name,
            va="center",
            ha="right",
            fontsize=9,
            family="monospace",
            transform=ax.transAxes,
        )

    fig.subplots_adjust(left=0.16, right=0.99, top=0.99, bottom=0.01, hspace=0.5)
    fig.savefig(OUT, dpi=150)
    print("wrote", OUT, "with", len(order), "color maps")


if __name__ == "__main__":
    main()
