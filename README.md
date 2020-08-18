A toy-grade container
----------------------------------

## Index
* [Overview](#overview)
* [Limitations](#limitation)
* [Prerequisite](#prerequisite)
* [How to Build](#how-to-build)
* [Building Blocks and Key Concepts](#building-blocks-and-key-concepts)
    - [image](#image)
    - [union mount](#union-mount)
    - [pivot root](#pivot-root)
    - [container](#container)
    - [volume](#volume)
    - [control group](#control-group)
    - [virtual networking](#virtul-networking)
* [Operation](#operation)
    - [import image](#import-image)
    - [start container](#start-container)
    - [stop container](#stop-container)
    - [resume container](#resume-container)
    - [Remove container][#remove-container]

* [Resource](#resouce)

### Overview
TODO: finish the README, fix typo etc, etc.

I have been curious about what under the hood of the Docker container. After
reading some articles about Docker internal, I decide to roll out my
own toy-grade container.

In this work, I focus only on how to implement those relevant key concept/technology,
and how to orchestrate them making them work together, and completely ignore
code quality as well as other aspects that are normally important to a project.
I will not invest time to polish the code albeit it's in poor shape.

As of today, it can run some full fledged Docker images.
Following commands demonstrate how to play with this toy container.
```bash
# import ubuntu:latest image from Docker registry
sudo ./import_image.sh ubuntu:latest

# I didn't get chance to setup NAT in the container. This script enable ip-fowarding
# and create NAT rule. You don't have to run this script if you don't need to access
# network outside the host computer.
sudo ./vnet_todo.sh

# start the container and run /bin/bash
sudo ./diy_docker run --name mytoy ubuntu /bin/bash

# -------------> inside the container <---------------
apt-get update
apt-get install -y curl nginx
nginx # start nginx daemon listening at port 80

# -------------> in another terminal of same host computer <--------------
curl 172.19.0.2 # will you get the response from nginx

# -------------> switch back to the termianl running container <------------
ip addr
...
veth0_mytoy@if8: ...
    ...
    inet 172.19.0.2/16 brd 172.19.255.255 scope global veth0_mytoy
...
exit # exit container
...
# shutdown computer; out to grab some yummy food.
...
# return home, reboot the machine, resume the container
sudo ./diy_docker start --name mytoy
which nginx
/usr/sbin/nginx # nice the change persist across reboot
```

### Limitations and Troubleshooting
There are lots of limitations, in particular:

* Since the names of virtual-networking-devices as well the IP addresses bound to them
  are hard-coded, there should be no more than one active (i.e. running not stopped)
  container at any given time.
* It does not perform properly clean-up before premature exit. So, if you run into
  some strange bug,
    - Check if those veth created by stopped containers are deleted
       via command `ip link | grep "<the_stopped_container_name>"`.
      Delete them manually by `sudo ip link delete <veth_name>`
    - Check if file system is unmounted properly by command
      `mount | grep /var/lib/docker_diy/containers/<stopped_container_name`.

### How to build
```bash
git submodule update --init --recursive # if submodule is old not not present
make
```

### Building Blocks and Key Concepts
#### Image
We *steal* image from Docker. First, export Docker image into
a tarball, and then expand the resulting tarball under the directory of
`/var/lib/docker_diy/images/<image_name>/<image_tag>`. Script `import_image.sh`
can be used to steal image from Docker.

Images are read only.

#### Container
Containder resides under `/var/lib/docker_diy/containers/<container_name>`.
Currently, there are five files/dirs directly under this folder:
* `pid.txt`: the pid of the entry process
* `info.json`: sort of meta data of the container. We write to this file when
    we start a new container, and read from this file when resuming the stopped
    container.
* `merge`,  `upper` and `workdir`: there dirs are `layers` in Overlay2 file system.
    see [next section](#union-Mount) for details.

#### Union Mount
As with Docker, we are using [Overlay Filesystem](#https://www.kernel.org/doc/Documentation/filesystems/overlayfs.txt)
for union mount. There are bunch of layers/dirs involved in OverlayFS:
* lower: the read-only image
* upper: If you made some change (e.g. install some packages) to the container.
  All change actually goes to this layer.
* workdir: nothing but an empty dir
* merge: the resulting file system unioning the lower and the upper layer.

#### Pivot Root
The container need to run under new root directory pointing to resulting merged union mount
file system (see [union mount](#union-mount) for details). Function `PivotRoot()` is for that
purpose. I steal code from [here](https://lwn.net/Articles/800381/) with minor change to make
it capable of mounting [volume](#volume).

After changing the root directory, two dirs need to be created: `/proc` and `/dev`.
Devices need to created manually. So far I populate `/dev/` by creating
`/dev/zero` and `/dve/null`.

#### Volume
`volume` is used to mount host's file system to container's file system. For example,
to mount host dir `/my/host/dir` into containder `/my/container/dir`, do

`sudo ./diy_docker run --name "mytoy" --volume '/my/host/dir:/my/container/dir' ubuntu /bin/bash`.

Optionally, appending `:ro` to the volume flag to make the mount read-only.

I don't know how to do it right. My approach seems to be working anyway. Follow above example,
mounting volume requires following steps:
* before union-mount, create mounting point
    `/var/lib//docker_diy/containers/mytoy/upper/my/container/dir` in the upper layer.
* Union mount
* bind-mount `/my/host/dir` to the mount-point created by the first step.

These steps need to be done before [privoting-root](#pivot-root)

#### Control group
So far, only implement cgroup regarding memory limit. Use `--memory` to restrict memory usage.
e.g.:  `sudo ./diy_docker run --name "lol" --memory 1G ubuntu /bin/bash`.

#### Virtual Networking
##### Name resolution
In order to resolve name inside the container, we need to bind-mount following regular files to into container.
* `/etc/hosts`
* `/etc/hostname`
* `/run/systemd/resolve/resolv.conf` or `/etc/resolv.conf`

If [systemd-resolved daemon](#https://wiki.archlinux.org/index.php/Systemd-resolved) is enabled, the
nameserver specified in `/etc/resolv.conf` is `127.0.0.254` which is not accessiable from container.
Therefore, in this case, we should go for `/run/systemd/resolve/resolv.conf`.

##### Vitual Device and Isolation
This [article](#https://ops.tips/blog/using-network-namespaces-and-bridge-to-isolate-servers/) depict
what I'm trying to achive. Unfortunately, I'm not a hard-working person. I cut the corner by creating
a pair of [veth](#https://man7.org/linux/man-pages/man4/veth.4.html) only:
* veth0_<container_name>: bind to ip `172.19.0.2`
* veth1_<container_name>: bind to ip `172.19.0.3`

With these two veth, we can access services running in hosts from container, or access services
running in container from host (using 172.19.0.2 ip). If you wish to access services provided
in other computer, we need to enable ip-forwarding and create NAT rule. `./vnet_todo.sh` is provided
for that purpose.

### Operation
#### import image
`sudo ./import_image.sh <image_name>:<image_tag>`

#### start container
e.g.
`sudo ./diy_docker run --name mytoy --volume /my/host/dir:/foo/bar:ro --memory 1G  ubuntu /bin/bash`

### stop container
just exit the running shell

### resume container
e.g. 
`sudo ./diy_docker start --name lol`

### Remove container
stop it first, then run
`./rm_container <container_name>`

### Resource
TBD
