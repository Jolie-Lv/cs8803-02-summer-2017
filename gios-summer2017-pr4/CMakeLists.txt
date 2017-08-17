cmake_minimum_required(VERSION 3.7)
project(gios_summer2017_pr4)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES
        rpc_mt/magickminify.c
        rpc_mt/magickminify.h
        rpc_mt/magickminify_test.c
        rpc_mt/minify_via_rpc.c
        rpc_mt/minifyjpeg.c
        rpc_mt/minifyjpeg.h
        rpc_mt/minifyjpeg_clnt.c
        rpc_mt/minifyjpeg_main.c
        rpc_mt/minifyjpeg_svc.c
        rpc_mt/minifyjpeg_xdr.c
        rpc_mt/steque.c
        rpc_mt/steque.h
        magickminify.c
        magickminify.h
        magickminify_test.c
        minify_via_rpc.c
        minifyjpeg.c
        minifyjpeg.h
        minifyjpeg_clnt.c
        minifyjpeg_main.c
        minifyjpeg_svc.c
        minifyjpeg_xdr.c
        steque.c
        steque.h)

add_executable(gios_summer2017_pr4 ${SOURCE_FILES})