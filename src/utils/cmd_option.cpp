#include <string.h>
#include <iostream>
#include <iomanip> // for setw
#include "cmd_option_impl.hpp"
#include "cmd_option.hpp"

using namespace std;

// forward decl
static void PringUsage();
static void PrintCmdUsage(const CommandInfo&);

////////////////////////////////////////////////////////////////////////////
//
//                  Global data
//
////////////////////////////////////////////////////////////////////////////
//
static const char* ProgramName = 0;

#define ARRAY_SZ(a) (sizeof((a))/sizeof((a)[0]))

// global options
static const OptionInfo GlobalOptions[] = {
    {"--help", "-h", OPTION_VAL_NO,  0, 0, "Print Usage"}
};

static OptionGroup GlobalOptionGroup(GlobalOptions,
    sizeof(GlobalOptions)/sizeof(GlobalOptions[0]));

static const OptionInfo DummyOptions[] = {
    {"--help", "-h", OPTION_VAL_NO,  0, 0, "Print Usage"},
};

// "run" command options
static const OptionInfo RunOptions[] = {
    {"--help", "-h", OPTION_VAL_NO,  0, 0, "Print Usage"},
    {"--name", "-n", OPTION_VAL_YES, "container_name", 0, "container name"},
    {"--volume", "-v", OPTION_VAL_YES, "volume_specs", 0,
        "comma seperated volume-specs, e.g. src1:dest1:ro,src2:dest2"},
    {"--memory", "-m", OPTION_VAL_YES, "memory limit", 0, "e.g. 1G"}
    /*
    {"--tty",  "-t", OPTION_VAL_NO,  0, 0, "Allocate a pseudo-TTY"},
    {"--interactive", "-i", OPTION_VAL_NO, 0, 0,
        "Keep STDIN open even if not attached"}
    */
};

// "start" command options
static const OptionInfo StartOptions[] = {
    {"--help", "-h", OPTION_VAL_NO,  0, 0, "Print Usage"},
    {"--name", "-n", OPTION_VAL_YES, "container_name", 0, "container name"}
};

// We classify commands into two categories: admin command and non-admin cmds
static const CommandInfo NonAdminCmds[] = {
    {"run", RunOptions, ARRAY_SZ(RunOptions),
     "run [OPTIONS] IMAGE [COMMAND] [ARG...]",
     "Run a command in a running container"},

    {"start", StartOptions, ARRAY_SZ(StartOptions),
     "start [OPTIONS]",
     "Start a stopped container"},
};

static CommandGroup NonAdminCmdGroup("Commands", NonAdminCmds,
                                     ARRAY_SZ(NonAdminCmds));

static const CommandInfo AdminCmds[] = {
    {"system", DummyOptions, ARRAY_SZ(DummyOptions), "WTF", "WTF"}
};

static CommandGroup AdminCmdGroup("Commands", AdminCmds,
    sizeof(AdminCmds)/sizeof(AdminCmds[0]));

#undef ARRAY_SZ

static void
SetOptionMap(std::map<std::string, std::string>* result,
             const char* long_name, const char* short_name,
             const char* val) {
    const char* val_dup = val ? strdup(val) : "";
    if (long_name) {
        (*result)[long_name] = string(val_dup);
    }

    if (short_name) {
        (*result)[short_name] = string(val_dup);
    }
}

bool
ParseOptions(const OptionGroup& opt_group, ArgVect* argv,
             std::map<std::string, std::string>* result,
             std::string* err_msg) {
    result->clear();
    *err_msg = "";

    int idx = 0;
    int idx_e = argv->argc;
    char buf[256];

    while (idx < idx_e) {
        const char* opt_name = argv->argv[idx];
        if (opt_name[0] != '-')
            break;

        auto opt = opt_group.Lookup(opt_name);
        if (opt == 0) {
            // unknown option
            snprintf(&buf[0], sizeof(buf), "unknown option %s", opt_name);
            *err_msg = buf;
            return false;
        }

        const char* long_name = opt->LongName;
        const char* short_name = opt->ShortName;

        // case 1: this option should not take argument
        if (opt->TakeValue == OPTION_VAL_NO) {
            idx++;
            SetOptionMap(result, long_name, short_name, 0);
            continue;
        }

        // case 2: this option should take an argument
        if (opt->TakeValue == OPTION_VAL_YES) {
            if (idx + 1 == idx_e) {
                snprintf(&buf[0], sizeof(buf),
                         "option %s take an argument", opt_name);
                *err_msg = buf;
                return false;
            }
            SetOptionMap(result, long_name, short_name, argv->argv[idx+1]);
            idx += 2;
            continue;
        }

        // case 2: this option may or may not take an argument
        if (idx + 1 == idx_e)
            break;

        const char* peek = argv->argv[idx + 1];
        if (peek[0] == '-') {
            idx ++;
            SetOptionMap(result, long_name, short_name, opt->DefaultValue);
        } else {
            SetOptionMap(result, long_name, short_name, peek);
            idx += 2;
        }
    }

    argv->argc -= idx;
    argv->argv += idx;
    return true;
}

////////////////////////////////////////////////////////////////////////////
//
//           Implementation of OptionGroup
//
////////////////////////////////////////////////////////////////////////////
//
OptionGroup::OptionGroup(const OptionInfo* opts, int opt_num):
    _opts(opts), _opt_num(opt_num) {
    for (int idx = 0; idx < opt_num; idx++) {
        const OptionInfo* oi = opts + idx;
        const char* name = oi->LongName;
        if (name != 0) {
            _opt_map[name] = oi;
        }
        name = oi->ShortName;
        if (name != 0) {
            _opt_map[name] = oi;
        }
    }
}

const OptionInfo*
OptionGroup::Lookup(const char* opt_name) const {
    auto found = _opt_map.find(opt_name);
    if (found != _opt_map.cend()) {
        return found->second;
    }
    return 0;
}

void
OptionGroup::Print(const char* title) {
    // e.g.
    // 012345678901234567890.....
    //   -x, --wtf string   A very very vague option
    int long_opt_len = 0;
    for (int iter = 0, iter_e = _opt_num; iter < iter_e; ++iter) {
        int len = 0;

        const OptionInfo* opt = _opts + iter;
        if (opt->LongName != 0) {
            len += ::strlen(opt->LongName) + 1;
        }

        if (opt->TakeValue != OPTION_VAL_NO && opt->ValName != 0) {
            len += ::strlen(opt->ValName) + 1;
        }

        if (len > long_opt_len)
            long_opt_len = len;
    }

    cout << title << ":" << endl;
    for (int iter = 0, iter_e = _opt_num; iter < iter_e; iter++) {
        const OptionInfo* opt = _opts + iter;
        // print short option
        if (opt->ShortName)
            cout << "  " << opt->ShortName << ", ";
        else
            cout << "      ";

        // print long option and help-info
        string s = "";
        if (opt->LongName) {
            s = opt->LongName;
            if (opt->ValName) {
                s += " ";
                s += opt->ValName;
            }
        }
        cout << setw(long_opt_len + 4) << left << s << opt->HelpInfo << endl;
    }
}

////////////////////////////////////////////////////////////////////////////
//
//           Implementation of CommandInfo & CommandGroup
//
////////////////////////////////////////////////////////////////////////////
//
void
CommandInfo::Print() const {
    cout << "Usage: " << ProgramName << syntax << endl << endl
         << usage << endl;

    OptionGroup og(opt_grp, opt_num);
    og.Print("Options");
}

CommandGroup::CommandGroup(const char* group_name, const CommandInfo* cmds,
                           int cmd_num):
    _group_name(group_name), _cmds(cmds), _cmd_num(cmd_num)
{
}

const CommandInfo*
CommandGroup::GetCmdInfo(const char* name) {
    for (int idx = 0; idx < _cmd_num; idx++) {
        const CommandInfo* ci = _cmds + idx;
        if (::strcmp(ci->name, name) == 0) {
            return ci;
        }
    }
    return 0;
}

void
CommandGroup::Print() {
    cout << _group_name << ":" << endl;

    int cmd_name_width = 0;
    for (int idx = 0, idx_e = _cmd_num; idx < idx_e; idx++) {
        const char* name = _cmds[idx].name;
        if (name[0] == '_') { continue; /*internal command*/}

        int name_len = ::strlen(name);
        if (name_len > cmd_name_width)
            cmd_name_width = name_len;
    }

    for (int idx = 0, idx_e = _cmd_num; idx < idx_e; idx++) {
        if (_cmds[idx].name[0] == '_') continue;
        const CommandInfo& ci = _cmds[idx];
        cout << "  " << setw(cmd_name_width + 4) << ci.name << ci.usage << endl;
    }
}

////////////////////////////////////////////////////////////////////////////
//
//           Implementation of CmdLine
//
////////////////////////////////////////////////////////////////////////////
//
CmdLine::CmdLine(unsigned argc, const char** argv) {
    ProgramName = argv[0];
    _argc = argc - 1;
    _argv = argv + 1;

    if (_argc == 0) {
        static const char* myargv[] = {"-h"};
        _argc = 1;
        _argv = myargv;
    }
}

void __attribute__((noreturn))
CmdLine::_abort(const string& msg) {
    cerr << msg << endl;
    ::exit(1);
}

const ParseResult&
CmdLine::Parse() {
    // step 1: parse global options
    ArgVect arg_vect(_argc, _argv);

    string err_msg;
    map<string, string>& global_opt = _parse_result.global_opt;

    bool rc = ParseOptions(GlobalOptionGroup, &arg_vect,
                           &global_opt, &err_msg);
    if(!rc)
        _abort(err_msg);

    if (global_opt.find("--help") != global_opt.cend() ||
        global_opt.find("-h") != global_opt.cend()) {
        PringUsage();
        ::exit(0);
    }

    // step 2: get the command name
    if (arg_vect.argc == 0) {
        cerr<< "no command specified" << endl;
        ::exit(1);
    }

    auto cmd = arg_vect.argv[0];
    arg_vect = arg_vect.Shift(1);

    auto cmd_info = AdminCmdGroup.GetCmdInfo(cmd);
    if (cmd_info == 0) {
        cmd_info = NonAdminCmdGroup.GetCmdInfo(cmd);
    }

    if (!cmd_info) {
        cerr << "unknown command: " << cmd << endl;
        exit(1);
    }

    // step 3: parse the command specific options
    map<string, string>& cmd_opt = _parse_result.cmd_opt;
    OptionGroup opt_grp(cmd_info->opt_grp, cmd_info->opt_num);
    rc = ParseOptions(opt_grp, &arg_vect, &cmd_opt, &err_msg);
    if (!rc) {
        _abort(err_msg);
    }

    if (cmd_opt.find("-h") != cmd_opt.end() ||
        cmd_opt.find("--help") != cmd_opt.end() ||
        (cmd_opt.empty() && arg_vect.argc == 0)) {
        PrintCmdUsage(*cmd_info);
        exit(0);
    }

    _parse_result.cmd_name = cmd_info->name;
    _parse_result.cmd_argc = arg_vect.argc;
    _parse_result.cmd_argv = arg_vect.argv;

    return _parse_result;
}

////////////////////////////////////////////////////////////////////////////
//
//           Misc Fucntions
//
////////////////////////////////////////////////////////////////////////////
//
static void
PringUsage() {
    cout << "Usage: " << ProgramName
         << " [global options] COMMAND [command options] command-arguments"
         << endl
         << "A toy grade docker implementation" << endl;

    // print global options
    GlobalOptionGroup.Print("Global Options");

    // print 'Management Commands' group
    AdminCmdGroup.Print();

    cout << endl;

    // print 'normal commands' group
    NonAdminCmdGroup.Print();
}

static void
PrintCmdUsage(const CommandInfo& ci) {
    cout << " " << ci.syntax << endl;

    if (ci.opt_num != 0) {
        cout << endl;
        cout << "   Options include following:" << endl << endl;
    }

    for (int i = 0; i < ci.opt_num; i++) {
        const OptionInfo& oi = ci.opt_grp[i];

        const char* arg = oi.TakeValue == OPTION_VAL_YES ? oi.ValName : "";
        cout << "   " << oi.LongName << ", "
             << oi.ShortName << " ";
        if (arg[0]) {
            cout << arg << " ";
        }
        cout << ": " << oi.HelpInfo << endl;
    }
    cout << flush;
}
