cmake_minimum_required(VERSION 2.8.4)
project(asio_service_discovery)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Wextra -Wconversion")

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wdocumentation")
endif()

# this is here, so that users can use a custom boost installation
# used also by travis CI
if(EXISTS "$ENV{BOOST_ROOT}")
    link_directories("$ENV{BOOST_ROOT}/lib")
    include_directories("$ENV{BOOST_ROOT}/include")
endif(EXISTS "$ENV{BOOST_ROOT}")

# create documentation
# add a target to generate API documentation with Doxygen
find_package(Doxygen)
if(DOXYGEN_FOUND)
add_custom_target(
    doc
    ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/docs
    COMMENT "Generating API documentation with Doxygen" VERBATIM
)
endif(DOXYGEN_FOUND)


file(GLOB tests RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/tests" "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")
foreach(test_file ${tests})
    message(${test_file})

    get_filename_component(test_name ${test_file} NAME_WE)

    add_executable(${test_name} "${CMAKE_CURRENT_SOURCE_DIR}/tests/${test_file}")
    target_include_directories(${test_name} PUBLIC "./include")
    target_link_libraries(${test_name} "boost_system" "boost_unit_test_framework")
endforeach(test_file)
