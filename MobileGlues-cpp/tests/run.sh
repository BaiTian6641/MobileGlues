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
$CXX -o /tmp/mg_image_metadata_test tests/image_metadata_test.cpp gl/image_metadata.cpp
$CXX -o /tmp/mg_image_uniform_write_test tests/image_uniform_write_test.cpp
$CXX -o /tmp/mg_subgroup_test tests/compat_subgroup_test.cpp gl/glsl/compat_subgroup.cpp gl/glsl/compat_subgroup_partition.cpp
$CXX -o /tmp/mg_fsr_contract_test tests/fsr_contract_test.cpp
$CXX -o /tmp/mg_fsr1_core_test tests/fsr1_core_test.cpp
$CXX -o /tmp/mg_fsr3_viewport_test tests/fsr3_viewport_test.cpp
$CXX -o /tmp/mg_fsr3_motion_math_test tests/fsr3_motion_math_test.cpp 
$CXX -o /tmp/mg_diagnostic_format_test tests/diagnostic_format_test.cpp
$CXX -o /tmp/mg_diagnostic_disabled_test tests/diagnostic_disabled_test.cpp
/tmp/mg_fsr1_core_test
echo
/tmp/mg_fsr3_viewport_test
echo
/tmp/mg_fsr3_motion_math_test
/tmp/mg_diagnostic_format_test
/tmp/mg_diagnostic_disabled_test
echo
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
/tmp/mg_image_metadata_test
/tmp/mg_image_uniform_write_test
echo
/tmp/mg_subgroup_test
echo
/tmp/mg_fsr_contract_test
