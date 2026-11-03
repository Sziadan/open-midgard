# Independent GR2 reader

Vendored from [granny-ro-js](https://github.com/Stitchuuuu/granny-ro-js),
commit `955189e060bf38270cbc3ea8cba610e98c7f62dc` (MIT; see LICENSE).
This is an independent implementation, not the Granny SDK.

`src/` contains the upstream JavaScript sources. `client.js` selects the decoded
model data and top-level texture mip used by this client. `qt-compat.js` supplies
UTF-8 decoding for Qt's JavaScript engine. The checked-in generated header means
that neither Node nor esbuild is required to build or run the client.

Regenerate after changing these sources with esbuild 0.25.12:

```
python tools/build_gr2_reader.py --esbuild /path/to/esbuild
```

GR2 loading runs in isolated background Qt JavaScript engines. C++ owns the
decoded data, evaluates animation curves, composes skeleton poses and skins
meshes each frame. No external decoder process or proprietary runtime is used.
