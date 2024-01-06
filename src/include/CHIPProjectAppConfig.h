#pragma once

#define CHIP_DEVICE_CONFIG_DYNAMIC_ENDPOINT_COUNT 32

#define FATCONFDIR "/tmp/chip"
#define SYSCONFDIR "/tmp/chip"
#define LOCALSTATEDIR "/tmp/chip"
#define CHIP_CONFIG_KVS_PATH "/tmp/chip/chip_kvs"

// Found in connectedhomeip/src/include/platform/CHIPDeviceConfig.h
#define CHIP_DEVICE_CONFIG_DEVICE_VENDOR_NAME "Zack Elia"
#define CHIP_DEVICE_CONFIG_TEST_SERIAL_NUMBER "serial"
#define CHIP_DEVICE_CONFIG_DEVICE_PRODUCT_NAME "WLED Matter Bridge"
#define CHIP_DEVICE_CONFIG_DEVICE_SOFTWARE_VERSION_STRING "0.1.0"
#define CHIP_DEVICE_CONFIG_DEFAULT_DEVICE_HARDWARE_VERSION_STRING "0.1.0"

// TODO: Is this actually used anywhere? Broken for bridges?
// #define CHIP_DEVICE_CONFIG_DEVICE_NAME "WLED Matter Bridge"

#include <platform/Linux/CHIPPlatformConfig.h>
