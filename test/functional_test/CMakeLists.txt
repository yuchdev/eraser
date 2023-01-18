set(TARGET shredder_func_tests)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "../../bin")

find_package(Boost ${BOOST_MIN_VERSION} COMPONENTS unit_test_framework date_time REQUIRED) 

# NOTE! Always set this variable!
set(DEPENDENT_LIBRARIES teamcity_boost helpers file_shredder)

file(GLOB SOURCES *.cpp)
 
include_directories(
    ${Boost_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/file_shredder/include 
    )

add_executable(${TARGET} ${SOURCES})
add_dependencies(${TARGET} ${DEPENDENT_LIBRARIES})
target_link_libraries(${TARGET} ${Boost_LIBRARIES} ${DEPENDENT_LIBRARIES})

add_test(NAME ${TARGET} COMMAND ${TARGET})
set_property(TARGET ${TARGET} PROPERTY FOLDER "UnitTests")

set_property(TARGET ${TARGET} PROPERTY VS_USER_PROPS ${CMAKE_SOURCE_DIR}/.vc140.props)
