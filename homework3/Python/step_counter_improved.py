#!/usr/bin/python
# VeriHealthi Improved Step Counter v2
import numpy as np
import os, glob, re

ACC_GRAVITY = 4096
MEAN_WIN = 7
GYR_SMOOTH = 25
GYR_STATIONARY = 300
GYR_LOW_ACTIVE = 600
GYR_HIGH_ACTIVE = 1200
GYR_ACC_RATIO = 5.0
ACC_MIN_STD = 120
PEAK_AMP_MIN = 200
GYR_CV_SOFT = 0.75
RAW_PEAK_DENSITY_MAX = 0.12

INTERVALS = {'walk': (15, 60), 'brisk': (13, 45), 'run': (10, 35)}
CV_WALK_LIMIT = 0.55
CV_RUN_LIMIT = 0.85

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
    return (raw[:, 3], raw[:, 4], raw[:, 5], raw[:, 0], raw[:, 1], raw[:, 2])

def extract_truth(filename):
    m = re.search(r'_step(\d+)', filename)
    return int(m.group(1)) if m else 0

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
        return peaks
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
    return np.array(pl, dtype=int)

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

def step_counter(acc_x, acc_y, acc_z, gyr_x, gyr_y, gyr_z):
    gyr_mag = np.sqrt(gyr_x**2 + gyr_y**2 + gyr_z**2)
    gyr_smooth = mean_filter(gyr_mag, GYR_SMOOTH)
    gyr_mean = np.mean(gyr_smooth)
    gyr_std = np.std(gyr_smooth)
    gyr_cv = gyr_std / gyr_mean if gyr_mean > 0 else 0.0

    if gyr_mean < GYR_STATIONARY:
        return 0

    acc_mag_raw = np.sqrt(acc_x**2 + acc_y**2 + acc_z**2)
    acc_raw_std = np.std(acc_mag_raw)
    if acc_raw_std > 0 and gyr_std > GYR_ACC_RATIO * acc_raw_std:
        return 0

    if len(acc_mag_raw) > 500:
        unflt_mag = acc_mag_raw - np.mean(acc_mag_raw)
        raw_peaks = find_peaks_above_mean(unflt_mag)
        raw_density = len(raw_peaks) / len(acc_mag_raw) if len(acc_mag_raw) > 0 else 0
        if raw_density > RAW_PEAK_DENSITY_MAX:
            return 0

    if gyr_mean < GYR_LOW_ACTIVE:
        min_dist, max_dist = INTERVALS['walk']
        cv_limit = CV_WALK_LIMIT
    elif gyr_mean < GYR_HIGH_ACTIVE:
        min_dist, max_dist = INTERVALS['brisk']
        cv_limit = CV_RUN_LIMIT
    else:
        min_dist, max_dist = INTERVALS['run']
        cv_limit = CV_RUN_LIMIT

    acc_dc = acc_mag_raw - np.mean(acc_mag_raw)
    acc_filt = mean_filter(acc_dc, MEAN_WIN)

    acc_std = np.std(acc_filt)
    if acc_std < ACC_MIN_STD and gyr_mean < GYR_LOW_ACTIVE:
        return 0

    peaks = find_peaks_above_mean(acc_filt)
    if len(peaks) == 0:
        return 0

    valleys = find_valleys_below_mean(acc_filt)
    peaks = merge_close_peaks(peaks, valleys, acc_filt)
    peaks = filter_peaks_by_interval(peaks, min_dist, max_dist)

    n_steps = len(peaks)
    if n_steps == 0:
        return 0

    peak_ampl = np.mean([acc_filt[p] for p in peaks])
    if peak_ampl < PEAK_AMP_MIN and gyr_mean < GYR_LOW_ACTIVE + 200:
        return 0

    if gyr_mean < GYR_HIGH_ACTIVE and gyr_cv > GYR_CV_SOFT:
        n_steps = int(n_steps * min(1.0, GYR_CV_SOFT / gyr_cv))
        if n_steps == 0:
            return 0

    if n_steps >= 3:
        intervals = np.diff(peaks)
        mean_iv = np.mean(intervals)
        cv = np.std(intervals) / mean_iv if mean_iv > 0 else 0.0
        if cv > cv_limit:
            penalty = min(1.0, cv_limit / cv)
            n_steps = int(n_steps * penalty * penalty)
            if n_steps == 0:
                return 0

    return n_steps

def evaluate_directory(data_dir, verbose=True):
    results = []
    files = sorted(glob.glob(os.path.join(data_dir, '*.txt')))
    for fpath in files:
        fname = os.path.basename(fpath)
        truth = extract_truth(fname)
        acc_x, acc_y, acc_z, gyr_x, gyr_y, gyr_z = read_imu_file(fpath)
        pred = step_counter(acc_x, acc_y, acc_z, gyr_x, gyr_y, gyr_z)
        results.append((fname, truth, pred))
        if verbose:
            print(f"  {fname:50s}  True={truth:3d}  Pred={pred:3d}  Err={abs(pred-truth):3d}")
    return results

def print_metrics(results, category):
    truths = [r[1] for r in results]
    preds = [r[2] for r in results]
    errors = [abs(p - t) for t, p in zip(truths, preds)]
    mae = np.mean(errors)
    if all(t == 0 for t in truths):
        fp = sum(1 for p in preds if p > 0)
        print(f"  MAE: {mae:.2f}, False Positives: {fp}/{len(results)}")
    else:
        valid = [(t, p) for t, p in zip(truths, preds) if t > 0]
        mape = np.mean([abs(p - t) / t * 100 for t, p in valid]) if valid else 0
        print(f"  MAE: {mae:.2f} steps, MAPE: {mape:.2f}%")

def main():
    base_dir = os.path.dirname(os.path.abspath(__file__))
    accdata_dir = os.path.join(base_dir, '..', '..', 'AccData')
    print("=" * 80)
    print("  Step Counter v2 - Evaluation Report")
    print("=" * 80)
    all_results = []
    for category, subdir in [('Walking', 'walk'), ('Running', 'run'), ('Non-walking', 'others')]:
        path = os.path.join(accdata_dir, subdir)
        if not os.path.isdir(path):
            continue
        verbose = (category != 'Non-walking')
        results = evaluate_directory(path, verbose=verbose)
        all_results.extend(results)
        if category == 'Non-walking':
            print(f"\n--- {category} ---")
            print_metrics(results, category)
            fp_files = [(r[0], r[2]) for r in results if r[2] > 0]
            if fp_files:
                print("  False Positive files:")
                for fn, pr in fp_files:
                    print(f"    {fn}: predicted {pr} steps")
        else:
            print(f"\n--- {category} ---")
            print_metrics(results, category)
    all_truths = [r[1] for r in all_results]
    all_preds = [r[2] for r in all_results]
    overall_mae = np.mean([abs(p - t) for t, p in zip(all_truths, all_preds)])
    walk_run_pairs = [(t, p) for t, p in zip(all_truths, all_preds) if t > 0]
    overall_mape = np.mean([abs(p - t) / t * 100 for t, p in walk_run_pairs]) if walk_run_pairs else 0
    fp_count = sum(1 for t, p in zip(all_truths, all_preds) if t == 0 and p > 0)
    noise_count = sum(1 for t in all_truths if t == 0)
    print(f"\n{'=' * 80}")
    print(f"  OVERALL RESULTS")
    print(f"{'=' * 80}")
    print(f"  Total files: {len(all_results)}")
    print(f"  Overall MAE: {overall_mae:.2f} steps")
    print(f"  Overall MAPE (walk+run): {overall_mape:.2f}%")
    print(f"  False positive rate (noise): {fp_count}/{noise_count} ({fp_count/noise_count*100:.1f}%)" if noise_count > 0 else "")

if __name__ == "__main__":
    main()
