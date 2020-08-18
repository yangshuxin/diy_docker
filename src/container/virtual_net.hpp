#ifndef __VIRTUAL_NET_HPP__
#define __VIRTUAL_NET_HPP__

#include <string>

struct nlmsghdr;

class VirtualNetwork {
public:
    VirtualNetwork();
    VirtualNetwork(const char* container_id);
    ~VirtualNetwork();

    void SetContainerId(const char* id);
    bool Bringup(int child_pid);
    bool TearDown();

    const std::string& GetVethName();
    const std::string& GetVethPeerName();

    const std::string& GetLastErr() const { return _err_msg; }

private:
    bool _bringup_helper(int child_pid);
    void _close_socket();
    bool _create_veth();
    bool _send_rtnl_req(struct nlmsghdr*);
    bool _wait_response();
    bool _if_up(const std::string& if_name, const std::string& ip_addr,
                const std::string& mask);

    // given given veth to given net namespace
    bool _mv_veth_to_netns(const std::string& veth, int netns_fd);
    bool _enter_net_ns(int pid);

    int _get_netns_fd(int pid);
    bool _set_default_gw(const std::string& via_ip);

private:
    std::string _container_id;

    std::string _veth_name;
    std::string _veth_peer_name;

    std::string _err_msg;

    int _sock;
    bool _veth_created;
};

#endif
