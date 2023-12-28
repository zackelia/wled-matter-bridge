/*
 *
 *    Copyright (c) 2023 Zack Elia
 *    Copyright (c) 2021-2022 Project CHIP Authors
 *    Copyright (c) 2019 Google LLC.
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

#include "Device.h"

#include <cstdio>
#include <platform/CHIPDeviceLayer.h>

using namespace chip::app::Clusters::Actions;

Device::Device(const char * szDeviceName, std::string szLocation)
{
    chip::Platform::CopyString(mName, szDeviceName);
    mLocation   = szLocation;
    mReachable  = false;
    mEndpointId = 0;
}

bool Device::IsReachable()
{
    return mReachable;
}

void Device::SetReachable(bool aReachable)
{
    bool changed = (mReachable != aReachable);

    mReachable = aReachable;

    if (changed)
    {
        if (aReachable)
        {
            ChipLogProgress(DeviceLayer, "Device[%s]: ONLINE", mName);
        }
        else
        {
            ChipLogProgress(DeviceLayer, "Device[%s]: OFFLINE", mName);
        }
        HandleDeviceChange(this, kChanged_Reachable);
    }
}

void Device::SetName(const char * szName)
{
    bool changed = (strncmp(mName, szName, sizeof(mName)) != 0);

    chip::Platform::CopyString(mName, szName);

    if (changed)
    {
        ChipLogProgress(DeviceLayer, "Device[%s]: New Name=\"%s\"", mName, szName);
        HandleDeviceChange(this, kChanged_Name);
    }
}

void Device::SetLocation(std::string szLocation)
{
    bool changed = (mLocation.compare(szLocation) != 0);

    mLocation = szLocation;

    if (changed)
    {
        ChipLogProgress(DeviceLayer, "Device[%s]: Location=\"%s\"", mName, mLocation.c_str());
        HandleDeviceChange(this, kChanged_Location);
    }
}

DeviceOnOff::DeviceOnOff(const char * szDeviceName, std::string szLocation) : Device(szDeviceName, szLocation)
{
    mOn = false;
}

bool DeviceOnOff::IsOn()
{
    return mOn;
}

void DeviceOnOff::SetOnOff(bool aOn)
{
    bool changed;

    changed = aOn ^ mOn;
    mOn     = aOn;

    if ((changed) && (mChanged_CB))
    {
        ChipLogProgress(DeviceLayer, "Device[%s]: %s", mName, aOn ? "ON" : "OFF");
        mChanged_CB(this, kChanged_OnOff);
    }
}

void DeviceOnOff::Toggle()
{
    bool aOn = !IsOn();
    SetOnOff(aOn);
}

void DeviceOnOff::SetChangeCallback(DeviceCallback_fn aChanged_CB)
{
    mChanged_CB = aChanged_CB;
}

void DeviceOnOff::HandleDeviceChange(Device * device, Device::Changed_t changeMask)
{
    if (mChanged_CB)
    {
        mChanged_CB(this, (DeviceOnOff::Changed_t) changeMask);
    }
}

DeviceDimmable::DeviceDimmable(const char * szDeviceName, std::string szLocation) : DeviceOnOff(szDeviceName, szLocation)
{
    mLevel = 0;
}

uint8_t DeviceDimmable::Level()
{
    return mLevel;
}

void DeviceDimmable::SetLevel(uint8_t aLevel)
{
    bool changed;

    changed = aLevel ^ mLevel;
    mLevel  = aLevel;

    if ((changed) && (mChanged_CB))
    {
        ChipLogProgress(DeviceLayer, "Device[%s]: Level %d", mName, aLevel);
        mChanged_CB(this, kChanged_Level);
    }
}

void DeviceDimmable::SetChangeCallback(DeviceCallback_fn aChanged_CB)
{
    mChanged_CB = aChanged_CB;
}

void DeviceDimmable::HandleDeviceChange(Device * device, Device::Changed_t changeMask)
{
    if (mChanged_CB)
    {
        mChanged_CB(this, (DeviceDimmable::Changed_t) changeMask);
    }
}

DeviceColorTemperature::DeviceColorTemperature(const char * szDeviceName, std::string szLocation) :
    DeviceDimmable(szDeviceName, szLocation)
{
    mMireds = 0;
}

uint16_t DeviceColorTemperature::Capabilities()
{
    return static_cast<uint16_t>(chip::app::Clusters::ColorControl::ColorCapabilities::kColorTemperatureSupported);
}

uint16_t DeviceColorTemperature::Mireds()
{
    return mMireds;
}

void DeviceColorTemperature::SetMireds(uint16_t aMireds)
{
    bool changed;

    changed = aMireds ^ mMireds;
    mMireds = aMireds;

    if ((changed) && (mChanged_CB))
    {
        ChipLogProgress(DeviceLayer, "Device[%s]: Mireds %d", mName, aMireds);
        mChanged_CB(this, kChanged_Mireds);
    }
}

void DeviceColorTemperature::SetChangeCallback(DeviceCallback_fn aChanged_CB)
{
    mChanged_CB = aChanged_CB;
}

void DeviceColorTemperature::HandleDeviceChange(Device * device, Device::Changed_t changeMask)
{
    if (mChanged_CB)
    {
        mChanged_CB(this, (DeviceColorTemperature::Changed_t) changeMask);
    }
}

DeviceExtendedColor::DeviceExtendedColor(const char * szDeviceName, std::string szLocation) :
    DeviceColorTemperature(szDeviceName, szLocation)
{
    mHue        = 0;
    mSaturation = 0;
}

uint16_t DeviceExtendedColor::Capabilities()
{
    return static_cast<uint16_t>(chip::app::Clusters::ColorControl::ColorCapabilities::kColorTemperatureSupported) +
        static_cast<uint16_t>(chip::app::Clusters::ColorControl::ColorCapabilities::kHueSaturationSupported);
}

uint8_t DeviceExtendedColor::Hue()
{
    return mHue;
}

void DeviceExtendedColor::SetHue(uint8_t aHue)
{
    bool changed;

    changed = aHue ^ mHue;
    mHue    = aHue;

    if ((changed) && (mChanged_CB))
    {
        ChipLogProgress(DeviceLayer, "Device[%s]: Hue %d", mName, aHue);
        mChanged_CB(this, kChanged_Hue);
    }
}

uint8_t DeviceExtendedColor::Saturation()
{
    return mSaturation;
}

void DeviceExtendedColor::SetSaturation(uint8_t aSaturation)
{
    bool changed;

    changed     = aSaturation ^ mSaturation;
    mSaturation = aSaturation;

    if ((changed) && (mChanged_CB))
    {
        ChipLogProgress(DeviceLayer, "Device[%s]: Saturation %d", mName, aSaturation);
        mChanged_CB(this, kChanged_Saturation);
    }
}

void DeviceExtendedColor::SetChangeCallback(DeviceCallback_fn aChanged_CB)
{
    mChanged_CB = aChanged_CB;
}

void DeviceExtendedColor::HandleDeviceChange(Device * device, Device::Changed_t changeMask)
{
    if (mChanged_CB)
    {
        mChanged_CB(this, (DeviceExtendedColor::Changed_t) changeMask);
    }
}

EndpointListInfo::EndpointListInfo(uint16_t endpointListId, std::string name, EndpointListTypeEnum type)
{
    mEndpointListId = endpointListId;
    mName           = name;
    mType           = type;
}

EndpointListInfo::EndpointListInfo(uint16_t endpointListId, std::string name, EndpointListTypeEnum type,
                                   chip::EndpointId endpointId)
{
    mEndpointListId = endpointListId;
    mName           = name;
    mType           = type;
    mEndpoints.push_back(endpointId);
}

void EndpointListInfo::AddEndpointId(chip::EndpointId endpointId)
{
    mEndpoints.push_back(endpointId);
}

Room::Room(std::string name, uint16_t endpointListId, EndpointListTypeEnum type, bool isVisible)
{
    mName           = name;
    mEndpointListId = endpointListId;
    mType           = type;
    mIsVisible      = isVisible;
}

Action::Action(uint16_t actionId, std::string name, ActionTypeEnum type, uint16_t endpointListId, uint16_t supportedCommands,
               ActionStateEnum status, bool isVisible)
{
    mActionId          = actionId;
    mName              = name;
    mType              = type;
    mEndpointListId    = endpointListId;
    mSupportedCommands = supportedCommands;
    mStatus            = status;
    mIsVisible         = isVisible;
}
