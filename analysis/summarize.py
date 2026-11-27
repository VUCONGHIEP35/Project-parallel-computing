#!/usr/bin/env python3
"""Tong hop CSV tho cua hist_bench va dot_bench thanh bang thong ke (KE_HOACH muc 3.4).

    python3 analysis/summarize.py results/raw/E2_*.csv > results/summary/E2.csv
    python3 analysis/summarize.py --metric t_compute results/raw/E4_*.csv
    python3 analysis/summarize.py results/raw/E6_*.csv > results/summary/E6.csv

Moi nhom (variant, n, bins, dist, threads) cho: so lan do, median/min/max/Q1/Q3/IQR/std cua
metric, median tung pha, speedup S = median(ban tuan tu) / median(phuong an) cung (n, bins, dist),
hieu suat E = S / threads, thong luong (trieu phan tu/s, GB/s du lieu doc).
Ho phuong an lay tu tien to ten (truoc dau _ dau tien), quyet dinh moc speedup va so byte doc:
  hist_* : moc hist_seq, 4 byte/phan tu (1 float)
  dot_*  : moc dot_seq,  8 byte/phan tu (2 float: a[i], b[i])
Chi dung thu vien chuan cua Python 3.8+ (khong can numpy/pandas).
Dung lai neu CSV tron nhieu host (moi so lieu bao cao phai tu mot may do) hoac co dong ok = 0.
"""
import argparse
import csv
import statistics
import sys
from collections import defaultdict

PHASES = ("t_io", "t_init", "t_compute", "t_merge", "t_total")
KEY = ("variant", "n", "bins", "dist", "threads")
# So byte du lieu doc moi phan tu, theo ho phuong an. Ho khac: 4 (nhu truoc).
BYTES_PER_ELEM = {"hist": 4, "dot": 8}


def family(variant):
    """Ho phuong an: tien to truoc dau _ dau tien (hist_atomic -> hist, dot_partial -> dot)."""
    return variant.split("_", 1)[0]


def baseline(variant):
    """Phuong an tuan tu lam moc speedup cua cung ho (hist_seq, dot_seq)."""
    return family(variant) + "_seq"


def metric_value(row, metric):
    if metric == "kernel":  # chi phan cua phuong an, khong tinh doc file
        return float(row["t_init"]) + float(row["t_compute"]) + float(row["t_merge"])
    return float(row[metric])


def quartiles(xs):
    if len(xs) < 2:
        return xs[0], xs[0]
    q = statistics.quantiles(xs, n=4, method="inclusive")
    return q[0], q[2]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("csv", nargs="+", help="file CSV tho cua hist_bench hoac dot_bench")
    ap.add_argument("--metric", default="t_total", choices=PHASES + ("kernel",),
                    help="cot dung de tinh thong ke va speedup (mac dinh t_total; "
                         "kernel = t_init + t_compute + t_merge)")
    ap.add_argument("--allow-mixed-hosts", action="store_true",
                    help="cho phep tron nhieu host (chi dung cho pilot)")
    args = ap.parse_args()

    groups = defaultdict(list)
    hosts, bad = set(), 0
    for path in args.csv:
        with open(path, newline="") as f:
            for row in csv.DictReader(f):
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
        sys.exit(f"Co {bad} dong ok = 0 (ket qua sai so voi ban tuan tu hist_seq/dot_seq): "
                 f"sua ma truoc khi phan tich")

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

    # Moc speedup: ban tuan tu cung ho (hist_seq / dot_seq), cung (n, bins, dist)
    base = {(family(k[0]), k[1], k[2], k[3]): s["median"]
            for k, s in stats.items() if k[0] == baseline(k[0])}

    cols = list(KEY) + ["runs", "median", "min", "max", "q1", "q3", "iqr", "std"] + \
           ["med_" + p for p in PHASES] + ["speedup", "efficiency", "melem_s", "gbps"]
    w = csv.writer(sys.stdout, lineterminator="\n")
    w.writerow(cols)
    for key in sorted(stats, key=lambda k: (k[3], int(k[1]), int(k[2]), k[0], int(k[4]))):
        s = stats[key]
        n, threads = int(key[1]), int(key[4])
        t0 = base.get((family(key[0]), key[1], key[2], key[3]))
        sp = t0 / s["median"] if t0 and s["median"] > 0 else None
        nbytes = BYTES_PER_ELEM.get(family(key[0]), 4)
        out = list(key) + [s["runs"]] + [f"{s[c]:.9f}" for c in cols[6:13]] + \
              [f"{s['med_' + p]:.9f}" for p in PHASES]
        out += [f"{sp:.3f}" if sp else "", f"{sp / threads:.3f}" if sp else "",
                f"{n / s['median'] / 1e6:.1f}" if s["median"] > 0 else "",
                f"{nbytes * n / s['median'] / 1e9:.3f}" if s["median"] > 0 else ""]
        w.writerow(out)

    print(f"metric = {args.metric}; {len(stats)} nhom; host = {', '.join(sorted(hosts))}",
          file=sys.stderr)
    for fam in sorted({family(k[0]) for k in stats} - {b[0] for b in base}):
        print(f"Khong co {fam}_seq trong CSV -> cot speedup cua ho {fam} de trong", file=sys.stderr)


if __name__ == "__main__":
    main()
