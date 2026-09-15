# Provisional dual-FeFET pair response

MCAM supports an optional `mcam.pair_resistance` square matrix in the memory
device YAML. Entries are **numeric ohms**, indexed `[stored_symbol][query_symbol]`,
and describe the complete dual-FeFET cell, not either individual transistor.
The matrix takes precedence over `resistance_state` for vector reads. The legacy
`num_resistance_state` and `resistance_state` fields remain required for input
compatibility; the latter is not reinterpreted as eight measured diagonal values.
Without a pair matrix the existing sorted, absolute-distance model is unchanged.

The pair model retains direction and absolute symbol identity. It does not
symmetrize the matrix, force a monotonic response with distance, or force the
diagonal to have the lowest conductance. Searchline voltages retain symbol order.
It currently requires variation to be disabled and a common matchline precharge.
No independent Gaussian uncertainty is invented for the 64 entries.

## Source and limitations

The example under `config/2FeFET_MCAM_pair/` is a **provisional digitization** of
[Kazemi et al., Scientific Reports (2022), Fig. 1d](https://www.nature.com/articles/s41598-022-23116-w).
The query voltages are 25, 175, 325, 475, 625, 775, 925, and 1075 mV, with the
complementary line at `1.1 V - query_voltage`. Curve-to-symbol assignment follows
the left-to-right positions of the current minima and is an inference.
The 25 mV samples lie outside the visible curve segment and are extrapolated.
The extracted currents and extraction metadata are retained in
`results/mcam_pair_characterization/`.

An **assumed, unconfirmed 1.1 V matchline measurement bias** converts current to
effective resistance via `R = 1.1 / I`. This is not a reported calibration for
Fig. 1d. These are constant effective resistances, not a fitted transistor model;
current dependence on the discharging matchline voltage is missing. Absolute
timings and overlap therefore depend on the assumed bias and RC approximation.
The eight upstream resistance constants do not independently establish this
64-entry table. Digitization precision is not measurement accuracy.

## Timing and bounds

The common sensing instant uses the existing RC/Horowitz timing criterion applied
to the slowest nominal exact-match row: every cell uses the largest diagonal
resistance. Individual reported matchline delays still use the actual pair-vector
conductance; exact-match delays are no longer replaced by a mismatch delay in
pair mode. This is a maximum within the adopted table/model, not a guarantee over
uncharacterized devices or process variation.

`distance_voltage_bounds([], False)` in pair mode covers all stored and query
vectors, grouped by squared Euclidean distance. Supplying a query fixes that
query instead. Dynamic programming sums cell conductances and retains exact
nominal extrema and witness vectors, without Monte Carlo or enumerating every
vector pair. Unreachable distances are omitted. Bounds are ranges, not confidence
intervals or distributions. Rows need not realize every value between endpoints.

A single voltage threshold cannot universally separate exact matches if the
lowest exact-match voltage is below a mismatch voltage. Negative margins are
retained with non-strict sensing rather than hidden by changing the table. The
pair-mode `hit` is the ideal symbol equality; `sense_margin_pass` reports whether
the worst-case electrical gap meets the sensing requirement. Distance-threshold
decisions likewise retain the ideal logical criterion and report their gap.

When the global gap is nonpositive and sensing is non-strict, the amplifier's
configured positive input requirement supplies its nominal latency budget;
the negative gap is not passed into the amplifier's logarithmic timing formula.
The signed gap and failed sensing diagnostic are unchanged. A finite reported
search latency in this case is an exploratory timing budget, not a resolving time
or evidence that a reliable exact-match decision is possible. Strict sensing
continues to reject an insufficient gap.

Run `scripts/plot_mcam_pair_response.py` after building the Python binding to
write nominal all-pair bounds and witness CSVs under `results/mcam_pair_response/`.
The historical distance-only Monte Carlo outputs are left intact, rather than
relabeled as results from the pair model.
