cmake_minimum_required(VERSION 3.10)
project(rubberband)

# Find Meson
find_program(MESON_EXECUTABLE meson)
if(NOT MESON_EXECUTABLE)
    message(FATAL_ERROR "Meson not found. Please install Meson.")
endif()

# Find Ninja
find_program(NINJA_EXECUTABLE ninja)
if(NOT NINJA_EXECUTABLE)
    message(FATAL_ERROR "Ninja not found. Please install Ninja.")
endif()

# Set up Meson build directory
set(MESON_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/meson-build")

# Configure Meson build
execute_process(
    COMMAND ${MESON_EXECUTABLE} setup ${MESON_BUILD_DIR} ${CMAKE_CURRENT_SOURCE_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    RESULT_VARIABLE MESON_CONFIGURE_RESULT
)

if(NOT MESON_CONFIGURE_RESULT EQUAL 0)
    message(FATAL_ERROR "Meson configuration failed")
endif()

# Add custom target to build RubberBand using Meson
add_custom_target(rubberband ALL
    COMMAND ${NINJA_EXECUTABLE} -C ${MESON_BUILD_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Building RubberBand with Meson"
)

# Set include directories
set(RUBBERBAND_INCLUDE_DIRS 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/rubberband
    PARENT_SCOPE
)

# Set library file (adjust the name if different)
set(RUBBERBAND_LIBRARY 
    ${MESON_BUILD_DIR}/librubberband.a
    PARENT_SCOPE
)