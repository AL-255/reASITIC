# reASITIC · C++ / WebAssembly

A browser recreation of ASITIC, targeting GitHub Pages. The recovered C++
implementation from `~/github/asitic-re` is preserved unchanged under
`vendor/asitic`; `provenance.json` pins the source commit and every file hash.

Work in progress. The target is original ASITIC geometry and simulation algorithms,
including its technology model, filament solver, substrate effects, and metrics.
Identical results must be demonstrated against the original; compiling recovered
source to WebAssembly alone is not proof of numerical identity. In particular,
x87 extended arithmetic and transcendental instructions require a fidelity layer.

No original executable is needed by the deployed application. The C++ sources are
the math kernel. JavaScript handles the interface and GDSII serialization.
