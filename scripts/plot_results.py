#!/usr/bin/env python
from __future__ import print_function

"""Plot simulation or exec-scaling CSV files.

The script supports Python 2.7 and Python 3. It writes a dependency-free HTML
report with inline SVG charts. If matplotlib is available, it also writes PNGs.
"""

import argparse
import csv
import math
import os
import sys
from collections import defaultdict

try:
    from html import escape as html_escape
except ImportError:
    from cgi import escape as html_escape


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
NAN = float("nan")


def makedirs(path):
    if path and not os.path.isdir(path):
        os.makedirs(path)


def try_import_matplotlib():
    try:
        import matplotlib

        matplotlib.use("Agg")
        import matplotlib.pyplot as plt

        return plt
    except Exception:
        return None


def read_rows(path):
    with open(path, "r") as f:
        return list(csv.DictReader(f))


def parse_filters(items):
    filters = []
    for item in items:
        if "=" not in item:
            raise ValueError("invalid filter %r; expected key=value" % item)
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
        return NAN


def is_nan(value):
    return value != value


def available_metrics(rows):
    if not rows:
        return []
    columns = set(rows[0].keys())
    out = []
    for key, title in METRICS:
        if key in columns:
            out.append((key, None, title))
        elif "%s_mean" % key in columns:
            err = "%s_std" % key if "%s_std" % key in columns else None
            out.append(("%s_mean" % key, err, title))
    return out


def label_for(row, series):
    label = row.get(series, "")
    details = []
    for key in ("num_jobs", "arrival_rate", "nodes", "rr_quantum", "num_workers"):
        if key in row and row[key] not in ("", "-"):
            details.append("%s=%s" % (key, row[key]))
    return label if not details else label + "\n" + ",".join(details)


def group_rows(rows, series):
    grouped = defaultdict(list)
    for row in rows:
        grouped[row.get(series, "series")].append(row)
    return grouped


def save_matplotlib_chart(plt, rows, metric, err_col, title, out_path, x_col, series_col):
    fig, ax = plt.subplots(figsize=(8, 4.8))
    if x_col:
        for _, (name, group) in enumerate(sorted(group_rows(rows, series_col).items())):
            group = sorted(group, key=lambda r: numeric(r.get(x_col)))
            xs = [numeric(r.get(x_col)) for r in group]
            ys = [numeric(r.get(metric)) for r in group]
            yerr = [numeric(r.get(err_col)) for r in group] if err_col else None
            if yerr and any(v > 0 for v in yerr if not is_nan(v)):
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
        if yerr and any(v > 0 for v in yerr if not is_nan(v)):
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
        return (dst_min + dst_max) / 2.0
    return dst_min + (value - src_min) * (dst_max - dst_min) / (src_max - src_min)


def svg_bar(rows, metric, title, series_col):
    width, height = 900, 420
    left, right, top, bottom = 70, 20, 35, 95
    values = [max(0.0, numeric(r.get(metric))) for r in rows]
    max_v = max(values) if values else 1.0
    max_v = max_v if max_v > 0 else 1.0
    plot_w = width - left - right
    plot_h = height - top - bottom
    bar_w = plot_w / float(max(1, len(rows))) * 0.72
    parts = [
        '<svg viewBox="0 0 %d %d" width="%d" height="%d" xmlns="http://www.w3.org/2000/svg">'
        % (width, height, width, height),
        '<text x="%.1f" y="22" text-anchor="middle" font-size="18">%s</text>'
        % (width / 2.0, html_escape(title)),
        '<line x1="%d" y1="%d" x2="%d" y2="%d" stroke="#334155"/>' % (left, top, left, top + plot_h),
        '<line x1="%d" y1="%d" x2="%d" y2="%d" stroke="#334155"/>'
        % (left, top + plot_h, left + plot_w, top + plot_h),
    ]
    for i, pair in enumerate(zip(rows, values)):
        row, value = pair
        cx = left + (i + 0.5) * plot_w / float(max(1, len(rows)))
        h = value / max_v * plot_h
        y = top + plot_h - h
        color = COLORS[i % len(COLORS)]
        parts.append(
            '<rect x="%.1f" y="%.1f" width="%.1f" height="%.1f" fill="%s"/>'
            % (cx - bar_w / 2.0, y, bar_w, h, color)
        )
        parts.append(
            '<text x="%.1f" y="%.1f" text-anchor="middle" font-size="11">%.4g</text>'
            % (cx, y - 5, value)
        )
        label = html_escape(label_for(row, series_col).replace("\n", " "))
        parts.append(
            '<text x="%.1f" y="%d" text-anchor="middle" font-size="10" '
            'transform="rotate(25 %.1f %d)">%s</text>'
            % (cx, top + plot_h + 18, cx, top + plot_h + 18, label)
        )
    parts.append(
        '<text x="%d" y="%d" text-anchor="end" font-size="11">%.4g</text>'
        % (left - 8, top + 5, max_v)
    )
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
            if not is_nan(x) and not is_nan(y):
                series_points.append((x, y))
        if series_points:
            points.append((name, sorted(series_points)))
    all_x = [p[0] for _, group in points for p in group]
    all_y = [p[1] for _, group in points for p in group]
    if not all_x or not all_y:
        return "<p>No numeric data for %s</p>" % html_escape(title)
    x_min, x_max = min(all_x), max(all_x)
    y_min, y_max = min(0.0, min(all_y)), max(all_y)
    if y_max == y_min:
        y_max = y_min + 1.0
    parts = [
        '<svg viewBox="0 0 %d %d" width="%d" height="%d" xmlns="http://www.w3.org/2000/svg">'
        % (width, height, width, height),
        '<text x="%.1f" y="22" text-anchor="middle" font-size="18">%s</text>'
        % (width / 2.0, html_escape(title)),
        '<line x1="%d" y1="%d" x2="%d" y2="%d" stroke="#334155"/>' % (left, top, left, top + plot_h),
        '<line x1="%d" y1="%d" x2="%d" y2="%d" stroke="#334155"/>'
        % (left, top + plot_h, left + plot_w, top + plot_h),
        '<text x="%.1f" y="%d" text-anchor="middle" font-size="12">%s</text>'
        % (left + plot_w / 2.0, height - 12, html_escape(x_col)),
    ]
    for idx, pair in enumerate(points):
        name, group = pair
        color = COLORS[idx % len(COLORS)]
        coords = []
        for x, y in group:
            sx = scale(x, x_min, x_max, left, left + plot_w)
            sy = scale(y, y_min, y_max, top + plot_h, top)
            coords.append((sx, sy, x, y))
        if len(coords) > 1:
            path = " ".join("%.1f,%.1f" % (sx, sy) for sx, sy, _, _ in coords)
            parts.append(
                '<polyline points="%s" fill="none" stroke="%s" stroke-width="2"/>' % (path, color)
            )
        for sx, sy, _, y in coords:
            parts.append('<circle cx="%.1f" cy="%.1f" r="3.5" fill="%s"/>' % (sx, sy, color))
            parts.append(
                '<text x="%.1f" y="%.1f" text-anchor="middle" font-size="10">%.4g</text>'
                % (sx, sy - 7, y)
            )
        legend_y = top + 20 + idx * 20
        parts.append(
            '<rect x="%d" y="%d" width="12" height="12" fill="%s"/>'
            % (left + plot_w + 25, legend_y - 10, color)
        )
        parts.append(
            '<text x="%d" y="%d" font-size="12">%s</text>'
            % (left + plot_w + 42, legend_y, html_escape(name))
        )
    parts.append(
        '<text x="%d" y="%d" text-anchor="middle" font-size="11">%.4g</text>'
        % (left, top + plot_h + 18, x_min)
    )
    parts.append(
        '<text x="%d" y="%d" text-anchor="middle" font-size="11">%.4g</text>'
        % (left + plot_w, top + plot_h + 18, x_max)
    )
    parts.append(
        '<text x="%d" y="%d" text-anchor="end" font-size="11">%.4g</text>'
        % (left - 8, top + 5, y_max)
    )
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
        "<p>Rows plotted: %d. Series: <code>%s</code>.</p>"
        % (len(rows), html_escape(series_col)),
    ]
    if png_names:
        parts.append("<p>PNG files were also generated:</p><ul>")
        for name in png_names:
            parts.append("<li><a href='%s'>%s</a></li>" % (html_escape(name), html_escape(name)))
        parts.append("</ul>")
    for metric, _, title in metrics:
        parts.append("<section><h2>%s</h2>" % html_escape(title))
        if x_col:
            parts.append(svg_line(rows, metric, title, x_col, series_col))
        else:
            parts.append(svg_bar(rows, metric, title, series_col))
        parts.append("</section>")
    parts.append("</html>")
    with open(path, "w") as f:
        f.write("\n".join(parts))


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
    csv_path = args.csv_path
    if not os.path.exists(csv_path):
        print("%s not found; run experiments first" % csv_path, file=sys.stderr)
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
    out_dir = args.out_dir if args.out_dir else os.path.join(os.path.dirname(csv_path), "plots")
    makedirs(out_dir)

    plt = try_import_matplotlib()
    png_names = []
    if plt:
        for metric, err_col, title in metrics:
            name = metric.replace("_mean", "") + ".png"
            out_path = os.path.join(out_dir, name)
            save_matplotlib_chart(plt, rows, metric, err_col, title, out_path, args.x, args.series)
            png_names.append(name)
            print("saved %s" % out_path)
    else:
        print("matplotlib not available; writing HTML/SVG report only", file=sys.stderr)

    report = os.path.join(out_dir, "report.html")
    write_html_report(report, rows, metrics, args.x, args.series, png_names)
    print("saved %s" % report)
    return 0


if __name__ == "__main__":
    sys.exit(main())
