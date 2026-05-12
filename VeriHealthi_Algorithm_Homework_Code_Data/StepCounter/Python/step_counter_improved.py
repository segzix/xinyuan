#!/usr/bin/python
#
# VeriHealthi Improved Step Counter
# ===================================
# Adaptive interval-constrained peak counter with gyroscope-based
# activity classification.
#
# Algorithm Pipeline:
#   1. Compute acceleration magnitude (orientation-independent)
#   2. DC removal + mean filter
#   3. Peak detection above mean
#   4. Merge close peaks, interval filtering
#   5. Gyroscope verification (activity classification)
#   6. Periodicity check
#
# Key innovations over original:
#   - Acceleration magnitude (axis-independent)
#   - Gyroscope-based noise rejection
#   - Adaptive thresholds based on signal statistics
#   - Periodicity verification
#
import numpy as np
import os, glob, re, sys

# ============================================================
# Constants
# ============================================================
ACC_FS      = 50
ACC_GRAVITY = 4096
MEAN_WIN    = 7          # smoothing window size
GYR_SMOOTH  = 25         # gyroscope smoothing (0.5s)
MIN_PK_DIST = 15         # min peak distance (0.3s = 3.3Hz max)
MAX_PK_DIST = 100        # max peak distance (2s = 0.5Hz min)

# Activity classification thresholds
GYR_STATIONARY   = 300   # below this: stationary
GYR_LOW_ACTIVE   = 600   # low activity threshold
GYR_HIGH_ACTIVE  = 1200  # high activity threshold
GYR_ACC_RATIO    = 3.0   # gyro/acc ratio for non-walking
ACC_MIN_STD      = 150   # minimum acc std for walking
PEAK_AMP_MIN     = 200   # minimum peak amplitude

# Periodicity thresholds (coefficient of variation)
CV_LOW_GYR       = 0.5   # strict regularity for low gyro
CV_MED_GYR       = 0.7   # medium regularity
CV_HIGH_GYR      = 0.8   # relaxed regularity for high gyro


# ============================================================
# IO Functions
# ============================================================

def read_imu_file(filepath):
    """Read an IMU data file.
    
    File format: 5 header lines + 7-channel data (gx,gy,gz,ax,ay,az,debug)
    Returns: acc_x, acc_y, acc_z, gyr_x, gyr_y, gyr_z arrays
    """
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
    """Extract true step count from filename."""
    m = re.search(r'_step(\d+)', filename)
    return int(m.group(1)) if m else 0


# ============================================================
# Signal Processing
# ============================================================

def mean_filter(signal, window_size):
    """Symmetric moving average filter."""
    n = len(signal)
    half = window_size // 2
    result = np.zeros(n, dtype=np.float64)
    for i in range(n):
        lo = max(0, i - half)
        hi = min(n, i + half + 1)
        result[i] = np.mean(signal[lo:hi])
    return result


def find_peaks_above_mean(signal):
    """Find all local maxima above the signal mean."""
    m = np.mean(signal)
    peaks = []
    for i in range(1, len(signal) - 1):
        if signal[i] >= signal[i-1] and signal[i] > signal[i+1] and signal[i] > m:
            peaks.append(i)
    return np.array(peaks, dtype=int)


def find_valleys_below_mean(signal):
    """Find all local minima below the signal mean."""
    m = np.mean(signal)
    valleys = []
    for i in range(1, len(signal) - 1):
        if signal[i] <= signal[i-1] and signal[i] < signal[i+1] and signal[i] < m:
            valleys.append(i)
    return np.array(valleys, dtype=int)


def merge_close_peaks(peaks, valleys, signal):
    """Remove adjacent peaks without intervening valleys, keeping the higher one."""
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
    """Keep peaks with minimum and maximum distance constraints."""
    if len(peaks) == 0:
        return np.array([], dtype=int)
    valid = [peaks[0]]
    for i in range(1, len(peaks)):
        dist = peaks[i] - valid[-1]
        if min_dist <= dist <= max_dist:
            valid.append(peaks[i])
    return np.array(valid, dtype=int)


# ============================================================
# Core Step Counter
# ============================================================

def step_counter(acc_x, acc_y, acc_z, gyr_x, gyr_y, gyr_z):
    """Main step counting algorithm.
    
    Args:
        acc_x, acc_y, acc_z: accelerometer arrays (raw int16)
        gyr_x, gyr_y, gyr_z: gyroscope arrays (raw int16)
    
    Returns:
        step_count: int
    """
    # ---- 1. Preprocessing ----
    
    # Acceleration magnitude (orientation-independent)
    acc_mag = np.sqrt(acc_x**2 + acc_y**2 + acc_z**2)
    
    # Remove DC and smooth
    acc_dc = acc_mag - np.mean(acc_mag)
    acc_filt = mean_filter(acc_dc, MEAN_WIN)
    
    # Gyroscope magnitude (for activity classification)
    gyr_mag = np.sqrt(gyr_x**2 + gyr_y**2 + gyr_z**2)
    gyr_smooth = mean_filter(gyr_mag, GYR_SMOOTH)
    
    # ---- 2. Activity Gating ----
    
    gyr_mean = np.mean(gyr_smooth)
    gyr_std = np.std(gyr_smooth)
    acc_std = np.std(acc_filt)
    
    # Gate A: Stationary
    if gyr_mean < GYR_STATIONARY:
        return 0
    
    # Gate B: Gyro-dominated motion (arm swing without body movement)
    if acc_std > 0 and gyr_std > GYR_ACC_RATIO * acc_std:
        return 0
    
    # Gate C: Very subtle motion
    if acc_std < ACC_MIN_STD and gyr_mean < GYR_LOW_ACTIVE:
        return 0
    
    # ---- 3. Peak Detection ----
    
    peaks = find_peaks_above_mean(acc_filt)
    if len(peaks) == 0:
        return 0
    
    valleys = find_valleys_below_mean(acc_filt)
    
    # Merge close peaks (no valley between adjacent peaks)
    peaks = merge_close_peaks(peaks, valleys, acc_filt)
    
    # Interval-constrained filtering
    peaks = filter_peaks_by_interval(peaks, MIN_PK_DIST, MAX_PK_DIST)
    
    n_steps = len(peaks)
    if n_steps == 0:
        return 0
    
    # ---- 4. Peak Amplitude ----
    
    peak_ampl = np.mean([acc_filt[p] for p in peaks])
    
    # Gate D: Low amplitude + moderate gyro (not walking)
    if peak_ampl < PEAK_AMP_MIN and gyr_mean < GYR_LOW_ACTIVE + 200:
        return 0
    
    # ---- 5. Periodicity Verification ----
    
    if n_steps >= 3:
        intervals = np.diff(peaks)
        cv = np.std(intervals) / np.mean(intervals) if np.mean(intervals) > 0 else float('inf')
    else:
        cv = 0.0
    
    # ---- 6. Activity Classification ----
    
    if gyr_mean < GYR_LOW_ACTIVE:
        # Low gyro: require strict periodicity
        if cv < CV_LOW_GYR and n_steps >= 2:
            return n_steps
        return 0
    elif gyr_mean < GYR_HIGH_ACTIVE:
        # Medium gyro: moderate periodicity required
        if cv < CV_MED_GYR and n_steps >= 2:
            return n_steps
        return 0
    else:
        # High gyro: relaxed periodicity (running has more variability)
        if cv < CV_HIGH_GYR and n_steps >= 2:
            return n_steps
        return 0


# ============================================================
# Evaluation
# ============================================================

def evaluate_directory(data_dir, verbose=True):
    """Evaluate step counter on all files in a directory."""
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
    """Print MAE and MAPE for a set of results."""
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
    print("  VeriHealthi Improved Step Counter - Evaluation Report")
    print("=" * 80)
    print()
    print("  Algorithm: Adaptive Interval-Constrained Peak Counter")
    print("  Features:")
    print("    - Acceleration magnitude (orientation-independent)")
    print("    - Moving average filter ({} samples)".format(MEAN_WIN))
    print("    - Interval-constrained peak detection ({} to {} samples)".format(
        MIN_PK_DIST, MAX_PK_DIST))
    print("    - Gyroscope-based activity classification")
    print("    - Periodicity verification (CV-based)")
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
    
    # Overall metrics
    all_truths = [r[1] for r in all_results]
    all_preds = [r[2] for r in all_results]
    all_errors = [abs(p - t) for t, p in zip(all_truths, all_preds)]
    
    overall_mae = np.mean(all_errors)
    
    walk_run_pairs = [(t, p) for t, p in zip(all_truths, all_preds) if t > 0]
    if walk_run_pairs:
        overall_mape = np.mean([abs(p - t) / t * 100 for t, p in walk_run_pairs])
    else:
        overall_mape = 0.0
    
    fp_count = sum(1 for t, p in zip(all_truths, all_preds) if t == 0 and p > 0)
    noise_count = sum(1 for t in all_truths if t == 0)
    
    print(f"\n{'=' * 80}")
    print(f"  OVERALL RESULTS")
    print(f"{'=' * 80}")
    print(f"  Total files tested: {len(all_results)}")
    print(f"  Overall MAE: {overall_mae:.2f} steps")
    print(f"  Overall MAPE (walk+run): {overall_mape:.2f}%")
    print(f"  False positive rate (noise): {fp_count}/{noise_count} "
          f"({fp_count/noise_count*100:.1f}%)" if noise_count > 0 else "")
    print(f"{'=' * 80}")


if __name__ == "__main__":
    main()
