# Independent GR2 reader (algorithm reference)

Vendored from [granny-ro-js](https://github.com/Stitchuuuu/granny-ro-js),
commit `955189e060bf38270cbc3ea8cba610e98c7f62dc` (MIT; see LICENSE).
This is an independent implementation, not the Granny SDK.

`src/` holds the upstream JavaScript sources used as the byte-exact oracle while
porting. The client runtime is the C++ port under `src/res/gr2/` (Oodle0,
type-tree walk, IGC/BinkTC, mesh/skeleton/curve extract). Node and esbuild are
not required to build or run the client.

This archive is not a client build input. `client.js` is the former data
adapter; `qt-compat.js` preserves the former Qt-engine compatibility shim.
`package.json` lets development tools import the modules directly in Node
without bundling or installing packages. Run
`python tools/tests/check_gr2_js_parity.py` from the repository root to compare
this reference reader against the native port. The client build and regular
native regression tests do not require Node or these files.

Native-port license notices remain in `third_party/gr2-reader/LICENSE` and are
deployed to `licenses/gr2-reader/LICENSE`.
