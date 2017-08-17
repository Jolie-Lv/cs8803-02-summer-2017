cmake_minimum_required(VERSION 3.7)
project(gios_summer2017_pr3)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES
        cache/shm_channel.c
        cache/shm_channel.h
        cache/simplecache.c
        cache/simplecache.h
        cache/simplecached.c
        cache/webproxy.c
        server/gfserver.h
        server/handle_with_cache.c
        server/handle_with_curl.c
        server/handle_with_file.c
        server/steque.c
        server/steque.h
        server/webproxy.c)

add_executable(gios_summer2017_pr3 ${SOURCE_FILES})