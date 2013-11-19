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

add_definitions("-std=c++11 -Wall -Wextra -Werror -pedantic -g -O4")
include_directories("${PROJECT_SOURCE_DIR}/src")

link_libraries(rt)
link_libraries(pthread)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)


#------------------------------------------------------------------------------#
# TCMALLOC
#------------------------------------------------------------------------------#

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

install(
    FILES
    src/provider.h src/client.h src/naming.h

    DESTINATION
    include/slick)

add_library(
    slick
    SHARED
    src/provider.cpp src/cient.h)

install(TARGETS slick DESTINATION lib)


#------------------------------------------------------------------------------#
# TESTS
#------------------------------------------------------------------------------#

function(slick_test name)
    add_executable(${name}_test tests/${name}_test.cpp)
    target_link_libraries(${name}_test slick boost_unit_test_framework)
    add_test(${name} bin/${name}_test)
endfunction()

