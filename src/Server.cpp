#include <arpa/inet.h>
#include <cmath>
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
#include <unordered_map>
#include <vector>

#include <regex>

const uint16_t BUFFER_SIZE = 4096;
const std::string PONG_RESPONSE = "+PONG\r\n";
const std::string RESP_OK = "+OK\r\n";
const std::string RESP_NULL = "$-1\r\n";

enum Options
{
    Command,
    Ping,
    InvalidCommand,
    Echo,
    Set,
    Get,
};

Options resolveOption(std::string);
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
    return InvalidCommand;
};

std::vector<std::vector<std::string>> getCommand(std::string);
std::vector<std::vector<std::string>> readCommandsFromRespArray(std::string);
int writeResponse(int fd, std::string buf);
std::string getBulkString(std::string);
// void handleSet(std::vector<std::string>, std::unordered_map<std::string, std::string>);
// int handleGet(std::string, std::unordered_map<std::string, std::string>, int);

// void handleSet(std::vector<std::string> singleCommand, std::unordered_map<std::string, std::string> redisDb)
//{
//     auto key = singleCommand[1];
//     auto val = singleCommand[2];
//     redisDb[key] = val;
// }

class MainDB
{
        std::unordered_map<std::string, std::string> htmap;

    public:
        void set(std::string key, std::string value)
        {
            htmap[key] = value;
        }

        const int get(const std::string key, int fd)
        {
            for (const auto &[key, value] : htmap)
            {
                std::cerr << "IN KV LOOP\n";
                std::cerr << " found key: " << key << "\nval: " << value << "\n";
            }
            auto res = htmap.find(key);
            std::cerr << "map size: " << htmap.size() << "\n";
            if (res != htmap.end())
            {
                auto val = htmap[key];
                std::cerr << "Writing GET val: " << val << "\n";
                return writeResponse(fd, getBulkString(val));
            }
            std::cerr << "Failed GET for: " << key << "\n";
            writeResponse(fd, RESP_NULL);
            return 0;
        }
};

std::string getBulkString(std::string origStr)
{
    std::string retStr;
    retStr.append("$");
    retStr.append(std::to_string(origStr.length()));
    retStr.append("\r\n");
    retStr.append(origStr);
    retStr.append("\r\n");
    return retStr;
}

int writeResponse(int fd, std::string buf)
{

    auto actualBuf = buf.c_str();
    std::cerr << "Writing to stream buff: " << actualBuf << "\n";
    return write(fd, actualBuf, buf.length());
}

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
            std::cerr << "got size line:" << curr_s << "\n";
            int arr_size = std::stoi(curr_s.substr(1, curr_s.length()));
            std::cerr << "got SIZE:" << arr_size << "\n";
            std::vector<std::string> single_command;
            // int comm_end = iter + arr_size;
            iter++;
            for (; iter != end; ++iter)
            {
                curr_s = (std::string)*iter;
                std::cerr << "in second for, curr_s:" << curr_s << "\n";

                if (curr_s[0] == '*')
                    break;
                if (curr_s[0] == '$')
                    continue;

                std::cerr << "got str line:" << curr_s << "\n";
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

        // TODO: Specify write fds

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
                            mainDb.set(singleCommand[1], singleCommand[2]);
                            numWritten = writeResponse(sd, RESP_OK);
                            break;

                        case Get:
                            mainDb.get(singleCommand[1], sd);
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
