

#pragma once

#include <stdlib.h>

#include <stcp/dpdk.h>
#include <stcp/config.h>
#include <stcp/mbuf.h>
    
#include <stcp/protocol.h>
#include <stcp/ethernet.h>
#include <stcp/arp.h>
#include <stcp/ip.h>
#include <stcp/icmp.h>


namespace slank {
    

using stat = log<class status_infos>;
using rxcap = log<class rx_packet_log>;
using txcap = log<class tx_packet_log>;

class core {
public:
    icmp_module icmp;
    ip_module  ip;
    arp_module arp;
    ether_module ether;
    dpdk_core dpdk;

private:
    core()
    {
        stat::instance().open_new("stcp.stat.log");
        rxcap::instance().open_new("stcp.rx.log");
        txcap::instance().open_new("stcp.tx.log");
    }
    ~core() {}
    core(const core&) = delete;
    core& operator=(const core&) = delete;

public:
    static core& instance();
    void init(int argc, char** argv);
    void ifs_proc();
    void run(bool endless=true);
    void stat_all();
};




} /* namespace */
