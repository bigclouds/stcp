


#include <stcp/arp.h>
#include <stcp/config.h>
#include <stcp/dpdk.h>
#include <stcp/rte.h>
#include <stcp/stcp.h>

#include <pgen2.h>


namespace slank {



static void get_mymac(ether_addr* mymac, uint8_t port)
{
    for (ifaddr& ifa : core::instance().dpdk.devices[port].addrs) {
        if (ifa.family == STCP_AF_LINK) {
            for (int i=0; i<6; i++)
                mymac->addr_bytes[i] = ifa.raw.sa_data[i];
            return ;
        }
    }
    throw exception("not found my link address");
}


static void get_myip(stcp_in_addr* myip, uint8_t port)
{
    for (ifaddr& ifa : core::instance().dpdk.devices[port].addrs) {
        if (ifa.family == STCP_AF_INET) {
            stcp_sockaddr_in* sin = reinterpret_cast<stcp_sockaddr_in*>(&ifa.raw);
            *myip = sin->sin_addr;
            return ;
        }
    }
    throw exception("not found my inet address");
}


// TODO #15 this function will be included in sockaddr-class
static bool hw_sockaddr_is_same(const stcp_sockaddr* a, const stcp_sockaddr* b)
{
    for (int i=0; i<6; i++) {
        if (a->sa_data[i] != b->sa_data[i])
            return false;
    }
    return true;
}

// TODO #15 this function will be included in sockaddr-class
static bool p_sockaddr_is_same(const stcp_sockaddr* a, const stcp_sockaddr* b)
{
    const stcp_sockaddr_in* sina = reinterpret_cast<const stcp_sockaddr_in*>(a);
    const stcp_sockaddr_in* sinb = reinterpret_cast<const stcp_sockaddr_in*>(b);
    return ( sina->sin_addr == sinb->sin_addr );
}


void arp_module::stat() 
{
    m.stat();
    printf("\n");
    printf("\tFlags\n");
    printf("\tUse dynamic arp: %s\n", use_dynamic_arp ? "YES" : "NO");
    printf("\n");
    printf("\tARP-chace\n");
    printf("\t%-16s %-20s %s\n", "Address", "HWaddress", "Iface");
    for (stcp_arpreq& a : table) {
        printf("\t%-16s %-20s %d\n",
                p_sockaddr_to_str(&a.arp_pa),  // TODO #15 this function will be included in sockaddr-class
                hw_sockaddr_to_str(&a.arp_ha), a.arp_ifindex); // TODO #15 this function will be included in sockaddr-class
    }
}


static bool is_request_to_me(struct stcp_arphdr* ah, uint8_t port)
{
	for (ifaddr& ifa : core::instance().dpdk.devices[port].addrs) {
        stcp_sockaddr_in* sin = reinterpret_cast<stcp_sockaddr_in*>(&ifa.raw);
		if (ifa.family == STCP_AF_INET && sin->sin_addr==ah->pdst)
			return true;
	}
	return false;
}

void arp_module::proc() 
{
    while (m.rx_size() > 0) {
        mbuf* msg = m.rx_pop();
        struct stcp_arphdr* ah  = rte::pktmbuf_mtod<struct stcp_arphdr*>(msg);
        uint8_t port = msg->port;

        if (ah->operation == rte::bswap16(STCP_ARPOP_REPLY)) {

            /*
             * Proc ARP-Reply Packet 
             * to ARP-Table.
             */
                
            stcp_sockaddr     sa_pa;
            stcp_sockaddr     sa_ha;
            stcp_sockaddr_in *sin_pa = reinterpret_cast<stcp_sockaddr_in*>(&sa_pa);

            sin_pa->sin_addr = ah->psrc;
            for (int i=0; i<6; i++)
                sa_ha.sa_data[i] = ah->hwsrc.addr_bytes[i];

            stcp_arpreq req(&sa_pa, &sa_ha, port);
            ioctl_siocaarpent(&req);
            rte::pktmbuf_free(msg);

        } else if (ah->operation == rte::bswap16(STCP_ARPOP_REQUEST)) {
            if (is_request_to_me(ah, port)) {

                /* 
                 * Reply ARP-Reply Packet
                 */

                mbuf* msg = rte::pktmbuf_alloc(core::instance().dpdk.get_mempool());
                msg->data_len = sizeof(stcp_arphdr);
                msg->pkt_len  = sizeof(stcp_arphdr);
                msg->port = port;

                stcp_arphdr* rep_ah = rte::pktmbuf_mtod<stcp_arphdr*>(msg);
                rep_ah->hwtype = rte::bswap16(0x0001);
                rep_ah->ptype  = rte::bswap16(0x0800);
                rep_ah->hwlen  = 6;
                rep_ah->plen   = 4;
                rep_ah->operation = rte::bswap16(STCP_ARPOP_REPLY);
                get_mymac(&rep_ah->hwsrc, port);
                rep_ah->psrc  = ah->pdst;
                rep_ah->hwdst = ah->hwsrc;
                rep_ah->pdst  = ah->psrc;

                stcp_sockaddr sa; // TODO #15 fix this init
                sa.sa_fam = STCP_AF_ARP;
                core::instance().ether.tx_push(port, msg, &sa);
            }
        }
    }

    while (m.tx_size() > 0) {
        mbuf* msg = m.tx_pop();

        stcp_sockaddr sa; // TODO #15 fix this init
        sa.sa_fam = STCP_AF_ARP;
        core::instance().ether.tx_push(msg->port, msg, &sa);
    }
}


void arp_module::ioctl(uint64_t request, void* arg)
{
    switch (request) {
        case STCP_SIOCAARPENT:
        {
            stcp_arpreq* req = reinterpret_cast<stcp_arpreq*>(arg);
            ioctl_siocaarpent(req);
            break;
        }
        case STCP_SIOCDARPENT:
        {
            stcp_arpreq* req = reinterpret_cast<stcp_arpreq*>(arg);
            ioctl_siocdarpent(req);
            break;
        }
        case STCP_SIOCGARPENT:
        {
            std::vector<stcp_arpreq>** tbl = 
                reinterpret_cast<std::vector<stcp_arpreq>**>(arg);
            ioctl_siocgarpent(tbl);
            break;
        }
        case STCP_SIOCSDARP:
        {
            const bool* b = reinterpret_cast<const bool*>(arg);
            ioctl_siocsdarp(b);
            break;
        }
        case STCP_SIOCGDARP:
        {
            bool* b = reinterpret_cast<bool*>(arg);
            ioctl_siocgdarp(b);
            break;
        }
        default:
        {
            std::string errstr = "invalid arguments " + std::to_string(request);
            throw exception(errstr.c_str());
            break;
        }
    }
}

/*
 * IOCTL SocketIO Add ARP Entry
 *
 * Description
 * This function evaluate only these variable of stcp_arpreq.
 *  - req->arp_ha
 *  - req->arp_ha.sa_data[0-6]
 */
void arp_module::ioctl_siocaarpent(stcp_arpreq* req)
{
    for (stcp_arpreq& ent : table) {
        if (p_sockaddr_is_same(&ent.arp_pa, &req->arp_pa)) {
            if (hw_sockaddr_is_same(&ent.arp_ha, &req->arp_ha)) { // TODO #15
                return;
            } else {
                for (int i=0; i<6; i++)
                    ent.arp_ha.sa_data[i] = req->arp_ha.sa_data[i];
                return;
            }
        } else {
            continue;
        }
    }
    table.push_back(*req);
}


/* 
 * IOCTL SocketIO Delete ARP Entry
 *
 * Description
 * This functino evaluate only these variables of stcp_arpreq.
 *  - arp_ifindex
 */
void arp_module::ioctl_siocdarpent(stcp_arpreq* req)
{
    for (size_t i=0; i<table.size(); i++) {
        if (hw_sockaddr_is_same(&req->arp_pa, &table[i].arp_pa) &&
                table[i].arp_ifindex == req->arp_ifindex) {
            table.erase(table.begin() + i);
            return ;
        }
    }
    throw exception("arp record not found");
}


/*
 * IOCTL SocketIO Get ARP Entrys
 *
 * Description
 *  User can get raw-arptable's pointer. and it is no-const pointer.
 *  But now, I don't assume unsafe operation to arptable's pointer.
 *  So this function's argument will be const pointer to be safety.
 */
void arp_module::ioctl_siocgarpent(std::vector<stcp_arpreq>** tbl)
{
    *tbl = &table;
}


void arp_module::ioctl_siocsdarp(const bool* b)
{
    use_dynamic_arp = *b;
}
void arp_module::ioctl_siocgdarp(bool* b)
{
    *b = use_dynamic_arp;
}



// TODO to change as below.
// void arp_module::arp_resolv(
//         uint8_t port, const stcp_sockaddr *dst, ether_addr* dsten, bool checkcacheonly)
void arp_module::arp_resolv(
        uint8_t port, const stcp_sockaddr *dst, uint8_t* dsten, bool checkcacheonly)
{
    const stcp_sockaddr_in* dst_in = reinterpret_cast<const stcp_sockaddr_in*>(dst);
    for (stcp_arpreq& req : table) {
        stcp_sockaddr_in* req_in = reinterpret_cast<stcp_sockaddr_in*>(&req.arp_pa);
        if (req_in->sin_addr==dst_in->sin_addr && req.arp_ifindex==port) {
            for (int i=0; i<6; i++)
                dsten[i] = req.arp_ha.sa_data[i];
            return;
        }
    }
    if (checkcacheonly) {
        for (int i=0; i<6; i++)
            dsten[i] = 0x00;
        return;
    }

    if (use_dynamic_arp) {
        arp_request(port, &dst_in->sin_addr);
        for (int i=0; i<6; i++)
            dsten[i] = 0x00;
    } else {
        throw exception("no such record in arp-table");
    }
}




void arp_module::arp_request(uint8_t port, const stcp_in_addr* tip)
{
    mbuf* msg = rte::pktmbuf_alloc(core::instance().dpdk.get_mempool());
    msg->data_len = sizeof(stcp_arphdr);
    msg->pkt_len  = sizeof(stcp_arphdr);
    msg->port = port;

    stcp_arphdr* req_ah = rte::pktmbuf_mtod<stcp_arphdr*>(msg);
    req_ah->hwtype = rte::bswap16(0x0001);
	req_ah->ptype  = rte::bswap16(0x0800);
	req_ah->hwlen  = 6;
	req_ah->plen   = 4;
    req_ah->operation = rte::bswap16(STCP_ARPOP_REQUEST);
    get_mymac(&req_ah->hwsrc, port);
    get_myip(&req_ah->psrc, port);
    for (int i=0; i<6; i++) {
        req_ah->hwdst.addr_bytes[i] = 0x00;
    }
    req_ah->pdst = *tip;
    tx_push(msg);
}



} /* namespace */
