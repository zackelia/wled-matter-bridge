#include <limits.h>

#include <lib/support/logging/CHIPLogging.h>
#include <platform/KeyValueStoreManager.h>

#include "kvs.hpp"

using namespace chip;
using namespace chip::DeviceLayer;
using namespace chip::DeviceLayer::PersistedStorage;
using namespace wled;

static const std::string WLED_PREFIX   = "WLED_";
static const std::string WLED_BITS_KEY = WLED_PREFIX + "BITS";

static constexpr bool is_bit_set(uint32_t num, uint8_t bit)
{
    return ((num & (1 << bit)) != 0) ? 1 : 0;
}

static constexpr uint32_t set_bit(uint32_t num, uint8_t bit)
{
    return num | (1 << bit);
}

static constexpr uint32_t clear_bit(uint32_t num, uint8_t bit)
{
    return (num & (~(1 << bit)));
}

static void handle_chip_error(ChipError err)
{
    char error_str[255];
    chip::FormatCHIPError(error_str, sizeof(error_str), err);
    ChipLogError(DeviceLayer, "%s", error_str);
}

KVS::KVS(uint8_t aMax_endpoints) : max_endpoints(aMax_endpoints)
{
    ChipError err;

    err = KeyValueStoreMgr().Get(WLED_BITS_KEY.c_str(), &endpoint_bits);
    if (err != CHIP_NO_ERROR)
    {
        handle_chip_error(err);
        err = KeyValueStoreMgr().Put(WLED_BITS_KEY.c_str(), 0);
        if (err != CHIP_NO_ERROR)
        {
            handle_chip_error(err);
            chipAbort();
        }
    }
}

struct wled_instance
{
    char ip[HOST_NAME_MAX + 1];
    char location[40];
    uint8_t endpoint;
};

std::vector<std::tuple<uint8_t, WLED *>> KVS::get_wleds()
{
    std::vector<std::tuple<uint8_t, WLED *>> wleds;
    ChipError err;

    for (uint8_t i = 0; i < max_endpoints; i++)
    {
        if (is_bit_set(endpoint_bits, i))
        {
            auto key           = WLED_PREFIX + std::to_string(i);
            wled_instance inst = {};

            err = KeyValueStoreMgr().Get(key.c_str(), &inst);
            if (err != CHIP_NO_ERROR)
            {
                handle_chip_error(err);
                ChipLogError(DeviceLayer, "Could not get WLED device at endpoint %d!", i);
                continue;
            }

            wleds.push_back({ i, new WLED(inst.ip, inst.location) });
        }
    }

    return wleds;
}

bool KVS::store_wled(uint8_t endpoint, WLED * wled)
{
    auto key = WLED_PREFIX + std::to_string(endpoint);

    wled_instance inst;
    strncpy(inst.ip, wled->GetIP().c_str(), sizeof(inst.ip));
    strncpy(inst.location, wled->GetLocation().c_str(), sizeof(inst.location));

    ChipError err;

    err = KeyValueStoreMgr().Put(key.c_str(), inst);
    if (err != CHIP_NO_ERROR)
    {
        handle_chip_error(err);
        ChipLogError(DeviceLayer, "Could store WLED device (%s) at endpoint %d!", inst.ip, endpoint);
        return false;
    }

    if (!is_bit_set(endpoint_bits, endpoint))
    {
        endpoint_bits = set_bit(endpoint_bits, endpoint);
        err           = KeyValueStoreMgr().Put(WLED_BITS_KEY.c_str(), endpoint_bits);
        if (err != CHIP_NO_ERROR)
        {
            handle_chip_error(err);
            ChipLogError(DeviceLayer, "Could not update WLED KVS!");
            return false;
        }
    }

    return true;
}

bool KVS::delete_wled(uint8_t endpoint)
{
    auto key = WLED_PREFIX + std::to_string(endpoint);

    ChipError err;

    err = KeyValueStoreMgr().Delete(key.c_str());
    if (err != CHIP_NO_ERROR)
    {
        handle_chip_error(err);
        ChipLogError(DeviceLayer, "Could not delete WLED at endpoint %d!", endpoint);
        return false;
    }

    endpoint_bits = clear_bit(endpoint_bits, endpoint);
    err           = KeyValueStoreMgr().Put(WLED_BITS_KEY.c_str(), endpoint_bits);
    if (err != CHIP_NO_ERROR)
    {
        handle_chip_error(err);
        ChipLogError(DeviceLayer, "Could not update WLED KVS!");
        return false;
    }

    return true;
}
