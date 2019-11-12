#pragma once
#include <vector>
#include <map>
#include <string>

class CmdParams {
public:
    using ParameterMap = std::map<std::string, std::string>;

    CmdParams(int argc, char** argv);
    CmdParams(const std::string& arg_line);
    CmdParams(ParameterMap params);

    bool ParamExists(const std::string& name) const;
    std::string GetParam(const std::string& name) const;
    const std::map<std::string, std::string>& ToMap() const;
    std::string ToString() const;

private:
    void FillParamMap(const std::string& line);
    static std::vector<std::string> SplitTokens(const std::string& line);
    static std::string JoinTokens(const ParameterMap& params);

private:
    ParameterMap m_values;
};
