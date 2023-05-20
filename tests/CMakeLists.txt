cmake_minimum_required(VERSION 3.2)

project(ecs_unit_tests)

add_subdirectory(googletest)

add_executable(
    ${CMAKE_PROJECT_NAME} 

    main.cpp
)

target_link_libraries(
    ${CMAKE_PROJECT_NAME}

    PUBLIC gtest
 )
