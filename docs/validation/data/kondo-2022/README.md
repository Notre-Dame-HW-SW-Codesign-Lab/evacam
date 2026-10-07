# Kondo and Tanzawa (2022) reference observations

Source: J. Kondo and T. Tanzawa, *Electronics* 11, 1926,
[DOI 10.3390/electronics11131926](https://doi.org/10.3390/electronics11131926).
The article and its figures are CC BY 4.0, copyright the authors. These
extracted observations attribute that source; they are not measurements from
EvaCAM or data supplied by the authors.

- `curves.json`: 296 source observations in 32 series from Figures 3, 4, 6,
  and 7. Coordinates and uncertainty are in SI units. Each observation retains
  its source-image pixel coordinate. Different curves sharing a trace or
  overlapping markers are correlated observations, not independent samples.
- `extraction.json`: embedded-JPEG hashes, native and digitization dimensions,
  affine axis calibration, held-back labeled ticks, source-pixel refinement
  history, and explicit exclusions. Coordinates use top-left image origin.
- `quality.json`: a second extraction of 18 current points using native-image
  blue-pixel thresholds, independent of the palette-distance refinement.
  The maximum difference is 4.10 display pixels; current uncertainty uses
  five vertical pixels. Other coordinates use three pixels.

Originals are cached locally under
`output/validation/nand-kondo-2022/source-cache/`, not committed. The source
manifest records the PDF URL, version, SHA-256, date, and circuit assumptions.
`make validate-nand-kondo` needs only the committed observations, NumPy,
SciPy, Matplotlib, PyYAML, and the compiled probe; it performs no network I/O.

The initial manual picks were checked by drawing them over the source figures.
This caught displaced picks, a mistaken plot-border calibration, and two
obscured points. Those were corrected or excluded using source pixels before
literature residual evaluation. The extraction audit preserves the manual
seeds and subsequent corrections. The current axes use a least-squares fit
through the printed 0, 100, and 200 nA label centers, with the 50 and 150 nA
labels held back. Border coordinates are not assumed to be zero ticks.
Acceptance allowances were not changed to accommodate model residuals.

The upper panels of Figures 6 and 7 lack numerical time labels for their
spatial profiles. They are inventoried but cannot support time-specific
voltage comparisons. Clipped switching peaks, unreadable overlaps, and the
unspecified interior I_BLF probe location are likewise excluded explicitly.
The Figure 3a data-state label and the data-0 settling rule remain ambiguous.
