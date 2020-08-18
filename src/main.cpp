#include <string.h>
#include <iostream>

#include "cmd_option.hpp"

extern int cmd_run(const ParseResult&);
extern int cmd_start(const ParseResult&);

int
main (int argc, char** argv) {
    CmdLine cmd((unsigned)argc, (const char**)argv);
    const ParseResult& cmd_info = cmd.Parse();

    if (!::strcmp(cmd_info.cmd_name, "run")) {
        return cmd_run(cmd_info);
    }

    if (!::strcmp(cmd_info.cmd_name, "start")) {
        return cmd_start(cmd_info);
    }

    return 0;
}
