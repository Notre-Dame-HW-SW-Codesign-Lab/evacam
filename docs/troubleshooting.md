# Troubleshooting

## Build Fails Due To Missing `yaml-cpp`

Symptoms:

- compiler cannot find YAML headers
- linker fails to resolve `yaml-cpp`

Check:

- `yaml-cpp` is installed
- headers are visible to the compiler
- the library is visible to the linker

## `Config file ... does not exist`

EvaCAM validates the input config path before parsing. Confirm the file exists and that you are running from the expected working directory.

## YAML Parse Errors

Symptoms usually look like:

- `YAML error at line ...`

Check:

- indentation
- misspelled keys
- malformed lists such as `[1, 1]`
- invalid unit suffixes

## Missing Or Broken `memory.cell_file`

If the top-level config parses but the referenced cell file is wrong, the run will fail during config loading. Verify the path under `memory.cell_file` first.

## `No valid solutions.`

This is usually not a parser problem. It normally means the selected constraints, geometry, or peripheral settings do not produce any legal design point.

Try:

- starting from a shipped example
- changing one parameter at a time
- relaxing constraints
- disabling unusual peripheral options
- comparing against a nearby known-good config

## Unexpected Result File Location

Default YAML output goes to:

```text
results/<config-base>_results.yaml
```

Use `-o` if you need an explicit path.

## Exploration Run Did Not Write CSV

The exploration CSV is only written for full-exploration runs without pruning. If pruning is enabled, the YAML results still exist but the CSV may not.

## Verbose Logging

Use:

```bash
./EvaCAM -v yaml/config/2FeFET_TCAM_config.yaml
```

Verbose mode is the fastest way to see where startup or config loading stops.

For unsupported configurations and hard runtime limits, see [limitations.md](/home/jbech002/Research/evacam/docs/limitations.md).
