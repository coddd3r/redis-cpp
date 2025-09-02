#include <chrono>
#include <ranges>
#include <stdexcept>
#include <vector>

#include "redisTrie.hpp"

// STREAMID
std::string StreamID::toString() const
{
    return std::to_string(timestamp) + "-" + std::to_string(sequence);
}

StreamID StreamID::fromString(const std::string &id_str)
{
    auto dash_pos = id_str.find('-');
    if (dash_pos == std::string::npos)
    {
        throw std::invalid_argument("Invalid Stream ID format");
    }

    // strip whitespace and convert to int
    uint64_t ts = std::stoull(id_str.substr(0, dash_pos));
    uint64_t seq = std::stoull(id_str.substr(dash_pos + 1));
    return StreamID(ts, seq);
}

// RADIX
StreamID RedisStreamRadix::generateID(const std::string &id_spec)
{
    if (id_spec == "*")
    {
        // Auto-generate both parts
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

        if (last_id.timestamp == now)
            return StreamID(now, last_id.sequence + 1);
        else
            return StreamID(now, 0);
    }
    else if (id_spec.find('*') != std::string::npos)
    {
        // generate only sequence part
        auto dash_pos = id_spec.find('-');
        uint64_t ts = std::stoull(id_spec.substr(0, dash_pos));

        if (last_id.timestamp == ts)
            return StreamID(ts, last_id.sequence + 1);
        else
            return StreamID(ts, 0);
    }
    else
        return StreamID::fromString(id_spec);
}

void RedisStreamRadix::insertToRadix(const StreamID &id, const std::string &value)
{
    std::string key = id.toString();
    RadixNode *node = root.get();

    for (size_t i = 0; i < key.size(); ++i)
    {
        char c = key[i];
        // if the char does not exist as a child make it
        if (node->children.find(c) == node->children.end())
        {
            node->children[c] = std::make_unique<RadixNode>();
        }
        node = node->children[c].get();
    }
    // set curr node as a leaf
    node->is_leaf = true;
    node->id = id;
    node->value = value;
}

std::vector<std::pair<StreamID, std::string>> RedisStreamRadix::rangeQuery(const StreamID &start, const StreamID &end) const
{
    std::vector<std::pair<StreamID, std::string>> result;
    std::string start_key = start.toString();
    std::string end_key = end.toString();

    rangeQueryHelper(root.get(), "", start_key, end_key, result);
    return result;
}

void RedisStreamRadix::rangeQueryHelper(const RadixNode *node, const std::string &prefix, const std::string &start, const std::string &end, std::vector<std::pair<StreamID, std::string>> &result) const
{
    // add found leaf nodes
    if (node->is_leaf && node->id)
    {
        auto full_key = prefix;
        if (full_key >= start && full_key <= end)
        {
            result.emplace_back(*node->id, node->value);
        }
    }

    // got through all children recursively
    for (const auto &[c, child] : node->children)
    {
        std::string new_prefix = prefix + c;

        if (new_prefix <= end && (new_prefix += "~") >= start)
        {
            rangeQueryHelper(child.get(), new_prefix, start, end, result);
        }
    }
}

StreamID RedisStreamRadix::xadd(std::vector<std::string> singleCommand,
                                std::unordered_map<std::string, RedisStreamRadix> &streamsDB)
{
    const std::string stream_key = singleCommand[1];
    const std::string &id_spec = singleCommand[2];
    // TODO:
    const std::string value;
    StreamID new_id = generateID(id_spec);
    insertToRadix(new_id, value);
    last_id = new_id;
    return new_id;
}

std::vector<std::pair<StreamID, std::string>> RedisStreamRadix::xrange(const StreamID &start, const StreamID &end) const
{
    return rangeQuery(start, end);
}
