#include <atomic>
#include <map>
#include <shared_mutex>
#include <random>

#include "common/log.h"
#include "common/time_helper.h"

namespace network
{

class port_manager_t
{
private:
    struct port_data_t
    {
        port_data_t(uint16_t base_port, uint64_t timestamp):
            current_port(base_port),
            timestamp_ms(timestamp)
        {
          
        }

        std::atomic<uint16_t> current_port;
        uint64_t timestamp_ms;
    
    };
    typedef std::shared_ptr<port_data_t> port_data_auto_pointer_t;

  
    std::map<uint16_t, port_data_auto_pointer_t> port_res_port_map;
    std::__shared_mutex_pthread map_change_lock;
    std::mutex port_reset_mutex;
    std::default_random_engine generator;
    std::normal_distribution<double> distribution;

    std::atomic<uint16_t> current_port;

    uint16_t simulated_normal_allocs_per_second_ = 0;
    uint16_t reserved_port_range_per_alloc_;
    uint16_t min_port_;
public:


    port_manager_t(uint32_t reserved_port_range_per_unit = 50, uint32_t min_port = 5000) :
        generator(get_time_in_ms()),
        distribution(1.0,0.6),
        reserved_port_range_per_alloc_(reserved_port_range_per_unit),
        min_port_(min_port)
    {
        std::srand(get_time_in_ms());
        current_port = min_port + rand() % (( 65000 + 1 ) - min_port);
    };
    ~port_manager_t() {};
    
    void set_simulated_allocs_per_second(uint16_t alloc_per_sec)
    {
        simulated_normal_allocs_per_second_ = alloc_per_sec;
    }

    void set_reserved_port_range_per_alloc(uint16_t reserved_range)
    {
        reserved_port_range_per_alloc_ = reserved_range;
    }


    uint16_t assign_next_from_base(uint16_t base_port)
    {
        std::shared_lock<std::__shared_mutex_pthread> lock(map_change_lock);
        auto it = port_res_port_map.find(base_port);
        if(it != port_res_port_map.end())
        {
            uint16_t offset = 0;
            if(simulated_normal_allocs_per_second_ != 0)
            {
                uint64_t old_time = it->second->timestamp_ms;
                it->second->timestamp_ms = get_time_in_ms();
                double rnd_val = distribution(generator);
                rnd_val = rnd_val > 0 ? rnd_val : 0;

                double tdiff = ((it->second->timestamp_ms - old_time) / 1000.0);

                offset =  tdiff * simulated_normal_allocs_per_second_ * rnd_val + 0.5;
                //log_t::debug("Got offset %hu for tdiff %llu", offset + 1, it->second->timestamp_ms - old_time);
            }

            uint16_t port = it->second->current_port.fetch_add(offset + 1) + offset;
            if(port < min_port_)
            {
                port += min_port_;
            }
         
            //log_t::debug("Portman | Assigned from base %hu port %hu",base_port,port);
            return port;
        }
        else
        {
            return 0;
        }
    };

    uint16_t assign_next()
    {
        std::map<uint16_t, port_data_auto_pointer_t>::iterator it;
        uint16_t my_port;
        {
            std::shared_lock<std::__shared_mutex_pthread> lock(map_change_lock);
            my_port = current_port.fetch_add(reserved_port_range_per_alloc_);
            if(my_port < min_port_)
            {
                std::lock_guard<std::mutex> change_lock(port_reset_mutex);
                if(current_port < min_port_)
                {
                    current_port = min_port_;
                }
                my_port = current_port.fetch_add(reserved_port_range_per_alloc_);
            }
            it = port_res_port_map.find(my_port);
        }

        if(it == port_res_port_map.end())
        {
            std::unique_lock<std::__shared_mutex_pthread> lock(map_change_lock);
            port_res_port_map.insert(std::pair<uint16_t, port_data_auto_pointer_t>(my_port, new port_data_t(my_port + 1 , get_time_in_ms())));
        }
        else
        {
            it->second->current_port.store(my_port + 1);
        }

       // log_t::debug("Portman | Assigned new base port %hu",my_port);
        return my_port;
    };
};

}
