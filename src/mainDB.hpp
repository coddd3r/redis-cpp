#ifndef MAIN_DB_H
#define MAIN_DB_H

#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>

struct dbValue
{
        std::string val;
        std::chrono::system_clock::time_point expiryTime;
        bool hasExpiry;
};

class MainDB
{
    private:
        std::unordered_map<std::string, dbValue> htmap;

    public:
        void set(std::vector<std::string> singleCommand, std::string value);
        const int get(const std::string key, int fd);
        int getType(std::vector<std::string> singleCommand, int fd);
};

#endif
