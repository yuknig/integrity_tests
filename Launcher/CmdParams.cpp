#include "CmdParams.h"

CmdParams::CmdParams(int argc, char** argv) {
    std::string line;
    for (int arg_num = 1; arg_num < argc; ++arg_num) {
        const char* arg = argv[arg_num];
        if (!arg)
            continue;

        if (arg_num>1)
            line += ' ';
        line += arg;
    }
    FillParamMap(line);
}

CmdParams::CmdParams(const std::string& arg_line) {
    FillParamMap(arg_line);
}

CmdParams::CmdParams(ParameterMap params)
    : m_values(std::move(params))
{}

bool CmdParams::ParamExists(const std::string& name) const {
    return (m_values.count(name) == 1);
}

std::string CmdParams::GetParam(const std::string& name) const {
    return m_values.at(name);
}

const std::map<std::string, std::string>& CmdParams::ToMap() const {
    return m_values;
}

std::string CmdParams::ToString() const {
    return JoinTokens(m_values);
}

void CmdParams::FillParamMap(const std::string& line) {
    std::map<std::string, std::string> values;

    auto tokens = SplitTokens(line);
    size_t tok_num = 0;
    while (tok_num < tokens.size()) {
        std::string& key = tokens[tok_num++];
        if (key.empty())
            continue;

        if (key.find("--") == 0)
            key.erase(0, 2);

        std::string value;
        if (tok_num < tokens.size() && tokens[tok_num] == "=") {
            ++tok_num;
            if (tok_num < tokens.size())
                value = std::move(tokens[tok_num]);
            ++tok_num;
        }
        values.emplace(key, value);
    }
    m_values = std::move(values);
}

std::vector<std::string> CmdParams::SplitTokens(const std::string& line) {
    std::vector<std::string> tokens;
    size_t pos = 0;
    while (pos < line.size()) {
        size_t sep_pos = line.find_first_of(" \t=\'", pos);
        if (sep_pos == std::string::npos)
            sep_pos = line.size();

        {
            auto token = line.substr(pos, sep_pos - pos);
            if (!token.empty())
                tokens.emplace_back(std::move(token));
        }

        pos = sep_pos + 1;
        if (sep_pos >= line.size())
            break;

        const char sep = line[sep_pos];
        if (sep == '=')
            tokens.emplace_back("=");
        else if (sep == '\'') {
            sep_pos = line.find('\'', pos);
            if (sep_pos == std::string::npos)
                sep_pos = line.size();
            tokens.emplace_back(line.substr(pos, sep_pos - pos)); // allow empty string value if it's inside quotes
            pos = sep_pos + 1;
        }
    }
    return tokens;
}

std::string CmdParams::JoinTokens(const ParameterMap& params) {
    std::string result;
    for (const auto& key_value : params) {
        if (!result.empty())
            result += " ";
        result += key_value.first;
        if (!key_value.second.empty())
            result += "=\'" + key_value.second + "\'";
    }
    return result;
}
