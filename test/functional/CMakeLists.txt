set(TARGET shredder_functional_tests)

find_package(Boost ${BOOST_MIN_VERSION} COMPONENTS unit_test_framework date_time REQUIRED) 

file(GLOB SOURCES shredder_func_tests.cpp)
 
include_directories(
    ${Boost_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/eraser/include 
    )

add_executable(${TARGET} ${SOURCES})
target_link_libraries(${TARGET} 
PRIVATE 
    ${Boost_LIBRARIES} 
    winapi_helpers 
    eraser
)

add_test(NAME ${TARGET} COMMAND ${TARGET})
set_property(TARGET ${TARGET} PROPERTY FOLDER "UnitTests")
