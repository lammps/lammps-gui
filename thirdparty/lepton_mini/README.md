# LeptonMini (vendored Lepton subset)

This is a reduced, vendored copy of the [Lepton](https://github.com/openmm/openmm)
expression parser, taken from the copy bundled with LAMMPS at `lib/lepton/`
(which originates from the OpenMM project by Peter Eastman et al.).

It is used by LAMMPS-GUI for parsing, evaluating, and symbolically
differentiating user-supplied mathematical expressions (e.g. custom
plot functions and custom fit models).

The umbrella header is `lepton_mini.h` and everything lives in the
**`LeptonMini`** namespace (see "Namespace" below).

## License

Lepton is distributed under a permissive MIT-style license (see `LICENSE`),
which is compatible with the GPLv2+ license of LAMMPS-GUI.

## What was kept vs. removed

Only the **core** functionality needed by LAMMPS-GUI is vendored here:

- `Parser`            -- parse an expression string into an AST
- `ParsedExpression`  -- the parsed expression: `evaluate()`, `optimize()`,
                         and analytic `differentiate()`
- `ExpressionTreeNode`, `Operation` -- the AST node types
- `ExpressionProgram` -- an interpreted (stack-machine) evaluator
- `Exception`, `CustomFunction` -- support types

The following was **removed**, because LAMMPS-GUI only needs interpreted
evaluation and symbolic differentiation, not native code generation:

- the bundled `asmjit/` JIT assembler library (~3.8 MB)
- `CompiledExpression` and `CompiledVectorExpression` (the asmjit-based
  just-in-time evaluators)

Accordingly, the umbrella header no longer includes `CompiledExpression.h`, and
the `ParsedExpression::createCompiledExpression()` /
`createCompiledVectorExpression()` methods were dropped from
`ParsedExpression.{h,cpp}`.

## Namespace

LAMMPS itself contains a (full) copy of Lepton in its original `Lepton`
namespace. To avoid duplicate-symbol / ODR clashes when the GUI is linked
against or dynamically loads `liblammps`, this subset has been moved into the
dedicated **`LeptonMini`** namespace. The folder, the umbrella header
(`lepton_mini.h`), and the internal include subdirectory (`lepton_mini/`) were
renamed to match. These (the namespace rename, the dropped JIT path, and the
renames) are the only deviations from the upstream source.

## Build integration

The top-level `CMakeLists.txt` builds these sources into a `lepton_mini`
STATIC library (with `include/` exposed as a PUBLIC include directory) and
links it into `lammps-gui`. The `test/test_lepton` unit test exercises the
vendored subset (parsing, evaluation, optimization, symbolic differentiation,
and custom functions).

## TODO before use in the GUI

- Vendor a compact least-squares / Levenberg-Marquardt routine for nonlinear
  fits, and wire custom-function plotting and fitting into the chart
  post-processing dialog.
