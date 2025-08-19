#include <arpa/inet.h>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <ostream>
#include <string>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

#include <regex>

const uint16_t BUFFER_SIZE = 4096;
const char PONG_RESPONSE[] = "+PONG\r\n";
const char RESP_OK[] = "+OK\r\n";

enum Options
{
    Command,
    Ping,
    InvalidCommand
};

Options resolveOption(std::string input);

Options resolveOption(std::string input)
{
    std::transform(input.begin(), input.end(), input.begin(),
                   [](unsigned char c)
                   { return std::tolower(c); });
    if (input == "comand")
        return Command;
    if (input == "ping")
        return Ping;
    return InvalidCommand;
};

struct clientConnection
{
        int32_t server_fd;

        clientConnection(void)
        {
            this->server_fd = -1;
        }
};

std::vector<std::vector<std::string>> readCommandsFromRespArray(std::string respArr)
{
    int sizeStart = 1;
    int sizeEnd;

    std::vector<std::vector<std::string>> retVec;

    std::regex rgx("(\r\n)+");
    std::sregex_token_iterator iter(respArr.begin(),
                                    respArr.end(),
                                    rgx,
                                    -1);
    std::sregex_token_iterator end;
    for (; iter != end; ++iter)
    {
        std::string curr_s = (std::string)*iter;
        if (curr_s[0] == '*')
        {
            std::cout << "got size line:" << curr_s << "\n";
            int arr_size = std::stoi(curr_s.substr(1, curr_s.length()));
            std::cout << "got SIZE:" << arr_size << "\n";
            std::vector<std::string> single_command;
            // int comm_end = iter + arr_size;
            iter++;
            for (; iter != end; ++iter)
            {
                curr_s = (std::string)*iter;
                std::cout << "in second for, curr_s:" << curr_s << "\n";

                if (curr_s[0] == '*')
                    break;
                if (curr_s[0] == '$')
                    continue;

                std::cout << "got str line:" << curr_s << "\n";
                single_command.push_back(curr_s);
            }
            retVec.push_back(single_command);
        }
    }
    return retVec;
}

// std::string getCommand(std::string redisMessage)
std::vector<std::vector<std::string>> getCommand(std::string redisMessage)

{
    std::vector<std::vector<std::string>> vecRet;
    switch (redisMessage[0])
    {
    case '*':
        return readCommandsFromRespArray(redisMessage);
    default:
        return vecRet;
    }
}

int main(int argc, char **argv)
{
    // Flush after every std::cout / std::cerr
    std::cout << std::unitbuf;
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
    std::cout << "Waiting for a client to connect...\n";

    fd_set readfds;
    size_t numBytesRead;
    int maxfd;
    int sd = 0;
    int activity;

    std::vector<int> clientList;

    while (true)
    {
        std::cout << "waiting for activity, serverfd is:" << server_fd << "\n";
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

        // TODO: Specify write fds

        std::cout << "before checking activity, maxfd:" << maxfd << "\n";
        activity = select(maxfd + 1, &readfds, NULL, NULL, NULL);

        if (activity < 0)
        {
            std::cerr << "select error\n";
            continue;
        }

        std::cout << "got activity: " << activity << "\n";

        if (FD_ISSET(server_fd, &readfds))
        {
            int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, (socklen_t *)&client_addr_len);
            std::cout << "got client: " << client_fd << "\n";
            if (client_fd < 0)
            {
                std::cerr << "accept error\n";
                continue;
            }
            std::cout << "Client connected at socket fd:" << client_fd << "\n"
                      << "at ip: " << inet_ntoa(server_addr.sin_addr)
                      << ", port: " << ntohs(server_addr.sin_port) << "\n";

            clientList.push_back(client_fd);
        }

        std::cout << "After client if fds:[ ";
        for (auto e : clientList)
            std::cout << e << ", " << "\n";
        std::cout << "]\n";

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
                    std::cout << "client disconnected\n";
                    std::cout << "host disconnected, ip: " << inet_ntoa(server_addr.sin_addr)
                              << ", port: " << ntohs(server_addr.sin_port) << "\n";
                    close(sd);
                    clientList.erase(clientList.begin() + i);
                }
                else
                {
                    std::cout << "message from client: " << buf << "\n";
                    std::string clientMessage = std::string(buf);

                    auto all_commands = getCommand(clientMessage);

                    int numWritten = 0;
                    for (auto singleCommand : all_commands)
                    {

                        auto cmd = singleCommand[0];
                        std::cout << "Command: " << cmd << "\n";
                        switch (resolveOption(cmd))
                        {
                        case Command:
                            numWritten = write(sd, &RESP_OK, sizeof(RESP_OK) - 1);
                            break;
                        case Ping:
                            numWritten = write(sd, &PONG_RESPONSE, sizeof(PONG_RESPONSE) - 1);
                            break;
                        default:
                            numWritten = write(sd, &RESP_OK, sizeof(RESP_OK) - 1);
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
