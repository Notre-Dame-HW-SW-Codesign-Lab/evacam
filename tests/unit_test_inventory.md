# Unit Test Inventory

`unit_test_inventory.tsv` is the Phase 1 source of truth for dedicated unit-test
coverage. It inventories maintained callable definitions in `include/`, `src/`,
`bindings/`, `evacam/`, and `scripts/`.

The current baseline contains 975 callables:

- 49 `covered`
- 827 `missing`
- 99 `exempt`
- 821 C++ callables and 154 Python callables

The deliberately conservative classification counts a callable as covered only
when a named `Test*` or `test_*` case reaches a matching call in a focused test
module. Calls made only from a broad `main`, calls from regression tests, and
ambiguous same-arity overloads remain missing. This avoids treating incidental
execution as dedicated coverage. The behavior column is derived from the names
of the focused cases that exercise the callable.

The C++ inventory uses a tree-sitter C++ syntax tree, including inline methods,
templates, overloads, file-local helpers, custom special members, and
defaulted/deleted/pure-virtual declarations. Python uses the standard-library
`ast` module. Defaulted/deleted special members and pure-virtual declarations
are retained as reviewed exemptions rather than disappearing from the list.

Generate or verify the inventory with:

```sh
make unit-test-inventory
make check-unit-test-inventory
```

The generator requires the development-only `tree-sitter-language-pack` Python
package. It does not affect the EvaCAM runtime package.

When adding a test, give it a descriptive `Test*` or `test_*` name and regenerate
the inventory in the same change. A `covered` row is a starting assertion-level
mapping, not permission to skip boundary and error branches. Update the
behavior/refactor policy in the generator when a callable needs a reviewed
classification that cannot be inferred safely.
