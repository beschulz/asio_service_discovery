cmake_minimum_required(VERSION 2.8.4)
project(asio_service_discovery)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Wextra")

file(GLOB_RECURSE tests RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/tests" "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")

if(EXISTS "$ENV{BOOST_ROOT}")
    set(CMAKE_LIBRARY_PATH "$ENV{BOOST_ROOT}/lib" ${CMAKE_LIBRARY_PATH})
    set(CMAKE_INCLUDE_PATH "$ENV{BOOST_ROOT}/include" ${CMAKE_INCLUDE_PATH})

    message("******************************")
    message(${CMAKE_LIBRARY_PATH})
    message("******************************")
    message(${CMAKE_INCLUDE_PATH})
endif(EXISTS "$ENV{BOOST_ROOT}")


foreach(test_file ${tests})
    message(${test_file})

    get_filename_component(test_name ${test_file} NAME_WE)

    add_executable(${test_name} "${CMAKE_CURRENT_SOURCE_DIR}/tests/${test_file}")
    target_include_directories(${test_name} PUBLIC "./include")
    target_link_libraries(${test_name} "boost_system" "boost_unit_test_framework")
endforeach(test_file)
