## Checkout

```
git clone https://github.com/torturelabs/debug_h265 --recursive
```

## Download sample H.265 bytestream

```
wget https://camera.torturelabs.com/var.h265
```

## Build

Prepare configuration, native code:

```
cmake -H. -Bbuild -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DSUPPORTS_SSE4_1=ON
```

Compilation to WASM:

```
source ~/git/emsdk/emsdk_env.sh
emcmake cmake -H. ......
```

Build using Intel C++ compiler (native code only), add to cmake flags:

```
-DCMAKE_CXX_COMPILER=icpc -DCMAKE_C_COMPILER=icc
```

Make build (in all cases)

```
cmake --build build -- VERBOSE=1
```

## Run

WASM version with standalone V8 installed:

```
cd build ; d8 debug_h265.js
```
