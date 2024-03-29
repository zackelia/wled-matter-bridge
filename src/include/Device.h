/*
 *
 *    Copyright (c) 2023 Zack Elia
 *    Copyright (c) 2020 Project CHIP Authors
 *    All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#pragma once

#include <app/util/attribute-storage.h>

#include <chrono>
#include <functional>
#include <stdbool.h>
#include <stdint.h>
#include <thread>
#include <vector>

#include "clusters.h"

class Device : public IdentifyInterface
{
public:
    static const int kDeviceNameSize = 32;

    enum Changed_t
    {
        kChanged_Reachable = 1u << 0,
        kChanged_Location  = 1u << 1,
        kChanged_Name      = 1u << 2,
        kChanged_Last      = kChanged_Name,
    } Changed;

    Device(const char * szDeviceName, std::string szLocation);
    virtual ~Device() {}

    Device(const Device &)              = delete;
    Device & operator=(const Device &)  = delete;
    Device(Device && other)             = delete;
    Device & operator=(Device && other) = delete;

    bool IsReachable();
    virtual void SetReachable(bool aReachable);
    void SetName(const char * szDeviceName);
    void SetLocation(std::string szLocation);
    inline void SetEndpointId(chip::EndpointId id) { mEndpointId = id; };
    inline chip::EndpointId GetEndpointId() { return mEndpointId; };
    inline void SetParentEndpointId(chip::EndpointId id) { mParentEndpointId = id; };
    inline chip::EndpointId GetParentEndpointId() { return mParentEndpointId; };
    inline char * GetName() { return mName; };
    inline std::string GetLocation() { return mLocation; };
    inline std::string GetZone() { return mZone; };
    inline void SetZone(std::string zone) { mZone = zone; };

    virtual inline std::string GetManufacturer() = 0;
    virtual inline std::string GetSerialNumber() = 0;
    virtual inline std::string GetModel()        = 0;

private:
    virtual void HandleDeviceChange(Device * device, Device::Changed_t changeMask) = 0;

protected:
    bool mReachable             = false;
    char mName[kDeviceNameSize] = { 0 };
    std::string mLocation;
    chip::EndpointId mEndpointId       = 0;
    chip::EndpointId mParentEndpointId = 0;
    std::string mZone                  = "";
};

class DeviceOnOff : public Device
{
public:
    enum Changed_t
    {
        kChanged_OnOff = kChanged_Last << 1,
    } Changed;

    DeviceOnOff(const char * szDeviceName, std::string szLocation);

    virtual bool IsOn();
    virtual void SetOnOff(bool aOn);
    virtual void Toggle();

    using DeviceCallback_fn = std::function<void(DeviceOnOff *, DeviceOnOff::Changed_t)>;
    void SetChangeCallback(DeviceCallback_fn aChanged_CB);

private:
    void HandleDeviceChange(Device * device, Device::Changed_t changeMask);
    DeviceCallback_fn mChanged_CB;

    virtual void AnimateIdentify()
    {
        do
        {
            for (int i = 0; i < 4; i++)
            {
                Toggle();
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
            }
        } while (--remaining_time);
    }

protected:
    bool mOn = false;
};

class DeviceDimmable : public DeviceOnOff
{
public:
    enum Changed_t
    {
        kChanged_Level = kChanged_OnOff << 1,
    } Changed;

    DeviceDimmable(const char * szDeviceName, std::string szLocation);

    virtual uint8_t Level();
    virtual void SetLevel(uint8_t aLevel);

    using DeviceCallback_fn = std::function<void(DeviceDimmable *, DeviceDimmable::Changed_t)>;
    void SetChangeCallback(DeviceCallback_fn aChanged_CB);

private:
    void HandleDeviceChange(Device * device, Device::Changed_t changeMask);
    DeviceCallback_fn mChanged_CB;

protected:
    uint8_t mLevel = 0;
};

class DeviceColorTemperature : public DeviceDimmable, public ColorControlInterface
{
public:
    enum Changed_t
    {
        kChanged_Mireds = kChanged_Level << 1,
    } Changed;

    DeviceColorTemperature(const char * szDeviceName, std::string szLocation);

    virtual uint16_t Capabilities();
    virtual uint16_t Mireds();
    virtual void SetMireds(uint16_t mireds);

    using DeviceCallback_fn = std::function<void(DeviceColorTemperature *, DeviceColorTemperature::Changed_t)>;
    void SetChangeCallback(DeviceCallback_fn aChanged_CB);

private:
    void HandleDeviceChange(Device * device, Device::Changed_t changeMask);
    DeviceCallback_fn mChanged_CB;

protected:
    uint16_t mireds = 0;
};

class DeviceExtendedColor : public DeviceColorTemperature
{
public:
    enum Changed_t
    {
        kChanged_Hue        = kChanged_Mireds << 1,
        kChanged_Saturation = kChanged_Hue << 1,
    } Changed;

    DeviceExtendedColor(const char * szDeviceName, std::string szLocation);

    virtual uint16_t Capabilities();

    virtual uint8_t Hue();
    virtual void SetHue(uint8_t aHue);

    virtual uint8_t Saturation();
    virtual void SetSaturation(uint8_t aSaturation);

    using DeviceCallback_fn = std::function<void(DeviceExtendedColor *, DeviceExtendedColor::Changed_t)>;
    void SetChangeCallback(DeviceCallback_fn aChanged_CB);

private:
    void HandleDeviceChange(Device * device, Device::Changed_t changeMask);
    DeviceCallback_fn mChanged_CB;
};

class EndpointListInfo
{
public:
    EndpointListInfo(uint16_t endpointListId, std::string name, chip::app::Clusters::Actions::EndpointListTypeEnum type);
    EndpointListInfo(uint16_t endpointListId, std::string name, chip::app::Clusters::Actions::EndpointListTypeEnum type,
                     chip::EndpointId endpointId);
    void AddEndpointId(chip::EndpointId endpointId);
    inline uint16_t GetEndpointListId() { return mEndpointListId; };
    std::string GetName() { return mName; };
    inline chip::app::Clusters::Actions::EndpointListTypeEnum GetType() { return mType; };
    inline chip::EndpointId * GetEndpointListData() { return mEndpoints.data(); };
    inline size_t GetEndpointListSize() { return mEndpoints.size(); };

private:
    uint16_t mEndpointListId = static_cast<uint16_t>(0);
    std::string mName;
    chip::app::Clusters::Actions::EndpointListTypeEnum mType = static_cast<chip::app::Clusters::Actions::EndpointListTypeEnum>(0);
    std::vector<chip::EndpointId> mEndpoints;
};

class Room
{
public:
    Room(std::string name, uint16_t endpointListId, chip::app::Clusters::Actions::EndpointListTypeEnum type, bool isVisible);
    inline void setIsVisible(bool isVisible) { mIsVisible = isVisible; };
    inline bool getIsVisible() { return mIsVisible; };
    inline void setName(std::string name) { mName = name; };
    inline std::string getName() { return mName; };
    inline chip::app::Clusters::Actions::EndpointListTypeEnum getType() { return mType; };
    inline uint16_t getEndpointListId() { return mEndpointListId; };

private:
    bool mIsVisible;
    std::string mName;
    uint16_t mEndpointListId;
    chip::app::Clusters::Actions::EndpointListTypeEnum mType;
};

class Action
{
public:
    Action(uint16_t actionId, std::string name, chip::app::Clusters::Actions::ActionTypeEnum type, uint16_t endpointListId,
           uint16_t supportedCommands, chip::app::Clusters::Actions::ActionStateEnum status, bool isVisible);
    inline void setName(std::string name) { mName = name; };
    inline std::string getName() { return mName; };
    inline chip::app::Clusters::Actions::ActionTypeEnum getType() { return mType; };
    inline chip::app::Clusters::Actions::ActionStateEnum getStatus() { return mStatus; };
    inline uint16_t getActionId() { return mActionId; };
    inline uint16_t getEndpointListId() { return mEndpointListId; };
    inline uint16_t getSupportedCommands() { return mSupportedCommands; };
    inline void setIsVisible(bool isVisible) { mIsVisible = isVisible; };
    inline bool getIsVisible() { return mIsVisible; };

private:
    std::string mName;
    chip::app::Clusters::Actions::ActionTypeEnum mType;
    chip::app::Clusters::Actions::ActionStateEnum mStatus;
    uint16_t mActionId;
    uint16_t mEndpointListId;
    uint16_t mSupportedCommands;
    bool mIsVisible;
};
