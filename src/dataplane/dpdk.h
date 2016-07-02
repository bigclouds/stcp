
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>
#include <errno.h>

#include <string>
#include <iostream>
#include <sstream>
#include <exception>

#include <rte_config.h>
#include <rte_version.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <rte_hexdump.h>




namespace rte {


class exception : public std::exception {
    private:
        std::string str;
    public:
        explicit exception(const char* s="") noexcept {
            int e = errno;
            str = s; 
            if (e != 0)
                str += strerror(e);
        } 

        template<class T>
        exception& operator<<(const T& t) noexcept {
            std::ostringstream os;
            os << " " << t ;
            str += os.str();
            return *this;
        }
        const char* what() const noexcept {
            return str.c_str(); 
        }
};





void eth_dev_init(int argc, char** argv);
void eth_dev_configure(uint8_t port_id, uint16_t nb_rx_queue,
        uint16_t nb_tx_queue, const struct rte_eth_conf* eth_conf);
void eth_rx_queue_setup(uint8_t port_id, 
        uint16_t rx_queue_id, 
        uint16_t nb_rx_desc, uint32_t socket_id, 
        const struct rte_eth_rxconf* rx_conf,
        struct rte_mempool* mbuf_pool);
void eth_tx_queue_setup(uint8_t port_id, 
        uint16_t tx_queue_id, 
        uint16_t nb_tx_desc, uint32_t socket_id, 
        const struct rte_eth_txconf* tx_conf);
void eth_dev_start(uint8_t);
void eth_promiscuous_enable(uint8_t port_id);
void eth_promiscuous_disable(uint8_t port_id);
int  eth_promiscuous_get(uint8_t port_id);
struct rte_mempool* pktmbuf_pool_create(const char* name, 
        uint32_t n, uint32_t cache_size, uint16_t priv_size, 
        uint16_t data_room_size, int socket_id);
void eth_macaddr_get(uint8_t port, struct ether_addr* addr);
size_t eth_dev_count();
size_t lcore_count();
uint16_t eth_rx_burst(uint8_t port_id, uint16_t queue_id, 
        struct rte_mbuf** rx_pkts, const uint16_t nb_pkts);
uint16_t eth_tx_burst(uint8_t port_id, uint16_t queue_id, 
        struct rte_mbuf** tx_pkts, uint16_t nb_pkts);
void pktmbuf_free(struct rte_mbuf* m);
uint32_t socket_id();
void pktmbuf_dump(FILE* f,const struct rte_mbuf* m, unsigned dump_len);	
int eth_dev_socket_id(uint8_t port_id);





} /* namespace rte */








#include <rte_ethdev.h>
class dpdk_conf {
public:
    struct rte_eth_conf port_conf;

    dpdk_conf()
    {
        init_port_conf();
    }
    ~dpdk_conf() {}

    void init_port_conf()
    {
        struct rte_eth_rxmode rxmode;
        memset(&rxmode, 0, sizeof rxmode);
        rxmode.max_rx_pkt_len = ETHER_MAX_LEN;

        memset(&port_conf, 0, sizeof(port_conf));
        port_conf.rxmode = rxmode;
    }
};

