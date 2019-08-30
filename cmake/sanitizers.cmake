#
# Set CMAKE_BUILD_TYPE to one of the sanitizers ('asan', 'tsan' or 'ubsan') to automatically build with that sanitizer enabled
#

if("${CMAKE_BUILD_TYPE}" STREQUAL "asan")
  message(STATUS "Build will have ASAN enabled!")
elseif("${CMAKE_BUILD_TYPE}" STREQUAL "tsan")
  message(STATUS "Build will have TSAN enabled!")
elseif("${CMAKE_BUILD_TYPE}" STREQUAL "lsan")
  message(STATUS "Build will have LSAN enabled!")
elseif("${CMAKE_BUILD_TYPE}" STREQUAL "msan")
  message(STATUS "Build will have MSAN enabled!")
elseif("${CMAKE_BUILD_TYPE}" STREQUAL "ubsan")
  message(STATUS "Build will have UBSAN enabled!")
endif()

# AddressSanitize
set(CMAKE_C_FLAGS_ASAN "-fsanitize=address -fno-omit-frame-pointer -g -Og")
set(CMAKE_CXX_FLAGS_ASAN "-fsanitize=address -fno-omit-frame-pointer -g -Og")

# ThreadSanitizer
set(CMAKE_C_FLAGS_TSAN "-fsanitize=thread -fno-omit-frame-pointer -g -Og")
set(CMAKE_CXX_FLAGS_TSAN "-fsanitize=thread -fno-omit-frame-pointer -g -Og")

# LeakSanitizer
set(CMAKE_C_FLAGS_LSAN "-fsanitize=leak -fno-omit-frame-pointer -g -Og")
set(CMAKE_CXX_FLAGS_LSAN "-fsanitize=leak -fno-omit-frame-pointer -g -Og")

# MemorySanitizer (Clang only!)
set(CMAKE_C_FLAGS_MSAN "-fsanitize=memory -fno-optimize-sibling-calls -fsanitize-memory-track-origins=2 -fno-omit-frame-pointer -g -O2")
set(CMAKE_CXX_FLAGS_MSAN "-fsanitize=memory -fno-optimize-sibling-calls -fsanitize-memory-track-origins=2 -fno-omit-frame-pointer -g -O2")

# UndefinedBehaviour
# Run with: UBSAN_OPTIONS=print_stacktrace=1 environment variable
set(CMAKE_C_FLAGS_UBSAN "-fsanitize=undefined -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fno-omit-frame-pointer -g -Og")
set(CMAKE_CXX_FLAGS_UBSAN "-fsanitize=undefined -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fno-omit-frame-pointer -g -Og")
