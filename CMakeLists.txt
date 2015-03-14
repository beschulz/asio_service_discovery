cmake_minimum_required(VERSION 2.8.4)
project(asio_service_discovery)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Wextra")

file(GLOB_RECURSE tests RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/tests" "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")

foreach(test_file ${tests})
    message(${test_file})

    get_filename_component(test_name ${test_file} NAME_WE)

    add_executable(${test_name} "${CMAKE_CURRENT_SOURCE_DIR}/tests/${test_file}")
    target_include_directories(${test_name} PUBLIC "./include")
    target_link_libraries(${test_name} "boost_system")
endforeach(test_file)
