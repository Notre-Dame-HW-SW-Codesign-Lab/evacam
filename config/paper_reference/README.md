# Paper reference configurations

These seven fixtures match the published fields identified in
[the reference manifest](../../docs/validation/named-cam.reference.yaml).
Each subdirectory has a runnable config and its local component files;
technology and sense-amplifier libraries are shared from `config/lib/`.

They are **partial reconstructions**, not calibrated reproductions. The manifest
distinguishes sourced values from inherited assumptions. In particular, generic
sensing, device models, temperature and measurement scope still limit comparisons.

Run all fixtures and regenerate the comparison:

```sh
make validate-named-cam
```

Run one fixture:

```sh
./EvaCAM config/paper_reference/MRAM-6T2R-VLSIC11/MRAM-6T2R-VLSIC11.config.yaml
```

See [current results and remaining work](../../docs/validation/analytical-cam-timing.md).
`make validate-analytical-cam` isolates the decision, scheduling and amplifier
effects for the two references using explicit analytical timing.
The original conference-named exploration examples remain available alongside
this directory. New paper evidence should update the manifest and corresponding
fixture together; a smaller numerical gap alone is not evidence for an input value.
