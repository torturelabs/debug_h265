## Build

Prepare configuration:

```
cmake -H. -Bbuild -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DSUPPORTS_SSE4_1=ON
```

Make build

```
cmake --build build -- VERBOSE=1
```
