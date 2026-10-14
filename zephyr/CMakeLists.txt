cmake_minimum_required(VERSION 3.20)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ores_embedded_zephyr_reference C)

# Compile the actual .ores source using the LLVM compiler and cross-compile
# its emitted IR to a Cortex-M3 object, then link that object into Zephyr.
# This explicitly does NOT compile guest actor/scheduler support.
if(NOT DEFINED ORESLANG_LLVMC OR NOT EXISTS "${ORESLANG_LLVMC}")
  message(FATAL_ERROR "Pass -DORESLANG_LLVMC=/absolute/path/to/oreslang-llvmc")
endif()
find_package(Python3 COMPONENTS Interpreter REQUIRED)
find_program(ORE_CLANG NAMES clang-19 clang-18 clang-17 clang-16 clang REQUIRED)
set(ORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
set(ORE_SOURCE "${CMAKE_CURRENT_LIST_DIR}/ores/firmware.ores")
set(ORE_OBJECT "${CMAKE_CURRENT_BINARY_DIR}/oreslang_cortexm_probe.o")
add_custom_command(
  OUTPUT "${ORE_OBJECT}"
  COMMAND "${Python3_EXECUTABLE}"
    "${ORE_ROOT}/scripts/compile-cortexm-object.py"
    --compiler "${ORESLANG_LLVMC}" --clang "${ORE_CLANG}"
    --source "${ORE_SOURCE}" --output "${ORE_OBJECT}"
  DEPENDS "${ORE_SOURCE}" "${ORE_ROOT}/scripts/compile-cortexm-object.py"
          "${ORESLANG_LLVMC}"
  COMMENT "Compiling real Oreslang guest source to Cortex-M3 native object"
  VERBATIM)
set_source_files_properties("${ORE_OBJECT}" PROPERTIES GENERATED TRUE EXTERNAL_OBJECT TRUE)
target_sources(app PRIVATE src/main.c "${ORE_ROOT}/src/ores_embedded_mailbox.c" "${ORE_OBJECT}")
target_include_directories(app PRIVATE "${ORE_ROOT}/include")
