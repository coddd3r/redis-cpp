#ifndef HELPER_UTILS_H
#define HELPER_UTILS_H

#include <string>
#include <unistd.h>
#include <vector>

std::vector<std::vector<std::string>> getCommand(std::string);
std::vector<std::vector<std::string>> readCommandsFromRespArray(std::string);
int writeResponse(int fd, std::string buf);
std::string getBulkString(std::string);
std::string getRespInt(int);
std::string getRespArray(std::vector<std::string> elems);

#endif
