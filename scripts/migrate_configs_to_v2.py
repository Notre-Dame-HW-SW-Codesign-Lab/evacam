#!/usr/bin/env python3
"""Mechanically generate v2 input files from legacy EvaCAM configs."""

from __future__ import annotations

import argparse
import csv
import os
import tempfile
from copy import deepcopy
from pathlib import Path
from typing import Any

import yaml


ROOT = Path(__file__).resolve().parents[1]
CONFIG_ROOT = ROOT / "config"
TECH_LEGACY = "../lib/technology/cmos.legacy.yaml"
TECH_UPDATED = "../lib/technology/cmos.updated.yaml"
SENSE_AMP_DEFAULT = "../lib/sense_amp/nvsim_vol.sense_amp.yaml"


def load_yaml(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as handle:
        data = yaml.safe_load(handle) or {}
    if not isinstance(data, dict):
        raise ValueError(f"{path} must contain a YAML map")
    return data


def write_yaml(path: Path, data: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(
        "w", encoding="utf-8", dir=path.parent, delete=False
    ) as handle:
        temporary_path = Path(handle.name)
        yaml.safe_dump(data, handle, sort_keys=False, default_flow_style=False)
    try:
        os.replace(temporary_path, path)
    except BaseException:
        temporary_path.unlink(missing_ok=True)
        raise


def role_base(path: Path, suffix: str) -> str:
    name = path.name
    if not name.endswith(suffix):
        raise ValueError(f"{path} does not end with {suffix}")
    return name[: -len(suffix)]


def legacy_cell_base(path: Path) -> str:
    name = path.name
    if name.endswith("_cell_config.yaml"):
        return role_base(path, "_cell_config.yaml")
    if name.endswith("_config.yaml"):
        return role_base(path, "_config.yaml")
    raise ValueError(f"{path} is not a supported legacy cell filename")


def relpath(from_file: Path, to_file: Path) -> str:
    return Path("../" + str(to_file.relative_to(CONFIG_ROOT))).as_posix() \
        if from_file.parent == CONFIG_ROOT else Path(
            __import__("os").path.relpath(to_file, from_file.parent)
        ).as_posix()


def relative_reference(from_file: Path, target: Path) -> str:
    import os

    return Path(os.path.relpath(target, from_file.parent)).as_posix()


def normalize_type(value: Any) -> str:
    return str(value).lower()


def convert_tool(tool_file: Path) -> Path:
    root = load_yaml(tool_file)
    base = role_base(tool_file, "_tool_config.yaml")
    out = tool_file.with_name(f"{base}.config.yaml")
    arch_ref = root["architecture_file"]
    cell_ref = root["cell_file"]
    arch_legacy = (tool_file.parent / arch_ref).resolve()
    cell_legacy = (tool_file.parent / cell_ref).resolve()
    arch_base = role_base(arch_legacy, "_architecture_config.yaml")
    cell_base = legacy_cell_base(cell_legacy)
    propagate_match_transistor(cell_legacy, arch_legacy.with_name(f"{arch_base}.architecture.yaml"))

    arch_out = arch_legacy.with_name(f"{arch_base}.architecture.yaml")
    cell_out = cell_legacy.with_name(f"{cell_base}.cell.yaml")

    data: dict[str, Any] = {
        "schema": "config",
        "name": base,
        "architecture": relative_reference(out, arch_out),
        "cell": relative_reference(out, cell_out),
        "technology": relative_reference(
            out,
            CONFIG_ROOT / "lib" / "technology"
            / ("cmos.updated.yaml" if root.get("modeling", {}).get("use_updated_lib", False)
               else "cmos.legacy.yaml")),
        "optimization": deepcopy(root["optimization"]),
    }
    if "design_constraints" in root:
        data["design_constraints"] = deepcopy(root["design_constraints"])
    if "exploration" in root:
        data["exploration"] = deepcopy(root["exploration"])
    if "modeling" in root:
        modeling = deepcopy(root["modeling"])
        modeling.pop("use_updated_lib", None)
        if modeling:
            data["modeling"] = modeling
    if "custom_sense_amplifier_file" in root:
        data["custom_sense_amplifier_file"] = root["custom_sense_amplifier_file"]
    if "output" in root:
        output = deepcopy(root["output"])
        if "yaml_file" in output and "results" not in output:
            output["results"] = output.pop("yaml_file")
        data["output"] = output

    write_yaml(out, data)
    return out


def propagate_match_transistor(cell_legacy: Path, arch_v2: Path) -> None:
    cell_root = load_yaml(cell_legacy)
    match_width = cell_root.get("match", {}).get("cmos_width")
    if match_width is None or not arch_v2.exists():
        return
    arch_root = load_yaml(arch_v2)
    arch_root.setdefault("matchline", {})
    current = arch_root["matchline"].get("match_transistor", {}).get("cmos_width")
    if current is not None and current != match_width:
        raise ValueError(
            f"Conflicting match transistor widths for {arch_v2}: {current} vs {match_width}"
        )
    arch_root["matchline"].setdefault("match_transistor", {})["cmos_width"] = match_width
    write_yaml(arch_v2, arch_root)


def convert_architecture(architecture_file: Path) -> tuple[Path, Path]:
    root = load_yaml(architecture_file)
    base = role_base(architecture_file, "_architecture_config.yaml")
    arch_out = architecture_file.with_name(f"{base}.architecture.yaml")
    sensing_out = architecture_file.with_name(f"{base}.sensing.yaml")

    memory = deepcopy(root.get("memory", {}))
    if "cell_file" in memory:
        memory.pop("cell_file")
    if "real_capacity" in memory:
        memory["physical_capacity"] = memory.pop("real_capacity")
    legacy_cell = architecture_file.with_name(
        f"{base}_cell_config.yaml"
    )
    if legacy_cell.exists():
        cell_root = load_yaml(legacy_cell)
        if str(cell_root.get("cell", {}).get("cam_type", "")).upper() == "MCAM":
            if "word_width" in memory:
                legacy_width = str(memory.pop("word_width")).strip().lower()
                for suffix in ("bits", "bit"):
                    if legacy_width.endswith(suffix):
                        legacy_width = legacy_width[: -len(suffix)].strip()
                        break
                memory["vector_dimensions"] = int(legacy_width)

    data: dict[str, Any] = {"schema": "architecture"}
    for key in ("design",):
        if key in root:
            data[key] = deepcopy(root[key])
    data["memory"] = memory
    for key in ("routing", "peripherals", "wires", "organization", "array", "flash"):
        if key in root:
            data[key] = deepcopy(root[key])
    if "array" in data and "organization" not in data:
        data["organization"] = data.pop("array")
    data["sensing"] = f"./{sensing_out.name}"

    if "matchline" in root:
        data["matchline"] = deepcopy(root["matchline"])
    if "extra" in root:
        extra = root["extra"]
        physical_limits: dict[str, Any] = {}
        if "max_driver_current" in extra:
            physical_limits["max_driver_current"] = extra["max_driver_current"]
        if physical_limits:
            data["physical_limits"] = physical_limits
    if "advanced" in root:
        advanced = root["advanced"]
        if "max_nmos_size" in advanced:
            data.setdefault("physical_limits", {})["max_nmos_size"] = advanced["max_nmos_size"]
        if "input_encoder_type" in advanced:
            data.setdefault("peripherals", {}).setdefault("input", {})["encoder_type"] = advanced[
                "input_encoder_type"
            ]
        if "bit_serial_width" in advanced:
            data.setdefault("organization", {})["bit_serial_width"] = advanced["bit_serial_width"]

    sensing = deepcopy(root.get("sensing", {}))
    sensing_data: dict[str, Any] = {
        "schema": "sensing",
        "internal": sensing.get("internal", True),
    }
    if "worst_case_sense_margin" in sensing:
        sensing_data["worst_case_sense_margin"] = sensing["worst_case_sense_margin"]
    sensing_data["sense_amplifier"] = SENSE_AMP_DEFAULT

    write_yaml(arch_out, data)
    write_yaml(sensing_out, sensing_data)
    return arch_out, sensing_out


def make_memory_device(root: dict[str, Any], base: str) -> dict[str, Any]:
    cell = root["cell"]
    data: dict[str, Any] = {
        "schema": "memory_device",
        "name": cell.get("name", base),
        "type": cell["type"],
    }
    for key in (
        "resistance",
        "capacitance",
        "device",
        "read",
        "write",
        "dram",
        "sram",
        "flash",
        "variation",
        "mcam",
    ):
        if key in root:
            data[key] = deepcopy(root[key])
    variation = data.get("variation")
    if isinstance(variation, dict):
        enabled = variation.pop("with_variation", True)
        if not enabled:
            data.pop("variation", None)
        elif "mode" not in variation:
            variation["mode"] = "single_point"
    return data


def convert_cell(cell_file: Path, report_rows: list[dict[str, Any]]) -> list[Path]:
    root = load_yaml(cell_file)
    base = legacy_cell_base(cell_file)
    cell = root["cell"]
    cell_out = cell_file.with_name(f"{base}.cell.yaml")
    memory_out = cell_file.with_name(f"{base}.memory_device.yaml")
    access_root = root.get("access_device", {})
    access_type = normalize_type(access_root.get("type", "none"))

    data: dict[str, Any] = {
        "schema": "cell",
        "name": cell.get("name", base),
        "cam_type": cell.get("cam_type", "TCAM"),
        "memory_device": f"./{memory_out.name}",
        "layout": {
            "cell_process_node": cell["cell_process_node"],
            "area": cell["area"],
            "aspect_ratio": cell["aspect_ratio"],
        },
        "ports": {},
    }
    if access_root:
        data["access_device"] = deepcopy(access_root)

    for axis in ("row", "column"):
        ports = root.get("ports", {}).get(axis, {})
        if not ports:
            continue
        data["ports"][axis] = {}
        for index, port in ports.items():
            num_cmos = int(port.get("num_cmos", 0))
            region = str(port.get("cmos_region", "none"))
            width = port.get("cmos_width", access_root.get("cmos_width", "1F"))
            is_nmos = bool(port.get("is_nmos", True))
            if num_cmos == 0 or access_type == "none":
                kind = "inline"
                reason = "num_cmos == 0" if num_cmos == 0 else "cell access_device.type == none"
            elif access_type in ("cmos", "diode"):
                kind = "inline"
                reason = f"cell access_device.type == {access_type}"
            else:
                raise ValueError(f"Unclassified port in {cell_file}: access type {access_type}")

            converted = {
                "type": port["type"],
                "cmos_region": region if region != "none" else "drain",
                "num_cmos": num_cmos,
                "cmos_width": width,
                "is_nmos": is_nmos,
                "wire_width": port["wire_width"],
            }
            for key in ("leak", "is_nvm_discharge"):
                if key in port:
                    converted[key] = port[key]
            if "voltages" in port:
                converted["voltages"] = deepcopy(port["voltages"])
            data["ports"][axis][index] = converted

            report_rows.append(
                {
                    "file": str(cell_file.relative_to(ROOT)),
                    "axis": axis,
                    "index": index,
                    "port_type": port.get("type"),
                    "legacy_cmos_region": region,
                    "legacy_num_cmos": num_cmos,
                    "legacy_cmos_width": width,
                    "legacy_is_nmos": is_nmos,
                    "cell_memory_type": cell["type"],
                    "cell_access_type": access_type,
                    "v2_connection_kind": kind,
                    "reason": reason,
                }
            )

    write_yaml(memory_out, make_memory_device(root, base))
    write_yaml(cell_out, data)
    return [cell_out, memory_out]


def iter_legacy_files(pattern: str) -> list[Path]:
    return sorted(
        path
        for path in CONFIG_ROOT.rglob(pattern)
        if "v1schema" not in path.parts and "old_style_config" not in path.parts
    )


def referenced_legacy_cell_files() -> list[Path]:
    files: set[Path] = set(iter_legacy_files("*_cell_config.yaml"))
    for tool_file in iter_legacy_files("*_tool_config.yaml"):
        root = load_yaml(tool_file)
        cell_file = (tool_file.parent / root["cell_file"]).resolve()
        if "v1schema" not in cell_file.parts and "old_style_config" not in cell_file.parts:
            files.add(cell_file)
    return sorted(files)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--report", default="config/v2_port_migration_report.csv")
    args = parser.parse_args()

    report_rows: list[dict[str, Any]] = []
    written: list[Path] = []
    for architecture_file in iter_legacy_files("*_architecture_config.yaml"):
        written.extend(convert_architecture(architecture_file))
    for cell_file in referenced_legacy_cell_files():
        written.extend(convert_cell(cell_file, report_rows))
    for tool_file in iter_legacy_files("*_tool_config.yaml"):
        written.append(convert_tool(tool_file))

    report_path = ROOT / args.report
    report_path.parent.mkdir(parents=True, exist_ok=True)
    with report_path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(report_rows[0].keys()))
        writer.writeheader()
        writer.writerows(report_rows)

    print(f"wrote {len(set(written))} generated v2 files")
    print(f"wrote {len(report_rows)} port classifications to {report_path.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
