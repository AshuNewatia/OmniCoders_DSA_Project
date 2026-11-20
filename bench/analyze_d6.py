"""Summarize repeated D6 runs, draw SVG figures, and build the analyst PDF.

Uses only Python's standard library plus ReportLab for the PDF. Run from any
directory: python bench/analyze_d6.py (or py -3 bench/analyze_d6.py).
"""
from __future__ import annotations

import csv
import math
import statistics
from collections import defaultdict
from pathlib import Path

try:
    from reportlab.lib import colors
    from reportlab.lib.enums import TA_LEFT
    from reportlab.lib.pagesizes import letter
    from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
    from reportlab.lib.units import inch
    from reportlab.platypus import (SimpleDocTemplate, Paragraph, Spacer, Table,
                                    TableStyle, PageBreak, Flowable)
    HAVE_REPORTLAB = True
except ImportError:
    HAVE_REPORTLAB = False


ROOT = Path(__file__).resolve().parents[1]
RESULTS = ROOT / "results"
DOCS = ROOT / "docs" / "derivations"
COLORS = {"A": "#167D78", "B": "#E39B42", "oracle": "#667085"}
LABELS = {"A": "Indexed heap A", "B": "Lazy heap B", "oracle": "Array oracle"}
CPU = "13th Gen Intel Core i7-13650HX; 14 cores / 20 threads; 24 MB Intel Smart Cache; max turbo 4.90 GHz; registry reported ~2.8 GHz at capture (dynamic clock; not pinned)"
CPU_SOURCE = "https://www.intel.com/content/www/us/en/products/sku/232101/intel-core-i713650hx-processor-24m-cache-up-to-4-90-ghz/specifications.html"


def read_csv(path: Path):
    with path.open(newline="", encoding="utf-8-sig") as f:
        return list(csv.DictReader(f))


def mean_sd(values):
    return statistics.mean(values), (statistics.stdev(values) if len(values) > 1 else 0.0)


def fnum(row, key):
    return float(row.get(key) or 0)


def summarize_main():
    raw = read_csv(RESULTS / "d6_measurements.csv")
    groups = defaultdict(list)
    for r in raw:
        groups[(r["implementation"], r["policy"], int(r["requested_revision_pct"]))].append(r)
    if not groups or any(len(runs) < 5 for runs in groups.values()):
        raise ValueError("D6 primary results need at least five repetitions per case")
    rows = []
    op_columns = (("insert", "insert_ns", "insert_count"),
                  ("extract", "extract_ns", "extract_count"),
                  ("revision", "revision_ns", "revision_count"),
                  ("delete", "delete_ns", "delete_count"),
                  ("escalation", "escalation_ns", "escalation_count"))
    for (impl, policy, pct), runs in sorted(groups.items()):
        times = [fnum(r, "total_queue_ns") / 1e6 for r in runs]
        memories = [fnum(r, "peak_working_set_bytes") / 1e6 for r in runs]
        means = {k: statistics.mean([fnum(r, k) for r in runs]) for k in
                 ("arrivals", "treatments", "departures", "revisions", "escalations", "peak_live", "mean_live_queue")}
        # Work units follow the D6 model. Calibrate one constant per queue and
        # policy on the 0% revision case, then predict the other shares.
        live = max(1.0, means["mean_live_queue"])
        heap_size = 2.0 * means["peak_live"] + 1.0
        if impl == "oracle":
            # Extract-min scans the whole array; successful lookups of random
            # revision/departure IDs examine about half of it on average.
            work = (means["arrivals"] + means["treatments"] * live +
                    0.5 * (means["revisions"] + means["departures"] + means["escalations"]) * live)
        else:
            log_size = math.log2((heap_size if impl == "B" else live) + 1.0)
            work = (means["arrivals"] + means["treatments"] + means["revisions"] +
                    means["departures"] + means["escalations"]) * log_size
        row = {
            "implementation": impl, "aging": policy, "revision_share_pct": pct,
            "repetitions": len(runs), "mean_time_ms": statistics.mean(times),
            "sd_time_ms": statistics.stdev(times) if len(times) > 1 else 0.0,
            "min_time_ms": min(times), "max_time_ms": max(times),
            "mean_peak_memory_mb": statistics.mean(memories),
            "sd_peak_memory_mb": statistics.stdev(memories) if len(memories) > 1 else 0.0,
            "mean_live_queue": means["mean_live_queue"], "peak_live": means["peak_live"],
            "mean_revisions": means["revisions"], "mean_escalations": means["escalations"],
            "model_work_units": work,
        }
        for op, time_col, count_col in op_columns:
            per_run_us = [fnum(r, time_col) / fnum(r, count_col) / 1000.0 for r in runs if fnum(r, count_col) > 0]
            row[f"mean_{op}_us_per_op"] = statistics.mean(per_run_us) if per_run_us else ""
            row[f"sd_{op}_us_per_op"] = statistics.stdev(per_run_us) if len(per_run_us) > 1 else (0.0 if per_run_us else "")
        rows.append(row)
    # Baseline-normalized predicted time and measured/predicted gap.
    for impl in ("A", "B", "oracle"):
        for policy in ("linear", "stepwise"):
            subset = [r for r in rows if r["implementation"] == impl and r["aging"] == policy]
            baseline = next((r for r in subset if r["revision_share_pct"] == 0), None)
            if baseline and baseline["model_work_units"]:
                for r in subset:
                    r["predicted_time_ms"] = baseline["mean_time_ms"] * r["model_work_units"] / baseline["model_work_units"]
                    r["measured_vs_predicted_gap_pct"] = (r["mean_time_ms"] / r["predicted_time_ms"] - 1.0) * 100 if r["predicted_time_ms"] else 0.0
    fields = list(rows[0].keys())
    with (RESULTS / "d6_summary.csv").open("w", newline="", encoding="utf-8") as f:
        out = csv.DictWriter(f, fieldnames=fields)
        out.writeheader()
        out.writerows(rows)
    return rows


def summarize_scale():
    raw = read_csv(RESULTS / "d6_scale_measurements.csv")
    groups = defaultdict(list)
    for r in raw:
        groups[(r["implementation"], int(r["patient_count"]), int(r["requested_revision_pct"]))].append(r)
    if not groups or any(len(runs) < 5 for runs in groups.values()):
        raise ValueError("D6 scale results need at least five repetitions per case")
    rows = []
    for (impl, n, pct), runs in sorted(groups.items()):
        def stats(key):
            values = [fnum(r, key) / 1e6 for r in runs]
            return mean_sd(values)
        b, bsd = stats("build_ns"); o, osd = stats("operation_ns"); t, tsd = stats("total_including_build_ns")
        m, msd = stats("peak_working_set_bytes")
        rows.append({"implementation": impl, "patient_count": n, "revision_share_pct": pct,
                     "repetitions": len(runs), "mean_build_ms": b, "sd_build_ms": bsd,
                     "mean_operation_ms": o, "sd_operation_ms": osd,
                     "mean_total_ms": t, "sd_total_ms": tsd,
                     "mean_peak_memory_mb": m, "sd_peak_memory_mb": msd,
                     "mean_revision_count": statistics.mean([fnum(r, "revision_count") for r in runs]),
                     "mean_extract_count": statistics.mean([fnum(r, "extract_count") for r in runs])})
    with (RESULTS / "d6_scale_summary.csv").open("w", newline="", encoding="utf-8") as f:
        out = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        out.writeheader(); out.writerows(rows)
    return rows


def svg_bar_chart(path: Path, title: str, categories, series, y_label: str, logarithmic=False):
    """Write a compact grouped bar figure using SVG and no plotting dependency."""
    width, height = 1080, 590
    left, right, top, bottom = 120, 35, 95, 110
    plot_w, plot_h = width - left - right, height - top - bottom
    raw_max = max(max(vals) for _, vals, _ in series) or 1
    floor = 0.01
    min_plot = math.log10(floor) if logarithmic else 0.0
    max_plot = math.ceil(math.log10(raw_max * 1.16)) if logarithmic else raw_max * 1.16
    def plotted(value): return math.log10(max(floor, value)) if logarithmic else value
    parts = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
             '<rect width="100%" height="100%" fill="#ffffff"/>',
             f'<text x="{width/2}" y="42" text-anchor="middle" font-family="Arial" font-size="27" font-weight="700" fill="#172B4D">{title}</text>',
             f'<text x="25" y="{top+plot_h/2}" transform="rotate(-90 25,{top+plot_h/2})" text-anchor="middle" font-family="Arial" font-size="17" fill="#455468">{y_label}</text>']
    tick_values = list(range(int(min_plot), int(max_plot) + 1)) if logarithmic else [max_plot * i / 5 for i in range(6)]
    for tick, val in enumerate(tick_values):
        y = top + plot_h - plot_h * (val-min_plot) / (max_plot-min_plot)
        parts.append(f'<line x1="{left}" y1="{y:.1f}" x2="{left+plot_w}" y2="{y:.1f}" stroke="#E4E7EC"/>')
        tick_label = f"1e{int(val)}" if logarithmic else f"{val:.1f}"
        parts.append(f'<text x="{left-12}" y="{y+5:.1f}" text-anchor="end" font-family="Arial" font-size="13" fill="#667085">{tick_label}</text>')
    ncat, nseries = len(categories), len(series)
    group_w = plot_w / max(ncat, 1)
    bar_w = min(58, group_w * 0.72 / nseries)
    for ci, cat in enumerate(categories):
        center = left + (ci + 0.5) * group_w
        parts.append(f'<text x="{center:.1f}" y="{top+plot_h+31}" text-anchor="middle" font-family="Arial" font-size="15" fill="#344054">{cat}</text>')
        for si, (name, vals, color) in enumerate(series):
            x = center - nseries*bar_w/2 + si*bar_w
            val = vals[ci]
            bh = plot_h * (plotted(val) - min_plot) / (max_plot - min_plot)
            y = top + plot_h - bh
            parts.append(f'<rect x="{x:.1f}" y="{y:.1f}" width="{bar_w-5:.1f}" height="{bh:.1f}" rx="3" fill="{color}"/>')
            parts.append(f'<text x="{x+(bar_w-5)/2:.1f}" y="{max(top+15,y-7):.1f}" text-anchor="middle" font-family="Arial" font-size="11" fill="#344054">{val:.2g}</text>')
    legend_y = height - 28
    total_w = nseries * 190
    start_x = (width-total_w)/2
    for i, (name, _, color) in enumerate(series):
        x = start_x + i*190
        parts.append(f'<rect x="{x:.1f}" y="{legend_y-13}" width="15" height="15" fill="{color}"/>')
        parts.append(f'<text x="{x+23:.1f}" y="{legend_y}" font-family="Arial" font-size="15" fill="#344054">{name}</text>')
    parts.append('</svg>')
    path.write_text('\n'.join(parts), encoding='utf-8')


def svg_line_chart(path: Path, title: str, shares, series):
    width, height = 1080, 590
    left, right, top, bottom = 105, 35, 90, 105
    pw, ph = width-left-right, height-top-bottom
    max_time = max(max(s["means"]) for s in series)
    max_exp = math.ceil(math.log10(max_time * 1.2))
    min_exp = 0
    parts = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
             '<rect width="100%" height="100%" fill="#ffffff"/>',
             f'<text x="{width/2}" y="42" text-anchor="middle" font-family="Arial" font-size="27" font-weight="700" fill="#172B4D">{title}</text>',
             f'<text x="25" y="{top+ph/2}" transform="rotate(-90 25,{top+ph/2})" text-anchor="middle" font-family="Arial" font-size="17" fill="#455468">Mean total queue time (ms, log scale)</text>']
    for exponent in range(min_exp, max_exp+1):
        y = top + ph - ph*(exponent-min_exp)/(max_exp-min_exp)
        parts.append(f'<line x1="{left}" y1="{y:.1f}" x2="{left+pw}" y2="{y:.1f}" stroke="#E4E7EC"/>')
        parts.append(f'<text x="{left-12}" y="{y+5:.1f}" text-anchor="end" font-family="Arial" font-size="13" fill="#667085">1e{exponent}</text>')
    xs = [left + pw*i/(len(shares)-1) for i in range(len(shares))]
    for i, share in enumerate(shares):
        parts.append(f'<text x="{xs[i]:.1f}" y="{top+ph+30}" text-anchor="middle" font-family="Arial" font-size="14" fill="#344054">{share}%</text>')
    for s in series:
        points = []
        for i, value in enumerate(s["means"]):
            lo = max(1.0, value-s["sds"][i]); hi = value+s["sds"][i]
            y = lambda v: top+ph-ph*(math.log10(v)-min_exp)/(max_exp-min_exp)
            points.append((xs[i], y(value)))
            ey1, ey2 = y(lo), y(hi)
            parts.append(f'<line x1="{xs[i]:.1f}" y1="{ey1:.1f}" x2="{xs[i]:.1f}" y2="{ey2:.1f}" stroke="{s["color"]}" stroke-width="2"/>')
            parts.append(f'<line x1="{xs[i]-4:.1f}" y1="{ey1:.1f}" x2="{xs[i]+4:.1f}" y2="{ey1:.1f}" stroke="{s["color"]}"/>')
            parts.append(f'<line x1="{xs[i]-4:.1f}" y1="{ey2:.1f}" x2="{xs[i]+4:.1f}" y2="{ey2:.1f}" stroke="{s["color"]}"/>')
        point_text = ' '.join(f'{x:.1f},{y:.1f}' for x,y in points)
        parts.append(f'<polyline points="{point_text}" fill="none" stroke="{s["color"]}" stroke-width="3"/>')
        for x,y in points:
            parts.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="5" fill="{s["color"]}"/>')
    legend_y = height-28; start_x = 335
    for i,s in enumerate(series):
        x = start_x+i*170
        parts.append(f'<line x1="{x}" y1="{legend_y-5}" x2="{x+22}" y2="{legend_y-5}" stroke="{s["color"]}" stroke-width="3"/>')
        parts.append(f'<text x="{x+30}" y="{legend_y}" font-family="Arial" font-size="15" fill="#344054">{s["name"]}</text>')
    parts.append('</svg>')
    path.write_text('\n'.join(parts), encoding='utf-8')


def make_figures(main, scale):
    for policy in ("linear", "stepwise"):
        shares = sorted({r["revision_share_pct"] for r in main if r["aging"] == policy})
        series = []
        for impl in ("A", "B", "oracle"):
            rows = [next(r for r in main if r["aging"] == policy and r["implementation"] == impl and r["revision_share_pct"] == p) for p in shares]
            series.append({"name": LABELS[impl], "color": COLORS[impl],
                           "means": [r["mean_time_ms"] for r in rows], "sds": [r["sd_time_ms"] for r in rows]})
        svg_line_chart(DOCS / f"D6_{policy}_revisions.svg", f"D6 total queue time by revision share - {policy} aging", shares, series)
    categories = ["Indexed heap A", "Lazy heap B", "Array oracle"]
    subset = [next(r for r in scale if r["implementation"] == impl) for impl in ("A", "B", "oracle")]
    svg_bar_chart(DOCS / "D6_million_patient_scale.svg", "D6 scale run · 1,000,000 live patients · 90% revisions (log y-axis)",
                  categories, [("Build heap", [r["mean_build_ms"] for r in subset], "#167D78"),
                               ("Revisions + 16 extracts", [r["mean_operation_ms"] for r in subset], "#E39B42")], "Mean time (ms, log scale)", logarithmic=True)


class GroupedBarChart(Flowable if HAVE_REPORTLAB else object):
    def __init__(self, title, categories, series, width=500, height=175, logarithmic=False):
        if HAVE_REPORTLAB:
            super().__init__()
        self.title, self.categories, self.series = title, categories, series
        self.width, self.height = width, height
        self.logarithmic = logarithmic

    def wrap(self, availWidth, availHeight):
        self.width = min(self.width, availWidth)
        return self.width, self.height

    def draw(self):
        if not HAVE_REPORTLAB:
            return
        c = self.canv
        left, right, top, bottom = 48, 16, 34, 56
        pw, ph = self.width - left - right, self.height - top - bottom
        raw_maximum = max(max(s["values"]) for s in self.series) or 1
        floor = 0.01
        minimum = math.log10(floor) if self.logarithmic else 0.0
        maximum = math.ceil(math.log10(raw_maximum * 1.22)) if self.logarithmic else raw_maximum * 1.22
        def plotted(value): return math.log10(max(floor, value)) if self.logarithmic else value
        c.setFillColor(colors.HexColor("#172B4D")); c.setFont("Helvetica-Bold", 12)
        c.drawCentredString(self.width/2, self.height-16, self.title)
        c.setStrokeColor(colors.HexColor("#D0D5DD")); c.setFillColor(colors.HexColor("#667085")); c.setFont("Helvetica", 7)
        tick_values = list(range(int(minimum), int(maximum) + 1)) if self.logarithmic else [maximum * i / 4 for i in range(5)]
        for i, value in enumerate(tick_values):
            y = bottom + ph * (value-minimum) / (maximum-minimum)
            c.line(left, y, left + pw, y)
            tick_label = f"1e{int(value)}" if self.logarithmic else f"{value:.0f}"
            c.drawRightString(left - 5, y - 2, tick_label)
        c.setStrokeColor(colors.HexColor("#667085")); c.line(left, bottom, left, bottom+ph); c.line(left, bottom, left+pw, bottom)
        group_w = pw / len(self.categories)
        bw = min(30, group_w * 0.68 / len(self.series))
        for ci, cat in enumerate(self.categories):
            center = left + group_w*(ci+0.5)
            c.setFillColor(colors.HexColor("#344054")); c.setFont("Helvetica", 8); c.drawCentredString(center, bottom-13, cat)
            for si, s in enumerate(self.series):
                x = center - len(self.series)*bw/2 + si*bw
                val = s["values"][ci]
                bh = ph * (plotted(val) - minimum) / (maximum - minimum)
                c.setFillColor(colors.HexColor(s["color"])); c.rect(x, bottom, bw-3, bh, stroke=0, fill=1)
                sd = s.get("sds", [0]*len(self.categories))[ci]
                low = max(floor, val-sd) if self.logarithmic else max(0, val-sd)
                high = val+sd
                ey1 = bottom + ph*(plotted(low)-minimum)/(maximum-minimum)
                ey2 = bottom + ph*(plotted(high)-minimum)/(maximum-minimum)
                c.setStrokeColor(colors.HexColor("#344054")); c.line(x+(bw-3)/2, ey1, x+(bw-3)/2, ey2)
                c.line(x+(bw-3)/2-3, ey1, x+(bw-3)/2+3, ey1); c.line(x+(bw-3)/2-3, ey2, x+(bw-3)/2+3, ey2)
                c.setFont("Helvetica", 6); c.drawCentredString(x+(bw-3)/2, min(bottom+ph-2, bottom+bh+4), f"{val:.2g}")
        legend_y = 14
        legend_width = self.width / len(self.series)
        for si, s in enumerate(self.series):
            x = si*legend_width + 16
            c.setFillColor(colors.HexColor(s["color"])); c.rect(x, legend_y, 9, 9, stroke=0, fill=1)
            c.setFillColor(colors.HexColor("#344054")); c.setFont("Helvetica", 7); c.drawString(x+13, legend_y+1, s["name"])


def make_pdf(main, scale):
    if not HAVE_REPORTLAB:
        print("ReportLab is missing; summaries and SVG figures were generated, but PDF regeneration was skipped. Install it with: py -3 -m pip install reportlab")
        return
    out = DOCS / "D6_Analyst.pdf"
    doc = SimpleDocTemplate(str(out), pagesize=letter, rightMargin=0.55*inch, leftMargin=0.55*inch,
                            topMargin=0.5*inch, bottomMargin=0.55*inch, title="D6 Analyst Report")
    styles = getSampleStyleSheet()
    styles.add(ParagraphStyle(name="Small", parent=styles["BodyText"], fontSize=8.2, leading=11, alignment=TA_LEFT))
    styles.add(ParagraphStyle(name="Sub", parent=styles["Heading2"], fontSize=13, leading=16, textColor=colors.HexColor("#172B4D"), spaceBefore=8, spaceAfter=5))
    story = [Paragraph("D6 - Analyst report", styles["Title"]),
             Paragraph("Emergency triage queue: cost model, repeated measurements and 10^6-scale check", styles["Heading2"]),
             Paragraph("<b>Purpose.</b> Compare indexed heap A, lazy heap B and the linear-scan array oracle under linear and step-wise aging. Report mean and sample standard deviation across repeated runs.", styles["BodyText"]), Spacer(1, 8),
             Paragraph("Cost model", styles["Sub"]),
             Paragraph("Let I, X, R, D and E count inserts, treatments, clinical revisions, departures and step-wise escalations. Let L be mean live queue size, P the peak live size and M the physical size of B's heap. The analyst's work proxies are: A ~= (I+X+R+D+E) log2(L+1); B ~= (I+X+R+D+E) log2(2P+2), plus stale cleanup amortized by compaction; oracle ~= I + XL + (R+D+E)L/2. Extract-min scans the array; random successful ID lookups average about half a scan. Linear aging has E=0 because its relative priority ordering stays fixed. Step-wise aging adds one update for each escalation.", styles["BodyText"]),
             Paragraph("Pre-measurement work model (q=1 for this trace): A_linear = c_build*I + (c_insert*I + c_extract*X + c_revision*q*R + c_delete*D) log2(L+1); A_step adds c_escalation*E*log2(L+1). For B, stale fraction f=S/M, M=P/(1-f) and compaction keeps f<=1/2; W_B = c_build*I + (c_insert*I + c_extract*(X+S) + c_revision*q*R) log2(M+1) + c_delete*D + c_compact*sum(C_j). Oracle work = c_insert*I + c_extract*X*L + (c_revision*q*R+c_delete*D+c_escalation*E)*L/2. The constants c are implementation-specific.", styles["Small"]),
             Paragraph("The work proxies have implementation-dependent constants. For the gap analysis, each implementation/policy constant is calibrated only on its 0% revision result; the model then predicts the other revision shares. This is a baseline-normalized model check, not a blind forecast.", styles["Small"]), Spacer(1, 6),
             Paragraph("Experimental setup", styles["Sub"]),
             Paragraph("The primary simulator replays 30 simulated days, with one arrival every 5 minutes, a treatment opportunity every 8 minutes and a departure opportunity every 120 minutes. It tests both aging policies and revision shares from 0% to 90%, using seeds 12345-12349. A separate scale check builds 1,000,000 live patients, applies 144 revisions and extracts 16 patients at a 90% revision share, using seeds 12350-12354. This isolates large-queue build and operation costs; it does not claim a million-patient clinical census.", styles["BodyText"]),
             Paragraph("Machine: " + CPU + ". Cache and core counts are the processor's published specifications; the run used the local Windows host, MinGW g++ C++17, -O2 -DNDEBUG and QueryPerformanceCounter. Source: " + CPU_SOURCE, styles["Small"]),
             Paragraph("Memory is process peak working set, so it includes simulator state and runtime overhead, not just the queue. Timings include queue operations and exclude trace generation. Per-operation means and SDs (microseconds per operation) are in the summary CSV.", styles["Small"]),
             PageBreak(), Paragraph("Repeated results and model gap", styles["Title"]),
             Paragraph("At 90% revisions: measured mean +/- sample SD, baseline-calibrated model prediction, and process peak working set.", styles["BodyText"]), Spacer(1, 6)]
    table_data = [[Paragraph("Policy", styles["Small"]), Paragraph("Queue", styles["Small"]),
                   Paragraph("Measured<br/>mean +/- SD (ms)", styles["Small"]),
                   Paragraph("Predicted<br/>(ms)", styles["Small"]),
                   Paragraph("Peak MB<br/>mean +/- SD", styles["Small"]), Paragraph("Gap", styles["Small"])]]
    for policy in ("linear", "stepwise"):
        for impl in ("A", "B", "oracle"):
            r = next(x for x in main if x["aging"] == policy and x["implementation"] == impl and x["revision_share_pct"] == 90)
            table_data.append([policy.title(), LABELS[impl], f"{r['mean_time_ms']:.2f} +/- {r['sd_time_ms']:.2f}",
                               f"{r['predicted_time_ms']:.2f}", f"{r['mean_peak_memory_mb']:.2f} +/- {r['sd_peak_memory_mb']:.2f}",
                               f"{r.get('measured_vs_predicted_gap_pct', 0):+.1f}%"])
    t = Table(table_data, colWidths=[0.75*inch, 1.1*inch, 1.45*inch, 0.95*inch, 1.1*inch, 0.6*inch], repeatRows=1)
    t.setStyle(TableStyle([("BACKGROUND",(0,0),(-1,0),colors.HexColor("#172B4D")),("TEXTCOLOR",(0,0),(-1,0),colors.white),
                           ("FONTNAME",(0,0),(-1,0),"Helvetica-Bold"),("FONTSIZE",(0,0),(-1,-1),8),
                           ("GRID",(0,0),(-1,-1),0.4,colors.HexColor("#D0D5DD")),
                           ("ROWBACKGROUNDS",(0,1),(-1,-1),[colors.white,colors.HexColor("#F2F4F7")]),
                           ("VALIGN",(0,0),(-1,-1),"MIDDLE"),("TOPPADDING",(0,0),(-1,-1),6),("BOTTOMPADDING",(0,0),(-1,-1),6)]))
    story.extend([t, Spacer(1, 8)])
    a90 = next(x for x in main if x["aging"] == "linear" and x["implementation"] == "A" and x["revision_share_pct"] == 90)
    b90 = next(x for x in main if x["aging"] == "linear" and x["implementation"] == "B" and x["revision_share_pct"] == 90)
    asw = next(x for x in main if x["aging"] == "stepwise" and x["implementation"] == "A" and x["revision_share_pct"] == 90)
    bsw = next(x for x in main if x["aging"] == "stepwise" and x["implementation"] == "B" and x["revision_share_pct"] == 90)
    sa = next(x for x in scale if x["implementation"] == "A")
    sb = next(x for x in scale if x["implementation"] == "B")
    so = next(x for x in scale if x["implementation"] == "oracle")
    story.append(Paragraph(f"Result: lazy heap B is fastest in the 30-day trace; its version table trades time for higher memory. A is slower than the oracle at these queue sizes. At 10^6 patients, A's measured build_heap mean is {sa['mean_build_ms']:.0f} ms, versus {sb['mean_build_ms']:.0f} ms for B and {so['mean_build_ms']:.1f} ms for the array copy. A's O(n) expected bound assumes expected constant-time locator operations; its measured constant is high. Code inspection points to string conversion during hashing and locator probes on heap swaps as likely contributors. Step-wise aging adds many key updates and increases elapsed time, especially for A.", styles["BodyText"]))
    story.append(Paragraph(f"At 90% linear-aging revisions, A measured {a90['mean_time_ms']:.1f} ms against the model's {a90['predicted_time_ms']:.1f} ms (gap {a90['measured_vs_predicted_gap_pct']:+.1f}%). The heap proxy charges every key update a full log-height sift; shorter observed sift paths are a plausible reason for the lower measured time, but the benchmark did not count sift levels. For the oracle, the model assumes a full scan for extract-min and half a scan for random successful ID lookup; observed positions and cache effects still change the constant. These baseline-normalized estimates explain scaling, not exact time.", styles["BodyText"]))
    story.extend([Spacer(1, 8), Paragraph("10^6-live-patient scale check", styles["Sub"]),
                  Paragraph("90% revision share: each run built one million entries, applied revisions, then extracted 16 minima. Five repetitions per implementation; values below are mean +/- sample SD.", styles["BodyText"]), Spacer(1, 5)])
    scale_data = [["Queue", "Build ms", "Revisions + extracts ms", "Total ms", "Peak MB"]]
    for impl in ("A", "B", "oracle"):
        r = next(x for x in scale if x["implementation"] == impl)
        scale_data.append([LABELS[impl], f"{r['mean_build_ms']:.1f} +/- {r['sd_build_ms']:.1f}",
                           f"{r['mean_operation_ms']:.3f} +/- {r['sd_operation_ms']:.3f}",
                           f"{r['mean_total_ms']:.1f} +/- {r['sd_total_ms']:.1f}",
                           f"{r['mean_peak_memory_mb']:.1f} +/- {r['sd_peak_memory_mb']:.1f}"])
    t2 = Table(scale_data, colWidths=[1.25*inch, 1.1*inch, 1.55*inch, 1.1*inch, 1.0*inch], repeatRows=1)
    t2.setStyle(TableStyle([("BACKGROUND",(0,0),(-1,0),colors.HexColor("#172B4D")),("TEXTCOLOR",(0,0),(-1,0),colors.white),
                            ("FONTNAME",(0,0),(-1,0),"Helvetica-Bold"),("FONTSIZE",(0,0),(-1,-1),8),
                            ("GRID",(0,0),(-1,-1),0.4,colors.HexColor("#D0D5DD")),
                            ("ROWBACKGROUNDS",(0,1),(-1,-1),[colors.white,colors.HexColor("#F2F4F7")]),
                            ("VALIGN",(0,0),(-1,-1),"MIDDLE"),("TOPPADDING",(0,0),(-1,-1),6),("BOTTOMPADDING",(0,0),(-1,-1),6)]))
    story.extend([t2, Spacer(1, 8), Paragraph("Interpretation and limits", styles["Sub"]),
                  Paragraph("Large-scale results must be read alongside operation mix: the scale check emphasizes build_heap and uses a fixed 16 extracts to keep the O(P) oracle revisions measurable. The 30-day simulator is the aging-policy comparison. Process memory includes the executable, input trace and simulator maps. Five repetitions improve spread estimates but do not represent diverse machines or real hospital arrivals.", styles["Small"]),
                  Paragraph("Published comparison: Chen et al. report that on fixed-average-degree G(n,m) graphs, their no-decrease-key Dijkstra implementations ran at least 1.4x faster than decrease-key implementations. This supports the direction seen here (lazy re-insertion B beats indexed decrease-key A), but their Dijkstra workload, heap variants and hardware differ, so the ratios are not directly comparable. Source: https://www3.cs.stonybrook.edu/~rezaul/papers/TR-07-54.pdf", styles["Small"]),
                  PageBreak(), Paragraph("Benchmark figures", styles["Title"]),
                  Paragraph("Bars show mean time. Error bars show +/- one sample standard deviation.", styles["Small"]), Spacer(1, 6)])
    for policy in ("linear", "stepwise"):
        subset = [next(x for x in main if x["aging"] == policy and x["implementation"] == impl and x["revision_share_pct"] == 90) for impl in ("A", "B", "oracle")]
        story.append(GroupedBarChart(f"90% revisions - {policy} aging (ms)", ["A", "B", "Oracle"], [
            {"name":"Queue-operation time", "values":[x["mean_time_ms"] for x in subset], "sds":[x["sd_time_ms"] for x in subset], "color":"#167D78"}
        ])); story.append(Spacer(1, 6))
    subset = [next(x for x in scale if x["implementation"] == impl) for impl in ("A", "B", "oracle")]
    story.append(GroupedBarChart("10^6 patients - build and operations (ms, log scale)", ["A", "B", "Oracle"], [
        {"name":"Build heap", "values":[x["mean_build_ms"] for x in subset], "sds":[x["sd_build_ms"] for x in subset], "color":"#167D78"},
        {"name":"Revisions + extracts", "values":[x["mean_operation_ms"] for x in subset], "sds":[x["sd_operation_ms"] for x in subset], "color":"#E39B42"}
    ], logarithmic=True))
    def footer(canvas, doc):
        canvas.saveState(); canvas.setFont("Helvetica", 8); canvas.setFillColor(colors.HexColor("#667085"))
        canvas.drawString(0.55*inch, 0.3*inch, "D6 Analyst - generated from five-run summaries")
        canvas.drawRightString(7.95*inch, 0.3*inch, f"Page {doc.page}"); canvas.restoreState()
    doc.build(story, onFirstPage=footer, onLaterPages=footer)


def make_markdown(main, scale):
    out = DOCS / "D6_Analyst.md"
    lines = ["# D6 — Analyst Report", "", "## Cost model", "",
             "For operation counts I (insert), X (extract), R (clinical revision), D (departure), and E (step-wise escalation), mean live queue size L, peak P and B's physical heap size M:", "",
             "Let `q` be the fraction of revisions that strictly improve priority (q = 1 in this trace). For B, let `f = stale/M`, so `M = P/(1-f)` and the compaction rule keeps `f <= 1/2` (hence `M <= 2P + 1`). Let `S` count stale roots discarded and `C_j` be the heap size scanned by compaction j.", "",
             "Pre-measurement work model:", "",
             "- A, linear: `W_A = c_build*I + (c_insert*I + c_extract*X + c_revision*q*R + c_delete*D) log2(L+1)`.",
             "- A, step-wise: `W_A_step = W_A_linear + c_escalation*E*log2(L+1)`.",
             "- B: `W_B = c_build*I + (c_insert*I + c_extract*(X+S) + c_revision*q*R) log2(P/(1-f)+1) + c_delete*D + c_compact*sum(C_j)`.",
             "- Oracle: `W_oracle = c_insert*I + c_extract*X*L + (c_revision*q*R + c_delete*D + c_escalation*E)*L/2`; successful ID lookups assume random positions.", "",
             "The `c` values represent implementation-dependent costs. The formulas were fixed before the measurements. To show predicted milliseconds, this report calibrates one constant per implementation and aging policy using only its 0% revision run, then predicts the other shares. This is a baseline-calibrated model check, not an independent forecast. Operation-level mean and SD (microseconds per operation) are also in `results/d6_summary.csv`.", "",
             "## Method", "",
             "The C++ replay covers 30 simulated days, with one arrival every 5 minutes, treatment opportunity every 8 minutes and departure opportunity every 120 minutes. It tests both aging policies and revision shares 0-90% with seeds 12345-12349. The separate 10^6 scale check builds one million live patients, applies 144 revisions and extracts 16 patients at 90% revision share, with seeds 12350-12354. That check isolates large-queue build and operation costs; it is not a million-patient clinical census.", "",
             f"Machine: {CPU}. [Intel processor specifications]({CPU_SOURCE}). Build: MinGW g++ C++17, `-O2 -DNDEBUG`; timing: Windows QueryPerformanceCounter. Peak process working set includes the queue, trace, simulator maps and runtime. Trace generation is excluded from timed queue operations.", "",
             "## Primary benchmark: 90% revision share", "",
             "Mean +/- sample SD across five runs. Memory is mean +/- sample SD of peak process working set. The model constant is calibrated at 0% revisions for each queue and policy.", "",
             "| Aging | Queue | Measured time (ms) | Predicted time (ms) | Peak memory (MB) | Gap |", "|:--|:--|--:|--:|--:|--:|"]
    for policy in ("linear", "stepwise"):
        for impl in ("A", "B", "oracle"):
            r = next(x for x in main if x["aging"] == policy and x["implementation"] == impl and x["revision_share_pct"] == 90)
            lines.append(f"| {policy.title()} | {LABELS[impl]} | {r['mean_time_ms']:.2f} +/- {r['sd_time_ms']:.2f} | {r['predicted_time_ms']:.2f} | {r['mean_peak_memory_mb']:.2f} +/- {r['sd_peak_memory_mb']:.2f} | {r.get('measured_vs_predicted_gap_pct', 0):+.1f}% |")
    lines += ["", "## 10^6-patient scale check", "", "Mean +/- sample SD across five runs at 90% revisions; each run builds one million patients, performs 144 revisions, then extracts 16 minima.", "", "| Queue | Build (ms) | Revisions + extracts (ms) | Total (ms) | Peak memory (MB) |", "|:--|--:|--:|--:|--:|"]
    for impl in ("A", "B", "oracle"):
        r = next(x for x in scale if x["implementation"] == impl)
        lines.append(f"| {LABELS[impl]} | {r['mean_build_ms']:.1f} +/- {r['sd_build_ms']:.1f} | {r['mean_operation_ms']:.3f} +/- {r['sd_operation_ms']:.3f} | {r['mean_total_ms']:.1f} +/- {r['sd_total_ms']:.1f} | {r['mean_peak_memory_mb']:.1f} +/- {r['sd_peak_memory_mb']:.1f} |")
    a90 = next(x for x in main if x["aging"] == "linear" and x["implementation"] == "A" and x["revision_share_pct"] == 90)
    b90 = next(x for x in main if x["aging"] == "linear" and x["implementation"] == "B" and x["revision_share_pct"] == 90)
    asw = next(x for x in main if x["aging"] == "stepwise" and x["implementation"] == "A" and x["revision_share_pct"] == 90)
    bsw = next(x for x in main if x["aging"] == "stepwise" and x["implementation"] == "B" and x["revision_share_pct"] == 90)
    sa = next(x for x in scale if x["implementation"] == "A")
    sb = next(x for x in scale if x["implementation"] == "B")
    so = next(x for x in scale if x["implementation"] == "oracle")
    lines += ["", "## Findings and limits", "", f"At 90% revisions, B was {a90['mean_time_ms']/b90['mean_time_ms']:.1f}x faster than A under linear aging and {asw['mean_time_ms']/bsw['mean_time_ms']:.1f}x under step-wise aging, while using more process memory. This agrees directionally with Chen et al.: on fixed-average-degree G(n,m) graphs, their no-decrease-key Dijkstra implementations ran at least 1.4x faster than decrease-key implementations. The workloads, queue variants and hardware differ, so the ratios are not directly comparable ([TR-07-54](https://www3.cs.stonybrook.edu/~rezaul/papers/TR-07-54.pdf)).", "", f"The baseline-calibrated model overpredicts 90%-revision time (A linear: {a90['predicted_time_ms']:.0f} ms predicted vs {a90['mean_time_ms']:.0f} ms measured; {a90['measured_vs_predicted_gap_pct']:+.1f}%). It charges a full-height sift per update; actual sift depths were not instrumented. At one million live entries, A's build averaged {sa['mean_build_ms']:.0f} ms, suggesting high constants in locator/hash work despite its expected O(n) bound. Scale test uses only 16 extracts; timing and process-wide memory are machine- and workload-specific. Results use five seeds.", "", "Raw runs: [d6_measurements.csv](../../results/d6_measurements.csv) and [d6_scale_measurements.csv](../../results/d6_scale_measurements.csv). Summaries: [d6_summary.csv](../../results/d6_summary.csv) and [d6_scale_summary.csv](../../results/d6_scale_summary.csv)."]
    out.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main():
    DOCS.mkdir(parents=True, exist_ok=True)
    main_rows = summarize_main()
    scale_rows = summarize_scale()
    make_figures(main_rows, scale_rows)
    make_markdown(main_rows, scale_rows)
    make_pdf(main_rows, scale_rows)
    print(f"Wrote {RESULTS / 'd6_summary.csv'}")
    print(f"Wrote {RESULTS / 'd6_scale_summary.csv'}")
    print(f"Wrote D6 SVG figures and Markdown report under {DOCS}")
    if HAVE_REPORTLAB:
        print(f"Wrote {DOCS / 'D6_Analyst.pdf'}")


if __name__ == "__main__":
    main()
