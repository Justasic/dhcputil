cmake_minimum_required(VERSION 3.5 FATAL_ERROR)

# set a default build type if none was specified
set(default_build_type "Debug")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
	set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
		STRING "Choose the type of build." FORCE)
	# Set the possible values of build type for cmake-gui
	set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
		"Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)

# Always use color diagnostics
set (CMAKE_COLOR_DIAGNOSTICS TRUE)
# Always export compile commands
set (CMAKE_EXPORT_COMPILE_COMMANDS TRUE)


# Define C++ project
project(dhcputil
	VERSION 2.0.0
	DESCRIPTION "A C++ DHCP debugging and information utility"
	LANGUAGES CXX C
)

set(CXX_SOURCES)
file(GLOB_RECURSE CXX_SOURCES ${CMAKE_CURRENT_SOURCE_DIR} "src/*.cpp")
list(SORT CXX_SOURCES)

set_source_files_properties(${CXX_SOURCES} 
	PROPERTIES 
		LANGUAGE CXX
)

add_executable(${PROJECT_NAME} ${CXX_SOURCES})

# Set compile flags that span across all source files
target_compile_options(${PROJECT_NAME}
	PRIVATE
		-Wall
		-Wextra
		-Werror=shadow
		-Wpedantic
		-Wno-c++98-compat
		-Wconversion
		-Wno-unused-parameter
		-Wimplicit-fallthrough
		-pedantic
		# Format options
		-fno-rtti
)

# Add our include directories
target_include_directories(${PROJECT_NAME}
	PUBLIC
		# ${FMT_SOURCE_DIR}/include/fmt
		${CMAKE_CURRENT_SOURCE_DIR}/include
		${CMAKE_CURRENT_SOURCE_DIR}
		${CMAKE_CURRENT_BINARY_DIR}
)

set_target_properties(${PROJECT_NAME}
	PROPERTIES
		LINKER_LANGUAGE CXX
		CXX_STANDARD 20
		CXX_STANDARD_REQUIRED YES
		CXX_EXTENSIONS NO
)

if(${CMAKE_BUILD_TYPE} MATCHES "Release")
	target_compile_definitions(${PROJECT_NAME} PRIVATE _FORTIFY_SOURCE=2 NDEBUG)
endif(${CMAKE_BUILD_TYPE} MATCHES "Release")
