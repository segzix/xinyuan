#!/usr/bin/python
#
# Copyright (c) 2026, VeriSilicon Holdings Co., Ltd. All rights reserved
#
# VeriHealthi Step Counter - Unified Simulation & Evaluation
# ===========================================================
# Default: batch evaluation with MAE/MAPE across all data files,
#          plus one representative visualization plot auto-saved.
#
# Options:
#   python step_counter.py                    # batch eval + 1 auto plot
#   python step_counter.py --no-plot          # batch eval only, no plot
#   python step_counter.py --plot <filepath>  # single file with plot
#   python step_counter.py --plot-all         # batch eval + plot every file
#   python step_counter.py --plot-file-index <N>   # plot file #N in walk set
#
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
import os, glob, re, sys

# ============================================================
# Constants
# ============================================================
ACC_FS      = 50
ACC_GRAVITY = 4096
MEAN_WIN    = 7
GYR_SMOOTH  = 25

GYR_STATIONARY   = 300
GYR_LOW_ACTIVE   = 600
GYR_HIGH_ACTIVE  = 1200
GYR_ACC_RATIO    = 5.0
ACC_MIN_STD      = 120
PEAK_AMP_MIN     = 200

INTERVALS = {
    'walk':  (15, 60),
    'brisk': (13, 45),
    'run':   (10, 35),
}

CV_WALK_LIMIT = 0.55
CV_RUN_LIMIT  = 0.85

PEAK_VALLEY_DIFFERENCE = ACC_GRAVITY // 14
VALLEY_TIME_MIN = 4
VALLEY_TIME_MAX = 40

MAX_STEP_FREQ_RATIO = 1.2

# ============================================================
# IO
# ============================================================

def read_imu_file(filepath):
    with open(filepath, 'r') as f:
        lines = f.readlines()
    data_start = 0
    for i, line in enumerate(lines):
        if 'TYPE' in line:
            data_start = i + 1
            break
    raw = np.array([float(x.strip()) for x in lines[data_start:] if x.strip()])
    n_samples = len(raw) // 7
    raw = raw[:n_samples * 7].reshape(n_samples, 7)
    return (raw[:, 3], raw[:, 4], raw[:, 5],
            raw[:, 0], raw[:, 1], raw[:, 2])

def extract_truth(filename):
    m = re.search(r'_step(\d+)', filename)
    return int(m.group(1)) if m else 0

# ============================================================
# Signal Processing
# ============================================================

def mean_filter(signal, window_size):
    n = len(signal)
    half = window_size // 2
    result = np.zeros(n, dtype=np.float64)
    for i in range(n):
        lo = max(0, i - half)
        hi = min(n, i + half + 1)
        result[i] = np.mean(signal[lo:hi])
    return result

def find_peaks_above_mean(signal):
    m = np.mean(signal)
    peaks = []
    for i in range(1, len(signal) - 1):
        if signal[i] >= signal[i-1] and signal[i] > signal[i+1] and signal[i] > m:
            peaks.append(i)
    return np.array(peaks, dtype=int)

def find_valleys_below_mean(signal):
    m = np.mean(signal)
    valleys = []
    for i in range(1, len(signal) - 1):
        if signal[i] <= signal[i-1] and signal[i] < signal[i+1] and signal[i] < m:
            valleys.append(i)
    return np.array(valleys, dtype=int)

def merge_close_peaks(peaks, valleys, signal):
    if len(peaks) <= 1:
        return peaks, valleys
    pl = list(peaks)
    vl = list(valleys)
    i = 1
    while i < len(pl):
        has_valley = False
        for j in range(pl[i-1] + 1, pl[i]):
            if j in vl:
                has_valley = True
                break
        if not has_valley:
            if signal[pl[i-1]] > signal[pl[i]]:
                pl.pop(i)
            else:
                pl.pop(i-1)
        else:
            i += 1
    return np.array(pl, dtype=int), np.array(vl, dtype=int)

def filter_peaks_by_interval(peaks, min_dist, max_dist):
    if len(peaks) == 0:
        return np.array([], dtype=int)
    valid = [peaks[0]]
    for i in range(1, len(peaks)):
        dist = peaks[i] - valid[-1]
        if min_dist <= dist <= max_dist:
            valid.append(peaks[i])
        elif dist > max_dist:
            valid.append(peaks[i])
    return np.array(valid, dtype=int)

def remove_asymmetric_peaks(peaks, valleys, signal):
    if len(valleys) <= 1:
        return np.array([], dtype=int)
    vl = list(valleys)
    pl_out = []
    for p_idx in peaks:
        p_val = signal[p_idx]
        for j in range(1, len(vl)):
            if vl[j-1] < p_idx < vl[j]:
                h1 = p_val - signal[vl[j-1]]
                h2 = p_val - signal[vl[j]]
                t1 = p_idx - vl[j-1]
                t2 = vl[j] - p_idx
                if (h1 > PEAK_VALLEY_DIFFERENCE and
                    h2 > PEAK_VALLEY_DIFFERENCE and
                    h1 > h2 / 2 and h1 < h2 * 2 and
                    t1 >= VALLEY_TIME_MIN and t1 <= VALLEY_TIME_MAX and
                    t2 >= VALLEY_TIME_MIN and t2 <= VALLEY_TIME_MAX):
                    pl_out.append(p_idx)
                break
    return np.array(pl_out, dtype=int)

# ============================================================
# Core Step Counter (returns debug info for plotting)
# ============================================================

def step_counter(acc_x, acc_y, acc_z, gyr_x, gyr_y, gyr_z,
                 return_debug=False):
    gyr_mag = np.sqrt(gyr_x**2 + gyr_y**2 + gyr_z**2)
    gyr_smooth = mean_filter(gyr_mag, GYR_SMOOTH)
    gyr_mean = np.mean(gyr_smooth)
    gyr_std = np.std(gyr_smooth)

    if gyr_mean < GYR_STATIONARY:
        return (0, None) if return_debug else 0

    acc_mag_raw = np.sqrt(acc_x**2 + acc_y**2 + acc_z**2)
    acc_raw_std = np.std(acc_mag_raw)
    if acc_raw_std > 0 and gyr_std > GYR_ACC_RATIO * acc_raw_std:
        return (0, None) if return_debug else 0

    if gyr_mean < GYR_LOW_ACTIVE:
        min_dist, max_dist = INTERVALS['walk']
        cv_limit = CV_WALK_LIMIT
        activity = 'walk'
    elif gyr_mean < GYR_HIGH_ACTIVE:
        min_dist, max_dist = INTERVALS['brisk']
        cv_limit = CV_RUN_LIMIT
        activity = 'brisk'
    else:
        min_dist, max_dist = INTERVALS['run']
        cv_limit = CV_RUN_LIMIT
        activity = 'run'

    acc_mag = acc_mag_raw
    acc_dc = acc_mag - np.mean(acc_mag)
    acc_filt = mean_filter(acc_dc, MEAN_WIN)
    acc_std = np.std(acc_filt)

    if acc_std < ACC_MIN_STD and gyr_mean < GYR_LOW_ACTIVE:
        return (0, None) if return_debug else 0

    peaks0 = find_peaks_above_mean(acc_filt)
    if len(peaks0) == 0:
        return (0, None) if return_debug else 0

    valleys0 = find_valleys_below_mean(acc_filt)
    if len(valleys0) <= 1:
        return (0, None) if return_debug else 0

    peaks1 = remove_asymmetric_peaks(peaks0, valleys0, acc_filt)
    if len(peaks1) == 0:
        return (0, None) if return_debug else 0

    peaks2, valleys2 = merge_close_peaks(peaks1, valleys0, acc_filt)
    peaks3 = filter_peaks_by_interval(peaks2, min_dist, max_dist)

    n_steps = len(peaks3)
    if n_steps == 0:
        return (0, None) if return_debug else 0

    peak_ampl = np.mean([acc_filt[p] for p in peaks3])
    if peak_ampl < PEAK_AMP_MIN and gyr_mean < GYR_LOW_ACTIVE + 200:
        return (0, None) if return_debug else 0

    signal_len = len(acc_filt)
    max_steps = int(signal_len / min_dist * MAX_STEP_FREQ_RATIO)
    if n_steps > max_steps:
        return (0, None) if return_debug else 0

    if n_steps >= 3:
        intervals = np.diff(peaks3)
        mean_iv = np.mean(intervals)
        cv = np.std(intervals) / mean_iv if mean_iv > 0 else 0.0
        if cv > cv_limit:
            penalty = min(1.0, cv_limit / cv)
            n_steps = max(0, int(n_steps * penalty * penalty))
    else:
        cv = 0.0

    if return_debug:
        debug = {
            'gyr_mag': gyr_mag,
            'gyr_smooth': gyr_smooth,
            'gyr_mean': gyr_mean,
            'gyr_std': gyr_std,
            'acc_mag_raw': acc_mag_raw,
            'acc_filt': acc_filt,
            'acc_std': acc_std,
            'peaks0': peaks0,
            'valleys0': valleys0,
            'peaks1': peaks1,
            'peaks2': peaks2,
            'valleys2': valleys2,
            'peaks3': peaks3,
            'activity': activity,
            'min_dist': min_dist,
            'max_dist': max_dist,
            'cv': cv,
            'cv_limit': cv_limit,
        }
        return n_steps, debug
    return n_steps

# ============================================================
# Evaluation
# ============================================================

def evaluate_directory(data_dir, verbose=True):
    results = []
    files = sorted(glob.glob(os.path.join(data_dir, '*.txt')))
    for fpath in files:
        fname = os.path.basename(fpath)
        truth = extract_truth(fname)
        acc_x, acc_y, acc_z, gyr_x, gyr_y, gyr_z = read_imu_file(fpath)
        pred = step_counter(acc_x, acc_y, acc_z, gyr_x, gyr_y, gyr_z)
        results.append((fname, truth, pred, fpath))
        if verbose:
            print("  %-50s  True=%3d  Pred=%3d  Err=%3d" % (
                fname, truth, pred, abs(pred - truth)))
    return results

def print_metrics(results, category):
    truths = [r[1] for r in results]
    preds = [r[2] for r in results]
    errors = [abs(p - t) for t, p in zip(truths, preds)]
    mae = np.mean(errors)
    if all(t == 0 for t in truths):
        fp = sum(1 for p in preds if p > 0)
        print("  MAE: %.2f, False Positives: %d/%d" % (mae, fp, len(results)))
    else:
        valid = [(t, p) for t, p in zip(truths, preds) if t > 0]
        mape = np.mean([abs(p - t) / t * 100 for t, p in valid]) if valid else 0
        print("  MAE: %.2f steps, MAPE: %.2f%%" % (mae, mape))

# ============================================================
# Visualization
# ============================================================

def plot_algorithm_stages(filepath, out_path):
    """Generate a 2x3 visualization of all algorithm stages and save to out_path."""
    fname = os.path.basename(filepath)
    acc_x, acc_y, acc_z, gyr_x, gyr_y, gyr_z = read_imu_file(filepath)
    n_steps, dbg = step_counter(acc_x, acc_y, acc_z, gyr_x, gyr_y, gyr_z,
                                 return_debug=True)

    if dbg is None:
        print("  [plot] %s: no steps detected, skipping plot" % fname)
        return

    fig, axes = plt.subplots(2, 3, figsize=(18, 10))
    fig.suptitle("%s  |  truth=%d  |  pred=%d steps  |  activity=%s" % (
        fname, extract_truth(fname), n_steps, dbg['activity']),
        fontsize=11, fontweight='bold')

    t = np.arange(len(dbg['acc_filt']))
    fs = ACC_FS

    # (0,0) Gyroscope analysis
    ax = axes[0, 0]
    ax.plot(dbg['gyr_mag'], alpha=0.3, color='gray', label='gyro raw')
    ax.plot(dbg['gyr_smooth'], color='purple', label='gyro smooth')
    ax.axhline(GYR_STATIONARY, color='green', ls='--', alpha=0.6, label='stationary')
    ax.axhline(GYR_LOW_ACTIVE, color='orange', ls='--', alpha=0.6, label='walk/brisk')
    ax.axhline(GYR_HIGH_ACTIVE, color='red', ls='--', alpha=0.6, label='run')
    ax.set_title("Gyroscope Activity | mean=%.0f std=%.0f" % (dbg['gyr_mean'], dbg['gyr_std']))
    ax.set_xlabel("sample"); ax.set_ylabel("gyro magnitude"); ax.legend(fontsize=7)
    ax.grid(True, alpha=0.4)

    # (0,1) Raw vs filtered acceleration magnitude
    ax = axes[0, 1]
    ax.plot(dbg['acc_mag_raw'], alpha=0.3, color='gray', label='raw acc mag')
    ax.plot(dbg['acc_filt'], color='red', linewidth=1.2, label='filtered')
    ax.axhline(0, color='black', ls='-', alpha=0.3)
    ax.set_title("Acc Magnitude (DC-removed + mean filter %d)" % MEAN_WIN)
    ax.set_xlabel("sample"); ax.set_ylabel("acc magnitude"); ax.legend(fontsize=7)
    ax.grid(True, alpha=0.4)

    # (0,2) Preliminary peak detection
    ax = axes[0, 2]
    ax.plot(dbg['acc_filt'], color='red', linewidth=1.0, alpha=0.6)
    if len(dbg['peaks0']) > 0:
        ax.plot(dbg['peaks0'], dbg['acc_filt'][dbg['peaks0']], 'go', ms=4, label='peaks')
    if len(dbg['valleys0']) > 0:
        ax.plot(dbg['valleys0'], dbg['acc_filt'][dbg['valleys0']], 'bo', ms=4, label='valleys')
    ax.axhline(np.mean(dbg['acc_filt']), color='gray', ls=':', alpha=0.5)
    ax.set_title("Preliminary Peaks/Valleys | %d peaks" % len(dbg['peaks0']))
    ax.set_xlabel("sample"); ax.set_ylabel("acc magnitude"); ax.legend(fontsize=7)
    ax.grid(True, alpha=0.4)

    # (1,0) After asymmetric verification
    ax = axes[1, 0]
    ax.plot(dbg['acc_filt'], color='red', linewidth=1.0, alpha=0.6)
    if len(dbg['peaks1']) > 0:
        ax.plot(dbg['peaks1'], dbg['acc_filt'][dbg['peaks1']], 'go', ms=5, label='verified peaks')
    if len(dbg['valleys0']) > 0:
        ax.plot(dbg['valleys0'], dbg['acc_filt'][dbg['valleys0']], 'bo', ms=3, alpha=0.4,
                label='valleys')
    ax.set_title("After Asymmetric Verification | %d peaks" % len(dbg['peaks1']))
    ax.set_xlabel("sample"); ax.set_ylabel("acc magnitude"); ax.legend(fontsize=7)
    ax.grid(True, alpha=0.4)

    # (1,1) After merge close peaks + interval filter
    ax = axes[1, 1]
    ax.plot(dbg['acc_filt'], color='red', linewidth=1.0, alpha=0.6)
    if len(dbg['peaks3']) > 0:
        ax.plot(dbg['peaks3'], dbg['acc_filt'][dbg['peaks3']], 'gs', ms=7, mfc='none',
                label='final peaks (%d)' % len(dbg['peaks3']))
    if len(dbg['valleys2']) > 0:
        ax.plot(dbg['valleys2'], dbg['acc_filt'][dbg['valleys2']], 'bo', ms=3, alpha=0.3)
    # Show interval constraints
    ax.axhline(0, color='black', ls='-', alpha=0.3)
    ax.set_title("After Merge + Interval Filter [%d,%d] | %d steps" % (
        dbg['min_dist'], dbg['max_dist'], len(dbg['peaks3'])))
    ax.set_xlabel("sample"); ax.set_ylabel("acc magnitude"); ax.legend(fontsize=7)
    ax.grid(True, alpha=0.4)

    # (1,2) Summary statistics
    ax = axes[1, 2]
    ax.axis('off')
    summary_lines = [
        "File: %s" % fname,
        "True steps: %d" % extract_truth(fname),
        "Pred steps: %d" % n_steps,
        "Error: %d" % abs(extract_truth(fname) - n_steps),
        "",
        "Activity: %s" % dbg['activity'],
        "Gyro mean: %.0f  std: %.0f" % (dbg['gyr_mean'], dbg['gyr_std']),
        "Acc std: %.0f" % dbg['acc_std'],
        "",
        "Peaks pipeline:",
        "  raw peaks: %d" % len(dbg['peaks0']),
        "  after asymmetric: %d" % len(dbg['peaks1']),
        "  after merge: %d" % len(dbg['peaks2']),
        "  after interval: %d" % len(dbg['peaks3']),
        "",
        "Interval bounds: [%d, %d]" % (dbg['min_dist'], dbg['max_dist']),
        "CV: %.3f  (limit %.2f)" % (dbg['cv'], dbg['cv_limit']),
    ]
    for i, line in enumerate(summary_lines):
        ax.text(0.05, 0.95 - i * 0.045, line, transform=ax.transAxes,
                fontsize=9, family='monospace', verticalalignment='top')

    plt.tight_layout(rect=[0, 0, 1, 0.95])
    plt.savefig(out_path, dpi=120, bbox_inches='tight')
    plt.close()
    print("  [plot] saved: %s" % out_path)

def auto_pick_plot_file(results, category='walk'):
    """Pick the walk file closest to median |error|."""
    walk_results = [(r[0], r[1], r[2], r[3], abs(r[2] - r[1]))
                    for r in results if r[1] > 0 and 'walk' in os.path.dirname(r[3])]
    if not walk_results:
        return None
    walk_results.sort(key=lambda x: x[4])
    mid = len(walk_results) // 2
    fname, truth, pred, fpath, err = walk_results[mid]
    return fpath

# ============================================================
# Main
# ============================================================

def main():
    base_dir = os.path.dirname(os.path.abspath(__file__))
    accdata_dir = os.path.join(base_dir, '..', '..', 'AccData')
    plot_dir = os.path.join(base_dir, '..', 'plots')
    os.makedirs(plot_dir, exist_ok=True)

    args = sys.argv[1:]

    # --- Single-file plot mode ---
    if '--plot' in args and '--plot-all' not in args:
        idx = args.index('--plot')
        if idx + 1 < len(args):
            filepath = args[idx + 1]
        else:
            print("Usage: python step_counter.py --plot <filepath>")
            sys.exit(1)
        fname = os.path.basename(filepath)
        truth = extract_truth(fname)
        out_path = os.path.join(plot_dir, fname.replace('.txt', '.png'))
        plot_algorithm_stages(filepath, out_path)
        print("  Truth=%d  ->  see plot: %s" % (truth, out_path))
        return

    # --- Single file by index ---
    if '--plot-file-index' in args:
        idx = args.index('--plot-file-index')
        if idx + 1 < len(args):
            n = int(args[idx + 1])
        else:
            n = 0
        walk_dir = os.path.join(accdata_dir, 'walk')
        files = sorted(glob.glob(os.path.join(walk_dir, '*.txt')))
        if 0 <= n < len(files):
            filepath = files[n]
            fname = os.path.basename(filepath)
            out_path = os.path.join(plot_dir, fname.replace('.txt', '.png'))
            plot_algorithm_stages(filepath, out_path)
            print("  Truth=%d  ->  see plot: %s" % (extract_truth(fname), out_path))
        else:
            print("Invalid index %d, walk/ has %d files" % (n, len(files)))
        return

    # --- Batch evaluation ---
    print("=" * 80)
    print("  VeriHealthi Step Counter - Evaluation Report")
    print("=" * 80)
    print()
    print("  Algorithm: Magnitude Peak Counter + Gyro Activity Gating")
    print("  Features:")
    print("    - Gyroscope activity classification (stationary/walk/run)")
    print("    - Asymmetric peak verification (depth + symmetry)")
    print("    - Chain-restart interval filter")
    print("    - Adaptive interval bounds (walk/brisk/run)")
    print("    - Peak density sanity check")
    print("    - Soft periodicity penalty (CV-based)")
    print()

    all_results = []

    for category, subdir in [('Walking', 'walk'), ('Running', 'run'),
                              ('Non-walking', 'others')]:
        path = os.path.join(accdata_dir, subdir)
        if not os.path.isdir(path):
            continue
        verbose = (category != 'Non-walking')
        results = evaluate_directory(path, verbose=verbose)
        all_results.extend(results)

        if category == 'Non-walking':
            print("\n--- %s ---" % category)
            print_metrics(results, category)
            fp_files = [(r[0], r[2]) for r in results if r[2] > 0]
            if fp_files:
                print("  False Positive files:")
                for fn, pr in fp_files:
                    print("    %s: predicted %d steps" % (fn, pr))
        else:
            print("\n--- %s ---" % category)
            print_metrics(results, category)

    all_truths = [r[1] for r in all_results]
    all_preds = [r[2] for r in all_results]
    all_errors = [abs(p - t) for t, p in zip(all_truths, all_preds)]
    overall_mae = np.mean(all_errors)
    walk_run_pairs = [(t, p) for t, p in zip(all_truths, all_preds) if t > 0]
    overall_mape = (np.mean([abs(p - t) / t * 100 for t, p in walk_run_pairs])
                    if walk_run_pairs else 0.0)
    fp_count = sum(1 for t, p in zip(all_truths, all_preds) if t == 0 and p > 0)
    noise_count = sum(1 for t in all_truths if t == 0)

    print("\n" + "=" * 80)
    print("  OVERALL RESULTS")
    print("=" * 80)
    print("  Total files tested: %d" % len(all_results))
    print("  Overall MAE: %.2f steps" % overall_mae)
    print("  Overall MAPE (walk+run): %.2f%%" % overall_mape)
    if noise_count > 0:
        print("  False positive rate (noise): %d/%d (%.1f%%)" % (
            fp_count, noise_count, fp_count / noise_count * 100))
    print("=" * 80)

    # --- Auto-generate one representative plot ---
    if '--no-plot' not in args:
        plot_all = ('--plot-all' in args)
        print("\n--- Generating visualization plot(s) ---")
        if plot_all:
            for r in all_results:
                fpath = r[3]
                fname = os.path.basename(fpath)
                out_path = os.path.join(plot_dir, fname.replace('.txt', '.png'))
                plot_algorithm_stages(fpath, out_path)
        else:
            representative = auto_pick_plot_file(all_results)
            if representative:
                fname = os.path.basename(representative)
                out_path = os.path.join(plot_dir, 'representative_' +
                                        fname.replace('.txt', '.png'))
                plot_algorithm_stages(representative, out_path)
                print("  Representative walk file: %s" % fname)
        print("  All plots saved to: %s/" % plot_dir)

if __name__ == "__main__":
    main()
