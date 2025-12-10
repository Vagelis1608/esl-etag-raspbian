cmake_minimum_required(VERSION 3.14)

add_library(btlib STATIC ./btferret/btlib.c)

project(pcProject LANGUAGES CXX)

include(FetchContent)

FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz)
FetchContent_MakeAvailable(json)

FetchContent_Declare(cpr GIT_REPOSITORY https://github.com/libcpr/cpr.git
                         GIT_TAG c492b4fed1fd7288eacec6fe8d89e37df78d373) # Replace with your desired git commit from: https://github.com/libcpr/cpr/releases
FetchContent_MakeAvailable(cpr)

add_executable(pc pc.cpp)
set_property(TARGET pc PROPERTY CXX_STANDARD 20)
set_property(TARGET pc PROPERTY CXX_STANDARD_REQUIRED ON)
target_link_libraries(pc PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(pc PRIVATE cpr::cpr)
target_link_libraries(pc PRIVATE btlib)

project(eslProject)
add_executable(esl esl.cpp)
set_property(TARGET esl PROPERTY CXX_STANDARD 20)
set_property(TARGET esl PROPERTY CXX_STANDARD_REQUIRED ON)
target_link_libraries(esl PRIVATE btlib)