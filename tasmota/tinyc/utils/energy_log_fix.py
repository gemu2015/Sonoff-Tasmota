#!/usr/bin/env python3
# energy_log_fix.py — clean + gap-fill + rebuild-index for the energy_logger files.
#
# For a tab-separated log (TSTAMP<TAB>cols..., German "D.M.YY H:MM" timestamps):
#   1. drop malformed + all-zero rows
#   2. drop near-duplicate rows (< MIN_GAP_MIN after the previous kept row) — this
#      removes exact-dup timestamps AND rapid on-demand/test rows
#   3. PRESERVE FILE ORDER (never sort — at DST fall-back local 02:00-03:00 repeats,
#      so the append order is the true chronology)
#   4. linear-interpolate genuine gaps (> GAP_THRESH_MIN) — cumulative counters get
#      the right distribution; DST spring-forward gaps (the vanished 02:00-03:00 hr)
#      are left alone
#   5. rebuild the .ind index (TSTAMP<TAB>byteoffset) consistent with the cleaned .txt
#
# Usage: energy_log_fix.py <file.txt> [--daily]
# Writes <file>.clean and <base>.ind.clean, prints a before/after report, and a
# <base>.synthetic.txt list of the interpolated timestamps (so they stay identifiable).

import sys, datetime as dt

MIN_GAP_MIN    = 5      # < this many min after previous kept row -> drop (dup/test)
GAP_THRESH_15  = 20     # 15-min file: > this many min between rows = a gap to fill
GAP_THRESH_DAY = 36*60  # daily file: > this many min (36 h) = missing day(s)
SLOT_MIN       = 15

def parse_ts(s):
    try:
        d, t = s.split(" ")
        dd, mm, yy = d.split(".")
        H, M = t.split(":")
        return dt.datetime(2000 + int(yy), int(mm), int(dd), int(H), int(M))
    except Exception:
        return None

def fmt_ts(x):
    return f"{x.day}.{x.month}.{x.year % 100} {x.hour}:{x.minute:02d}"

def is_dst_spring(prev, nxt):
    # spring-forward: late March, 02:00 -> 03:00 local; the 02:xx hour does not exist
    return (prev.month == 3 and nxt.month == 3 and nxt.hour == 3
            and prev.hour in (1, 2) and (nxt - prev).total_seconds() <= 90 * 60)

ZEROS = {"0", "0.0", "0.00", "0.000", "0.0000", "-0.0", "-0.000"}
def all_zero(cols):
    vs = cols[1:]
    return len(vs) > 0 and all(v.strip() in ZEROS for v in vs)

def process(txt_path, is_daily):
    with open(txt_path, errors="replace") as f:
        raw = f.read().split("\n")
    header = raw[0] if raw and raw[0].startswith("TSTAMP") else None
    body = raw[1:] if header else raw

    st = dict(read=0, bad=0, zero=0, dup=0, gaps=0, dst_skip=0, synth=0)

    # parse + drop malformed / all-zero
    rows = []
    for line in body:
        if not line.strip():
            continue
        cols = line.split("\t")
        ts = parse_ts(cols[0])
        if ts is None:
            st['bad'] += 1; continue
        st['read'] += 1
        if all_zero(cols):
            st['zero'] += 1; continue
        rows.append((ts, cols))

    # drop near-dups (preserve order)
    kept = []
    for ts, cols in rows:
        if kept:
            d = (ts - kept[-1][0]).total_seconds() / 60.0
            if 0 <= d < MIN_GAP_MIN:
                st['dup'] += 1; continue
        kept.append((ts, cols))

    # gap-fill by linear interpolation
    out = []
    synth = []
    step = dt.timedelta(days=1) if is_daily else dt.timedelta(minutes=SLOT_MIN)
    gap_thresh = GAP_THRESH_DAY if is_daily else GAP_THRESH_15
    for ts, cols in kept:
        if out:
            p_ts, p_cols = out[-1]
            d = (ts - p_ts).total_seconds() / 60.0
            if d > gap_thresh:
                if (not is_daily) and is_dst_spring(p_ts, ts):
                    st['dst_skip'] += 1
                else:
                    st['gaps'] += 1
                    if is_daily:
                        t = p_ts + step
                    else:
                        base = p_ts.replace(second=0, microsecond=0)
                        addm = (15 - base.minute % 15) % 15 or 15
                        t = base + dt.timedelta(minutes=addm)
                    span = (ts - p_ts).total_seconds()
                    ncol = min(len(p_cols), len(cols))
                    while t < ts - dt.timedelta(minutes=1):
                        frac = (t - p_ts).total_seconds() / span
                        new = [fmt_ts(t)]
                        for j in range(1, ncol):
                            try:
                                a = float(p_cols[j]); b = float(cols[j])
                                new.append(f"{a + (b - a) * frac:.3f}")
                            except Exception:
                                new.append(p_cols[j])
                        out.append((t, new)); synth.append(fmt_ts(t)); st['synth'] += 1
                        t += step
        out.append((ts, cols))

    # write cleaned .txt + rebuilt .ind (offsets consistent with cleaned .txt)
    base = txt_path[:-4] if txt_path.endswith(".txt") else txt_path
    out_txt, out_ind, out_syn = txt_path + ".clean", base + ".ind.clean", base + ".synthetic.txt"
    off = 0
    with open(out_txt, "w") as ft, open(out_ind, "w") as fi:
        if header:
            hl = header + "\n"; ft.write(hl); off += len(hl.encode())
        for ts, cols in out:
            row = "\t".join(cols) + "\n"
            fi.write(f"{cols[0]}\t{off}\n")   # index ts == the row's own timestamp string
            ft.write(row); off += len(row.encode())
    with open(out_syn, "w") as fs:
        fs.write("\n".join(synth) + ("\n" if synth else ""))

    span = f"{fmt_ts(out[0][0])} .. {fmt_ts(out[-1][0])}" if out else "-"
    print(f"\n== {txt_path}  ({'daily' if is_daily else '15-min'}) ==")
    print(f"  read={st['read']}  dropped: malformed={st['bad']} all_zero={st['zero']} near_dup={st['dup']}")
    print(f"  gaps filled={st['gaps']}  synthetic rows added={st['synth']}  DST-spring gaps left alone={st['dst_skip']}")
    print(f"  OUTPUT rows={len(out)}  span {span}")
    print(f"  -> {out_txt}\n  -> {out_ind}\n  -> {out_syn} ({len(synth)} synthetic ts)")
    return st

if __name__ == "__main__":
    p = sys.argv[1]
    process(p, "--daily" in sys.argv)
