#ifndef _CMD_OPTION_IMPL_H_
#define _CMD_OPTION_IMPL_H_

#include <string.h>
#include <string>
#include <vector>
#include <map>

struct ArgVect {
    unsigned argc;
    const char** argv;
    ArgVect(unsigned c, const char** v): argc(c), argv(v) {}

    ArgVect Shift(unsigned n) {
        return n > argc ?
               ArgVect(0, 0) :
               ArgVect(argc - n, argv + n);
    }
};

typedef enum {
    OPTION_VAL_NO,
    OPTION_VAL_YES,
    OPTION_VAL_OPTIONAL,
} OPTION_TAKE_VAL;

// describe both global-option and command-specific option
typedef struct {
    const char* LongName;
    const char* ShortName;
    OPTION_TAKE_VAL TakeValue;
    const char* ValName;
    const char* DefaultValue;
    const char* HelpInfo;
} OptionInfo;

class OptionGroup {
public:
    OptionGroup(const OptionInfo* opts, int opt_num);
    ~OptionGroup() {};

    void Print(const char* title);
    const OptionInfo* Lookup(const char* opt_name) const;

    const OptionInfo* GetOptions() const { return _opts; }
    const unsigned int GetOptionNum() const { return _opt_num; }
    const std::map<std::string, const OptionInfo*>& GetOptionMap() const {
        return _opt_map;
    }

private:
    std::map<std::string, const OptionInfo*> _opt_map;
    const OptionInfo* _opts;
    unsigned int _opt_num;
};

bool ParseOptions(const OptionGroup& opt_group, ArgVect* argv,
                  std::map<std::string, const char*>* result,
                  std::string* err_msg);


struct CommandInfo {
    const char* name; // command name
    const OptionInfo* opt_grp; // supported options
    int opt_num;
    const char* syntax;
    const char* usage;

    void Print() const;
};

class CommandGroup {
public:
    CommandGroup(const char* group_name, const CommandInfo* cmds, int cmd_num);
    const CommandInfo* GetCmdInfo(const char* name);

    void Print();
private:
    const char* _group_name;
    const CommandInfo* _cmds;
    int _cmd_num;
};

#endif
