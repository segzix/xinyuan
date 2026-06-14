#!/usr/bin/env python3
"""Python mirror of the current C IMU gesture algorithm."""

from __future__ import annotations

import argparse
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Sequence

SAMPLE_RATE_HZ = 50
SAMPLE_BYTES = 14
CHANNELS = 7
BLOCK_SAMPLES = 64
WINDOW_SAMPLES = 100
MIN_CLASSIFY_SAMPLES = 50
COOLDOWN_SAMPLES = 100
EVAL_INTERVAL_SAMPLES = 25
GESTURE_NAMES = {"pinch", "clench", "up", "down"}
GESTURE_ORDER = ("pinch", "clench", "up", "down")
DATASET_CLASS_ORDER = ("pinch", "clench", "up", "down", "others")


@dataclass(frozen=True)
class Sample:
    gx: int
    gy: int
    gz: int
    ax: int
    ay: int
    az: int
    debug: int


@dataclass
class WindowFeatures:
    min_ax: int
    max_ax: int
    min_ay: int
    max_ay: int
    min_az: int
    max_az: int
    min_gx: int
    max_gx: int
    min_gy: int
    max_gy: int
    min_gz: int
    max_gz: int
    accel_jerk_sum: int = 0
    gyro_energy_sum: int = 0
    peak_accel: int = 0
    peak_gyro: int = 0
    accel_range: int = 0
    gyro_range: int = 0
    z_delta: int = 0
    y_delta: int = 0
    x_delta: int = 0
    mean_az: int = 0
    mean_ay: int = 0


@dataclass
class PathResult:
    ok: bool
    py_lines: list[str]
    host_lines: list[str]


class GestureAlgoContext:
    def __init__(self) -> None:
        self.window: list[Sample] = []
        self.cooldown_samples = 0
        self.eval_countdown = 0

    def process(self, samples: Sequence[Sample]) -> str:
        result = "other"
        for sample in samples:
            self._append_window_sample(sample)
            self._tick_classify_timers()

            if (
                result == "other"
                and len(self.window) >= MIN_CLASSIFY_SAMPLES
                and self.cooldown_samples == 0
                and self.eval_countdown == 0
            ):
                result = classify_window(self.window)
                self.eval_countdown = EVAL_INTERVAL_SAMPLES
                if result != "other":
                    self.cooldown_samples = COOLDOWN_SAMPLES

        return result

    def _append_window_sample(self, sample: Sample) -> None:
        if len(self.window) < WINDOW_SAMPLES:
            self.window.append(sample)
            return
        self.window = self.window[1:] + [sample]

    def _tick_classify_timers(self) -> None:
        if self.cooldown_samples > 0:
            self.cooldown_samples -= 1
        if self.eval_countdown > 0:
            self.eval_countdown -= 1


class AlgoManager:
    def __init__(self) -> None:
        self.algo = GestureAlgoContext()
        self.processed_bytes = 0

    def process_block(self, block: Sequence[Sample]) -> tuple[int, str] | None:
        if not block:
            return None
        result = self.algo.process(block)
        self.processed_bytes += len(block) * SAMPLE_BYTES
        if result == "other":
            return None
        return self.processed_time_ms(), result

    def processed_time_ms(self) -> int:
        sample_count = self.processed_bytes // SAMPLE_BYTES
        return (sample_count // SAMPLE_RATE_HZ) * 1000 + ((sample_count % SAMPLE_RATE_HZ) * 1000) // SAMPLE_RATE_HZ


def clamp_int16(value: int) -> int:
    if value < -32768:
        return -32768
    if value > 32767:
        return 32767
    return value


def parse_line_value(line: str) -> int | None:
    stripped = line.strip()
    if not stripped:
        return None
    try:
        # Match strtol behavior used by the C host: accept a numeric prefix.
        end = 0
        if stripped[0] in "+-":
            end = 1
        while end < len(stripped) and stripped[end].isdigit():
            end += 1
        if end == 0 or stripped[:end] in {"+", "-"}:
            return None
        return clamp_int16(int(stripped[:end], 10))
    except ValueError:
        return None


def read_imu_text(path: Path) -> list[Sample]:
    values: list[int] = []
    samples: list[Sample] = []
    has_data = False

    with path.open("r", encoding="ascii", errors="ignore") as fp:
        for line in fp:
            if not has_data:
                if "TYPE" in line:
                    has_data = True
                continue

            value = parse_line_value(line)
            if value is None:
                continue

            values.append(value)
            if len(values) != CHANNELS:
                continue

            samples.append(Sample(*values))
            values = []

    return samples


def range16(min_value: int, max_value: int) -> int:
    return max_value - min_value


def c_div_toward_zero(value: int, divisor: int) -> int:
    if value >= 0:
        return value // divisor
    return -((-value) // divisor)


def compute_window_features(samples: Sequence[Sample]) -> WindowFeatures:
    first = samples[0]
    features = WindowFeatures(
        min_ax=first.ax,
        max_ax=first.ax,
        min_ay=first.ay,
        max_ay=first.ay,
        min_az=first.az,
        max_az=first.az,
        min_gx=first.gx,
        max_gx=first.gx,
        min_gy=first.gy,
        max_gy=first.gy,
        min_gz=first.gz,
        max_gz=first.gz,
    )

    previous: Sample | None = None
    for sample in samples:
        features.min_ax = min(features.min_ax, sample.ax)
        features.max_ax = max(features.max_ax, sample.ax)
        features.min_ay = min(features.min_ay, sample.ay)
        features.max_ay = max(features.max_ay, sample.ay)
        features.min_az = min(features.min_az, sample.az)
        features.max_az = max(features.max_az, sample.az)
        features.min_gx = min(features.min_gx, sample.gx)
        features.max_gx = max(features.max_gx, sample.gx)
        features.min_gy = min(features.min_gy, sample.gy)
        features.max_gy = max(features.max_gy, sample.gy)
        features.min_gz = min(features.min_gz, sample.gz)
        features.max_gz = max(features.max_gz, sample.gz)

        if previous is not None:
            features.accel_jerk_sum += abs(sample.ax - previous.ax)
            features.accel_jerk_sum += abs(sample.ay - previous.ay)
            features.accel_jerk_sum += abs(sample.az - previous.az)

        accel_abs = abs(sample.ax) + abs(sample.ay) + abs(sample.az)
        gyro_abs = abs(sample.gx) + abs(sample.gy) + abs(sample.gz)
        features.peak_accel = max(features.peak_accel, accel_abs)
        features.peak_gyro = max(features.peak_gyro, gyro_abs)
        features.gyro_energy_sum += gyro_abs
        features.mean_az += sample.az
        features.mean_ay += sample.ay
        previous = sample

    features.accel_range = (
        range16(features.min_ax, features.max_ax)
        + range16(features.min_ay, features.max_ay)
        + range16(features.min_az, features.max_az)
    )
    features.gyro_range = (
        range16(features.min_gx, features.max_gx)
        + range16(features.min_gy, features.max_gy)
        + range16(features.min_gz, features.max_gz)
    )
    features.z_delta = samples[-1].az - samples[0].az
    features.y_delta = samples[-1].ay - samples[0].ay
    features.x_delta = samples[-1].ax - samples[0].ax
    features.mean_az = c_div_toward_zero(features.mean_az, len(samples))
    features.mean_ay = c_div_toward_zero(features.mean_ay, len(samples))
    return features


def classify_window(samples: Sequence[Sample]) -> str:
    if len(samples) < MIN_CLASSIFY_SAMPLES:
        return "other"

    features = compute_window_features(samples)

    if (
        features.gyro_energy_sum > 100000
        and features.gyro_energy_sum < 280000
        and features.peak_gyro > 5000
        and features.peak_gyro < 18000
        and features.gyro_range > 7000
        and range16(features.min_ay, features.max_ay) > 5500
        and features.peak_accel < 18000
        and abs(features.x_delta) < 4500
        and features.y_delta > 2800
        and features.z_delta > 500
    ):
        return "up"

    if (
        features.gyro_energy_sum > 100000
        and features.gyro_energy_sum < 280000
        and features.peak_gyro > 5000
        and features.peak_gyro < 18000
        and features.gyro_range > 7000
        and range16(features.min_ay, features.max_ay) > 5500
        and features.peak_accel < 18000
        and abs(features.x_delta) < 4500
        and features.y_delta < -2800
        and features.z_delta < -500
    ):
        return "down"

    if (
        features.mean_az < 2500
        or features.mean_ay < -1800
        or features.gyro_energy_sum > 60000
        or abs(features.y_delta) > 1000
        or abs(features.z_delta) > 1000
    ):
        return "other"

    if (
        features.accel_range > 7000
        and features.accel_range < 22000
        and features.gyro_range > 2800
        and features.gyro_range < 9000
        and features.accel_jerk_sum > 35000
        and features.peak_gyro > 1300
        and features.peak_gyro < 7000
        and features.peak_accel > 8500
        and features.peak_accel * 2 > features.peak_gyro * 3
    ):
        return "clench"

    if (
        features.accel_range > 1800
        and features.accel_range < 5500
        and features.gyro_range > 900
        and features.gyro_range < 3000
        and features.accel_jerk_sum > 9000
        and features.accel_jerk_sum < 30000
        and features.gyro_energy_sum < 18000
        and features.peak_gyro > 450
        and features.peak_gyro < 1600
        and features.peak_accel < 8500
    ):
        return "pinch"

    return "other"


def run_python_algo(path: Path) -> list[str]:
    samples = read_imu_text(path)
    manager = AlgoManager()
    lines: list[str] = []

    for offset in range(0, len(samples), BLOCK_SAMPLES):
        block = samples[offset : offset + BLOCK_SAMPLES]
        output = manager.process_block(block)
        if output is not None:
            time_ms, result = output
            lines.append(f"{time_ms}ms, {result}")

    return lines


def run_host(host_bin: Path, path: Path) -> tuple[int, list[str], str]:
    result = subprocess.run(
        [str(host_bin), str(path)],
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    lines = [
        line
        for line in result.stdout.splitlines()
        if ", " in line and line.rsplit(", ", 1)[1] in GESTURE_NAMES
    ]
    return result.returncode, lines, result.stderr


def dataset_paths(dataset_dir: Path) -> list[Path]:
    return sorted(path for path in dataset_dir.rglob("*.txt") if path.name != "ReadMe.txt")


def resolve_paths(args: argparse.Namespace) -> list[Path]:
    paths = [Path(path) for path in args.paths]
    if args.dataset_dir is not None:
        paths.extend(dataset_paths(args.dataset_dir))
    return paths


def print_lines(title: str, lines: Iterable[str]) -> None:
    print(f"{title}:")
    printed = False
    for line in lines:
        print(line)
        printed = True
    if not printed:
        print("(no gesture output)")


def process_path(path: Path, args: argparse.Namespace) -> PathResult:
    py_lines = run_python_algo(path)

    if not args.compare_host:
        if not args.quiet:
            print(f"\n== {path} ==")
            print_lines("python", py_lines)
        return PathResult(ok=True, py_lines=py_lines, host_lines=[])

    if args.host_bin is None:
        print("missing --host-bin for --compare-host", file=sys.stderr)
        return PathResult(ok=False, py_lines=py_lines, host_lines=[])

    returncode, host_lines, host_stderr = run_host(args.host_bin, path)
    ok = returncode == 0 and py_lines == host_lines
    if not args.quiet or not ok:
        print(f"\n== {path} ==")
        print_lines("python", py_lines)
        if host_stderr:
            print(host_stderr, end="", file=sys.stderr)
        print_lines("host", host_lines)
        print("compare: ok" if ok else "compare: mismatch")
    return PathResult(ok=ok, py_lines=py_lines, host_lines=host_lines)


def output_label_counts(lines: Iterable[str]) -> dict[str, int]:
    counts = {gesture: 0 for gesture in GESTURE_ORDER}
    for line in lines:
        if ", " not in line:
            continue
        gesture = line.rsplit(", ", 1)[1]
        if gesture in counts:
            counts[gesture] += 1
    return counts


def dataset_group(path: Path, args: argparse.Namespace) -> str:
    if args.dataset_dir is not None:
        try:
            relative = path.relative_to(args.dataset_dir)
            if len(relative.parts) > 1:
                return relative.parts[0]
        except ValueError:
            pass
    return path.parent.name


def sorted_groups(groups: Iterable[str]) -> list[str]:
    ordered = [name for name in DATASET_CLASS_ORDER if name in groups]
    extra = sorted(name for name in groups if name not in DATASET_CLASS_ORDER)
    return ordered + extra


def empty_group_stats() -> dict[str, object]:
    return {
        "files": 0,
        "no_output_files": 0,
        "outputs": {gesture: 0 for gesture in GESTURE_ORDER},
    }


def update_group_stats(stats: dict[str, object], lines: list[str]) -> None:
    stats["files"] = int(stats["files"]) + 1
    if not lines:
        stats["no_output_files"] = int(stats["no_output_files"]) + 1
    outputs = stats["outputs"]
    assert isinstance(outputs, dict)
    for gesture, count in output_label_counts(lines).items():
        outputs[gesture] += count


def format_output_counts(counts: dict[str, int]) -> str:
    return " ".join(f"{gesture}={counts[gesture]}" for gesture in GESTURE_ORDER)


def print_quiet_summary(args: argparse.Namespace, checked_count: int, failed_count: int, groups: dict[str, dict[str, object]]) -> None:
    summary_name = "compare" if args.compare_host else "python"
    failure_name = "mismatches" if args.compare_host else "failures"
    print(f"{summary_name} summary:")
    if args.dataset_dir is not None:
        print(f"  dataset={args.dataset_dir}")
    print(f"  files={checked_count} matched={checked_count - failed_count} {failure_name}={failed_count}")
    print("  by directory:")
    for group in sorted_groups(groups.keys()):
        stats = groups[group]
        outputs = stats["outputs"]
        assert isinstance(outputs, dict)
        print(
            f"    {group:<7} files={stats['files']} "
            f"no_output_files={stats['no_output_files']} "
            f"py_outputs: {format_output_counts(outputs)}"
        )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("paths", nargs="*", type=Path, help="host-format IMU text files")
    parser.add_argument("--dataset-dir", type=Path, help="recursively process IMU text files under this directory")
    parser.add_argument("--host-bin", type=Path, help="C host binary used by --compare-host")
    parser.add_argument("--compare-host", action="store_true", help="compare Python output with C host output")
    parser.add_argument("--quiet", action="store_true", help="print summary statistics and mismatches")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    paths = resolve_paths(args)
    if not paths:
        print("no input files; pass DATA or --dataset-dir", file=sys.stderr)
        return 1

    failed = False
    checked_count = 0
    failed_count = 0
    groups: dict[str, dict[str, object]] = {}
    for path in paths:
        if not path.is_file():
            print(f"missing input file: {path}", file=sys.stderr)
            failed = True
            failed_count += 1
            continue
        checked_count += 1
        result = process_path(path, args)
        group = dataset_group(path, args)
        if group not in groups:
            groups[group] = empty_group_stats()
        update_group_stats(groups[group], result.py_lines)
        if not result.ok:
            failed = True
            failed_count += 1

    if args.quiet:
        print_quiet_summary(args, checked_count, failed_count, groups)

    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
