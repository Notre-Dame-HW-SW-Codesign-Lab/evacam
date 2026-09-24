# Plan: extract Park et al. 3D NAND string references

Status: planned; no extracted dataset or fitted device model is claimed here.

## Objective and source

Create an auditable reference dataset from Park et al.,
[Current-Voltage Modeling of 3D-NAND String Using Genetic Algorithm](https://www.jsts.org/jsts/XmlViewer/f438850),
JSTS 25(4), 420-426 (2025),
[DOI 10.5573/JSTS.2025.25.4.420](https://doi.org/10.5573/JSTS.2025.25.4.420).
Separate data used to fit a future device model from data reserved to evaluate
its predictions.

The published references are TCAD simulations, compared with a fitted BSIM-CMG
model. Agreement would be correlation with those simulations, not validation
against measured production NAND. Neither CAM timing nor search energy is
established by these static curves.

## Known reference inventory

The article provides the following basis for extraction. Figure details must
be checked against the actual images before finalizing each case:

| Source | Information | Proposed role |
| --- | --- | --- |
| Figure 2 and device description | Geometry and material assumptions | Fixed inputs and provenance |
| Table I, referenced in the text | Selected BSIM-CMG parameters | Audit availability and completeness |
| Figure 4(c) | 1-, 3-, and 5-WL string curves; pass 6 V, bitline 0.7 V | Calibration |
| Figure 5 | 32, 64, 128, 300, and 500 WLs | Held-out layer-count validation |
| Figure 6 | Bitline-voltage sweep, 32 WLs | Held-out bias validation |
| Figure 7 | WL0, WL16, and WL31 of a 32-WL string; linear/log current and transconductance | Held-out position validation |
| Figures 8-9 | Channel taper geometry and current response | Separate geometry challenge |

The text specifies a simplified single-crystalline silicon channel and 50 nm
WL/spacer lengths. Do not silently replace that device with a polysilicon
commercial process. Source: [article, Sections II-III](https://www.jsts.org/jsts/XmlViewer/f438850).

## 1. Acquire and audit the reference material

- Cache the publisher article, full-resolution figures, and accessible PDF.
  Record URLs, version, retrieval date, hashes, and figure/table locations.
  Automated dataset checks must not need network access.
- Retrieve Table I from the PDF if the HTML omits it. Distinguish parameter
  names and optimization bounds from final fitted values. A partial table is
  not a complete model card.
- Check for an author dataset, model card, circuit deck, or supplement. Prefer
  supplied numeric data where available, preserving the original file and units.
  Record unsuccessful access attempts without treating them as evidence that
  the material does not exist.
- Record source licensing and attribution requirements before redistributing
  figure images or extracted material. Do not embed full paper PDFs in the
  repository by default.

## 2. Establish a case manifest

For each curve, capture the selected physical WL, total active storage layers,
SSL/GSL devices, pass/BL/SL biases, sweep variable, threshold/charge state,
channel shape, dimensions, doping, temperature, and relevant simulator/model
version where disclosed. Distinguish string gate counts from EvaCAM's CAM key
width and validity-pair count.

Classify each input as `reported`, `derived`, `assumed`, or `unavailable`, with
its source location and derivation. Use explicit nulls and reasons for missing
values. Do not default unspecified temperature, voltages, geometry, or device
states to plausible values without labeling and sensitivity analysis.

Assign an immutable case ID, observable, units, current sign convention,
axis type, source trace identity, and intended dataset split. Preserve both
the publication units and converted SI values.

## 3. Extract points with traceable uncertainty

1. Calibrate each panel using multiple labeled axis ticks and verify them with
   held-back ticks. Apply logarithmic transforms to log axes.
2. Extract TCAD markers as the primary reference and the authors' fitted SPICE
   curves as a separate comparison series. Never merge those two populations.
3. Retain original pixel coordinates, transformations, extracted coordinates,
   trace labels, and any manual edits. Overlapping or indistinguishable markers
   are flagged rather than invented.
4. Estimate voltage/current uncertainty from line thickness, marker size, image
   resolution, and repeated extraction. Log-current uncertainty is recorded in
   decades; an unreadable low-current region is censored, not set to zero.
5. Check representative points with an independent second extraction and compare
   the linear and logarithmic panels where their observable and conditions agree.
6. Keep directly digitized observations separate from interpolated samples.
   Interpolation must not increase the effective number of independent points.

For transconductance, distinguish values read from the published panel from
derivatives calculated from extracted current. If a derivative is needed,
record smoothing, differentiation method, and uncertainty amplification; do
not count it as an independent current measurement.

## 4. Freeze calibration and validation partitions

Use complete curves as the splitting unit. Points from the same curve must not
appear in both calibration and held-out validation sets.

- Fit only the small-string calibration family identified in the inventory.
  Choose parameter bounds and optimization settings using those cases and
  numerical controls, without using held-out residuals.
- Reserve larger strings, alternate BL biases, and alternate selected-WL
  positions for prediction tests using one frozen parameter set.
- Keep taper cases in a separate challenge set. They become validation only
  when diameter dependence is predicted from independently specified geometry.
  If taper curves are used to fit that dependence, explicitly move them into
  a new calibration version and reserve other complete cases for testing.
- Freeze a versioned manifest and dataset hash before fitting. Any later
  repartition produces a new version and explains which previous results are
  exploratory rather than held out.

The dataset can be extracted independently of the nonlinear implementation.
Do not tune current resistances in EvaCAM during extraction or incorporate
EvaCAM outputs into the reference coordinates.

## 5. Publish the data contract and quality checks

Proposed artifacts:

| Path | Content |
| --- | --- |
| `docs/validation/nand-park-2025.reference.yaml` | Bibliography, source hashes, geometry, case conditions, and split assignments |
| `docs/validation/data/park-2025/curves.json` | SI observations, uncertainties, source-series identity, and censoring flags |
| `docs/validation/data/park-2025/extraction.json` | Axis transforms, pixel coordinates, and extraction audit |
| `scripts/check_nand_park_reference.py` | Schema, provenance, split, and numerical sanity checks |
| `tests/test_nand_park_reference.py` | Invalid-input and dataset-integrity regressions |
| `docs/validation/nand-park-2025.md` | Source inventory, usable cases, unresolved inputs, and extraction quality |
| `output/validation/nand-park-2025/` | Reconstructed plots and extraction residual/uncertainty figures |

Checks must cover unique IDs, finite values, valid SI conversions, axis inversion,
monotonic sweep coordinates where applicable, positive uncensored log currents,
unambiguous units, trace/source provenance, and disjoint case partitions.
Use compact synthetic extraction fixtures to test known linear/log coordinates;
tests must not merely compare the extractor with itself.

Add a proposed `make test-nand-park-reference` target to the appropriate test
aggregation and `.github/workflows/cpp-tests.yml`. A proposed
`make validate-nand-park-reference` command should regenerate quality reports
from the frozen data offline. Keep future device definitions in ordinary
`.memory_device.yaml` files; this reference manifest is experimental data,
not a new device input format.

## Completion gates and unresolved information

The extraction is complete when every requested curve is either represented
with traceable coordinates and uncertainty or listed as unusable with a reason;
all case conditions have explicit provenance/status; splits are frozen; checks
pass; and plots have been visually compared with the source figures.

Acceptance is per case. Cases lacking decisive operating conditions remain
ineligible for quantitative model validation even if their curve is readable.
A missing full BSIM card does not prevent curve extraction, but it prevents
claiming an exact reproduction of the authors' circuit implementation.

Publish a handoff table identifying which observables can support fixed-bias
resistance checks, nonlinear DC calibration, held-out nonlinear DC validation,
or no present quantitative claim. Include suggested additional information
to request from the authors, without sending messages as part of this work.

## Dependencies and next use

This plan is independent of the
[Kondo bitline benchmark](nand-kondo-2022-plan.md). Its frozen dataset and
missing-input audit are prerequisites for paper-based calibration in the
[nonlinear NAND3D model plan](../nand-3d-nonlinear-model-plan.md).
