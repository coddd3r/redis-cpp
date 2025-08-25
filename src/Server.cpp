#include "Server.hpp"
#include "constants.hpp"
#include <algorithm>
#include <iostream>

int main(int argc, char **argv)
{
    // Flush after every std::cerr / std::cerr
    std::cerr << std::unitbuf;
    std::cout << std::unitbuf;

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

                        case Lpop:
                            listDb.handleLpop(singleCommand, sd);
                            break;

                        case Blpop:
                            listDb.handleBlock(singleCommand, sd);
                            break;

                        case Type:
                            mainDb.getType(singleCommand, sd);
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

Options resolveOption(std::string input)
{
    std::transform(input.begin(), input.end(), input.begin(),
                   [](unsigned char c)
                   { return std::tolower(c); });
    if (input == "comand")
        return Command;
    if (input == "ping")
        return Ping;
    if (input == "echo")
        return Echo;
    if (input == "set")
        return Set;
    if (input == "get")
        return Get;
    if (input == "rpush")
        return Rpush;
    if (input == "lrange")
        return Lrange;
    if (input == "lpush")
        return Lpush;
    if (input == "llen")
        return Llen;
    if (input == "lpop")
        return Lpop;
    if (input == "blpop")
        return Blpop;
    if (input == "type")
        return Type;
    return InvalidCommand;
};
