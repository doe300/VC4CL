include_directories(..)
include_directories(../include)

#Include the source files
file( GLOB SRCS *.cpp */*.cpp *.h */*.h)

option(TEST_OUTPUT_CONSOLE "Prints the test-output to console" ON)
if(TEST_OUTPUT_CONSOLE)
	#Create macro-definition to enable output to console
	add_definitions(-DTEST_OUTPUT_CONSOLE=1)
endif()

include_directories ("${PROJECT_SOURCE_DIR}/lib/cpptest-lite/include")

add_executable(TestVC4CL ${SRCS})
target_link_libraries(TestVC4CL cpptest-lite)
target_link_libraries(TestVC4CL VC4CL)
