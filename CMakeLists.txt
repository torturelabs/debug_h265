cmake_minimum_required(VERSION 3.15)

project(debug_h265)

include(CMakePackageConfigHelpers)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

include_directories(libde265)
include_directories(.)

include(CheckIncludeFile)

check_include_file("malloc.h" HAVE_MALLOC_H)

include(CheckFunctionExists)
check_function_exists(posix_memalign HAVE_POSIX_MEMALIGN)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)
include_directories(${PROJECT_BINARY_DIR})

if(CMAKE_C_COMPILER MATCHES ".*emcc")
    set(EMSCRIPTEN 1)
endif()

if(NOT EMSCRIPTEN)
    list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/sdl2)
    find_package(SDL2 REQUIRED)
    add_definitions(-DHAVE_SDL2)
    list(APPEND TARGET_LIBS SDL2::Main)
endif()

set (libde265_sources
  hw.c
  alloc_pool.cc
  bitstream.cc
  cabac.cc
  configparam.cc
  contextmodel.cc
  de265.cc
  deblock.cc
  decctx.cc
  dpb.cc
  fallback-dct.cc
  fallback-motion.cc
  fallback.cc
  image-io.cc
  image.cc
  intrapred.cc
  md5.cc
  motion.cc
  nal-parser.cc
  nal.cc
  pps.cc
  quality.cc
  refpic.cc
  sao.cc
  scan.cc
  sei.cc
  slice.cc
  sps.cc
  threads.cc
  transform.cc
  util.cc
  visualize.cc
  vps.cc
  vui.cc
)

set (libde265_headers
  acceleration.h
  alloc_pool.h
  bitstream.h
  cabac.h
  configparam.h
  deblock.h
  decctx.h
  dpb.h
  en265.h
  fallback-dct.h
  fallback-motion.h
  fallback.h
  image-io.h
  image.h
  intrapred.h
  md5.h
  motion.h
  nal-parser.h
  nal.h
  pps.h
  quality.h
  refpic.h
  sao.h
  scan.h
  sei.h
  slice.h
  sps.h
  threads.h
  transform.h
  util.h
  visualize.h
  vps.h
  vui.h
)


add_definitions(-DLIBDE265_EXPORTS)

if(SUPPORTS_SSE4_1)
  add_definitions(-DHAVE_SSE4_1)
  add_subdirectory (x86)
endif()

if(EMSCRIPTEN AND WASM_SIMD)
    option(WASM_SIMD "Turn on SIMD support for WASM" OFF)
    add_definitions(-DWASM_SIMD)
    add_subdirectory(wasm)
endif()

add_executable(${PROJECT_NAME} ${libde265_sources} ${ENCODER_OBJECTS}
    ${X86_OBJECTS} ${WASM_OBJECTS})

if(PERF_TRACE)
    target_compile_options(${PROJECT_NAME} PUBLIC "-fno-omit-frame-pointer")
endif()

if(EMSCRIPTEN)
    option(BROWSER_RUN "Compiles and run in browser" OFF)
    if(BROWSER_RUN)
        set(CMAKE_EXECUTABLE_SUFFIX ".html")
    endif()

    option(WASM_THREADS "Turn on threads support for WASM" OFF)
    if(WASM_THREADS)
        add_definitions(-DTHREADS_SUPPORT)
        target_compile_options(${PROJECT_NAME} PUBLIC "SHELL:-s USE_PTHREADS=1")
        target_compile_options(${PROJECT_NAME} PUBLIC "SHELL:-s PTHREAD_POOL_SIZE=2")
        target_link_options(${PROJECT_NAME} PUBLIC "SHELL:-s USE_PTHREADS=1")
        target_link_options(${PROJECT_NAME} PUBLIC "SHELL:-s PTHREAD_POOL_SIZE=2")
    endif()

    if(WASM_SIMD)
        target_compile_options(${PROJECT_NAME} PUBLIC "SHELL:-msimd128")
    endif()

    target_link_options(${PROJECT_NAME} PUBLIC "SHELL:-s ASSERTIONS=1")
    target_link_options(${PROJECT_NAME} PUBLIC "SHELL:-s TOTAL_MEMORY=1073741824")

    set(H265_FILENAME var.h265)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/${H265_FILENAME} ${CMAKE_CURRENT_BINARY_DIR} COPYONLY)
    target_link_options(${PROJECT_NAME} PUBLIC --embed-file
        ${CMAKE_CURRENT_BINARY_DIR}/${H265_FILENAME})
else()
    find_package(Threads)
    target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads ${TARGET_LIBS})
    add_definitions(-DTHREADS_SUPPORT)
endif()
