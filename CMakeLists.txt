
cmake_minimum_required(VERSION 3.15)
project(airline_system CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-fpermissive -Wno-template-body)

find_package(SQLite3 REQUIRED)

include_directories(SYSTEM include ${SQLite3_INCLUDE_DIRS})

add_executable(airline_server src/main.cpp)

if(WIN32)
    target_link_libraries(airline_server PRIVATE SQLite3::SQLite3 pthread ws2_32 mswsock)
else()
    target_link_libraries(airline_server PRIVATE SQLite3::SQLite3 pthread)
endif()