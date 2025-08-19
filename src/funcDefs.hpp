std::vector<std::vector<std::string>> getCommand(std::string);
std::vector<std::vector<std::string>> readCommandsFromRespArray(std::string);
int writeResponse(int fd, std::string buf);
std::string getBulkString(std::string);
std::string getRespInt(int);
