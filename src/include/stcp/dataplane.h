
#pragma once

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>

#include <vector>
#include <string>

#include <stcp/ifnet.h>
#include <stcp/config.h>
#include <stcp/tuning.h>
#include <stcp/mempool.h>
#include <stcp/arch/dpdk/rte.h>



namespace stcp {



class dataplane {
    friend class ifnet;
    mempool* mp;
public:

    dataplane() : mp(nullptr) {}
    ~dataplane() {}

    std::vector<ifnet> devices;
    void init(int argc, char** argv)
    {
        rte::eth_dev_init(argc, argv);
        mp = pool_create(
                "Dataplane Mem Pool",
                ST_DATAPLANE_MEMPOOL_NSEG * rte::eth_dev_count(),
                ST_DATAPLANE_MP_CACHESIZ,
                ST_MBUF_BUFSIZ,
                rte::socket_id());

        for (size_t port=0; port<rte::eth_dev_count(); port++) {
            ifnet dev(port);
            dev.init();
            devices.push_back(dev);
        }
    }
    void print_stat() const;
};

} /* namespace */
