# Park 2025 reference extraction

Status: version 1 extracted with explicit exclusions. The reference checks pass;
no EvaCAM nonlinear model has been fitted or validated against these data.

The next literature step after Kondo is Park et al.,
[Current-Voltage Modeling of 3D-NAND String Using Genetic Algorithm](https://www.jsts.org/jsts/XmlViewer/f438850),
JSTS 25(4), 420–426 (2025),
[DOI 10.5573/JSTS.2025.25.4.420](https://doi.org/10.5573/JSTS.2025.25.4.420).
It supplies static string-current references for the
[nonlinear NAND3D plan](../nand-3d-nonlinear-model-plan.md).
The reference is **synthetic TCAD of single-crystalline silicon**, compared with
the authors' fitted BSIM-CMG implementation. It is not measured commercial NAND,
and it establishes neither transient timing nor CAM energy.

## Frozen data and partitions

The [manifest](nand-park-2025.reference.yaml) contains conditions, source hashes,
and partitions. [curves.json](data/park-2025/curves.json) retains publication and
SI coordinates; [extraction.json](data/park-2025/extraction.json) retains native
pixels, axis transforms, manual-pick history, rechecks, and exclusions.

There are 20 cases, 56 sampled series, and 347 observations: **182 TCAD marker
samples and 165 samples of the authors' Spectre fits**. These are separate
populations, not 347 independent reference measurements. Each population has
28 series, including five single-point on-current inset series.

| Partition | Source and cases | Cases | Series | Points |
| --- | --- | ---: | ---: | ---: |
| Calibration | Figure 4(c): 1, 3, 5 active WLs | 3 | 6 | 57 |
| Held-out layer count | Figure 5: 32, 64, 128, 300, 500 WLs; main plot and inset | 5 | 20 | 81 |
| Held-out BL bias | Figure 6: 32 WLs, BL 0.7, 0.5, 0.3 V | 3 | 6 | 57 |
| Held-out selected position | Figure 7: WL0, WL16, WL31; linear current and Gm peak samples | 3 | 12 | 59 |
| Geometry challenge | Figure 9: taper 0°, 0.1°, 0.2° at WL16; 0.1° taper at WL0, WL16, WL31 | 6 | 12 | 93 |

Calibration uses only the three short-string families. All populations and
observables belonging to one case stay together. Potentially repeated operating
points across figures are listed as dependency groups and cannot cross the
calibration boundary. Inset on-current and published transconductance are not
extra independent current measurements. No derivative was calculated from the
digitized I-V points.

Partitions and data hashes were frozen before fitting. A later repartition
requires a new reference version and must identify any previously inspected
cases as exploratory. No EvaCAM resistance, device parameter, or prediction
was used to set these reference coordinates.

## Source audit

The publisher HTML and native PNGs for Figures 1, 2, and 4–9 were retrieved on
2026-09-25. URLs, byte counts, and SHA-256 hashes are in the manifest. Original
images and access responses are cached under the ignored
`output/validation/nand-park-2025/source-cache/` directory. Automated checks use
only the committed JSON/YAML and require no network access.

Table I is referenced in the text but is absent from the accessed HTML. The
publisher's HTTPS PDF route failed certificate validation; its HTTP download
route returned an authorization error. Neither the PDF nor Table I was obtained.
The publisher page, University of Seoul publication record, and author AIDL
publication list exposed no complete model card, numeric dataset, netlist, or
supplement in the accessed material. This does not establish that such material
is unavailable elsewhere. No authors were contacted.

The article identifies Sentaurus TCAD, Cadence Spectre 21.1, BSIM-CMG with
`GEOMOD=3`, and optimization of 24 parameters. It does not disclose the exact
TCAD version or an accessible complete fitted card. The citation of a BSIM-CMG
111.2.1 manual is recorded separately from the unknown implementation version.
Reproducing the authors' exact circuit implementation is therefore not claimed.

The publisher identifies the article as CC BY-NC 3.0, copyright the Institute of
Electronics and Information Engineers. The extracted reference carries this
attribution and the [license link](https://creativecommons.org/licenses/by-nc/3.0/).
Full paper images and PDFs are not committed. See the
[dataset attribution and format notes](data/park-2025/README.md).

## Operating conditions and missing inputs

The common geometry is a macaroni channel with 13.5 nm filler radius and 10 nm
silicon thickness, giving a derived 47 nm outer channel diameter. The figure
reports a 5/5/6 nm oxide/nitride/oxide stack, 50 nm WL/spacer lengths, 50 nm
select-gate lengths, and a 4.8 eV WL work function. Reported channel and BL/SL
dopant concentrations are converted from cm⁻³ to m⁻³ in the manifest.
The channel doping type is unavailable.

The calibration figure explicitly specifies pass voltage 6 V and BL voltage
0.7 V; Figure 4 places the selected device at WL0, WL1, and WL2 for the 1-, 3-,
and 5-WL strings respectively. Unread gates include SSL and GSL. The later
figures inherit 6 V pass bias as a labeled assumption. Selected WLs for the
layer-count and BL-bias families are not disclosed. All cases retain nulls for
source voltage, temperature, and threshold/charge state.
Figure 9(a) inherits 32 active WLs from the companion panel's caption as an
explicit assumption; its own panel labels only the selected WL and taper angles.

Figure 8 supplies sample diameters along a tapered string, but does not identify
the example's taper angle or a complete rule mapping angle and layer to diameter.
The example cannot silently become the diameter profile for every Figure 9 case.
Those geometry inputs remain unresolved.

The Figure 5 inset uses on-current at selected-WL voltage 6 V, recorded on its
series. Its pixel-derived x coordinates may differ slightly from integer layer
counts: use the case's reported integer count as the model input, and retain the
pixel coordinate only as extraction evidence. Active WL count excludes the two
select gates and is not EvaCAM CAM key width or validity-pair count.

**No case is currently eligible for an unconditional quantitative model-validation
claim.** Each has a `blocking_inputs` list. Missing inputs may later be resolved
from additional sources or explored as explicit, bounded assumptions; they must
not be inferred by minimizing held-out prediction errors.

## Extraction quality and excluded regions

Axes use multiple labeled ticks, with additional ticks held back from fitting.
The maximum inverse-axis tick residual is 0.786 native pixels, below the declared
2-pixel tick tolerance. Native-pixel overlays of all eight extracted panels and
the ambiguous log panel were visually inspected against the cached originals.

TCAD center uncertainty is estimated as ±4 pixels horizontally and ±6 vertically;
Spectre line centers use ±1 and ±2 pixels. Inset centers use ±3 pixels on both
axes. These are extraction-resolution estimates, not statistical confidence
intervals or estimates of TCAD model error. Fourteen isolated plateau markers
were reread using a gray-threshold envelope method; its largest vertical
difference from the manual pick was 5.5 pixels. This second extraction method
checks selected markers, not every point or an independent experimental dataset.

Linear-axis uncertainty is converted to SI intervals. Log-axis uncertainty is
transformed multiplicatively and retained in decades. For the usable Figure 5
inset it is ±0.01695 decades, about −3.8%/+4.0% in current. The extracted TCAD
on-currents are approximately 15.46, 8.17, 4.21, 1.81, and 1.08 µA for 32, 64,
128, 300, and 500 WLs. The main linear panel has much poorer relative resolution
for the smallest currents; a blanket 10% comparison at every sampled point
would not be justified. The paper's much smaller reported fitting errors cannot
be independently verified from these raster samples.

The reference deliberately has partial domains:

- Figure 4(c): roughly 2–5.8 V samples; overlapping low-current traces are
  excluded. There is no usable short-string log-current calibration family.
- Figure 5: the inset obscures lower main-panel traces beyond approximately
  3.3 V. Named layer counts retain separate inset endpoints; additional unnamed
  inset counts are not guessed from their horizontal positions.
- Figure 7(a) and Figure 9(b): separately identifiable rising traces are retained;
  converged same-color plateaus are not counted as distinct measurements.
- Figure 7(c): four TCAD peak-region samples per position plus separate Spectre
  samples. Ambiguous full tails, crossings, and annotation overlap are excluded.
- Figure 7(b): all three TCAD and all three Spectre log-current curves are
  excluded from usable data. The vertical label says µA, while its numerical
  tick range and the linear panel disagree by roughly six decades under that
  interpretation. Treating the ticks as amperes is a plausible explanation,
  not an adopted correction. Unassigned diagnostic pixels retain both
  interpretations without a usable SI curve or inferred WL identity.
- Figures 7(d) and 8 are circuit/geometry diagrams, not additional numerical
  current observations. Table I remains unverified.

Unresolved low-current regions are censored/excluded separately, never replaced
with zero. All usable points are direct source samples. Dotted connections in
the reconstructed plots are visual guides between the authors' Spectre samples;
they do not add interpolation points to the dataset.

## Handoff to electrical modeling

| Proposed use | Reference support | Present limit |
| --- | --- | --- |
| Fixed-bias string resistance | Current or on-current at a specified bias | Needs SL bias before deriving `(VBL − VSL) / IBL`; this is whole-string resistance, not a uniquely identified local cell resistance |
| Nonlinear DC calibration | Three Figure 4(c) short-string TCAD families only | Resolve or explicitly bound missing conditions; readable samples mainly constrain strong inversion |
| Held-out nonlinear DC checks | Layer-count, BL-bias, and selected-position families | Use one frozen parameter set; missing WL/protocol conditions remain blockers |
| Geometry prediction challenge | Figure 9 taper families | Requires independently specified layer diameters and diameter dependence |
| Gm diagnostic | Published Figure 7(c) peak-region samples | Partial derivative observable, correlated with the I-V family; no full Gm reproduction claim |
| Log-current / subthreshold check | Figure 7(b) diagnostic pixels only | Unit and trace-identity ambiguity prevents quantitative use |
| Transient/CAM validation | None from these DC curves | Requires separate capacitance, phase, state, sensing, and energy evidence |

A reduced local cell law remains an approximation: these string curves do not
uniquely identify a general transistor surface, subthreshold behavior, or stored
state separation. Start with a small shared parameter set, publish sensitivity
and nonuniqueness, and assess held-out errors without introducing per-layer or
per-position correction factors. A target around 10% belongs to the later model
comparison where source precision permits it; extraction residuals are not that
accuracy target. The 24 parameters in the authors' BSIM fit are not parameters
added to EvaCAM by this work.

Useful additional information to request from the authors: numeric TCAD curves;
Table I and the complete fitted card/deck; SL bias, temperature and charge state;
selected WLs and pass/select-gate protocol for Figures 5–6; the Figure 7(b) current
unit; and diameter profiles or the taper construction rule. This list does not
authorize or send a message.

## Reproduce the checks

```sh
make test-nand-park-reference
make validate-nand-park-reference
# Optional, when the original cached files are available:
python3 scripts/check_nand_park_reference.py \
  --source-cache output/validation/nand-park-2025/source-cache
```

The output directory contains `report.json`, `reference-curves.png/.svg`,
`extraction-quality.png`, and optional `source-overlay-*.png`. Default success
means reference-integrity checks passed. The report status remains
`reference_checked_model_validation_pending`; `--require-model-validation`
returns exit code 2. The focused tests include known synthetic linear/log axes,
tampered hashes, units, partitions, uncertainty, source provenance, and preservation
of that pending status. The target is included in unit-test aggregation and CI.
