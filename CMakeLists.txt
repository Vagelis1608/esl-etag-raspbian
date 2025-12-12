cmake_minimum_required(VERSION 3.14)

project(uploaderProject)

include(FetchContent)

FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz)
FetchContent_MakeAvailable(json)

FetchContent_Declare(Boost URL https://github.com/boostorg/boost/releases/download/boost-1.90.0/boost-1.90.0-cmake.tar.gz)
FetchContent_MakeAvailable(Boost)

FetchContent_Declare(cpr GIT_REPOSITORY https://github.com/libcpr/cpr.git
                         GIT_TAG c492b4fed1fd7288eacec6fe8d89e37df78d373) # Replace with your desired git commit from: https://github.com/libcpr/cpr/releases
FetchContent_MakeAvailable(cpr)

add_library(btlib STATIC ${CMAKE_CURRENT_SOURCE_DIR}/btferret/btlib.c)

add_executable(uploader ${CMAKE_CURRENT_SOURCE_DIR}/uploader.cpp )
set_property(TARGET uploader PROPERTY CXX_STANDARD 20)
set_property(TARGET uploader PROPERTY CXX_STANDARD_REQUIRED ON)
target_link_libraries(uploader PRIVATE Boost::json Boost::program_options cpr::cpr btlib)
