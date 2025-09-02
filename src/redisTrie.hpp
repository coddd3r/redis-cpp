#include <cstdint>
#include <ctime>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

struct StreamID
{
        uint64_t timestamp;
        uint64_t sequence;

        StreamID() : timestamp(0), sequence(0) {}
        StreamID(uint64_t ts, uint64_t seq) : timestamp(ts), sequence(seq) {}

        // comparsion
        bool operator<(const StreamID &other) const
        {
            return timestamp < other.timestamp || (timestamp == other.timestamp && sequence < other.sequence);
        }

        bool operator==(const StreamID &other) const
        {
            return timestamp == other.timestamp && sequence == other.sequence;
        }
        std::string toString() const;
        static StreamID fromString(const std::string &id_str);
};

class RadixNode
{
    public:
        std::map<char, std::unique_ptr<RadixNode>> children;
        std::optional<StreamID> id;
        std::string value;
        bool is_leaf = false;

        RadixNode() = default;
};

class RedisStreamRadix
{
    private:
        std::unique_ptr<RadixNode> root;
        StreamID last_id{0, 0};

        StreamID generateID(const std::string &id_spec);
        void insertToRadix(const StreamID &id, const std::string &value);
        std::vector<std::pair<StreamID, std::string>> rangeQuery(const StreamID &start, const StreamID &end) const;
        void rangeQueryHelper(const RadixNode *node, const std::string &prefix, const std::string &start, const std::string &end, std::vector<std::pair<StreamID, std::string>> &result) const;

    public:
        RedisStreamRadix() : root(std::make_unique<RadixNode>()) {}
        StreamID xadd(const std::string &id_spec, const std::string &value);
        StreamID xadd(std::vector<std::string> singleCommand,
                      std::unordered_map<std::string, RedisStreamRadix> &streamsDB);
        std::vector<std::pair<StreamID, std::string>> xrange(const StreamID &start, const StreamID &end) const;
};
