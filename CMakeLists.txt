cmake_minimum_required(VERSION 3.10)

project(nwsapi)

find_package(CURL REQUIRED)

find_package(nlohmann_json REQUIRED)

add_executable(nwsapi main.cpp tableLookup.cpp)

target_link_libraries(nwsapi PRIVATE CURL::libcurl nlohmann_json::nlohmann_json)

install(TARGETS nwsapi RUNTIME DESTINATION bin)
