#!/usr/bin/env bash

pushd build
~/git/v8/v8/out.gn/x64.release/d8 --experimental-wasm-threads debug_h265.js
popd
