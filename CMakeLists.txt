cmake_minimum_required(VERSION 3.16)
project(robots)

set(CMAKE_C_FLAGS "-std=gnu17 -Wall -Wextra -Wconversion -Werror -O2")

add_executable(robots-client
        client/main.c
        client/utils/buffer.h
        client/utils/buffer.c
        client/utils/hmap.h
        client/utils/hmap.c
        client/args.h
        client/args.c
        client/msg.h
        client/msg.c
        client/game.h
        client/game.c
        client/net.h
        client/net.c)

add_executable(robots-server
        server/utils/err.h
        server/utils/buffer.h
        server/utils/buffer.c
        server/utils/hmap.h
        server/utils/hmap.c
        server/utils/random.h
        server/utils/random.c
        server/args.h
        server/args.c
        server/net.h
        server/net.c
        server/msg.h
        server/msg.c
        server/game.h
        server/game.c
        server/main.c)
