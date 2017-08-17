cmake_minimum_required(VERSION 3.7)
project(gios_summer2017_pr1)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES
        echo/echoclient.c
        echo/echoserver.c
        gflib/content.c
        gflib/content.h
        gflib/gfclient.c
        gflib/gfclient.h
        gflib/gfclient_download.c
        gflib/gfserver.c
        gflib/gfserver.h
        gflib/gfserver_main.c
        gflib/handler.c
        gflib/workload.c
        gflib/workload.h
        mtgf/content.c
        mtgf/content.h
        mtgf/gfclient.h
        mtgf/gfclient_download.c
        mtgf/gfserver.h
        mtgf/gfserver_main.c
        mtgf/handler.c
        mtgf/log.h
        mtgf/steque.c
        mtgf/steque.h
        mtgf/workload.c
        mtgf/workload.h
        transfer/transferclient.c
        transfer/transferserver.c)

add_executable(gios_summer2017_pr1 ${SOURCE_FILES})