#------------------------------------------------------------------------------#
# CMakeLists.txt
# Rémi Attab (remi.attab@gmail.com), 30 Dec 2012
# FreeBSD-style copyright and disclaimer apply
#
# Blah
#------------------------------------------------------------------------------#


#------------------------------------------------------------------------------#
# CONFIG
#------------------------------------------------------------------------------#

cmake_minimum_required(VERSION 2.6)
project(slick)

enable_testing()

add_definitions("-Wall -Wextra -Werror -pedantic -g -O4")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)


#------------------------------------------------------------------------------#
# COMPILER TESTS
#------------------------------------------------------------------------------#

include(CheckCXXCompilerFlag)

check_cxx_compiler_flag(-std=c++11 SLICK_FLAG_C11)
check_cxx_compiler_flag(-std=c++0x SLICK_FLAG_C0X)

if (SLICK_FLAG_C11)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
elseif (SLICK_FLAG_C0X)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
endif()


#------------------------------------------------------------------------------#
# ENV TEST
#------------------------------------------------------------------------------#

include_directories("includes")

include_directories("$ENV{HOME}/local/include")
link_directories("$ENV{HOME}/local/lib")


#------------------------------------------------------------------------------#
# LIBRARIES
#------------------------------------------------------------------------------#

add_subdirectory(lockless)
link_libraries(lockless)

link_libraries(rt)
link_libraries(pthread)

option(USE_TCMALLOC "Use tcmalloc for heap allocations." ON)
find_library(LIB_TCMALLOC tcmalloc)

if(USE_TCMALLOC)
    if(LIB_TCMALLOC MATCHES "NOTFOUND")
        message("WARNING: unable to find tcmalloc")
    else()
        link_libraries(tcmalloc)
        add_definitions("-fno-builtin-malloc -fno-builtin-calloc")
        add_definitions("-fno-builtin-realloc -fno-builtin-free")
    endif()
endif()


#------------------------------------------------------------------------------#
# BUILD
#------------------------------------------------------------------------------#

include_directories("${PROJECT_SOURCE_DIR}/src")

install(
    FILES
    src/utils.h
    src/uuid.h
    src/queue.h
    src/poll.h
    src/notify.h
    src/timer.h
    src/payload.h
    src/socket.h
    src/queue.h
    src/endpoint.h
    src/discovery.h
    src/endpoint_disc.h

    DESTINATION
    include/slick)

add_library(
    slick
    SHARED
    src/uuid.cpp
    src/poll.cpp
    src/notify.cpp
    src/timer.cpp
    src/payload.cpp
    src/socket.cpp
    src/endpoint.cpp
    src/discovery.cpp
    src/endpoint_disc.h)

install(TARGETS slick DESTINATION lib)


#------------------------------------------------------------------------------#
# TESTS
#------------------------------------------------------------------------------#

function(slick_test name)
    if(CMAKE_SOURCE_DIR STREQUAL ${PROJECT_SOURCE_DIR})
        add_executable(${name}_test tests/${name}_test.cpp)
        target_link_libraries(${name}_test slick boost_unit_test_framework)
        add_test(${name} bin/${name}_test)
    endif()
endfunction()

slick_test(pack)
slick_test(endpoint)

add_executable(packet_test tests/packet_test.cpp)
target_link_libraries(packet_test slick)
