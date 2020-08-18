#ifndef _CMD_OPTION_H_
#define _CMD_OPTION_H_

////////////////////////////////////////////////////////////////////////////
//
// A command line is in this format:
//  [global option] command [command-specific-option] command argument
//
////////////////////////////////////////////////////////////////////////////
//
#include <map>
#include <cstdlib>

typedef std::map<std::string, std::string> OptionPairs;

struct ParseResult {
    OptionPairs global_opt;
    const char* cmd_name;
    OptionPairs cmd_opt;
    int cmd_argc;
    const char** cmd_argv;

    ParseResult(): cmd_name(0), cmd_argc(0), cmd_argv(0) {}
    ~ParseResult() {}

    const char* getOptionValue(const char* opt) const {
        auto iter = cmd_opt.find(opt);
        if (iter != cmd_opt.cend()) {
            return iter->second.c_str();
        }
        return 0;
    }

    bool isOptionSpecified(const char* opt) const {
        return cmd_opt.find(opt) != cmd_opt.cend();
    }
};


class CmdLine {
public:
    CmdLine(unsigned argc, const char** argv);
    const ParseResult& Parse();
    void PrintUsage() const;

private:
    void _abort(const std::string&);

private:
    unsigned _argc;
    const char** _argv;
    ParseResult _parse_result;
};

#endif
