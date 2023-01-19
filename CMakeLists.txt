set(ERASER_TARGET eraser)
project(${ERASER_TARGET} CXX)

if(${BOOST_ROOT})
    set(BOOST_MIN_VERSION "1.74.0")
    set(Boost_ADDITIONAL_VERSIONS "1.74.0" "1.74")
    set(Boost_USE_STATIC_LIBS ON)
    find_package(Boost COMPONENTS unit_test_framework REQUIRED)
endif()

message("BUILD_SHARED_LIBS=" ${BUILD_SHARED_LIBS})
message("CMAKE_BUILD_TYPE=" ${CMAKE_BUILD_TYPE})

find_package(PythonInterp REQUIRED)
message("Python executable: " ${PYTHON_EXECUTABLE})

# Define DEBUG_DATABASE=1 so that 'eraser' file has been used 
# from application debug working directory instead of C:\ProgramData\CyberYozh
if(DEBUG_DATABASE)
    add_definitions(-DDEBUG_DATABASE)
endif()

# Check if building a static or shared library
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

# Create the library target
include(sources.cmake)
add_library(${ERASER_TARGET} ${ERASER_SOURCES})

# Set public include directories
target_include_directories(${ERASER_TARGET}
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE
        ${CMAKE_SOURCE_DIR}/winapi-helpers/include
        ${CMAKE_SOURCE_DIR}/cryptography/include
        ${CMAKE_SOURCE_DIR}/plog/include
)

# Add cross-compiler features
# Set C++17 as the standard
target_compile_features(${ERASER_TARGET} PUBLIC cxx_std_17)

# Library dependencies
target_link_libraries(${ERASER_TARGET}
    PRIVATE
        ${Boost_LIBRARIES}
        winapi_helpers
)

# Additional steps
set(DEBUG_CREATE_DATABASE  ${CMAKE_BINARY_DIR})
set(RELEASE_CREATE_DATABASE ${CMAKE_BINARY_DIR}/db)

add_custom_command(TARGET ${ERASER_TARGET}
    POST_BUILD
    COMMAND ${PYTHON_EXECUTABLE} create_database.py --db-name eraser --output-dir "$<$<CONFIG:debug>:${DEBUG_CREATE_DATABASE}>$<$<CONFIG:release>:${RELEASE_CREATE_DATABASE}>"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../scripts
    COMMENT "Creating database for debug..."
)
