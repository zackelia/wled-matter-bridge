# wled-matter-bridge
Matter bridge for WLED devices

## About

wled-matter-bridge exposes [WLED](https://github.com/Aircoookie/WLED) devices on your network as [Matter](https://github.com/project-chip/connectedhomeip) devices. It automatically discovers WLED devices using mDNS and controls them via WebSocket. Since the software runs remotely instead of on device, it is compatible with ESP32 and ESP8266.

## Usage

The recommended way to use wled-matter-bridge is through docker compose. A sample compose file is provided in this repo.

```
docker compose up -d
```

During operation, it may be needed to change what devices the bridge is connected to. For instance, there may be a WLED device not accessible by mDNS or there may be a WLED device that is no longer available.

There is a simple bridge.py script that can add/remove arbitrary IP addresses.

```
docker exec wled-matter-bridge /tools/bridge.py add 192.168.0.100
docker exec wled-matter-bridge /tools/bridge.py remove 192.168.0.101
```

## Compatability

### Matter

wled-matter-bridge *should* work in all Matter compliant environments including multiple at once.

I have personally tested when paired to Apple HomeKit, Home Assistant, and Matter's testing tool all at once.

### WLED

wled-matter-bridge *should* work with all WLED device types.

I have personally tested RGB, RGBW, and CCT devices. White and CCT+RGB devices should also work although I do not test them regularly.

## Limitations

### All ecosystems

* Cannot set the bridge default name without being registered in DCL
    * https://github.com/project-chip/connectedhomeip/issues/29489
    * https://webui.dcl.csa-iot.org/models

### HomeKit

* Turning a light off and then on resets the brightness to 100%
    * This is a visual glitch and only takes affect if you turn off/on again
    * Scenes don't seem to be affected
* User interface name changes do not take affect
* Identification does not work post-commissioning (via Eve app)

### Home Assistant

* Matter server can get backlogged and crash repeatedly when connected to multiple ecosystems
    * Seems to be addressed and working on Home Assistant 2024.1.3 and Python Matter Server 5.1.4
