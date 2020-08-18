#include <iostream>

#include "cmd_option.hpp"
#include "container/container.hpp"

using namespace std;

int
cmd_start(const ParseResult& cmd) {
    const char* container_name = cmd.getOptionValue("--name");

    ContainerInstance::exitIfNameInvalid(container_name);

    // check if the specified container exist
    if (!ContainerInfo::AlreadyExist(container_name)) {
        cerr << "container " << container_name << " does not exist" << endl;
        return 1;
    }

    // check if the containe is still running
    if (ContainerInfo::isRunning(container_name)) {
        cerr << "container " << container_name << " is running" << endl;
        return 1;
    }

    ContainerInstance inst(container_name);
    inst.deserialize();

    inst.CreateVolumeMountPoints();

    inst.setReadyToRun();

    bool rc = inst.run();
    inst.Cleanup();

    return rc ? 0 : -1;
}
