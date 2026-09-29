#!/usr/bin/env python3
"""Draws docs-site/assets/diagrams/ray-spreading.svg: a view's rays spread at
equal angles against through a camera plane, meeting the same straight wall.
The geometry is exact: an 80-degree view, 9 rays, the wall 200 units ahead.

Usage: scripts/make_ray_diagram.py [output.svg]"""
import math
import sys

ex, eye_y, wall_y, n, half = 180.0, 310.0, 110.0, 9, math.radians(40)
d_wall, plane_d = eye_y - wall_y, 80.0
steps = [-half + i * 2 * half / (n - 1) for i in range(n)]
eq_hits = [ex + d_wall * math.tan(a) for a in steps]
arc_pts = [(ex + plane_d * math.sin(a), eye_y - plane_d * math.cos(a)) for a in steps]
pw = plane_d * math.tan(half)
plane_pts = [(ex - pw + i * 2 * pw / (n - 1), eye_y - plane_d) for i in range(n)]
plane_hits = [ex + (x - ex) * d_wall / plane_d for x, _ in plane_pts]

out = []
w = out.append
w('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 760 400" width="760" height="400" '
  'font-family="system-ui, -apple-system, Segoe UI, Roboto, sans-serif">')
w("<title>Spreading a view's rays: equal angles against a camera plane</title>")
w('<style>.t{fill:#444}.s{fill:#666}.ray{stroke:#e4572e;stroke-width:1.6}.hit{fill:#e4572e}'
  '.wall{stroke:#9e9e9e;stroke-width:6;stroke-linecap:round}'
  '.guide{stroke:#1e88e5;stroke-width:1.6;stroke-dasharray:5 4;fill:none}.gp{fill:#1e88e5}.eye{fill:#444}'
  '@media (prefers-color-scheme: dark){.t{fill:#e0e0e0}.s{fill:#bdbdbd}.eye{fill:#e0e0e0}}</style>')


def panel(ox, title, subtitle, hits, guide, guide_label, verdict):
    X = lambda x: x + ox
    w(f'<text class="t" x="{X(180)}" y="26" text-anchor="middle" font-size="17" font-weight="600">{title}</text>')
    w(f'<text class="s" x="{X(180)}" y="48" text-anchor="middle" font-size="12">{subtitle}</text>')
    w(f'<line class="wall" x1="{X(8)}" y1="{wall_y}" x2="{X(352)}" y2="{wall_y}"/>')
    for h in hits:
        w(f'<line class="ray" x1="{X(ex):.1f}" y1="{eye_y}" x2="{X(h):.1f}" y2="{wall_y}"/>')
    for h in hits:
        w(f'<circle class="hit" cx="{X(h):.1f}" cy="{wall_y}" r="4.5"/>')
    # The gaps between neighbouring hits, above the wall where no ray goes
    for a, b in zip(hits, hits[1:]):
        w(f'<text class="s" x="{X((a + b) / 2):.1f}" y="{wall_y - 12}" text-anchor="middle" font-size="12">{b - a:.0f}</text>')
    guide(X)
    w(f'<circle class="eye" cx="{X(ex)}" cy="{eye_y}" r="6"/>')
    w(f'<text class="t" x="{X(180)}" y="{eye_y + 24}" text-anchor="middle" font-size="12">the eye: every ray starts here</text>')
    w(f'<text class="s" x="{X(180)}" y="{eye_y + 50}" text-anchor="middle" font-size="12">{guide_label}</text>')
    w(f'<text class="t" x="{X(180)}" y="{eye_y + 76}" text-anchor="middle" font-size="13" font-weight="600">{verdict}</text>')


def arc_guide(X):
    (x0, y0), (x1, y1) = arc_pts[0], arc_pts[-1]
    w(f'<path class="guide" d="M {X(x0):.1f} {y0:.1f} A {plane_d} {plane_d} 0 0 1 {X(x1):.1f} {y1:.1f}"/>')
    for x, y in arc_pts:
        w(f'<circle class="gp" cx="{X(x):.1f}" cy="{y:.1f}" r="3.2"/>')


def plane_guide(X):
    y = plane_pts[0][1]
    w(f'<line class="guide" x1="{X(plane_pts[0][0] - 18):.1f}" y1="{y}" x2="{X(plane_pts[-1][0] + 18):.1f}" y2="{y}"/>')
    for x, yy in plane_pts:
        w(f'<circle class="gp" cx="{X(x):.1f}" cy="{yy}" r="3.2"/>')


panel(0, "Equal angles (before)", "9 rays meeting a straight wall",
      eq_hits, arc_guide, "blue: equal steps around an arc (10° each)", "Uneven hits: the wall bends on screen")
panel(400, "Camera plane (now)", "the same wall and view (80°)",
      plane_hits, plane_guide, "blue: equal steps along a flat window", "Even hits: the wall stays straight")
w('<line x1="380" y1="62" x2="380" y2="392" stroke="#9e9e9e" stroke-opacity="0.35"/>')
w('</svg>')
target = sys.argv[1] if len(sys.argv) > 1 else "docs-site/assets/diagrams/ray-spreading.svg"
open(target, "w").write("\n".join(out) + "\n")
