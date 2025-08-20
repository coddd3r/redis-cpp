#include <iostream>
#include <string>
#include <vector>
class MainDB
{
        std::unordered_map<std::string, dbValue> htmap;

    public:
        void set(std::vector<std::string> singleCommand, std::string value)
        {
            dbValue newVal;

            newVal.val = value;
            newVal.hasExpiry = false;
            if (singleCommand.size() > 4)
            {
                auto timeType = singleCommand[3];
                std::transform(timeType.begin(), timeType.end(), timeType.begin(),
                               [](unsigned char c)
                               { return std::tolower(c); });
                if (timeType == "px")
                {
                    newVal.hasExpiry = true;
                    auto expTime = stoi(singleCommand[4]);
                    auto msTime = std::chrono::milliseconds(expTime);
                    auto currTime = std::chrono::system_clock::now();
                    auto useTime = currTime + msTime;
                    newVal.expiryTime = useTime;
                    std::cerr << "\n\nadding expiry of: " << msTime.count()
                              << "to current time: " << currTime.time_since_epoch().count() << "\n\n"
                              << " expiry set to " << useTime.time_since_epoch().count() << "\n\n";
                }
            }
            htmap[singleCommand[1]] = newVal;
        }

        const int get(const std::string key, int fd)
        {
            for (const auto &[key, value] : htmap)
                std::cerr << "IN KV LOOP\n"
                          << " found key: " << key << "\nval: " << value.val << "\n";

            std::cerr << "map size: " << htmap.size() << "\n";

            if (htmap.find(key) != htmap.end())
            {

                auto curr_val = htmap[key];
                std::cerr << "Writing GET val: " << curr_val.val << "\n"
                          << "val has expiry?: " << curr_val.hasExpiry << "\n"
                          << "val  expiry?: " << curr_val.expiryTime.time_since_epoch().count() << "\n";
                auto currTime = std::chrono::system_clock::now();
                std::cerr << "currTIme" << currTime.time_since_epoch().count() << "\n";

                if (!curr_val.hasExpiry ||
                    (curr_val.hasExpiry &&
                     curr_val.expiryTime > currTime))
                    return writeResponse(fd, getBulkString(curr_val.val));
            }
            std::cerr << "Failed GET for: " << key << "\n";
            writeResponse(fd, RESP_NULL);
            return 0;
        }

        int getType(std::vector<std::string> singleCommand, int fd)
        {
            if (singleCommand.size() > 1)
            {
                auto key = singleCommand[1];
                if (htmap.find(key) != htmap.end())
                    return writeResponse(fd, STRING_TYPE);
            }
            return writeResponse(fd, NONE_TYPE);
        }
};
