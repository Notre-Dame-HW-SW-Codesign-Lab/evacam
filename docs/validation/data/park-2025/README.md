# Park 2025 digitized reference, version 1

Source: Gihong Park, Jung Nam Kim, Jun Hui Park, Suk-Kang Sung, Garam Kim, and
Yoon Kim, *Current-Voltage Modeling of 3D-NAND String Using Genetic Algorithm*,
JSTS 25(4), 420–426 (2025).
[Publisher article](https://www.jsts.org/jsts/XmlViewer/f438850),
[DOI](https://doi.org/10.5573/JSTS.2025.25.4.420).
Copyright The Institute of Electronics and Information Engineers (IEIE).
The publisher identifies the work as [CC BY-NC 3.0](https://creativecommons.org/licenses/by-nc/3.0/).
This reference extraction retains that attribution and license for source-derived
material. It is an approximate digitization, not an author-supplied numeric dataset.

Retrieved 2026-09-25. The [manifest](../../nand-park-2025.reference.yaml) gives
source URLs/hashes, operating conditions, dataset hashes, exclusions and partitions.
Original images/PDFs are not redistributed in this directory.

- `curves.json`: 20 cases, 56 series, 347 sampled points. TCAD markers (182 points)
  and the authors' fitted Spectre results (165 points) are separate populations.
  Point pairs are `[abscissa, ordinate]`. Publication units, SI coordinates,
  source pixels and uncertainty intervals are retained together.
- `extraction.json`: native PNG axis calibrations and held-back ticks, original
  picks, extraction methods, 14 second-method rechecks, and excluded regions.
  Pixel origin is the upper-left corner. Log units are not silently corrected.

Whole-case splits are frozen before fitting: three short strings for calibration;
five layer-count, three BL-bias and three position cases held out; six taper cases
in a separate geometry challenge. Repeated observables and potentially coincident
physical cases are correlated. Insets and Gm samples do not increase the count of
independent current experiments. Pixel-derived inset layer coordinates retain
uncertainty; model inputs use the integer layer count in the corresponding case.

Only readable trace portions are sampled. No interpolated, synthetic, or
zero-filled low-current observations are present. Figure 7(b) is excluded because
its printed units conflict with the linear panel and trace identities overlap.
Figure 7(c) contains peak-region samples only. None of these cases presently
supports unconditional quantitative validation because decisive operating inputs
are missing. No EvaCAM fit or prediction is included.

Run `make test-nand-park-reference` and `make validate-nand-park-reference` from
the repository root for offline checks and reconstructed plots. See the
[extraction report](../../nand-park-2025.md) for quality, missing inputs, and the
modeling handoff. Any repartition or source-coordinate revision after fitting
requires a new version and an account of its effect on held-out status.
