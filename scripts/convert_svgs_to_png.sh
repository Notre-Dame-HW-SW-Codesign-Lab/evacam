#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage:
  scripts/convert_svgs_to_png.sh FILE.svg
  scripts/convert_svgs_to_png.sh --dir DIRECTORY

Options:
  --dir DIRECTORY      Convert every *.svg file in DIRECTORY.
  --dpi DPI            Output DPI passed to Inkscape. Default: 600.
  --width PIXELS       Output width passed to Inkscape. Mutually exclusive with --dpi.
  -h, --help           Show this help text.
EOF
}

die() {
    echo "error: $*" >&2
    exit 1
}

convert_file() {
    local file="$1"
    [[ -f "$file" ]] || die "SVG file not found: $file"
    [[ "$file" == *.svg ]] || die "input file must end in .svg: $file"

    local output="${file%.svg}.png"
    if [[ -n "$width" ]]; then
        inkscape "$file" \
            --export-type=png \
            --export-width="$width" \
            --export-filename="$output"
    else
        inkscape "$file" \
            --export-type=png \
            --export-dpi="$dpi" \
            --export-filename="$output"
    fi
    echo "Wrote $output"
}

dpi=600
width=""
input_file=""
input_dir=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --dir)
            [[ $# -ge 2 ]] || die "--dir requires a directory"
            input_dir="$2"
            shift 2
            ;;
        --dpi)
            [[ $# -ge 2 ]] || die "--dpi requires a value"
            dpi="$2"
            shift 2
            ;;
        --width)
            [[ $# -ge 2 ]] || die "--width requires a value"
            width="$2"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        -*)
            die "unknown option: $1"
            ;;
        *)
            [[ -z "$input_file" ]] || die "only one input file is supported"
            input_file="$1"
            shift
            ;;
    esac
done

command -v inkscape >/dev/null 2>&1 || die "inkscape not found in PATH"
[[ -z "$width" || "$dpi" == "600" ]] || die "--width and --dpi are mutually exclusive"
[[ -n "$input_file" || -n "$input_dir" ]] || die "provide FILE.svg or --dir DIRECTORY"
[[ -z "$input_file" || -z "$input_dir" ]] || die "provide either FILE.svg or --dir DIRECTORY, not both"

if [[ -n "$input_file" ]]; then
    convert_file "$input_file"
else
    [[ -d "$input_dir" ]] || die "directory not found: $input_dir"
    shopt -s nullglob
    files=("$input_dir"/*.svg)
    [[ ${#files[@]} -gt 0 ]] || die "no SVG files found in $input_dir"
    for file in "${files[@]}"; do
        convert_file "$file"
    done
fi
