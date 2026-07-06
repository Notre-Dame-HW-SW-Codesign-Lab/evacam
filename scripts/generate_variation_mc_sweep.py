#!/usr/bin/env python3
"""Generate local 2FeFET variation Monte Carlo sweep configs."""

from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
BASE_DIR = ROOT / "config" / "2FeFET_TCAM_var"
OUT_DIR = BASE_DIR / "mc_sweep"
RESULT_DIR = "results/variation_mc_sweep"
BASE_CELL = BASE_DIR / "2FeFET_TCAM_cell_config.yaml"
BASE_TOOL = BASE_DIR / "2FeFET_TCAM_tool_config.yaml"
STDEVS = [2, 3, 4, 5, 6, 7, 20, 50]
SAMPLES = 1000
SEED = 33333


def replace_variation_block(text: str, stdev: int) -> str:
    block = (
        "variation:\n"
        "  with_variation: true\n"
        f"  seed: {SEED}\n"
        "  mode: monte_carlo\n"
        f"  samples: {SAMPLES}\n"
        f"  memory_device_resistance_on_stdev: {stdev}%\n"
        f"  memory_device_resistance_off_stdev: {stdev}%\n"
    )
    return re.sub(r"variation:\n(?:  .*\n)+", block, text, count=1)


def replace_cell_file(text: str, cell_path: str) -> str:
    return re.sub(r"^cell_file: .*\n", f"cell_file: {cell_path}\n", text, count=1, flags=re.MULTILINE)


def replace_architecture_file(text: str) -> str:
    return re.sub(
        r"^architecture_file: .*\n",
        "architecture_file: ../2FeFET_TCAM_var_architecture_config.yaml\n",
        text,
        count=1,
        flags=re.MULTILINE,
    )


def set_output_yaml(text: str, output_path: str) -> str:
    if re.search(r"^output:\n(?:  .*\n)*", text, flags=re.MULTILINE):
        if re.search(r"^  yaml_file: .*$", text, flags=re.MULTILINE):
            return re.sub(
                r"^  yaml_file: .*$",
                f"  yaml_file: {output_path}",
                text,
                count=1,
                flags=re.MULTILINE,
            )
        return re.sub(
            r"^output:\n",
            f"output:\n  yaml_file: {output_path}\n",
            text,
            count=1,
            flags=re.MULTILINE,
        )
    return text.rstrip() + f"\n\noutput:\n  yaml_file: {output_path}\n"


def main() -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    cell_template = BASE_CELL.read_text()
    tool_template = BASE_TOOL.read_text()

    for stdev in STDEVS:
        tag = f"stdev{stdev:02d}"
        cell_rel = f"./2FeFET_TCAM_{tag}_cell_config.yaml"
        tool_path = OUT_DIR / f"2FeFET_TCAM_{tag}_tool_config.yaml"
        cell_path = OUT_DIR / f"2FeFET_TCAM_{tag}_cell_config.yaml"
        output_path = f"{RESULT_DIR}/2FeFET_TCAM_{tag}_results.yaml"

        cell_path.write_text(replace_variation_block(cell_template, stdev))
        tool = replace_architecture_file(replace_cell_file(tool_template, cell_rel))
        tool_path.write_text(set_output_yaml(tool, output_path))
        print(tool_path.relative_to(ROOT))


if __name__ == "__main__":
    main()
