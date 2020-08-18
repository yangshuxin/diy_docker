#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <unistd.h>

#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <linux/veth.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <strings.h> // bzero
#include <string.h> // memcpy
#include <stdio.h>
#include <fcntl.h>

#include <string>
#include <memory> // for std::shared_ptr
#include "err_handling.hpp"
#include "virtual_net.hpp"

//shit
#include <iostream>

using namespace std;

/////////////////////////////////////////////////////////////////////////
//
//          Implementation of NlReq
//
/////////////////////////////////////////////////////////////////////////
//
class RtNlReq {
public:
    RtNlReq(uint32_t rtnl_req_sz=1024);
    ~RtNlReq() { delete[] _rtnl_req_buf; }

    struct rtattr* Add_rtattr(uint32_t type, const char* data=0, uint32_t data_sz=0);
    void Close_rtattr(struct rtattr* attr);

    const string& GetErrMsg() const { return _last_err; }
    struct nlmsghdr* GetNlMsgHdr() const { return _rtnl_req; }

	struct rtattr* GetNextAttrAddr() const;
    void dump(const char* title);

private:
    uint32_t _rtnl_req_sz;
    union {
        nlmsghdr *_rtnl_req;
        char *_rtnl_req_buf;
    };
    string _last_err;
};

RtNlReq::RtNlReq(uint32_t rtnl_req_sz) :
    _rtnl_req_sz(rtnl_req_sz) {
    _rtnl_req_buf = new char[rtnl_req_sz];
    bzero(_rtnl_req_buf, rtnl_req_sz);

    // init the nlmsg_len to be sizeof(hdr) + sizeof(ifinfomsg) + alignment
}

struct rtattr*
RtNlReq::GetNextAttrAddr() const {
    char* ptr = _rtnl_req_buf + NLMSG_ALIGN(_rtnl_req->nlmsg_len);
    return (struct rtattr*)(void*)ptr;
}

void
RtNlReq::dump(const char* title) {
    // TODO: replace fprintf with cout
    if (title) {
        fprintf(stderr, "%s\n", title);
    }

    int _xor = 0;
    char* p = (char*)(void*)_rtnl_req;
    for (int i = 0; i < int(_rtnl_req->nlmsg_len); i++) {
        fprintf(stderr, "%02x ", p[i]);
        _xor = _xor ^ p[i];
        if (!((i+1) % 8)) {
            fprintf(stderr, "xor: %02x\n", _xor);
            _xor = 0;
        }
    }

    fprintf(stderr, "\n");
}

struct rtattr*
RtNlReq::Add_rtattr(uint32_t type, const char* data, uint32_t data_sz) {

    // rta_sz take into account the attribute payload and header
    auto rta_sz = RTA_LENGTH(data_sz);

    // check if the <_rtnl_req> is large enough to accomodate the
    // new attribute
    auto new_sz = NLMSG_ALIGN(_rtnl_req->nlmsg_len) + RTA_ALIGN(rta_sz);

    if (new_sz > _rtnl_req_sz) {
        _last_err = "not large enough to accomodate the new attribute";
        return 0;
    }

    struct rtattr *rta = GetNextAttrAddr();
    rta->rta_type = type;
    rta->rta_len = rta_sz;

    if (data_sz) {
        memcpy(RTA_DATA(rta), data, data_sz);
    }

    // update the message size
    _rtnl_req->nlmsg_len = new_sz;
    return rta;
}

void
RtNlReq::Close_rtattr(struct rtattr* attr) {
    attr->rta_len = (char *)(void *)GetNextAttrAddr() - (char *)(void *)attr;
}

/////////////////////////////////////////////////////////////////////////
//
//          Implementation of VirtualNetwork
//
/////////////////////////////////////////////////////////////////////////
//
VirtualNetwork::VirtualNetwork(const char* container_id) {
    _veth_created = false;
    _sock = -1;
    SetContainerId(container_id);
}

VirtualNetwork::VirtualNetwork() {
    _veth_created = false;
    _sock = -1;
}

VirtualNetwork::~VirtualNetwork() {
    _close_socket();
}

void
VirtualNetwork::SetContainerId(const char* id) {
    ASSERT0(!_veth_created);

    _container_id = id;

    _veth_name = "veth0_" + _container_id;
    _veth_peer_name = "veth1_" + _container_id;

    //TODO: fix this stupid naming solution
    if (_veth_name.size() > IF_NAMESIZE) {
        _veth_name = _veth_name.substr(0, IF_NAMESIZE);
    }
    if (_veth_peer_name.size() > IF_NAMESIZE) {
        _veth_peer_name = _veth_peer_name.substr(0, IF_NAMESIZE);
    }
}

void
VirtualNetwork::_close_socket() {
    if (_sock > 0) {
        close(_sock);
        _sock = -1;
    }
}

bool
VirtualNetwork::_create_veth() {
    ASSERT0(!_veth_created);

    _close_socket();
    _sock = ::socket(PF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_ROUTE);
    if (_sock < 0) {
        _err_msg = FmtErrno("socket", errno);
        _close_socket();
        return false;
    }

    RtNlReq req;

    struct nlmsghdr* msg_hdr = req.GetNlMsgHdr();
    char* msg_buf = (char *)(void *)msg_hdr;

    msg_hdr->nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_EXCL | NLM_F_ACK;
    msg_hdr->nlmsg_type = RTM_NEWLINK;

    /*
        struct ifinfomsg
        IFLA_IFNAME: "myvethname"
        IFLA_LINKINFO {
            IFLA_INFO_KIND: "veth"
            IFLA_INFO_DATA {
                VETH_INFO_PEER {
                    struct ifinfomsg
                    IFLA_IFNAME: "myvethnamepeer"
                }
            }
        }
     */
    // populate struct ifinfomsg
    // the if_info is right after the header
    auto if_info = (struct ifinfomsg*)(msg_buf + NLMSG_HDRLEN);
    if_info->ifi_family = PF_NETLINK;
    msg_hdr->nlmsg_len = NLMSG_LENGTH(sizeof(*if_info));

    // populate IFLA_IFNAME
    req.Add_rtattr(IFLA_IFNAME, _veth_name.c_str(), _veth_name.size() + 1);

    // start populate IFLA_LINKINFO
    auto IFLA_LINKINFO_attr = req.Add_rtattr(IFLA_LINKINFO);
    {
        // populate IFLA_LINKINFO.IFLA_INFO_KIND
        req.Add_rtattr(IFLA_INFO_KIND, "veth", 5);

        // start populating IFLA_INFO_DATA
        auto IFLA_INFO_DATA_attr = req.Add_rtattr(IFLA_INFO_DATA);

        // populate IFLA_LINKINFO.VETH_INFO_PEER
        auto VETH_INFO_PEER_attr = req.Add_rtattr(VETH_INFO_PEER);
        {
            // squeeze empty struct-ifinfomsg
            msg_hdr->nlmsg_len += sizeof(struct ifinfomsg);
            // populate IFLA_INFO_DATA.VETH_INFO_PEER.IFLA_IFNAME
            req.Add_rtattr(IFLA_IFNAME, _veth_peer_name.c_str(),
                           _veth_peer_name.size() + 1);
        }
        req.Close_rtattr(VETH_INFO_PEER_attr);
        req.Close_rtattr(IFLA_INFO_DATA_attr);
    }
    req.Close_rtattr(IFLA_LINKINFO_attr);

    auto res = _send_rtnl_req(msg_hdr);
    _close_socket();
     _veth_created = res;

    return res;
}

bool
VirtualNetwork::_wait_response() {
    int resp_buf_sz = 1024;
    shared_ptr<char> resp_buf(new char[resp_buf_sz], default_delete<char[]>());

    struct iovec iov;
    struct msghdr msg;

    bzero(&iov, sizeof(iov));
    bzero(&msg, sizeof(msg));

    iov.iov_base = resp_buf.get();
    iov.iov_len = resp_buf_sz;

    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    auto recv_sz = recvmsg(_sock, &msg, 0);
    if (recv_sz <= 0) {
        _err_msg = FmtErrno("recvmsg from netlink socket", errno);
        return false;
    }

    struct nlmsghdr *resp_hdr = (struct nlmsghdr *)(void*)resp_buf.get();
    // hint: nlmsg_len include the sizeof(hdr)
    if (resp_hdr->nlmsg_len > recv_sz ||
        resp_hdr->nlmsg_len < sizeof(resp_hdr)) {

        if (msg.msg_flags & MSG_TRUNC) {
            _err_msg = "recv buffer is not large enough";
        } else {
            _err_msg = "the response is corrupted";
        }
        return false;
    }

    if (resp_hdr->nlmsg_type == NLMSG_ERROR) {
        struct nlmsgerr *err = (struct nlmsgerr *) NLMSG_DATA(resp_hdr);
        if (err->error) {
            _err_msg = FmtErrno("recvmsg from netlink socket", -err->error);
            return false;
        }
    }

    return true;
}

bool
VirtualNetwork::_send_rtnl_req(struct nlmsghdr* req_hdr) {
    struct iovec iov;
    struct msghdr msg;

    bzero(&iov, sizeof(iov));
    bzero(&msg, sizeof(msg));

    iov.iov_base = req_hdr;
    iov.iov_len = req_hdr->nlmsg_len;

    msg.msg_iov = &iov,
    msg.msg_iovlen = 1;
    req_hdr->nlmsg_seq++;

    auto rc = sendmsg(_sock, &msg, 0);
    if (rc < 0) {
        _err_msg = FmtErrno("sendmsg", errno);
        return false;
    }

    return _wait_response();
}

bool
VirtualNetwork::_if_up(const std::string& if_name,
                       const std::string& ip_addr,
                       const std::string& mask) {
    if (if_name.size() > IF_NAMESIZE) {
        _err_msg = "invalid interface name: " + if_name;
        return false;
    }

    _close_socket();
    _sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (_sock < 0) {
        _err_msg = FmtErrno("socket", errno);
        return false;
    }

    struct ifreq req;
    bzero(&req, sizeof(req));
    strncpy(req.ifr_name, if_name.c_str(), if_name.size());

    auto addr = (struct sockaddr_in*)(void*)&req.ifr_addr;
    bzero(addr, sizeof(*addr));
    addr->sin_family = AF_INET;
    addr->sin_addr.s_addr = inet_addr(ip_addr.c_str());

    if (ioctl(_sock, SIOCSIFADDR, &req)) {
        _err_msg = FmtErrno("ioctl set ip addr", errno);
        return false;
    }

    bzero(addr, sizeof(*addr));
    addr->sin_family = AF_INET;
    addr->sin_addr.s_addr = inet_addr(mask.c_str());
    if (ioctl(_sock, SIOCSIFNETMASK, &req)) {
        _err_msg = FmtErrno("ioctl set mask", errno);
        return false;
    }

    req.ifr_flags |= IFF_UP | IFF_BROADCAST | IFF_RUNNING | IFF_MULTICAST;
    if (ioctl(_sock, SIOCSIFFLAGS, &req)) {
        _err_msg = FmtErrno("ioctl if-up", errno);
        return false;
    }

    _close_socket();
    return true;
}

int
VirtualNetwork::_get_netns_fd(int pid) {
    char buf[128];
    snprintf(buf, sizeof(buf), "/proc/%d/ns/net", pid);
    auto fd = open(buf, O_RDONLY);
    if (fd < 0) {
        _err_msg = FmtErrno("open", errno);
    }
    return fd;
}

bool
VirtualNetwork::_mv_veth_to_netns(const string& veth, int netns_fd) {
    _close_socket();
    _sock = socket(PF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_ROUTE);

    RtNlReq rtnl_req;
    auto msg = rtnl_req.GetNlMsgHdr();
    auto msg_buf = (char*)(void*)msg;

    msg->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
    msg->nlmsg_type = RTM_NEWLINK;

    auto if_info = (struct ifinfomsg*)(msg_buf + NLMSG_HDRLEN);
    if_info->ifi_family = PF_NETLINK;
    msg->nlmsg_len = NLMSG_LENGTH(sizeof(*if_info));

    int32_t netns_fd_i32 = netns_fd;
    rtnl_req.Add_rtattr(IFLA_NET_NS_FD, (char*)(void*)&netns_fd_i32, 4);
    rtnl_req.Add_rtattr(IFLA_IFNAME, veth.c_str(), veth.size() + 1);

    auto res = _send_rtnl_req(msg);
    _close_socket();

    return res;
}

// to do what this command tries to achieve:
//      ip route add default via 192.168.1.254
bool
VirtualNetwork::_set_default_gw(const string& via_ip) {
    RtNlReq rtnl_req;

    auto msg = rtnl_req.GetNlMsgHdr();
    auto msg_buf = (char*)(void*)msg;


    msg->nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg)),
    msg->nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_EXCL | NLM_F_ACK;
    msg->nlmsg_type = RTM_NEWROUTE;

    auto rt_msg = (struct rtmsg *)(msg_buf + NLMSG_HDRLEN);
    rt_msg->rtm_family = AF_INET;
    rt_msg->rtm_table = RT_TABLE_MAIN;
    rt_msg->rtm_scope = RT_SCOPE_NOWHERE;
    rt_msg->rtm_protocol = RTPROT_BOOT;
    rt_msg->rtm_scope = RT_SCOPE_UNIVERSE;
    rt_msg->rtm_type = RTN_UNICAST;

    in_addr_t addr = inet_addr(via_ip.c_str());
    if (addr == 0) {
        _err_msg = "failed to parse ip addr: " + via_ip;
        return false;
    }
    uint8_t addr_buf[] = {(uint8_t)(addr & 0xff),
                         (uint8_t)((addr >> 8) & 0xff),
                         (uint8_t)((addr >> 16) & 0xff),
                         (uint8_t)((addr >> 24) & 0xff)};

    rtnl_req.Add_rtattr(RTA_GATEWAY, (char*)addr_buf, sizeof(addr_buf));

    _close_socket();
    _sock = socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_ROUTE);
    if (_sock < 0) {
        _err_msg = FmtErrno("socket", errno);
        return false;
    }

    auto res = _send_rtnl_req(msg);
    _close_socket();

    return res;
}

bool
VirtualNetwork::Bringup(int child_pid) {
    auto rc = _bringup_helper(child_pid);
    if (!rc) {
        cerr << "failed to bring up virtual-network: " << _err_msg << endl;
    }
    return rc;
}

bool
VirtualNetwork::_bringup_helper(int child_pid) {
    if (!_create_veth()) {
        return false;
    }

    // TODO: dynamically allocate ip addr
    string addr = "172.19.0.2";
    string peer_addr = "172.19.0.3";
    string mask = "255.255.0.0";
    //string br_addr = "172.19.0.1";
    string br_addr = peer_addr;

    if (!_if_up(_veth_peer_name, peer_addr, mask)) {
        return false;
    }

    auto parent_netns_fd = _get_netns_fd(getpid());
    auto child_netns_fd = _get_netns_fd(child_pid);

    bool succ = true;
    if (child_netns_fd < 0 && parent_netns_fd < 0) {
        succ = false;
    }

    succ = succ &&
           _mv_veth_to_netns(_veth_name, child_netns_fd);

    if (succ) {
        if (setns(child_netns_fd, CLONE_NEWNET)) {
            _err_msg = FmtErrno("setns to child ns", errno);
            succ = false;
        }
    }

    succ = succ && _if_up(_veth_name, addr, mask)
                && _if_up("lo", "127.0.0.1", "255.255.0.0")
                && _set_default_gw(br_addr);

    if (succ) {
       if (setns(parent_netns_fd, CLONE_NEWNET)) {
            _err_msg = FmtErrno("setns restore back to original ns", errno);
            succ = false;
        }
    }

    _close_socket();
    return succ;
}
