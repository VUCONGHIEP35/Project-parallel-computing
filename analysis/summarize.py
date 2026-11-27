#!/usr/bin/env python3
"""Tong hop CSV tho cua hist_bench thanh bang thong ke (KE_HOACH muc 3.4).

    python3 analysis/summarize.py results/raw/E2_*.csv > results/summary/E2.csv
    python3 analysis/summarize.py --metric t_compute results/raw/E4_*.csv
    python3 analysis/summarize.py --metric kernel results/raw/E5_*.csv     # anh: bo t_io, t_conv

Moi nhom (variant, n, bins, dist, threads) cho: so lan do, median/min/max/Q1/Q3/IQR/std cua
metric, median tung pha, speedup S = median(hist_seq) / median(phuong an) cung (n, bins, dist),
hieu suat E = S / threads, thong luong (trieu phan tu/s, GB/s doc du lieu 4 byte/phan tu).
Chi dung thu vien chuan cua Python 3.8+ (khong can numpy/pandas).
Dung lai neu CSV tron nhieu host (moi so lieu bao cao phai tu mot may do) hoac co dong ok = 0.
"""
import argparse
import csv
import statistics
import sys
from collections import defaultdict

PHASES = ("t_io", "t_conv", "t_init", "t_compute", "t_merge", "t_total")
OPTIONAL = {"t_conv": "0"}  # CSV cu (truoc khi tach t_conv) khong co cot nay: coi la 0
KEY = ("variant", "n", "bins", "dist", "threads")


def metric_value(row, metric):
    if metric == "kernel":  # chi phan cua phuong an, khong tinh doc file (t_io) va doi anh (t_conv)
        return float(row["t_init"]) + float(row["t_compute"]) + float(row["t_merge"])
    return float(row[metric])


def quartiles(xs):
    if len(xs) < 2:
        return xs[0], xs[0]
    q = statistics.quantiles(xs, n=4, method="inclusive")
    return q[0], q[2]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("csv", nargs="+", help="file CSV tho cua hist_bench")
    ap.add_argument("--metric", default="t_total", choices=PHASES + ("kernel",),
                    help="cot dung de tinh thong ke va speedup (mac dinh t_total = t_io + t_conv + "
                         "t_total cua ham; kernel = t_init + t_compute + t_merge)")
    ap.add_argument("--allow-mixed-hosts", action="store_true",
                    help="cho phep tron nhieu host (chi dung cho pilot)")
    args = ap.parse_args()

    groups = defaultdict(list)
    hosts, bad = set(), 0
    for path in args.csv:
        with open(path, newline="") as f:
            for row in csv.DictReader(f):
                for col, default in OPTIONAL.items():
                    if row.get(col) in (None, ""):
                        row[col] = default
                hosts.add(row["host"])
                if row["ok"] != "1":
                    bad += 1
                groups[tuple(row[k] for k in KEY)].append(row)

    if not groups:
        sys.exit("Khong co dong du lieu nao")
    if len(hosts) > 1 and not args.allow_mixed_hosts:
        sys.exit(f"CSV tron nhieu host {sorted(hosts)}: moi bang chi lay tu mot may do "
                 f"(them --allow-mixed-hosts neu la pilot)")
    if bad:
        sys.exit(f"Co {bad} dong ok = 0 (ket qua sai so voi hist_seq): sua ma truoc khi phan tich")

    stats = {}
    for key, rows in groups.items():
        xs = sorted(metric_value(r, args.metric) for r in rows)
        q1, q3 = quartiles(xs)
        s = {
            "runs": len(xs),
            "median": statistics.median(xs),
            "min": xs[0],
            "max": xs[-1],
            "q1": q1,
            "q3": q3,
            "iqr": q3 - q1,
            "std": statistics.stdev(xs) if len(xs) > 1 else 0.0,
        }
        for p in PHASES:
            s["med_" + p] = statistics.median(float(r[p]) for r in rows)
        stats[key] = s

    # Moc speedup: hist_seq cung (n, bins, dist)
    base = {(k[1], k[2], k[3]): s["median"] for k, s in stats.items() if k[0] == "hist_seq"}

    cols = list(KEY) + ["runs", "median", "min", "max", "q1", "q3", "iqr", "std"] + \
           ["med_" + p for p in PHASES] + ["speedup", "efficiency", "melem_s", "gbps"]
    w = csv.writer(sys.stdout, lineterminator="\n")
    w.writerow(cols)
    for key in sorted(stats, key=lambda k: (k[3], int(k[1]), int(k[2]), k[0], int(k[4]))):
        s = stats[key]
        n, threads = int(key[1]), int(key[4])
        t0 = base.get((key[1], key[2], key[3]))
        sp = t0 / s["median"] if t0 and s["median"] > 0 else None
        out = list(key) + [s["runs"]] + [f"{s[c]:.9f}" for c in cols[6:13]] + \
              [f"{s['med_' + p]:.9f}" for p in PHASES]
        out += [f"{sp:.3f}" if sp else "", f"{sp / threads:.3f}" if sp else "",
                f"{n / s['median'] / 1e6:.1f}" if s["median"] > 0 else "",
                f"{4 * n / s['median'] / 1e9:.3f}" if s["median"] > 0 else ""]
        w.writerow(out)

    print(f"metric = {args.metric}; {len(stats)} nhom; host = {', '.join(sorted(hosts))}",
          file=sys.stderr)
    if not base:
        print("Khong co hist_seq trong CSV -> cot speedup de trong", file=sys.stderr)


if __name__ == "__main__":
    main()
