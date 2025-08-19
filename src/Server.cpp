#include <arpa/inet.h>
#include <chrono>
// #include <cmath>
// #include <cstddef>
// #include <cstdint>
// #include <cstdlib>
// #include <cmath>
#include <cstddef>
#include <cstring>
#include <ctime>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <ostream>
#include <regex>
#include <string>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

#include "constants.hpp"
#include "funcDefs.hpp"
#include "helperUtils.hpp"
#include "types.hpp"

#include "mainDB.hpp"

class ListDB
{
        std::unordered_map<std::string, std::vector<std::string>> listsMap;

    public:
        int handlePush(std::vector<std::string> singleCommand, int fd, bool left)
        {
            auto listKey = singleCommand[1];
            for (int i = 2; i < singleCommand.size(); i++)
                if (left)
                    listsMap[listKey].insert(listsMap[listKey].begin(), singleCommand[i]);
                else
                    listsMap[listKey].push_back(singleCommand[i]);
            return writeResponse(fd, getRespInt(listsMap[listKey].size()));
        }

        int getListLen(std::string listKey, int fd)
        {

            if (listsMap.find(listKey) == listsMap.end())
                return writeResponse(fd, ZERO_INT);
            return writeResponse(fd, getRespInt(listsMap[listKey].size()));
        }

        int getRange(std::vector<std::string> singleCommand, int fd)
        {
            auto listKey = singleCommand[1];
            if (listsMap.find(listKey) == listsMap.end())
                return writeResponse(fd, EMPTY_ARRAY);

            auto currList = listsMap[listKey];
            auto listLen = currList.size();
            auto start = std::stoi(singleCommand[2]);
            auto stop = std::stoi(singleCommand[3]);
            std::cerr << "initial start: " << start << " stop " << stop << "\n";
            if (start < 0)
                start = listLen + start;
            if (stop < 0)
                stop = listLen + stop;
            if (start < 0)
                start = 0;
            if (stop < 0)
                stop = 0;
            if (stop > listLen)
                stop = listLen - 1;

            std::cerr << "using indices: start:" << start << "stop:" << stop << "\n\n\n";
            if (start >= listLen || start > stop)
                return writeResponse(fd, EMPTY_ARRAY);

            auto elems = std::vector<std::string>(currList.begin() + start, currList.begin() + stop + 1);
            return writeResponse(fd, getRespArray(elems));
        }
};

int main(int argc, char **argv)
{
    // Flush after every std::cerr / std::cerr
    std::cerr << std::unitbuf;
    std::cerr << std::unitbuf;

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        std::cerr << "Failed to create server socket\n";
        return 1;
    }

    // Since the tester restarts your program quite often, setting SO_REUSEADDR
    // ensures that we don't run into 'Address already in use' errors
    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
    {
        std::cerr << "setsockopt failed\n";
        return 1;
    }

    const int port = 6379;
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0)
    {
        std::cerr << "Failed to bind to port 6379\n";
        return 1;
    }

    // max num of connections
    int connection_backlog = 5;
    if (listen(server_fd, connection_backlog) != 0)
    {
        std::cerr << "listen failed\n";
        return 1;
    }
    std::cerr << "Listening at server fd:" << server_fd << "\n";

    struct sockaddr_in client_addr;
    int client_addr_len = sizeof(client_addr);
    std::cerr << "Waiting for a client to connect...\n";

    fd_set readfds;
    size_t numBytesRead;
    int maxfd;
    int sd = 0;
    int activity;

    std::vector<int> clientList;
    MainDB mainDb;
    ListDB listDb;

    while (true)
    {
        std::cerr << "waiting for activity, serverfd is:" << server_fd << "\n";
        // zero out fd set and add server to set
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);

        maxfd = server_fd;

        // add all clients aand listen to them via select
        for (auto client_fd : clientList)
        {
            FD_SET(client_fd, &readfds);
            if (client_fd > maxfd)
            {
                maxfd = client_fd;
            }
        }

        if (sd > maxfd)
        {
            maxfd = sd;
        }

        /* using select for listen to multiple client
             select(int nfds, fd_set *restrict readfds, fd_set *restrict writefds,
             fd_set *restrict errorfds, struct timeval *restrict timeout);
        */

        std::cerr << "before checking activity, maxfd:" << maxfd << "\n";
        activity = select(maxfd + 1, &readfds, NULL, NULL, NULL);

        if (activity < 0)
        {
            std::cerr << "select error\n";
            continue;
        }

        std::cerr << "got activity: " << activity << "\n";

        if (FD_ISSET(server_fd, &readfds))
        {
            int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, (socklen_t *)&client_addr_len);
            std::cerr << "got client: " << client_fd << "\n";
            if (client_fd < 0)
            {
                std::cerr << "accept error\n";
                continue;
            }
            std::cerr << "Client connected at socket fd:" << client_fd << "\n"
                      << "at ip: " << inet_ntoa(server_addr.sin_addr)
                      << ", port: " << ntohs(server_addr.sin_port) << "\n";

            clientList.push_back(client_fd);
        }

        std::cerr << "After client if fds:[ ";
        for (auto e : clientList)
            std::cerr << e << ", " << "\n";
        std::cerr << "]\n";

        char buf[BUFFER_SIZE] = {0};

        for (int i = 0; i < clientList.size(); ++i)
        {
            sd = clientList[i];

            std::cerr << "checking client:" << sd << "\n";
            if (FD_ISSET(sd, &readfds))
            {
                memset(buf, 0x00, BUFFER_SIZE);
                numBytesRead = read(sd, buf, 1024);
                if (numBytesRead == 0)
                {
                    std::cerr << "client disconnected\n";
                    std::cerr << "host disconnected, ip: " << inet_ntoa(server_addr.sin_addr)
                              << ", port: " << ntohs(server_addr.sin_port) << "\n";
                    close(sd);
                    clientList.erase(clientList.begin() + i);
                }
                else
                {
                    std::cerr << "message from client: " << buf << "\n";
                    std::string clientMessage = std::string(buf);

                    auto all_commands = getCommand(clientMessage);

                    int numWritten = 0;
                    for (auto singleCommand : all_commands)
                    {

                        auto cmd = singleCommand[0];
                        std::cerr << "Command: " << cmd << "\n";
                        switch (resolveOption(cmd))
                        {
                        case Command:
                            numWritten = writeResponse(sd, RESP_OK);
                            break;

                        case Ping:
                            numWritten = writeResponse(sd, PONG_RESPONSE);
                            break;

                        case Echo:
                            std::cerr << "Echoing: " << singleCommand[1] << "\n";
                            numWritten = writeResponse(sd, getBulkString(singleCommand[1]));
                            break;

                        case Set:
                            mainDb.set(singleCommand, singleCommand[2]);
                            numWritten = writeResponse(sd, RESP_OK);
                            break;

                        case Get:
                            mainDb.get(singleCommand[1], sd);
                            break;

                        case Rpush:
                            listDb.handlePush(singleCommand, sd, false);
                            break;

                        case Lpush:
                            listDb.handlePush(singleCommand, sd, true);
                            break;

                        case Lrange:
                            listDb.getRange(singleCommand, sd);
                            break;

                        case Llen:
                            listDb.getListLen(singleCommand[1], sd);
                            break;

                        default:
                            numWritten = writeResponse(sd, RESP_OK);
                            break;
                        }
                        std::cerr << "Wrote:" << numWritten << "bytes\n";
                    }
                }
            }
        }

        std::cerr << "REACHED END\n";
    }
    close(server_fd);
    return 0;
}
