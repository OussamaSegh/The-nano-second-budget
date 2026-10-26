cmake_minimum_required(VERSION 3.20)
project(llds_book LANGUAGES CXX)

# Companion code for "The Nanosecond Budget: Fast, Correct Concurrent
# Data Structures in Modern C++".
#
#   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build -j
#   ctest --test-dir build                      # library + solution tests
#   ctest --test-dir build -L exercise          # your exercise attempts
#
#   cmake -S . -B build-tsan -DLLDS_SANITIZE=thread   # ThreadSanitizer build


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(LLDS_SANITIZE "" CACHE STRING "Sanitizer to enable: thread, address, undefined, or empty")
option(LLDS_NATIVE "Compile with -march=native" OFF)

find_package(Threads REQUIRED)

add_library(llds INTERFACE)
target_include_directories(llds INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(llds INTERFACE Threads::Threads)
target_compile_options(llds INTERFACE -Wall -Wextra -Wpedantic)
if(LLDS_NATIVE)
  target_compile_options(llds INTERFACE -march=native)
endif()
if(LLDS_SANITIZE)
  target_compile_options(llds INTERFACE -fsanitize=${LLDS_SANITIZE} -fno-omit-frame-pointer -g)
  target_link_options(llds INTERFACE -fsanitize=${LLDS_SANITIZE})
  if(LLDS_SANITIZE STREQUAL "thread" AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # GCC warns that TSan does not model std::atomic_thread_fence. That is
    # true (see Chapter 16) and the warning is noise in every TSan build.
    target_compile_options(llds INTERFACE -Wno-tsan)
  endif()
endif()

enable_testing()

# --- library tests ---------------------------------------------------------
file(GLOB LLDS_TESTS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_*.cpp)
foreach(src ${LLDS_TESTS})
  get_filename_component(name ${src} NAME_WE)
  add_executable(${name} ${src})
  target_link_libraries(${name} PRIVATE llds)
  add_test(NAME ${name} COMMAND ${name})
  set_tests_properties(${name} PROPERTIES LABELS "library" TIMEOUT 300)
endforeach()

# --- chapter examples and benchmarks ---------------------------------------
file(GLOB LLDS_EXAMPLES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/examples/*.cpp)
foreach(src ${LLDS_EXAMPLES})
  get_filename_component(name ${src} NAME_WE)
  add_executable(${name} ${src})
  target_link_libraries(${name} PRIVATE llds)
  # Every example must at least run to completion in "smoke" mode.
  add_test(NAME smoke_${name} COMMAND ${name} --smoke)
  set_tests_properties(smoke_${name} PROPERTIES LABELS "smoke" TIMEOUT 300)
endforeach()
# ch16_stress contains a deliberate data race. Under ThreadSanitizer the
# smoke test is *expected* to fail: that failure is the point of the example.
if(LLDS_SANITIZE STREQUAL "thread" AND TARGET ch16_stress)
  set_tests_properties(smoke_ch16_stress PROPERTIES WILL_FAIL TRUE)
endif()

# --- exercises ---------------------------------------------------------------
# Each exercise test in tests/exercises/chNN_*.cpp is built twice:
#   ex_<name>   against exercises/chNN/  (your work; label "exercise")
#   sol_<name>  against solutions/chNN/  (reference;  label "solution")
# The directory is the part of the name before the first underscore:
# chNN for chapter exercises, fNN for the Foundations (Part 0).
file(GLOB LLDS_EX_TESTS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/exercises/ch*.cpp
                                          ${CMAKE_CURRENT_SOURCE_DIR}/tests/exercises/f*.cpp)
foreach(src ${LLDS_EX_TESTS})
  get_filename_component(name ${src} NAME_WE)
  string(REGEX REPLACE "_.*$" "" chapter ${name})
  foreach(kind ex sol)
    if(kind STREQUAL "ex")
      set(dir ${CMAKE_CURRENT_SOURCE_DIR}/exercises/${chapter})
      set(label exercise)
    else()
      set(dir ${CMAKE_CURRENT_SOURCE_DIR}/solutions/${chapter})
      set(label solution)
    endif()
    add_executable(${kind}_${name} ${src})
    target_include_directories(${kind}_${name} PRIVATE ${dir} ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    target_link_libraries(${kind}_${name} PRIVATE llds)
    add_test(NAME ${kind}_${name} COMMAND ${kind}_${name})
    set_tests_properties(${kind}_${name} PROPERTIES LABELS ${label} TIMEOUT 300)
  endforeach()
endforeach()
