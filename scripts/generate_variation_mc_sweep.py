#!/usr/bin/env python3
"""Generate local 2FeFET variation Monte Carlo sweep configs."""

from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
BASE_DIR = ROOT / "config" / "2FeFET_TCAM_var"
OUT_DIR = BASE_DIR / "mc_sweep"
BASE_CELL = ROOT / "config" / "2FeFET_TCAM" / "2FeFET_TCAM.cell.yaml"
BASE_MEMORY_DEVICE = ROOT / "config" / "2FeFET_TCAM" / "2FeFET_TCAM.memory_device.yaml"
BASE_CONFIG = BASE_DIR / "2FeFET_TCAM.config.yaml"
STDEVS = [2, 3, 4, 5, 6, 7, 20, 50]
SAMPLES = 1000
SEED = 33333


def replace_or_append_variation_block(text: str, stdev: int) -> str:
    block = (
        "variation:\n"
        f"  seed: {SEED}\n"
        "  mode: monte_carlo\n"
        f"  samples: {SAMPLES}\n"
        f"  memory_device_resistance_on_stdev: {stdev}%\n"
        f"  memory_device_resistance_off_stdev: {stdev}%\n"
    )
    updated, count = re.subn(
        r"^variation:\n(?:^  .*\n)*",
        block,
        text,
        count=1,
        flags=re.MULTILINE,
    )
    if count == 1:
        return updated
    return text.rstrip() + "\n" + block


def replace_scalar(text: str, key: str, value: str) -> str:
    updated, count = re.subn(
        rf"^{re.escape(key)}: .*$",
        f"{key}: {value}",
        text,
        count=1,
        flags=re.MULTILINE,
    )
    if count != 1:
        raise RuntimeError(f"Could not replace {key}")
    return updated


def main() -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    cell_template = BASE_CELL.read_text()
    memory_device_template = BASE_MEMORY_DEVICE.read_text()
    config_template = BASE_CONFIG.read_text()

    for stdev in STDEVS:
        tag = f"stdev{stdev:02d}"
        name = f"2FeFET_TCAM_{tag}"
        cell_path = OUT_DIR / f"{name}.cell.yaml"
        memory_device_path = OUT_DIR / f"{name}.memory_device.yaml"
        config_path = OUT_DIR / f"{name}.config.yaml"

        memory_device_path.write_text(
            replace_or_append_variation_block(memory_device_template, stdev)
        )
        cell = replace_scalar(cell_template, "memory_device", f"./{memory_device_path.name}")
        cell_path.write_text(cell)

        config = replace_scalar(config_template, "name", name)
        config = replace_scalar(config, "architecture", "../2FeFET_TCAM_var.architecture.yaml")
        config = replace_scalar(config, "cell", cell_path.name)
        config = replace_scalar(config, "technology", "../../lib/technology/cmos.legacy.yaml")
        config_path.write_text(config)
        print(config_path.relative_to(ROOT))


if __name__ == "__main__":
    main()
