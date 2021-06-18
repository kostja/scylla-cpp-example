cmake_minimum_required(VERSION 2.8)

project(scylla-cxx-example C CXX)

function(update_submodules)
    message(STATUS "Updating submodules")
    execute_process(COMMAND ${GIT} submodule update --init --recursive
                    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endfunction()

if(NOT EXISTS ${CMAKE_SOURCE_DIR}/cpp-driver/examples)
    if (EXISTS "${CMAKE_SOURCE_DIR}/.git" AND GIT)
        update_submodules()
    else()
        message(FATAL_ERROR "Failed to find submodules")
    endif()
endif()

add_subdirectory(${PROJECT_SOURCE_DIR}/cpp-driver EXCLUDE_FROM_ALL)


add_executable(scylla-cxx-example main.cpp)

target_link_libraries(scylla-cxx-example cassandra)


