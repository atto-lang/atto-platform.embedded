cmake_minimum_required(VERSION 3.20)
project(ores_embedded_mailbox VERSION 0.1.0 LANGUAGES C)
include(CTest)

add_library(ores_embedded_mailbox STATIC src/ores_embedded_mailbox.c)
target_include_directories(ores_embedded_mailbox PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(ores_embedded_mailbox PUBLIC c_std_11)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(ores_embedded_mailbox PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_library(ores_embedded_irq_bridge STATIC src/ores_embedded_irq_bridge.c)
target_include_directories(ores_embedded_irq_bridge PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(ores_embedded_irq_bridge PUBLIC c_std_11)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(ores_embedded_irq_bridge PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_library(ores_embedded_static_actors STATIC src/ores_embedded_static_actors.c)
target_include_directories(ores_embedded_static_actors PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(ores_embedded_static_actors PUBLIC c_std_11)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(ores_embedded_static_actors PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
  find_package(Python3 COMPONENTS Interpreter REQUIRED)
  # Host-link probe: freestanding C actor adapter, NOT source OTP or core.
  add_test(NAME static_actor_freestanding_no_libc_link
    COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_static_actor_freestanding.py"
      --cc "${CMAKE_C_COMPILER}"
      --source-root "${CMAKE_CURRENT_SOURCE_DIR}")
  set_tests_properties(static_actor_freestanding_no_libc_link PROPERTIES TIMEOUT 30)
  add_test(NAME linker_orphan_log_regressions
    COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_check_linker_orphans.py")
  add_test(NAME qemu_guest_output_validator
    COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_verify_qemu_output.py")
  add_test(NAME elf_linker_layout_fixtures
    COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_audit_arm_elf.py")
  add_test(NAME bounded_process_regressions
    COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_bounded_process.py")
  add_test(NAME embedded_prelink_no_gc_gate
    COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_embedded_no_gc_gate.py")
  find_package(Threads REQUIRED)
  add_executable(test_embedded_irq_bridge tests/test_embedded_irq_bridge.c)
  target_link_libraries(test_embedded_irq_bridge PRIVATE
    ores_embedded_irq_bridge Threads::Threads)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_embedded_irq_bridge PRIVATE
      -UNDEBUG -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME irq_bridge_spsc_40000_events COMMAND test_embedded_irq_bridge)
  add_executable(test_embedded_static_actors tests/test_embedded_static_actors.c)
  target_link_libraries(test_embedded_static_actors PRIVATE ores_embedded_static_actors)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_embedded_static_actors PRIVATE -UNDEBUG -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME static_actor_contract COMMAND test_embedded_static_actors basic)
  add_executable(test_embedded_actor_abi
    tests/cortexm_static_actor_link_smoke.c
    tests/host_static_actor_abi_driver.c)
  target_link_libraries(test_embedded_actor_abi PRIVATE ores_embedded_static_actors)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_embedded_actor_abi PRIVATE -UNDEBUG -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME static_actor_abi_smoke COMMAND test_embedded_actor_abi)
  foreach(case stale overflow overflow-scrub invalid-query-scrub capacity-stress exhaustion-scrub reset fault-before-spawn-reset bad-initializer missing-initializer bad-handler fault-after-success handler-fault-scrub-others cross-actor-heap actor-to-actor-send actor-to-actor-try-full actor-to-actor-strict-full self-send-rejected constructor-send-rejected capability-copy-send capability-try-full capability-strict-full capability-stale capability-constructor-rejected capability-bad-slot owned-child-drain owned-child-constructor-fault post-retire-wipe fault-retire-wipe guest-cross-actor-last guest-cross-actor-deliveries guest-constructor-telemetry guest-global-faulted guest-global-spawns guest-global-active guest-global-constructor guest-spawn guest-stop guest-reset immutable-field double-stop)
    add_test(NAME static_actor_${case} COMMAND test_embedded_static_actors ${case})
  endforeach()
  add_executable(test_embedded_mailbox tests/test_embedded_mailbox.c)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_embedded_mailbox PRIVATE -UNDEBUG)
  endif()
  target_link_libraries(test_embedded_mailbox PRIVATE ores_embedded_mailbox)
  add_test(NAME mailbox_contract COMMAND test_embedded_mailbox)
  add_executable(test_embedded_mailbox_model tests/test_embedded_mailbox_model.c)
  target_link_libraries(test_embedded_mailbox_model PRIVATE ores_embedded_mailbox)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_embedded_mailbox_model PRIVATE -UNDEBUG -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME mailbox_model_131072_ops COMMAND test_embedded_mailbox_model)
  add_executable(embedded_host_demo examples/host_demo.c)
  target_link_libraries(embedded_host_demo PRIVATE ores_embedded_mailbox)
  add_test(NAME embedded_host_demo COMMAND embedded_host_demo)
endif()
