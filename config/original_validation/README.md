# Original EvaCAM paper validation fixtures

These fixtures reconstruct the direct two-FeFET, MRAM and PCM cases behind
DATE 2022 Tables I–II. They are separate from the seven named-paper diagnostics.
They are **partial analytical reconstructions**, not calibrated reproductions.

Run `make validate-original-evacam` from the repository root. See
[the source audit and remaining limits](../../docs/validation/original-evacam-validation.md)
and [every input's provenance](../../docs/validation/original-evacam.inputs.yaml).

PCM deliberately retains the inherited 70 mV sense requirement and is currently
infeasible with the corrected access/leakage model. The suite checks that outcome.
SAPIENS has an explicit architecture/workload record in the manifest; it has no
native exact-TCAM fixture because its shared-SA serial L1 computation differs.
