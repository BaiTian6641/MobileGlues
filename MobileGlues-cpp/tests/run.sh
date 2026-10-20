#!/bin/sh
# Host-side checks for the parts of the pixel and framebuffer code that are pure
# enough to run without a GPU. Both link the real translation units, not copies.
#
#   sh MobileGlues-cpp/tests/run.sh
set -e
cd "$(dirname "$0")/.."
INC="-I. -I./includes -I./include -I./3rdparty/xxhash"
CXX="${CXX:-g++} -std=gnu++20 -w $INC"
$CXX -o /tmp/mg_pixel_test  tests/pixel_size_test.cpp         gl/pixel.cpp
$CXX -o /tmp/mg_fb_test     tests/framebuffer_shuffle_test.cpp gl/framebuffer.cpp
$CXX -o /tmp/mg_shader_src_test tests/shader_source_test.cpp  gl/shader_source.cpp
$CXX -o /tmp/mg_depth_test  tests/depth_filter_test.cpp       gl/depth_filter.cpp
$CXX -o /tmp/mg_cache_key_test tests/cache_key_test.cpp      gl/glsl/cache_key.cpp
$CXX -o /tmp/mg_image_test tests/compat_image_test.cpp gl/glsl/compat_image.cpp
$CXX -o /tmp/mg_subgroup_test tests/compat_subgroup_test.cpp gl/glsl/compat_subgroup.cpp gl/glsl/compat_subgroup_partition.cpp
/tmp/mg_pixel_test
echo
/tmp/mg_fb_test
echo
/tmp/mg_shader_src_test
echo
/tmp/mg_depth_test
echo
/tmp/mg_cache_key_test
echo
/tmp/mg_image_test
echo
/tmp/mg_subgroup_test
