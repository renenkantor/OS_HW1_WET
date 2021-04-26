cmake_minimum_required(VERSION 2.8.11)
project(skeleton_smash)

set(CMAKE_CXX_STANDARD 11)

include_directories(.)

add_executable(skeleton_smash
        Commands.cpp
        Commands.h
        signals.cpp
        signals.h
        smash.cpp)
