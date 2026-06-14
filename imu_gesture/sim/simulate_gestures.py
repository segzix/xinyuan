#!/usr/bin/env python3
"""Generate deterministic synthetic IMU gesture datasets for host simulation."""

from __future__ import annotations

import argparse
import math
import subprocess
import sys
from pathlib import Path
from typing import Iterable, Sequence

CHANNELS = ("gx", "gy", "gz", "ax", "ay", "az", "debug")
GESTURES = ("pinch", "clench", "up", "down", "others")
SAMPLE_COUNT = 260


Sample = list[float]


def clamp_int16(value: float) -> int:
    rounded = int(round(value))
    if rounded < -32768:
        return -32768
    if rounded > 32767:
        return 32767
    return rounded


def neutral_samples(count: int = SAMPLE_COUNT) -> list[Sample]:
    return [[0.0, 0.0, 0.0, 0.0, 0.0, 4096.0, 0.0] for _ in range(count)]


def apply_pinch(samples: list[Sample], start: int) -> None:
    duration = 100
    for i in range(duration):
        t = i / (duration - 1)
        envelope = math.sin(math.pi * t)
        row = samples[start + i]
        row[3] = 1000.0 * math.sin(2.0 * math.pi * 8.0 * t) * envelope
        row[4] = 700.0 * math.sin(2.0 * math.pi * 7.0 * t + 0.8) * envelope
        row[5] = 4096.0 + 450.0 * math.sin(2.0 * math.pi * 6.0 * t + 1.4) * envelope

        if 38 <= i <= 48:
            row[0] = 700.0 * math.sin(math.pi * (i - 38) / 10.0)
        elif 49 <= i <= 59:
            row[0] = -700.0 * math.sin(math.pi * (i - 49) / 10.0)


def apply_clench(samples: list[Sample], start: int) -> None:
    duration = 100
    for i in range(duration):
        t = i / (duration - 1)
        envelope = math.sin(math.pi * t)
        row = samples[start + i]
        row[3] = 3400.0 * math.sin(2.0 * math.pi * 10.0 * t) * envelope
        row[4] = 2600.0 * math.sin(2.0 * math.pi * 9.0 * t + 0.6) * envelope
        row[5] = 4096.0 + 2100.0 * math.sin(2.0 * math.pi * 8.0 * t + 1.1) * envelope

        if 34 <= i <= 44:
            row[0] = 2200.0 * math.sin(math.pi * (i - 34) / 10.0)
            row[1] = 900.0 * math.sin(math.pi * (i - 34) / 10.0)
        elif 45 <= i <= 54:
            row[0] = -2200.0 * math.sin(math.pi * (i - 45) / 9.0)
            row[1] = -900.0 * math.sin(math.pi * (i - 45) / 9.0)


def apply_wrist_rotation(samples: list[Sample], start: int, *, upward: bool) -> None:
    duration = 140
    sign = 1.0 if upward else -1.0
    for i in range(duration):
        t = i / (duration - 1)
        row = samples[start + i]
        row[3] = 300.0 * math.sin(2.0 * math.pi * t)
        row[4] = -4500.0 + 9000.0 * t if upward else 4500.0 - 9000.0 * t
        row[5] = 1900.0 + 4200.0 * t if upward else 6100.0 - 4200.0 * t

        if 45 <= i <= 95:
            u = (i - 45) / 50.0
            row[0] = sign * 3600.0 * math.sin(math.pi * u)
            row[1] = sign * 2600.0 * math.sin(2.0 * math.pi * u)
            row[2] = sign * 1200.0 * math.sin(3.0 * math.pi * u)


def generate_samples(gesture: str) -> list[Sample]:
    samples = neutral_samples()
    if gesture == "pinch":
        apply_pinch(samples, start=60)
    elif gesture == "clench":
        apply_clench(samples, start=60)
    elif gesture == "up":
        apply_wrist_rotation(samples, start=40, upward=True)
    elif gesture == "down":
        apply_wrist_rotation(samples, start=40, upward=False)
    elif gesture != "others":
        raise ValueError(f"unsupported gesture: {gesture}")
    return samples


def write_imu_text(path: Path, samples: Iterable[Sequence[float]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="ascii") as fp:
        fp.write("SIMULATED IMU DATA\n")
        fp.write("SAMPLE_RATE = 50HZ\n")
        fp.write(f"CHANNELS = {' '.join(CHANNELS)}\n")
        fp.write("TYPE = 6-AXIS IMU RAW DATA\n")
        for sample in samples:
            for value in sample:
                fp.write(f"{clamp_int16(value)}\n")


def generate_dataset(output_dir: Path) -> list[Path]:
    paths: list[Path] = []
    for gesture in GESTURES:
        sample_path = output_dir / gesture / f"sim_{gesture}_001.txt"
        write_imu_text(sample_path, generate_samples(gesture))
        paths.append(sample_path)
    return paths


def run_host(host_bin: Path, paths: Sequence[Path], check: bool) -> int:
    if not host_bin.is_file():
        print(f"missing host binary: {host_bin}", file=sys.stderr)
        return 1

    failed = False
    for path in paths:
        expected = path.parent.name
        result = subprocess.run(
            [str(host_bin), str(path)],
            check=False,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        print(f"\n== {path} ==")
        if result.stdout:
            print(result.stdout, end="")
        if result.stderr:
            print(result.stderr, end="", file=sys.stderr)

        if result.returncode != 0:
            failed = True
            continue
        if check:
            detected = [line.rsplit(", ", 1)[1] for line in result.stdout.splitlines() if ", " in line]
            if expected == "others":
                ok = not detected
            else:
                ok = expected in detected and all(item == expected for item in detected)
            if not ok:
                print(f"check failed for {path}: expected {expected}, detected {detected}", file=sys.stderr)
                failed = True

    return 1 if failed else 0


def parse_args() -> argparse.Namespace:
    project_root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=project_root / "build" / "simulated_dataset",
        help="directory for generated host-format IMU text files",
    )
    parser.add_argument(
        "--host-bin",
        type=Path,
        default=project_root / "build" / "imu_gesture_host",
        help="host runner binary used with --run-host",
    )
    parser.add_argument("--run-host", action="store_true", help="run generated files through the host runner")
    parser.add_argument("--check", action="store_true", help="fail if host output does not match expected labels")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    paths = generate_dataset(args.output_dir)
    print(f"generated {len(paths)} simulated files under {args.output_dir}")
    if args.run_host or args.check:
        return run_host(args.host_bin, paths, check=args.check)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
