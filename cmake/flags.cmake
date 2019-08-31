set(VC4CL_ENABLED_WARNINGS
	-Wall -Wextra
	-Wold-style-cast -Wnon-virtual-dtor -Wnull-dereference
	-Wno-unused-parameter -Wno-missing-field-initializers -Wno-write-strings
	-Werror=return-type -Werror=unused-result -Werror=shift-count-overflow -Werror=missing-field-initializers -Werror=reorder
)
# Enable additional warnings, if available
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
	SET(VC4CL_ENABLED_WARNINGS
		${VC4CL_ENABLED_WARNINGS}
		-Weverything
		-Wswitch
		-Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-shadow -Wno-padded -Wno-shadow-field-in-constructor -Wno-global-constructors
		-Wno-exit-time-destructors -Wno-missing-prototypes -Wno-gnu-anonymous-struct -Wno-nested-anon-types -Wno-documentation
		-Wno-unused-command-line-argument -Wno-unused-member-function -Wno-gnu-zero-variadic-macro-arguments -Wno-covered-switch-default
		-Wno-switch-enum -Wno-return-std-move-in-c++11 -Wno-format-nonliteral
		-Werror=return-stack-address
	)
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	SET(VC4CL_ENABLED_WARNINGS
		${VC4CL_ENABLED_WARNINGS}
		-Wlogical-op -Wmisleading-indentation -Wduplicated-cond -Wdouble-promotion
		-fdelete-null-pointer-checks
		-Wuninitialized -Wsuggest-attribute=format -Wsuggest-override -Wconversion -Wzero-as-null-pointer-constant
		-Wno-psabi -Wno-unknown-pragmas -Wno-ignored-attributes
		-Werror=return-local-addr -Werror=uninitialized
	)
endif()