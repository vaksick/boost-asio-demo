cmake_minimum_required(VERSION 3.28)

project(service)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(THREADS_PREFER_PTHREAD_FLAG ON)

include(FetchContent)
FetchContent_Declare(spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.13.0
)
FetchContent_MakeAvailable(spdlog)


find_package(Boost REQUIRED thread system)
find_package(spdlog REQUIRED)

include_directories(
    include
)

file(GLOB sources src/*.cpp)
add_executable(service ${sources})

# target_include_directories(service PRIVATE ${Boost_INCLUDE_DIRS})
target_link_libraries(service ${Boost_LIBRARIES})
target_link_libraries(service spdlog)
