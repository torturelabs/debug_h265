## Checkout

```
git clone https://github.com/torturelabs/debug_h265 --recursive
```

## Build

Prepare configuration, native code:

```
cmake -H. -Bbuild -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DSUPPORTS_SSE4_1=ON
```

Compilation to WASM:

```
source ~/git/emsdk/emsdk_env.sh
emconfigure cmake -H. ......
```

Make build (in all cases)

```
cmake --build build -- VERBOSE=1
```
