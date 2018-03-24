include_directories(..)
include_directories(../include)

#Include the source files
file( GLOB SRCS *.cpp */*.cpp *.h */*.h)

include_directories ("${PROJECT_SOURCE_DIR}/lib/cpptest-lite/include")

add_executable(TestVC4CL ${SRCS})
add_dependencies(TestVC4CL cpptest-lite-project-build)
add_library(cpptest-lite SHARED IMPORTED)
ExternalProject_Get_Property(cpptest-lite-project BINARY_DIR)
set_target_properties(cpptest-lite PROPERTIES IMPORTED_LOCATION "${BINARY_DIR}/libcpptest-lite.so")
target_link_libraries(TestVC4CL cpptest-lite)
target_link_libraries(TestVC4CL VC4CL)

add_test(NAME System COMMAND ./build/test/TestVC4CL --system WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
add_test(NAME Platform COMMAND ./build/test/TestVC4CL --platform WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
add_test(NAME Device COMMAND ./build/test/TestVC4CL --device WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
add_test(NAME Context COMMAND ./build/test/TestVC4CL --context WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
add_test(NAME Queue COMMAND ./build/test/TestVC4CL --queue WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
add_test(NAME Buffer COMMAND ./build/test/TestVC4CL --buffer WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
add_test(NAME Images COMMAND ./build/test/TestVC4CL --images WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
add_test(NAME Programs COMMAND ./build/test/TestVC4CL --programs WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
add_test(NAME Kernel COMMAND ./build/test/TestVC4CL --kernels WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
add_test(NAME Events COMMAND ./build/test/TestVC4CL --events WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
add_test(NAME Extensions COMMAND ./build/test/TestVC4CL --extensions WORKING_DIRECTORY ${CMAKE_BINARY_DIR})