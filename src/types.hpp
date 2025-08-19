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
    if (input == "rpush")
        return Rpush;
    if (input == "lrange")
        return Lrange;
    if (input == "lpush")
        return Lpush;
    return InvalidCommand;
};

struct dbValue
{
        std::string val;
        std::chrono::system_clock::time_point expiryTime;
        bool hasExpiry;
};

struct clientConnection
{
        int32_t server_fd;

        clientConnection(void)
        {
            this->server_fd = -1;
        }
};
