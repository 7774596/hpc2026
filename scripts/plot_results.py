#!/usr/bin/env python3
"""Plot simulation or exec-scaling CSV files.

The script prefers matplotlib PNG output when matplotlib is installed. It also
always writes a dependency-free HTML report with inline SVG charts, so plotting
still works on restricted HPC environments.
"""

import argparse
import csv
import html
import math
import os
import sys
from collections import defaultdict
from pathlib import Path


METRICS = [
    ("makespan", "Makespan (sim s)"),
    ("avg_wait", "Average Wait Time (sim s)"),
    ("avg_turnaround", "Average Turnaround Time (sim s)"),
    ("throughput", "Throughput (jobs/s)"),
    ("utilization", "Cluster Utilization"),
    ("load_balance_cv", "Load Balance CV (lower is better)"),
    ("wall_time", "Wall Time (s)"),
    ("speedup", "Speedup"),
    ("efficiency", "Parallel Efficiency"),
]

COLORS = ["#3b82f6", "#ef4444", "#22c55e", "#f59e0b", "#8b5cf6", "#06b6d4", "#64748b"]


def try_import_matplotlib():
    try:
        import matplotlib

        matplotlib.use("Agg")
        import matplotlib.pyplot as plt

        return plt
    except Exception:
        return None


def read_rows(path):
    with open(path, newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def parse_filters(items):
    filters = []
    for item in items:
        if "=" not in item:
            raise ValueError(f"invalid filter {item!r}; expected key=value")
        key, value = item.split("=", 1)
        filters.append((key, value))
    return filters


def apply_filters(rows, filters):
    for key, value in filters:
        rows = [r for r in rows if r.get(key) == value]
    return rows


def numeric(value):
    try:
        return float(value)
    except (TypeError, ValueError):
        return math.nan


def available_metrics(rows):
    if not rows:
        return []
    columns = set(rows[0].keys())
    out = []
    for key, title in METRICS:
        if key in columns:
            out.append((key, None, title))
        elif f"{key}_mean" in columns:
            err = f"{key}_std" if f"{key}_std" in columns else None
            out.append((f"{key}_mean", err, title))
    return out


def label_for(row, series):
    label = row.get(series, "")
    details = []
    for key in ("num_jobs", "arrival_rate", "nodes", "rr_quantum", "num_workers"):
        if key in row and row[key] not in ("", "-"):
            details.append(f"{key}={row[key]}")
    return label if not details else f"{label}\n" + ",".join(details)


def group_rows(rows, series):
    grouped = defaultdict(list)
    for row in rows:
        grouped[row.get(series, "series")].append(row)
    return grouped


def save_matplotlib_chart(plt, rows, metric, err_col, title, out_path, x_col, series_col):
    fig, ax = plt.subplots(figsize=(8, 4.8))
    if x_col:
        for idx, (name, group) in enumerate(sorted(group_rows(rows, series_col).items())):
            group = sorted(group, key=lambda r: numeric(r.get(x_col)))
            xs = [numeric(r.get(x_col)) for r in group]
            ys = [numeric(r.get(metric)) for r in group]
            yerr = [numeric(r.get(err_col)) for r in group] if err_col else None
            if yerr and any(v > 0 for v in yerr if not math.isnan(v)):
                ax.errorbar(xs, ys, yerr=yerr, marker="o", capsize=3, label=name)
            else:
                ax.plot(xs, ys, marker="o", label=name)
        ax.set_xlabel(x_col)
        ax.legend()
    else:
        labels = [label_for(r, series_col) for r in rows]
        ys = [numeric(r.get(metric)) for r in rows]
        yerr = [numeric(r.get(err_col)) for r in rows] if err_col else None
        x_pos = list(range(len(labels)))
        kwargs = {}
        if yerr and any(v > 0 for v in yerr if not math.isnan(v)):
            kwargs["yerr"] = yerr
            kwargs["capsize"] = 3
        ax.bar(x_pos, ys, color="#3b82f6", **kwargs)
        ax.set_xticks(x_pos)
        ax.set_xticklabels(labels, rotation=25, ha="right")
    ax.set_title(title)
    ax.grid(axis="y", alpha=0.3)
    fig.tight_layout()
    fig.savefig(out_path, dpi=160)
    plt.close(fig)


def scale(value, src_min, src_max, dst_min, dst_max):
    if src_max == src_min:
        return (dst_min + dst_max) / 2
    return dst_min + (value - src_min) * (dst_max - dst_min) / (src_max - src_min)


def svg_bar(rows, metric, title, series_col):
    width, height = 900, 420
    left, right, top, bottom = 70, 20, 35, 95
    values = [max(0.0, numeric(r.get(metric))) for r in rows]
    max_v = max(values) if values else 1.0
    max_v = max_v if max_v > 0 else 1.0
    plot_w = width - left - right
    plot_h = height - top - bottom
    bar_w = plot_w / max(1, len(rows)) * 0.72
    parts = [
        f'<svg viewBox="0 0 {width} {height}" width="{width}" height="{height}" '
        'xmlns="http://www.w3.org/2000/svg">',
        f'<text x="{width / 2}" y="22" text-anchor="middle" font-size="18">{html.escape(title)}</text>',
        f'<line x1="{left}" y1="{top}" x2="{left}" y2="{top + plot_h}" stroke="#334155"/>',
        f'<line x1="{left}" y1="{top + plot_h}" x2="{left + plot_w}" y2="{top + plot_h}" stroke="#334155"/>',
    ]
    for i, (row, value) in enumerate(zip(rows, values)):
        cx = left + (i + 0.5) * plot_w / max(1, len(rows))
        h = value / max_v * plot_h
        y = top + plot_h - h
        color = COLORS[i % len(COLORS)]
        parts.append(f'<rect x="{cx - bar_w / 2:.1f}" y="{y:.1f}" width="{bar_w:.1f}" height="{h:.1f}" fill="{color}"/>')
        parts.append(f'<text x="{cx:.1f}" y="{y - 5:.1f}" text-anchor="middle" font-size="11">{value:.4g}</text>')
        label = html.escape(label_for(row, series_col).replace("\n", " "))
        parts.append(
            f'<text x="{cx:.1f}" y="{top + plot_h + 18}" text-anchor="middle" '
            f'font-size="10" transform="rotate(25 {cx:.1f} {top + plot_h + 18})">{label}</text>'
        )
    parts.append(f'<text x="{left - 8}" y="{top + 5}" text-anchor="end" font-size="11">{max_v:.4g}</text>')
    parts.append("</svg>")
    return "\n".join(parts)


def svg_line(rows, metric, title, x_col, series_col):
    width, height = 900, 420
    left, right, top, bottom = 70, 150, 35, 65
    plot_w = width - left - right
    plot_h = height - top - bottom
    points = []
    for name, group in group_rows(rows, series_col).items():
        series_points = []
        for row in group:
            x = numeric(row.get(x_col))
            y = numeric(row.get(metric))
            if not math.isnan(x) and not math.isnan(y):
                series_points.append((x, y))
        if series_points:
            points.append((name, sorted(series_points)))
    all_x = [p[0] for _, group in points for p in group]
    all_y = [p[1] for _, group in points for p in group]
    if not all_x or not all_y:
        return f"<p>No numeric data for {html.escape(title)}</p>"
    x_min, x_max = min(all_x), max(all_x)
    y_min, y_max = min(0.0, min(all_y)), max(all_y)
    if y_max == y_min:
        y_max = y_min + 1.0
    parts = [
        f'<svg viewBox="0 0 {width} {height}" width="{width}" height="{height}" '
        'xmlns="http://www.w3.org/2000/svg">',
        f'<text x="{width / 2}" y="22" text-anchor="middle" font-size="18">{html.escape(title)}</text>',
        f'<line x1="{left}" y1="{top}" x2="{left}" y2="{top + plot_h}" stroke="#334155"/>',
        f'<line x1="{left}" y1="{top + plot_h}" x2="{left + plot_w}" y2="{top + plot_h}" stroke="#334155"/>',
        f'<text x="{left + plot_w / 2}" y="{height - 12}" text-anchor="middle" font-size="12">{html.escape(x_col)}</text>',
    ]
    for idx, (name, group) in enumerate(points):
        color = COLORS[idx % len(COLORS)]
        coords = []
        for x, y in group:
            sx = scale(x, x_min, x_max, left, left + plot_w)
            sy = scale(y, y_min, y_max, top + plot_h, top)
            coords.append((sx, sy, x, y))
        if len(coords) > 1:
            path = " ".join(f"{sx:.1f},{sy:.1f}" for sx, sy, _, _ in coords)
            parts.append(f'<polyline points="{path}" fill="none" stroke="{color}" stroke-width="2"/>')
        for sx, sy, _, y in coords:
            parts.append(f'<circle cx="{sx:.1f}" cy="{sy:.1f}" r="3.5" fill="{color}"/>')
            parts.append(f'<text x="{sx:.1f}" y="{sy - 7:.1f}" text-anchor="middle" font-size="10">{y:.4g}</text>')
        legend_y = top + 20 + idx * 20
        parts.append(f'<rect x="{left + plot_w + 25}" y="{legend_y - 10}" width="12" height="12" fill="{color}"/>')
        parts.append(f'<text x="{left + plot_w + 42}" y="{legend_y}" font-size="12">{html.escape(name)}</text>')
    parts.append(f'<text x="{left}" y="{top + plot_h + 18}" text-anchor="middle" font-size="11">{x_min:.4g}</text>')
    parts.append(f'<text x="{left + plot_w}" y="{top + plot_h + 18}" text-anchor="middle" font-size="11">{x_max:.4g}</text>')
    parts.append(f'<text x="{left - 8}" y="{top + 5}" text-anchor="end" font-size="11">{y_max:.4g}</text>')
    parts.append("</svg>")
    return "\n".join(parts)


def write_html_report(path, rows, metrics, x_col, series_col, png_names):
    parts = [
        "<!doctype html><meta charset='utf-8'>",
        "<title>HPCSim Experiment Report</title>",
        "<style>body{font-family:system-ui,sans-serif;margin:24px;}"
        "section{margin:28px 0;} svg{max-width:100%;height:auto;border:1px solid #e2e8f0;}"
        "table{border-collapse:collapse}td,th{border:1px solid #cbd5e1;padding:4px 8px}</style>",
        "<h1>HPCSim Experiment Report</h1>",
        f"<p>Rows plotted: {len(rows)}. Series: <code>{html.escape(series_col)}</code>.</p>",
    ]
    if png_names:
        parts.append("<p>PNG files were also generated:</p><ul>")
        for name in png_names:
            parts.append(f"<li><a href='{html.escape(name)}'>{html.escape(name)}</a></li>")
        parts.append("</ul>")
    for metric, _, title in metrics:
        parts.append(f"<section><h2>{html.escape(title)}</h2>")
        if x_col:
            parts.append(svg_line(rows, metric, title, x_col, series_col))
        else:
            parts.append(svg_bar(rows, metric, title, series_col))
        parts.append("</section>")
    parts.append("</html>")
    path.write_text("\n".join(parts), encoding="utf-8")


def parse_args(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv_path", nargs="?", default="results/summary.csv")
    parser.add_argument("--out-dir", default="")
    parser.add_argument("--x", default="", help="numeric x-axis column for line charts")
    parser.add_argument("--series", default="scheduler", help="column used as legend/series")
    parser.add_argument(
        "--filter",
        action="append",
        default=[],
        help="keep only rows matching key=value; may be repeated",
    )
    return parser.parse_args(argv)


def main(argv=None):
    args = parse_args(argv or sys.argv[1:])
    csv_path = Path(args.csv_path)
    if not csv_path.exists():
        print(f"{csv_path} not found; run experiments first", file=sys.stderr)
        return 1
    rows = read_rows(csv_path)
    if not rows:
        print("CSV is empty", file=sys.stderr)
        return 1
    try:
        filters = parse_filters(args.filter)
    except ValueError as exc:
        print(str(exc), file=sys.stderr)
        return 1
    rows = apply_filters(rows, filters)
    if not rows:
        print("no rows left after filters", file=sys.stderr)
        return 1

    metrics = available_metrics(rows)
    if not metrics:
        print("no known numeric metric columns found", file=sys.stderr)
        return 1
    out_dir = Path(args.out_dir) if args.out_dir else csv_path.parent / "plots"
    out_dir.mkdir(parents=True, exist_ok=True)

    plt = try_import_matplotlib()
    png_names = []
    if plt:
        for metric, err_col, title in metrics:
            name = metric.replace("_mean", "") + ".png"
            save_matplotlib_chart(
                plt,
                rows,
                metric,
                err_col,
                title,
                out_dir / name,
                args.x,
                args.series,
            )
            png_names.append(name)
            print(f"saved {out_dir / name}")
    else:
        print("matplotlib not available; writing HTML/SVG report only", file=sys.stderr)

    report = out_dir / "report.html"
    write_html_report(report, rows, metrics, args.x, args.series, png_names)
    print(f"saved {report}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
