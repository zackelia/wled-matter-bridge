#pragma once

#include <stdint.h>
#include <tuple>
#include <vector>

#include "wled.h"

namespace wled {
class KVS
{
public:
    KVS(uint8_t max_endpoints);
    ~KVS() = default;

    KVS(const KVS &)              = delete;
    KVS & operator=(const KVS &)  = delete;
    KVS(KVS && other)             = delete;
    KVS & operator=(KVS && other) = delete;

    std::vector<std::tuple<uint8_t, WLED *>> get_wleds();
    bool store_wled(uint8_t endpoint, WLED * wled);
    bool delete_wled(uint8_t endpoint);

private:
    uint8_t max_endpoints  = 0;
    uint32_t endpoint_bits = 0;
};
} // namespace wled
