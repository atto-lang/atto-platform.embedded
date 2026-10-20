cmake_minimum_required(VERSION 3.20)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ores_embedded_zephyr_reference C)
# Zephyr 4.3.0 GNU Arm libgcc can contribute objects without .note.GNU-stack.
# Do not let those legacy inputs silently mark the final ARM ELF GNU_STACK
# executable. This affects ELF metadata, not MPU enforcement on the MCU.
zephyr_ld_options(-Wl,-z,noexecstack)

# Zephyr v4.3.0's ARM linker script only declares .ARM.extab when C++ or
# Rust is enabled. libgcc's integer arithmetic helpers emit unwind tables
# even in C-only builds. Explicitly place that input in flash, rather than
# relying on orphan placement, without enabling C++ on bare-metal targets.
# Do not add a second output section where Zephyr already provides one.
if(NOT CONFIG_CPP AND NOT CONFIG_RUST)
  if(CONFIG_CMAKE_LINKER_GENERATOR)
    zephyr_linker_section(NAME .ARM.extab GROUP ROM_REGION)
    zephyr_linker_section_configure(SECTION .ARM.extab INPUT ".ARM.extab*")
    zephyr_linker_section_configure(SECTION .ARM.extab INPUT ".gnu.linkonce.armextab.*")
  else()
    zephyr_linker_sources(ROM_SECTIONS
      "${CMAKE_CURRENT_LIST_DIR}/linker/ores_arm_extab.ld")
  endif()
endif()
# This LLVM target is ARMv7-M Thumb-2, not ARMv6-M or RISC-V.
if(NOT CONFIG_ARM OR NOT CONFIG_CPU_CORTEX_M)
  message(FATAL_ERROR "Oreslang thumbv7m requires an ARM Cortex-M board.")
endif()
if(CONFIG_ARMV6_M_ARMV8_M_BASELINE)
  message(FATAL_ERROR "Oreslang thumbv7m requires Thumb-2; Cortex-M0/M23 is unsupported.")
endif()
# The current Oreslang LLVM guest is emitted as thumbv7m-none-eabi with
# Cortex-M3-compatible, soft-float AAPCS32 calls. Zephyr may otherwise emit
# hard-float ARMv7E-M code that GNU ld refuses to combine with this guest.
# Fail during configure, not after linking hundreds of firmware objects.
if(CONFIG_FP_HARDABI)
  message(FATAL_ERROR
    "Oreslang thumbv7m-none-eabi guest uses soft-float AAPCS32, but Zephyr "
    "selected CONFIG_FP_HARDABI=y. Use a target-matched compiler backend "
    "or an explicitly reviewed CONFIG_FP_SOFTABI=y compatibility profile.")
endif()

# Zephyr 4.3.0's UNO R4 linker only places .ARM.extab explicitly in ROM
# when CPP or RUST is enabled. With neither, GNU libgcc's ARM unwind table
# can be orphaned into the 28-byte RA4M1 option-setting memory region.
# The opt-in uno_r4_compat.conf profile sets both FP_SOFTABI and CPP.
if(BOARD MATCHES "^arduino_uno_r4" AND NOT CONFIG_CPP AND NOT CONFIG_RUST)
  message(FATAL_ERROR
    "Zephyr 4.3.0 UNO R4 requires explicit .ARM.extab ROM placement "
    "for the Oreslang actor firmware. Use zephyr/uno_r4_compat.conf "
    "or fix the board linker script; do not suppress overflow errors.")
endif()
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
    --forbid-symbol "unreachable_tree_shake_probe"
  DEPENDS "${ORE_SOURCE}" "${ORE_ROOT}/scripts/compile-cortexm-object.py"
          "${ORE_ROOT}/scripts/bounded_process.py" "${ORESLANG_LLVMC}"
  COMMENT "Compiling real Oreslang source actors to Cortex-M native object"
  VERBATIM)
set_source_files_properties("${ORE_OBJECT}" PROPERTIES GENERATED TRUE EXTERNAL_OBJECT TRUE)
target_sources(app PRIVATE src/main.c "${ORE_ROOT}/src/ores_embedded_static_actors.c" "${ORE_ROOT}/src/ores_embedded_mailbox.c" "${ORE_OBJECT}")
target_include_directories(app PRIVATE "${ORE_ROOT}/include")
