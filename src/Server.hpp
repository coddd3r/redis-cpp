#ifndef FUNC_DEFS_H
#define FUNC_DEFS_H

#include <arpa/inet.h>
#include <cstring>
#include <ctime>
#include <netdb.h>
#include <netinet/in.h>
#include <string>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

enum Options
{
    Command,
    Ping,
    InvalidCommand,
    Echo,
    Set,
    Get,
    Rpush,
    Lrange,
    Lpush,
    Llen,
    Lpop,
    Blpop,
    Type,
};

Options resolveOption(std::string);

struct clientConnection
{
        int32_t server_fd;

        clientConnection(void)
        {
            this->server_fd = -1;
        }
};

#include "helperUtils.hpp"
#include "listDB.hpp"
#include "mainDB.hpp"
#endif
