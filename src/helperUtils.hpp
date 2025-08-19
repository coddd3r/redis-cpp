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

std::string getRespInt(int i)
{

    std::string retStr;
    retStr.append(":");
    retStr.append(std::to_string(i));
    retStr.append("\r\n");
    return retStr;
}

int writeResponse(int fd, std::string buf)
{

    auto actualBuf = buf.c_str();
    std::cerr << "Writing to stream buff: " << actualBuf << "\n";
    return write(fd, actualBuf, buf.length());
}

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
