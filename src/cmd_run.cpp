#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <sys/mount.h>
#include <sys/wait.h>
#include <sched.h>
#include <unistd.h>
#include <limits.h>
#include <stdio.h>

#include <string>
#include <iostream>
#include <sstream>

#include "cmd_option.hpp"
//#include "utils/cmd.hpp"
#include "utils/clone_exec.hpp"
#include "container/volume.hpp"
#include "container/container.hpp"

using namespace std;

int
cmd_run(const ParseResult& cmd) {
    // check if container name is specified
    const char* container_name = cmd.getOptionValue("--name");
    if (!container_name) {
        cerr << "container name is not specified" << endl;
        return 1;
    }

    ContainerInstance::exitIfNameInvalid(container_name);

    if (ContainerInfo::AlreadyExist(container_name)) {
        cerr << "container " << container_name << " already exist" << endl;
        return 1;
    }

    // check if image is specified
    if (cmd.cmd_argc < 1) {
        cerr << "no image specified\n";
        return 1;
    }
    const char** cmd_argv = cmd.cmd_argv;
    int cmd_argc = cmd.cmd_argc;
    const char* image_name = cmd_argv[0];

    cmd_argv++;
    cmd_argc--;
    ContainerInstance c_instance(image_name, container_name, cmd_argv, cmd_argc);

    // add volumes
    const char* volume_specs = cmd.getOptionValue("--volume");
    if (volume_specs) {
        stringstream ss(volume_specs);
        string vol_spec;

        while (std::getline(ss, vol_spec, ','/*delimiter*/)) {
            if (!Volume::IsValidSpec(vol_spec)) {
                cerr << "invalid volume spec: " << vol_spec << endl;
                return -1;
            }
            (void)c_instance.AddVolume(vol_spec);
        }
    }

    // apply cgroup
    const char* mem_limit = cmd.getOptionValue("--memory");
    if (mem_limit) {
        c_instance.AddCgroup(new CgroupMem(container_name, mem_limit));
    }

    (void)c_instance.UmountVolumes();
    (void)c_instance.UmountRoot(true/*force*/);

    if (!c_instance.InitDirsAndFiles()) {
        return -1;
    }

    c_instance.serialize();
    c_instance.setReadyToRun();
    bool succ = c_instance.run();
    c_instance.Cleanup();

    return succ ? 0 : -1;
}
