cmake_minimum_required(VERSION 3.15)

project(debug_h265)

include(CMakePackageConfigHelpers)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(libde265)
include_directories(.)

include(CheckIncludeFile)

check_include_file("malloc.h" HAVE_MALLOC_H)

include(CheckFunctionExists)
check_function_exists(posix_memalign HAVE_POSIX_MEMALIGN)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)
include_directories(${PROJECT_BINARY_DIR})

find_package(SDL2)
if(SDL2_FOUND)
    add_definitions(-DHAS_SDL)
endif(SDL2_FOUND)
include_directories(${SDL2_INCLUDE_DIRS})

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
  #  en265.cc
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

#add_subdirectory (encoder)

if(SUPPORTS_SSE4_1)
  add_definitions(-DHAVE_SSE4_1)
  add_subdirectory (x86)
endif()

add_executable(${PROJECT_NAME} ${libde265_sources} ${ENCODER_OBJECTS} ${X86_OBJECTS})
find_package(Threads)
target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads ${SDL2_LIBRARIES})
