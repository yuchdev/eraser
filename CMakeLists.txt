set(TARGET eraser)
 
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "../bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "../bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "../bin")

find_package(PythonInterp REQUIRED)
message("Python executable: " ${PYTHON_EXECUTABLE})

file(GLOB SOURCES *.cpp ../include/${TARGET}/*.h)

# Define DEBUG_DATABASE=1 so that 'eraser' file has been used 
# from application debug working directory instead of C:\ProgramData\CyberYozh
if(DEBUG_DATABASE)
    add_definitions(-DDEBUG_DATABASE)
endif()

include_directories(${Boost_INCLUDE_DIRS} 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_SOURCE_DIR}/helpers/include
    ${CMAKE_SOURCE_DIR}/encryption/include
    ${CMAKE_SOURCE_DIR}/3rd_party/plog/include
)

add_library(${TARGET} ${SOURCES})
add_dependencies(${TARGET} helpers)
target_link_libraries(${TARGET} ${Boost_LIBRARIES} helpers)

set(DEBUG_CREATE_DATABASE  ${CMAKE_BINARY_DIR})
set(RELEASE_CREATE_DATABASE ${CMAKE_BINARY_DIR}/cyberyozh_tools)

add_custom_command(TARGET ${TARGET} POST_BUILD
    COMMAND ${PYTHON_EXECUTABLE} create_database.py --db-name eraser --output-dir 
        "$<$<CONFIG:debug>:${DEBUG_CREATE_DATABASE}>$<$<CONFIG:release>:${RELEASE_CREATE_DATABASE}>"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../scripts
    COMMENT "Creating database for debug..."
)

set_property(TARGET ${TARGET} PROPERTY VS_USER_PROPS ${CMAKE_SOURCE_DIR}/.vc140.props)
