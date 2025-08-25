#ifndef LIST_DB_H
#define LIST_DB_H

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

class ListDB
{
    private:
        std::unordered_map<std::string, std::vector<std::string>> listsMap;
        std::unordered_map<std::string, std::vector<int>> waitingBlocks;
        std::mutex listLock;

    public:
        int getListLen(std::string listKey, int fd);
        int handleLpop(std::vector<std::string> singleCommand, int fd);
        std::string popFirst(std::string listKey);
        int handlePush(std::vector<std::string> singleCommand, int fd, bool left);
        int getRange(std::vector<std::string> singleCommand, int fd);
        int handleBlock(std::vector<std::string> singleCommand, int fd);
        int respondToBlock(int fd, std::string listKey);
        void timedBlock(int fd, std::string listKey, std::chrono::system_clock::time_point expiryTime, std::chrono::milliseconds msTime);
};

#endif
