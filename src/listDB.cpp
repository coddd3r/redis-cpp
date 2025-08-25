#include <iostream>
#include <thread>

#include "constants.hpp"
#include "helperUtils.hpp"
#include "listDB.hpp"

int ListDB::getListLen(std::string listKey, int fd)
{
    auto listLen = 0;
    {
        std::lock_guard<std::mutex> lenLock(listLock);
        if (listsMap.find(listKey) != listsMap.end())
            listLen = listsMap[listKey].size();
    }
    return writeResponse(fd, getRespInt(listLen));
}

int ListDB::handleLpop(std::vector<std::string> singleCommand, int fd)
{
    auto listKey = singleCommand[1];
    {
        std::lock_guard<std::mutex> xLock(listLock);
        if (listsMap.find(listKey) == listsMap.end() || listsMap[listKey].size() < 1)
            return writeResponse(fd, RESP_NULL);
    }

    if (singleCommand.size() > 2)
    {
        std::vector<std::string> poppedElems;
        {
            std::lock_guard<std::mutex> lpopLock(listLock);
            auto foundList = &listsMap[listKey];

            int numToRemove = std::stoi(singleCommand[2]);
            auto listSize = foundList->size();
            if (numToRemove > listSize)
                numToRemove = listSize;
            auto start = foundList->begin();
            auto end = foundList->begin() + numToRemove;
            poppedElems = std::vector<std::string>(start, end);
            foundList->erase(start, end);
        }
        return writeResponse(fd, getRespArray(poppedElems));
    }
    return writeResponse(fd, getBulkString(popFirst(listKey)));
}

std::string ListDB::popFirst(std::string listKey)
{
    std::lock_guard<std::mutex> popLock(listLock);
    auto firstElem = std::string(listsMap[listKey][0]);
    auto foundList = &listsMap[listKey];
    foundList->erase(foundList->begin());
    return firstElem;
}

int ListDB::handlePush(std::vector<std::string> singleCommand, int fd, bool left)
{
    auto listSize = 0;
    auto listKey = singleCommand[1];
    {
        std::lock_guard<std::mutex> pushLock(listLock);
        for (int i = 2; i < singleCommand.size(); i++)
            if (left)
                listsMap[listKey].insert(listsMap[listKey].begin(), singleCommand[i]);
            else
                listsMap[listKey].push_back(singleCommand[i]);

        listSize = listsMap[listKey].size();
    }

    if (waitingBlocks.find(listKey) != waitingBlocks.end())
    {
        auto oldVec = waitingBlocks[listKey];
        auto blockingFd = waitingBlocks[listKey][0];
        // remove the first one responded to
        waitingBlocks[listKey] = std::vector(oldVec.begin() + 1, oldVec.end());
        respondToBlock(blockingFd, listKey);
    }

    return writeResponse(fd, getRespInt(listSize));
}

int ListDB::getRange(std::vector<std::string> singleCommand, int fd)
{
    auto listKey = singleCommand[1];
    std::vector<std::string> currList;
    {
        std::lock_guard<std::mutex> rangeLock(listLock);
        if (listsMap.find(listKey) == listsMap.end())
            return writeResponse(fd, EMPTY_ARRAY);

        currList = listsMap[listKey];
    }
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

int ListDB::handleBlock(std::vector<std::string> singleCommand, int fd)
{
    auto listKey = singleCommand[1];
    auto passedTime = stof(singleCommand[2]);
    auto expTime = int(passedTime * 1000);
    auto msTime = std::chrono::milliseconds(expTime);
    auto currTime = std::chrono::system_clock::now();
    auto useTime = currTime + msTime;

    if (expTime == 0)
    {
        if (listsMap.find(listKey) == listsMap.end() || listsMap[listKey].empty())
        {
            waitingBlocks[listKey].push_back(fd);
            return 0;
        }
        return respondToBlock(fd, listKey);
    }

    if (expTime > 0)
    {
        std::thread waitForVal([this, listKey, fd, useTime, msTime]()
                               { this->timedBlock(fd, listKey, useTime, msTime); });
        waitForVal.detach();
    }
    return 0;
}

int ListDB::respondToBlock(int fd, std::string listKey)
{
    std::vector<std::string> retVec;
    retVec.push_back(listKey);
    retVec.push_back(popFirst(listKey));
    return writeResponse(fd, getRespArray(retVec));
}

void ListDB::timedBlock(int fd, std::string listKey, std::chrono::system_clock::time_point expiryTime, std::chrono::milliseconds msTime)
{
    while (1)
    {
        auto maxWait = std::chrono::milliseconds(100);
        auto fifthOfTime = msTime / 5;
        auto sleepTime = fifthOfTime < maxWait ? fifthOfTime : maxWait;
        std::this_thread::sleep_for(sleepTime);
        // std::cerr << "sleeping for:" << sleepTime.count() << "\n";

        if (listsMap.find(listKey) != listsMap.end() && listsMap[listKey].size() > 0)
        {
            respondToBlock(fd, listKey);
            return;
        }
        auto currTime = std::chrono::system_clock::now();
        if (currTime > expiryTime)
        {
            writeResponse(fd, RESP_NULL);
            return;
        }
    }
}
