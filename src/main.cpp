/*
 *
 *    Copyright (c) 2023 Zack Elia
 *    Copyright (c) 2021 Project CHIP Authors
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

#include <AppMain.h>
#include <platform/CHIPDeviceLayer.h>
#include <platform/PlatformManager.h>

#include <app-common/zap-generated/callback.h>
#include <app-common/zap-generated/ids/Attributes.h>
#include <app-common/zap-generated/ids/Clusters.h>
#include <app/ConcreteAttributePath.h>
#include <app/EventLogging.h>
#include <app/reporting/reporting.h>
#include <app/util/af-types.h>
#include <app/util/af.h>
#include <app/util/attribute-storage.h>
#include <app/util/util.h>
#include <credentials/DeviceAttestationCredsProvider.h>
#include <credentials/examples/DeviceAttestationCredsExample.h>
#include <lib/core/CHIPError.h>
#include <lib/support/CHIPMem.h>
#include <lib/support/ZclString.h>
#include <platform/CommissionableDataProvider.h>
#include <setup_payload/QRCodeSetupPayloadGenerator.h>
#include <setup_payload/SetupPayload.h>

#include <pthread.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "CommissionableInit.h"
#include "Device.h"
#include "main.h"
#include <app/server/Server.h>

#include <array>
#include <cassert>
#include <cstring>
#include <iostream>
#include <math.h>
#include <sys/select.h>
#include <vector>

#include "kvs.hpp"
#include "mdns.hpp"
#include "wled.h"

using namespace chip;
using namespace chip::app;
using namespace chip::Credentials;
using namespace chip::Inet;
using namespace chip::Transport;
using namespace chip::DeviceLayer;
using namespace chip::app::Clusters;

namespace {

const int kNodeLabelSize = 32;
// Current ZCL implementation of Struct uses a max-size array of 254 bytes
const int kDescriptorAttributeArraySize = 254;

const int MDNS_TIMEOUT = 300;

constexpr const char * WLED_FIFO_IN  = LOCALSTATEDIR "/wled-fifo-in";
constexpr const char * WLED_FIFO_OUT = LOCALSTATEDIR "/wled-fifo-out";

EndpointId gFirstDynamicEndpointId;
Device * gDevices[CHIP_DEVICE_CONFIG_DYNAMIC_ENDPOINT_COUNT];
std::vector<Room *> gRooms;
std::vector<Action *> gActions;

// ENDPOINT DEFINITIONS:
// =================================================================================
//
// Endpoint definitions will be reused across multiple endpoints for every instance of the
// endpoint type.
// There will be no intrinsic storage for the endpoint attributes declared here.
// Instead, all attributes will be treated as EXTERNAL, and therefore all reads
// or writes to the attributes must be handled within the emberAfExternalAttributeWriteCallback
// and emberAfExternalAttributeReadCallback functions declared herein. This fits
// the typical model of a bridge, since a bridge typically maintains its own
// state database representing the devices connected to it.

// Device types for dynamic endpoints: TODO Need a generated file from ZAP to define these!
// (taken from matter-devices.xml)
#define DEVICE_TYPE_BRIDGED_NODE 0x0013
// (taken from lo-devices.xml)
#define DEVICE_TYPE_LO_ON_OFF_LIGHT 0x0100
#define DEVICE_TYPE_LO_DIMMABLE_LIGHT 0x0101
#define DEVICE_TYPE_LO_COLOR_TEMPERATURE_LIGHT 0x010C
#define DEVICE_TYPE_LO_EXTENDED_COLOR_LIGHT 0x010D

// Device Version for dynamic endpoints:
#define DEVICE_VERSION_DEFAULT 1

// ---------------------------------------------------------------------------
//
// LIGHT ENDPOINT: contains the following clusters:
//   - Identify
//   - On/Off
//   - Level Control
//   - Color Conttrol
//   - Descriptor
//   - Bridged Device Basic Information

// Declare Identify cluster attributes
DECLARE_DYNAMIC_ATTRIBUTE_LIST_BEGIN(identifyAttrs)
DECLARE_DYNAMIC_ATTRIBUTE(Identify::Attributes::IdentifyTime::Id, INT16U, 2, ZAP_ATTRIBUTE_MASK(WRITABLE)),
    DECLARE_DYNAMIC_ATTRIBUTE(Identify::Attributes::IdentifyType::Id, BITMAP8, 1, 0), DECLARE_DYNAMIC_ATTRIBUTE_LIST_END();

// Declare On/Off cluster attributes
DECLARE_DYNAMIC_ATTRIBUTE_LIST_BEGIN(onOffAttrs)
DECLARE_DYNAMIC_ATTRIBUTE(OnOff::Attributes::OnOff::Id, BOOLEAN, 1, 0), /* on/off */
    DECLARE_DYNAMIC_ATTRIBUTE_LIST_END();

// Declare Light Control cluster attributes
DECLARE_DYNAMIC_ATTRIBUTE_LIST_BEGIN(levelControlAttrs)
DECLARE_DYNAMIC_ATTRIBUTE(LevelControl::Attributes::CurrentLevel::Id, INT8U, 1, ZAP_ATTRIBUTE_MASK(NULLABLE)),
    DECLARE_DYNAMIC_ATTRIBUTE(LevelControl::Attributes::RemainingTime::Id, INT16U, 2, 0),
    DECLARE_DYNAMIC_ATTRIBUTE(LevelControl::Attributes::MinLevel::Id, INT8U, 1, 0),
    // TODO: Spec says this is mandatory but it doesn't seem to be?
    // DECLARE_DYNAMIC_ATTRIBUTE(LevelControl::Attributes::OnLevel::Id, INT8U, 1,
    //                           ZAP_ATTRIBUTE_MASK(WRITABLE) | ZAP_ATTRIBUTE_MASK(NULLABLE)),
    DECLARE_DYNAMIC_ATTRIBUTE(LevelControl::Attributes::Options::Id, BITMAP8, 1, ZAP_ATTRIBUTE_MASK(WRITABLE)),
    DECLARE_DYNAMIC_ATTRIBUTE(LevelControl::Attributes::StartUpCurrentLevel::Id, INT8U, 1, ZAP_ATTRIBUTE_MASK(WRITABLE)),
    DECLARE_DYNAMIC_ATTRIBUTE(LevelControl::Attributes::ClusterRevision::Id, INT16U, 2, 0),
    DECLARE_DYNAMIC_ATTRIBUTE(LevelControl::Attributes::FeatureMap::Id, BITMAP32, 4, 0), DECLARE_DYNAMIC_ATTRIBUTE_LIST_END();

// Declare Color Control cluster attributes
DECLARE_DYNAMIC_ATTRIBUTE_LIST_BEGIN(colorControlAttrs)
DECLARE_DYNAMIC_ATTRIBUTE(ColorControl::Attributes::CurrentHue::Id, INT8U, 1, 0),
    DECLARE_DYNAMIC_ATTRIBUTE(ColorControl::Attributes::CurrentSaturation::Id, INT8U, 1, 0),
    DECLARE_DYNAMIC_ATTRIBUTE(ColorControl::Attributes::ColorTemperatureMireds::Id, INT16U, 2, 0),
    DECLARE_DYNAMIC_ATTRIBUTE(ColorControl::Attributes::ColorMode::Id, ENUM8, 1, 0),
    DECLARE_DYNAMIC_ATTRIBUTE(ColorControl::Attributes::Options::Id, BITMAP8, 1, ZAP_ATTRIBUTE_MASK(WRITABLE)),
    DECLARE_DYNAMIC_ATTRIBUTE(ColorControl::Attributes::EnhancedColorMode::Id, ENUM8, 1, 0),
    DECLARE_DYNAMIC_ATTRIBUTE(ColorControl::Attributes::ColorCapabilities::Id, BITMAP16, 2, 0),
    DECLARE_DYNAMIC_ATTRIBUTE(ColorControl::Attributes::ColorTempPhysicalMinMireds::Id, INT16U, 2, 0),
    DECLARE_DYNAMIC_ATTRIBUTE(ColorControl::Attributes::ColorTempPhysicalMaxMireds::Id, INT16U, 2, 0),
    DECLARE_DYNAMIC_ATTRIBUTE(ColorControl::Attributes::StartUpColorTemperatureMireds::Id, INT16U, 2, ZAP_ATTRIBUTE_MASK(WRITABLE)),
    DECLARE_DYNAMIC_ATTRIBUTE(ColorControl::Attributes::FeatureMap::Id, BITMAP32, 4, 0), DECLARE_DYNAMIC_ATTRIBUTE_LIST_END();

// Declare Descriptor cluster attributes
DECLARE_DYNAMIC_ATTRIBUTE_LIST_BEGIN(descriptorAttrs)
DECLARE_DYNAMIC_ATTRIBUTE(Descriptor::Attributes::DeviceTypeList::Id, ARRAY, kDescriptorAttributeArraySize, 0), /* device list */
    DECLARE_DYNAMIC_ATTRIBUTE(Descriptor::Attributes::ServerList::Id, ARRAY, kDescriptorAttributeArraySize, 0), /* server list */
    DECLARE_DYNAMIC_ATTRIBUTE(Descriptor::Attributes::ClientList::Id, ARRAY, kDescriptorAttributeArraySize, 0), /* client list */
    DECLARE_DYNAMIC_ATTRIBUTE(Descriptor::Attributes::PartsList::Id, ARRAY, kDescriptorAttributeArraySize, 0),  /* parts list */
    DECLARE_DYNAMIC_ATTRIBUTE_LIST_END();

// Declare Bridged Device Basic Information cluster attributes
DECLARE_DYNAMIC_ATTRIBUTE_LIST_BEGIN(bridgedDeviceBasicAttrs)
DECLARE_DYNAMIC_ATTRIBUTE(BridgedDeviceBasicInformation::Attributes::VendorName::Id, CHAR_STRING, kNodeLabelSize, 0),
    DECLARE_DYNAMIC_ATTRIBUTE(BridgedDeviceBasicInformation::Attributes::ProductName::Id, CHAR_STRING, kNodeLabelSize, 0),
    DECLARE_DYNAMIC_ATTRIBUTE(BridgedDeviceBasicInformation::Attributes::SerialNumber::Id, CHAR_STRING, kNodeLabelSize, 0),
    DECLARE_DYNAMIC_ATTRIBUTE(BridgedDeviceBasicInformation::Attributes::NodeLabel::Id, CHAR_STRING, kNodeLabelSize,
                              0),                                                                         /* NodeLabel */
    DECLARE_DYNAMIC_ATTRIBUTE(BridgedDeviceBasicInformation::Attributes::Reachable::Id, BOOLEAN, 1, 0),   /* Reachable */
    DECLARE_DYNAMIC_ATTRIBUTE(BridgedDeviceBasicInformation::Attributes::FeatureMap::Id, BITMAP32, 4, 0), /* feature map */
    DECLARE_DYNAMIC_ATTRIBUTE_LIST_END();

// Declare Cluster List for Bridged Light endpoint
// TODO: It's not clear whether it would be better to get the command lists from
// the ZAP config on our last fixed endpoint instead.
constexpr CommandId identifyIncomingCommands[] = {
    app::Clusters::Identify::Commands::Identify::Id,
};

constexpr CommandId onOffIncomingCommands[] = {
    app::Clusters::OnOff::Commands::Off::Id,
    app::Clusters::OnOff::Commands::On::Id,
    app::Clusters::OnOff::Commands::Toggle::Id,
    app::Clusters::OnOff::Commands::OffWithEffect::Id,
    app::Clusters::OnOff::Commands::OnWithRecallGlobalScene::Id,
    app::Clusters::OnOff::Commands::OnWithTimedOff::Id,
    kInvalidCommandId,
};

constexpr CommandId levelControlIncomingCommands[] = {
    app::Clusters::LevelControl::Commands::MoveToLevel::Id,
    app::Clusters::LevelControl::Commands::Move::Id,
    app::Clusters::LevelControl::Commands::Step::Id,
    app::Clusters::LevelControl::Commands::Stop::Id,
    app::Clusters::LevelControl::Commands::MoveToLevelWithOnOff::Id,
    app::Clusters::LevelControl::Commands::MoveWithOnOff::Id,
    app::Clusters::LevelControl::Commands::StepWithOnOff::Id,
    app::Clusters::LevelControl::Commands::StopWithOnOff::Id,
    kInvalidCommandId,
};

constexpr CommandId colorControlIncomingCommands[] = {
    app::Clusters::ColorControl::Commands::MoveToHue::Id,
    app::Clusters::ColorControl::Commands::MoveHue::Id,
    app::Clusters::ColorControl::Commands::StepHue::Id,
    app::Clusters::ColorControl::Commands::MoveToSaturation::Id,
    app::Clusters::ColorControl::Commands::MoveSaturation::Id,
    app::Clusters::ColorControl::Commands::StepSaturation::Id,
    app::Clusters::ColorControl::Commands::MoveToHueAndSaturation::Id,
    app::Clusters::ColorControl::Commands::MoveToColorTemperature::Id,
    app::Clusters::ColorControl::Commands::StopMoveStep::Id,
    app::Clusters::ColorControl::Commands::MoveColorTemperature::Id,
    app::Clusters::ColorControl::Commands::StepColorTemperature::Id,
    kInvalidCommandId,
};

DECLARE_DYNAMIC_CLUSTER_LIST_BEGIN(bridgedLightClusters)
DECLARE_DYNAMIC_CLUSTER(Identify::Id, identifyAttrs, identifyIncomingCommands, nullptr),
    DECLARE_DYNAMIC_CLUSTER(OnOff::Id, onOffAttrs, onOffIncomingCommands, nullptr),
    DECLARE_DYNAMIC_CLUSTER(LevelControl::Id, levelControlAttrs, levelControlIncomingCommands, nullptr),
    DECLARE_DYNAMIC_CLUSTER(ColorControl::Id, colorControlAttrs, colorControlIncomingCommands, nullptr),
    DECLARE_DYNAMIC_CLUSTER(Descriptor::Id, descriptorAttrs, nullptr, nullptr),
    DECLARE_DYNAMIC_CLUSTER(BridgedDeviceBasicInformation::Id, bridgedDeviceBasicAttrs, nullptr,
                            nullptr) DECLARE_DYNAMIC_CLUSTER_LIST_END;

// Declare Bridged Light endpoint
DECLARE_DYNAMIC_ENDPOINT(bridgedLightEndpoint, bridgedLightClusters);

wled::KVS * kvs;
wled::MDNS * mdns;
std::vector<std::string> deny_list;
std::vector<WLED *> gLights;
std::array<std::array<DataVersion, ArraySize(bridgedLightClusters)>, CHIP_DEVICE_CONFIG_DYNAMIC_ENDPOINT_COUNT> gDataVersions;

Room room1("Room 1", 0xE001, Actions::EndpointListTypeEnum::kRoom, true);

Action action1(0x1001, "Room 1 On", Actions::ActionTypeEnum::kAutomation, 0xE001, 0x1, Actions::ActionStateEnum::kInactive, true);

} // namespace

// REVISION DEFINITIONS:
// =================================================================================

#define ZCL_BRIDGED_DEVICE_BASIC_INFORMATION_CLUSTER_REVISION (2u)
#define ZCL_BRIDGED_DEVICE_BASIC_INFORMATION_FEATURE_MAP (0u)
#define ZCL_IDENTIFY_CLUSTER_REVISION (4u)
#define ZCL_ON_OFF_CLUSTER_REVISION (4u)
#define ZCL_LEVEL_CONTROL_CLUSTER_REVISION (5u)
#define ZCL_LEVEL_CONTROL_FEATURE_MAP (3u)
#define ZCL_LEVEL_CONTROL_OPTIONS (1u)
#define ZCL_COLOR_CONTROL_CLUSTER_REVISION (6u)
#define ZCL_COLOR_CONTROL_OPTIONS (1u)

// ---------------------------------------------------------------------------

int AddDeviceEndpoint(uint8_t index, Device * dev, EmberAfEndpointType * ep, const Span<const EmberAfDeviceType> & deviceTypeList,
                      const Span<DataVersion> & dataVersionStorage, chip::EndpointId parentEndpointId = chip::kInvalidEndpointId)
{
    if (nullptr == gDevices[index])
    {
        gDevices[index] = dev;
        EmberAfStatus ret;
        while (true)
        {
            // Todo: Update this to schedule the work rather than use this lock
            DeviceLayer::StackLock lock;
            dev->SetEndpointId(index + gFirstDynamicEndpointId);
            dev->SetParentEndpointId(parentEndpointId);
            ret = emberAfSetDynamicEndpoint(index, index + gFirstDynamicEndpointId, ep, dataVersionStorage, deviceTypeList,
                                            parentEndpointId);
            if (ret == EMBER_ZCL_STATUS_SUCCESS)
            {
                ChipLogProgress(DeviceLayer, "Added device %s to dynamic endpoint %d (index=%d)", dev->GetName(),
                                index + gFirstDynamicEndpointId, index);
                // TODO: This won't work for every device! Does that matter?
                // Seems to be tracked here: https://github.com/orgs/project-chip/projects/85
                emberAfLevelControlClusterServerInitCallback(dev->GetEndpointId());
                emberAfColorControlClusterServerInitCallback(dev->GetEndpointId());
                return index;
            }
            if (ret != EMBER_ZCL_STATUS_DUPLICATE_EXISTS)
            {
                ChipLogError(DeviceLayer, "Got unhandled error: %d", ret);
                abort();
                return -1;
            }
        }
    }

    ChipLogError(DeviceLayer, "Could not add device at index %d, it appears already used!", index);
    return -1;
}

// TODO: This needs to be modified later if we intend to call it
int RemoveDeviceEndpoint(Device * dev)
{
    uint8_t index = 0;
    while (index < CHIP_DEVICE_CONFIG_DYNAMIC_ENDPOINT_COUNT)
    {
        if (gDevices[index] == dev)
        {
            // Todo: Update this to schedule the work rather than use this lock
            DeviceLayer::StackLock lock;
            EndpointId ep   = emberAfClearDynamicEndpoint(index);
            gDevices[index] = nullptr;
            ChipLogProgress(DeviceLayer, "Removed device %s from dynamic endpoint %d (index=%d)", dev->GetName(), ep, index);
            // Silence complaints about unused ep when progress logging
            // disabled.
            UNUSED_VAR(ep);
            return index;
        }
        index++;
    }
    return -1;
}

std::vector<EndpointListInfo> GetEndpointListInfo(chip::EndpointId parentId)
{
    std::vector<EndpointListInfo> infoList;

    for (auto room : gRooms)
    {
        if (room->getIsVisible())
        {
            EndpointListInfo info(room->getEndpointListId(), room->getName(), room->getType());
            int index = 0;
            while (index < CHIP_DEVICE_CONFIG_DYNAMIC_ENDPOINT_COUNT)
            {
                if ((gDevices[index] != nullptr) && (gDevices[index]->GetParentEndpointId() == parentId))
                {
                    std::string location;
                    if (room->getType() == Actions::EndpointListTypeEnum::kZone)
                    {
                        location = gDevices[index]->GetZone();
                    }
                    else
                    {
                        location = gDevices[index]->GetLocation();
                    }
                    if (room->getName().compare(location) == 0)
                    {
                        info.AddEndpointId(gDevices[index]->GetEndpointId());
                    }
                }
                index++;
            }
            if (info.GetEndpointListSize() > 0)
            {
                infoList.push_back(info);
            }
        }
    }

    return infoList;
}

std::vector<Action *> GetActionListInfo(chip::EndpointId parentId)
{
    return gActions;
}

namespace {
void CallReportingCallback(intptr_t closure)
{
    auto path = reinterpret_cast<app::ConcreteAttributePath *>(closure);
    MatterReportingAttributeChangeCallback(*path);
    Platform::Delete(path);
}

void ScheduleReportingCallback(Device * dev, ClusterId cluster, AttributeId attribute)
{
    auto * path = Platform::New<app::ConcreteAttributePath>(dev->GetEndpointId(), cluster, attribute);
    PlatformMgr().ScheduleWork(CallReportingCallback, reinterpret_cast<intptr_t>(path));
}
} // anonymous namespace

void HandleDeviceStatusChanged(Device * dev, Device::Changed_t itemChangedMask)
{
    if (itemChangedMask & Device::kChanged_Reachable)
    {
        ScheduleReportingCallback(dev, BridgedDeviceBasicInformation::Id, BridgedDeviceBasicInformation::Attributes::Reachable::Id);
    }

    if (itemChangedMask & Device::kChanged_Name)
    {
        ScheduleReportingCallback(dev, BridgedDeviceBasicInformation::Id, BridgedDeviceBasicInformation::Attributes::NodeLabel::Id);
    }
}

void HandleDeviceOnOffStatusChanged(DeviceOnOff * dev, DeviceOnOff::Changed_t itemChangedMask)
{
    if (itemChangedMask & (DeviceOnOff::kChanged_Reachable | DeviceOnOff::kChanged_Name | DeviceOnOff::kChanged_Location))
    {
        HandleDeviceStatusChanged(static_cast<Device *>(dev), (Device::Changed_t) itemChangedMask);
    }

    if (itemChangedMask & DeviceOnOff::kChanged_OnOff)
    {
        ScheduleReportingCallback(dev, OnOff::Id, OnOff::Attributes::OnOff::Id);
    }
}

void HandleDeviceDimmableStatusChanged(DeviceDimmable * dev, DeviceDimmable::Changed_t itemChangedMask)
{
    if (itemChangedMask & (DeviceOnOff::kChanged_Reachable | DeviceOnOff::kChanged_Name | DeviceOnOff::kChanged_Location))
    {
        HandleDeviceStatusChanged(static_cast<Device *>(dev), (Device::Changed_t) itemChangedMask);
    }

    if (itemChangedMask & DeviceDimmable::kChanged_Level)
    {
        ScheduleReportingCallback(dev, LevelControl::Id, LevelControl::Attributes::CurrentLevel::Id);
    }
}

void HandleDeviceColorTemperatureStatusChanged(DeviceColorTemperature * dev, DeviceColorTemperature::Changed_t itemChangedMask)
{
    if (itemChangedMask & (DeviceOnOff::kChanged_Reachable | DeviceOnOff::kChanged_Name | DeviceOnOff::kChanged_Location))
    {
        HandleDeviceStatusChanged(static_cast<Device *>(dev), (Device::Changed_t) itemChangedMask);
    }

    if (itemChangedMask & DeviceColorTemperature::kChanged_Mireds)
    {
        ScheduleReportingCallback(dev, ColorControl::Id, ColorControl::Attributes::ColorTemperatureMireds::Id);
    }
}

void HandleDeviceExtendedColorStatusChanged(DeviceExtendedColor * dev, DeviceExtendedColor::Changed_t itemChangedMask)
{
    if (itemChangedMask & (DeviceOnOff::kChanged_Reachable | DeviceOnOff::kChanged_Name | DeviceOnOff::kChanged_Location))
    {
        HandleDeviceStatusChanged(static_cast<Device *>(dev), (Device::Changed_t) itemChangedMask);
    }

    if (itemChangedMask & DeviceExtendedColor::kChanged_Hue)
    {
        ScheduleReportingCallback(dev, ColorControl::Id, ColorControl::Attributes::CurrentHue::Id);
    }

    if (itemChangedMask & DeviceExtendedColor::kChanged_Saturation)
    {
        ScheduleReportingCallback(dev, ColorControl::Id, ColorControl::Attributes::CurrentSaturation::Id);
    }
}

void unhandled_attribute()
{
    ChipLogError(DeviceLayer,
                 "Unhandled "
                 "attribute!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
                 "!!!!!!!!!!!!!!!!!!!!");
#if ABORT_ON_UNHANDLED_ATTRIBUTE
    chipAbort();
#endif
}

EmberAfStatus HandleReadBridgedDeviceBasicAttribute(Device * dev, chip::AttributeId attributeId, uint8_t * buffer,
                                                    uint16_t maxReadLength)
{
    using namespace BridgedDeviceBasicInformation::Attributes;

    ChipLogProgress(DeviceLayer, "HandleReadBridgedDeviceBasicAttribute: attrId=%d, maxReadLength=%d", attributeId, maxReadLength);

    if ((attributeId == Reachable::Id) && (maxReadLength == 1))
    {
        *buffer = dev->IsReachable() ? 1 : 0;
    }
    else if ((attributeId == NodeLabel::Id) && (maxReadLength == 32))
    {
        MutableByteSpan zclNameSpan(buffer, maxReadLength);
        MakeZclCharString(zclNameSpan, dev->GetName());
    }
    else if ((attributeId == ClusterRevision::Id) && (maxReadLength == 2))
    {
        uint16_t rev = ZCL_BRIDGED_DEVICE_BASIC_INFORMATION_CLUSTER_REVISION;
        memcpy(buffer, &rev, sizeof(rev));
    }
    else if ((attributeId == FeatureMap::Id) && (maxReadLength == 4))
    {
        uint32_t featureMap = ZCL_BRIDGED_DEVICE_BASIC_INFORMATION_FEATURE_MAP;
        memcpy(buffer, &featureMap, sizeof(featureMap));
    }
    else if ((attributeId == VendorName::Id) && (maxReadLength == 32))
    {
        MutableByteSpan zclNameSpan(buffer, maxReadLength);
        MakeZclCharString(zclNameSpan, dev->GetManufacturer().c_str());
    }
    else if ((attributeId == ProductName::Id) && (maxReadLength == 32))
    {
        MutableByteSpan zclNameSpan(buffer, maxReadLength);
        MakeZclCharString(zclNameSpan, dev->GetModel().c_str());
    }
    else if ((attributeId == SerialNumber::Id) && (maxReadLength == 32))
    {
        MutableByteSpan zclNameSpan(buffer, maxReadLength);
        MakeZclCharString(zclNameSpan, dev->GetSerialNumber().c_str());
    }
    else
    {
        unhandled_attribute();
        return EMBER_ZCL_STATUS_FAILURE;
    }

    return EMBER_ZCL_STATUS_SUCCESS;
}

EmberAfStatus HandleReadIdentifyAttribute(Device * dev, chip::AttributeId attributeId, uint8_t * buffer, uint16_t maxReadLength)
{
    ChipLogProgress(DeviceLayer, "HandleReadIdentifyAttribute: attrId=%d, maxReadLength=%d", attributeId, maxReadLength);

    if ((attributeId == Identify::Attributes::IdentifyTime::Id) && (maxReadLength == 2))
    {
        uint16_t time = dev->IdentifyTime();
        memcpy(buffer, &time, 2);
        ChipLogProgress(DeviceLayer, "Identify::Attributes::IdentifyTime: %d", time);
    }
    else if ((attributeId == Identify::Attributes::IdentifyType::Id) && (maxReadLength == 1))
    {
        *buffer = (uint8_t) Identify::IdentifyTypeEnum::kLightOutput;
        ChipLogProgress(DeviceLayer, "Identify::Attributes::IdentifyTypeEnum: %d", *buffer);
    }
    else if ((attributeId == Identify::Attributes::ClusterRevision::Id) && (maxReadLength == 2))
    {
        uint16_t rev = ZCL_IDENTIFY_CLUSTER_REVISION;
        memcpy(buffer, &rev, 2);
        ChipLogProgress(DeviceLayer, "Identify::Attributes::ClusterRevision: %d", rev);
    }
    else
    {
        unhandled_attribute();
        return EMBER_ZCL_STATUS_FAILURE;
    }
    return EMBER_ZCL_STATUS_SUCCESS;
}

EmberAfStatus HandleReadOnOffAttribute(DeviceOnOff * dev, chip::AttributeId attributeId, uint8_t * buffer, uint16_t maxReadLength)
{
    ChipLogProgress(DeviceLayer, "HandleReadOnOffAttribute: attrId=%d, maxReadLength=%d", attributeId, maxReadLength);

    if ((attributeId == OnOff::Attributes::OnOff::Id) && (maxReadLength == 1))
    {
        *buffer = dev->IsOn() ? 1 : 0;
        ChipLogProgress(DeviceLayer, "OnOff::Attributes::OnOff: %d", *buffer);
    }
    else if ((attributeId == OnOff::Attributes::ClusterRevision::Id) && (maxReadLength == 2))
    {
        uint16_t rev = ZCL_ON_OFF_CLUSTER_REVISION;
        memcpy(buffer, &rev, sizeof(rev));
        ChipLogProgress(DeviceLayer, "OnOff::ClusterRevision::OnOff: %d", rev);
    }
    else
    {
        unhandled_attribute();
        return EMBER_ZCL_STATUS_FAILURE;
    }

    return EMBER_ZCL_STATUS_SUCCESS;
}

EmberAfStatus HandleReadLevelControlAttribute(DeviceDimmable * dev, chip::AttributeId attributeId, uint8_t * buffer,
                                              uint16_t maxReadLength)
{
    ChipLogProgress(DeviceLayer, "HandleReadLevelControlAttribute: attrId=%d, maxReadLength=%d", attributeId, maxReadLength);

    if ((attributeId == LevelControl::Attributes::CurrentLevel::Id) && (maxReadLength == 1))
    {
        *buffer = dev->Level();
        ChipLogProgress(DeviceLayer, "LevelControl::Attributes::CurrentLevel: %d", *buffer);
    }
    else if ((attributeId == LevelControl::Attributes::RemainingTime::Id) && (maxReadLength == 2))
    {
        *(uint16_t *) buffer = 0;
        ChipLogProgress(DeviceLayer, "LevelControl::Attributes::RemainingTime: %d", *buffer);
    }
    else if ((attributeId == LevelControl::Attributes::MinLevel::Id) && (maxReadLength == 1))
    {
        *buffer = 0;
        ChipLogProgress(DeviceLayer, "LevelControl::Attributes::MinLevel: %d", *buffer);
    }
    else if ((attributeId == LevelControl::Attributes::Options::Id) && (maxReadLength == 1))
    {
        *buffer = ZCL_LEVEL_CONTROL_OPTIONS;
        ChipLogProgress(DeviceLayer, "LevelControl::Attributes::Options: %d", *buffer);
    }
    else if ((attributeId == LevelControl::Attributes::StartUpCurrentLevel::Id) && (maxReadLength == 1))
    {
        *buffer = dev->Level();
        ChipLogProgress(DeviceLayer, "LevelControl::Attributes::StartUpCurrentLevel: %d", *buffer);
    }
    else if ((attributeId == LevelControl::Attributes::ClusterRevision::Id) && (maxReadLength == 2))
    {
        uint16_t rev = ZCL_LEVEL_CONTROL_CLUSTER_REVISION;
        memcpy(buffer, &rev, sizeof(rev));
        ChipLogProgress(DeviceLayer, "LevelControl::Attributes::ClusterRevision: %d", *buffer);
    }
    else if ((attributeId == LevelControl::Attributes::FeatureMap::Id) && (maxReadLength == 4))
    {
        uint32_t featureMap = ZCL_LEVEL_CONTROL_FEATURE_MAP;
        memcpy(buffer, &featureMap, sizeof(featureMap));
        ChipLogProgress(DeviceLayer, "LevelControl::Attributes::FeatureMap: %d", *buffer);
    }
    else
    {
        unhandled_attribute();
        return EMBER_ZCL_STATUS_FAILURE;
    }

    return EMBER_ZCL_STATUS_SUCCESS;
}

EmberAfStatus HandleReadColorControlAttribute(DeviceColorTemperature * dev, chip::AttributeId attributeId, uint8_t * buffer,
                                              uint16_t maxReadLength)
{
    ChipLogProgress(DeviceLayer, "HandleReadColorControlAttribute: attrId=%d, maxReadLength=%d", attributeId, maxReadLength);
    if ((attributeId == ColorControl::Attributes::CurrentHue::Id) && (maxReadLength == 1))
    {
        *buffer = dev->Hue();
        ChipLogProgress(DeviceLayer, "ColorControl::Attributes::CurrentHue: %d", *buffer);
    }
    else if ((attributeId == ColorControl::Attributes::CurrentSaturation::Id) && (maxReadLength == 1))
    {
        *buffer = dev->Saturation();
        ChipLogProgress(DeviceLayer, "ColorControl::Attributes::CurrentSaturation: %d", *buffer);
    }
    else if ((attributeId == ColorControl::Attributes::ColorTemperatureMireds::Id) && (maxReadLength == 2))
    {
        uint16_t level = dev->Mireds();
        memcpy(buffer, &level, sizeof(level));
        ChipLogProgress(DeviceLayer, "ColorControl::Attributes::ColorTemperatureMireds: %d", *(uint16_t *) buffer);
    }
    else if ((attributeId == ColorControl::Attributes::ColorMode::Id) && (maxReadLength == 1))
    {
        *buffer = dev->ColorMode();
        ChipLogProgress(DeviceLayer, "ColorControl::Attributes::ColorMode: %d", *buffer);
    }
    else if ((attributeId == ColorControl::Attributes::Options::Id) && (maxReadLength == 1))
    {
        *buffer = ZCL_COLOR_CONTROL_OPTIONS;
        ChipLogProgress(DeviceLayer, "ColorControl::Attributes::Options: %d", *buffer);
    }
    else if ((attributeId == ColorControl::Attributes::EnhancedColorMode::Id) && (maxReadLength == 1))
    {
        *buffer = dev->ColorMode();
        ChipLogProgress(DeviceLayer, "ColorControl::Attributes::EnhancedColorMode: %d", *buffer);
    }
    else if ((attributeId == ColorControl::Attributes::StartUpColorTemperatureMireds::Id) && (maxReadLength == 2))
    {
        uint16_t level = dev->Mireds();
        memcpy(buffer, &level, sizeof(level));
        ChipLogProgress(DeviceLayer, "ColorControl::Attributes::StartUpColorTemperatureMireds: %d", *(uint16_t *) buffer);
    }
    else if ((attributeId == ColorControl::Attributes::ColorCapabilities::Id) && (maxReadLength == 2))
    {
        uint16_t featureMap = dev->Capabilities();
        memcpy(buffer, &featureMap, sizeof(featureMap));
        ChipLogProgress(DeviceLayer, "ColorControl::Attributes::ColorCapabilities: %d", *(uint16_t *) buffer);
    }
    else if ((attributeId == ColorControl::Attributes::ColorTempPhysicalMinMireds::Id) && (maxReadLength == 2))
    {
        constexpr int WLED_KELVIN_MAX = 10091;
        uint16_t minK                 = static_cast<uint16_t>(ceil(1000000.0 / WLED_KELVIN_MAX));
        memcpy(buffer, &minK, sizeof(minK));
        ChipLogProgress(DeviceLayer, "ColorControl::Attributes::ColorTempPhysicalMinMireds: %d", *(uint16_t *) buffer);
    }
    else if ((attributeId == ColorControl::Attributes::ColorTempPhysicalMaxMireds::Id) && (maxReadLength == 2))
    {
        constexpr int WLED_KELVIN_MIN = 1900;
        uint16_t maxK                 = static_cast<uint16_t>(floor(1000000.0 / WLED_KELVIN_MIN));
        memcpy(buffer, &maxK, sizeof(maxK));
        ChipLogProgress(DeviceLayer, "ColorControl::Attributes::ColorTempPhysicalMaxMireds: %d", *(uint16_t *) buffer);
    }
    else if ((attributeId == ColorControl::Attributes::FeatureMap::Id) && (maxReadLength == 4))
    {
        uint32_t featureMap = dev->Capabilities();
        memcpy(buffer, &featureMap, sizeof(featureMap));
        ChipLogProgress(DeviceLayer, "ColorControl::Attributes::FeatureMap: %d", *(uint32_t *) buffer);
    }
    else if ((attributeId == ColorControl::Attributes::ClusterRevision::Id) && (maxReadLength == 2))
    {
        uint16_t rev = ZCL_COLOR_CONTROL_CLUSTER_REVISION;
        memcpy(buffer, &rev, sizeof(rev));
        ChipLogProgress(DeviceLayer, "ColorControl::Attributes::ClusterRevision: %d", *(uint16_t *) buffer);
    }
    else
    {
        unhandled_attribute();
        return EMBER_ZCL_STATUS_FAILURE;
    }

    return EMBER_ZCL_STATUS_SUCCESS;
}

EmberAfStatus HandleWriteIdentifyAttribute(Device * dev, chip::AttributeId attributeId, uint8_t * buffer)
{
    ChipLogProgress(DeviceLayer, "HandleWriteIdentifyAttribute: attrId=%d", attributeId);
    if ((attributeId == Identify::Attributes::IdentifyTime::Id) && (dev->IsReachable()))
    {
        uint16_t time = *(uint16_t *) buffer;
        dev->Identify(time);
        ChipLogProgress(DeviceLayer, "Identify::Attributes::Identify: %d", time);
    }
    else
    {
        unhandled_attribute();
        return EMBER_ZCL_STATUS_FAILURE;
    }

    return EMBER_ZCL_STATUS_SUCCESS;
}

EmberAfStatus HandleWriteOnOffAttribute(DeviceOnOff * dev, chip::AttributeId attributeId, uint8_t * buffer)
{
    ChipLogProgress(DeviceLayer, "HandleWriteOnOffAttribute: attrId=%d", attributeId);

    if ((attributeId == OnOff::Attributes::OnOff::Id) && (dev->IsReachable()))
    {
        if (*buffer)
        {
            dev->SetOnOff(true);
        }
        else
        {
            dev->SetOnOff(false);
        }
        ChipLogProgress(DeviceLayer, "OnOff::Attributes::OnOff: %d", *buffer);
    }
    else
    {
        unhandled_attribute();
        return EMBER_ZCL_STATUS_FAILURE;
    }

    return EMBER_ZCL_STATUS_SUCCESS;
}

EmberAfStatus HandleWriteLevelControlAttribute(DeviceDimmable * dev, chip::AttributeId attributeId, uint8_t * buffer)
{
    ChipLogProgress(DeviceLayer, "HandleWriteLevelControlAttribute: attrId=%d", attributeId);

    if ((attributeId == LevelControl::Attributes::CurrentLevel::Id) && (dev->IsReachable()))
    {
        dev->SetLevel(*buffer);
        ChipLogProgress(DeviceLayer, "LevelControl::Attributes::CurrentLevel: %d", *buffer);
    }
    else if ((attributeId == LevelControl::Attributes::RemainingTime::Id) && (dev->IsReachable()))
    {
        // TODO: This should not be writable???? Why is this getting called????
        ChipLogProgress(DeviceLayer, "LevelControl::Attributes::RemainingTime: %d", *buffer);
    }
    else
    {
        unhandled_attribute();
        return EMBER_ZCL_STATUS_FAILURE;
    }

    return EMBER_ZCL_STATUS_SUCCESS;
}

EmberAfStatus HandleWriteColorControlAttribute(DeviceColorTemperature * dev, chip::AttributeId attributeId, uint8_t * buffer)
{
    ChipLogProgress(DeviceLayer, "HandleWriteColorControlAttribute: attrId=%d", attributeId);

    if ((attributeId == ColorControl::Attributes::CurrentHue::Id) && (dev->IsReachable()))
    {
        dev->SetHue(*buffer);
        ChipLogProgress(DeviceLayer, "ColorControl::Attributes::CurrentHue: %d", *buffer);
    }
    else if ((attributeId == ColorControl::Attributes::CurrentSaturation::Id) && (dev->IsReachable()))
    {
        dev->SetSaturation(*buffer);
        ChipLogProgress(DeviceLayer, "ColorControl::Attributes::CurrentSaturation: %d", *buffer);
    }
    else if ((attributeId == ColorControl::Attributes::ColorTemperatureMireds::Id) && (dev->IsReachable()))
    {
        uint16_t mireds = *(uint16_t *) buffer;
        dev->SetMireds(mireds);
        ChipLogProgress(DeviceLayer, "ColorControl::Attributes::ColorTemperatureMireds: %d", mireds);
    }
    else if ((attributeId == ColorControl::Attributes::ColorMode::Id) && (dev->IsReachable()))
    {
        // TODO: This should not be writable???? Why is this getting called????
        dev->SetColorMode(*(ColorControl::ColorMode *) (buffer));
        ChipLogProgress(DeviceLayer, "ColorControl::Attributes::ColorMode: %d", *buffer);
    }
    else if ((attributeId == ColorControl::Attributes::EnhancedColorMode::Id) && (dev->IsReachable()))
    {
        // TODO: This should not be writable???? Why is this getting called????
        dev->SetColorMode(*(ColorControl::ColorMode *) (buffer));
        ChipLogProgress(DeviceLayer, "ColorControl::Attributes::EnhancedColorMode: %d", *buffer);
    }
    else
    {
        unhandled_attribute();
        return EMBER_ZCL_STATUS_FAILURE;
    }

    return EMBER_ZCL_STATUS_SUCCESS;
}

EmberAfStatus emberAfExternalAttributeReadCallback(EndpointId endpoint, ClusterId clusterId,
                                                   const EmberAfAttributeMetadata * attributeMetadata, uint8_t * buffer,
                                                   uint16_t maxReadLength)
{
    uint16_t endpointIndex = emberAfGetDynamicIndexFromEndpoint(endpoint);

    EmberAfStatus ret = EMBER_ZCL_STATUS_FAILURE;

    if ((endpointIndex < CHIP_DEVICE_CONFIG_DYNAMIC_ENDPOINT_COUNT) && (gDevices[endpointIndex] != nullptr))
    {
        Device * dev = gDevices[endpointIndex];

        if (clusterId == BridgedDeviceBasicInformation::Id)
        {
            ret = HandleReadBridgedDeviceBasicAttribute(dev, attributeMetadata->attributeId, buffer, maxReadLength);
        }
        else if (clusterId == Identify::Id)
        {
            ret = HandleReadIdentifyAttribute(static_cast<Device *>(dev), attributeMetadata->attributeId, buffer, maxReadLength);
        }
        else if (clusterId == OnOff::Id)
        {
            ret = HandleReadOnOffAttribute(static_cast<DeviceOnOff *>(dev), attributeMetadata->attributeId, buffer, maxReadLength);
        }
        else if (clusterId == LevelControl::Id)
        {
            ret = HandleReadLevelControlAttribute(static_cast<DeviceDimmable *>(dev), attributeMetadata->attributeId, buffer,
                                                  maxReadLength);
        }
        else if (clusterId == ColorControl::Id)
        {
            ret = HandleReadColorControlAttribute(static_cast<DeviceColorTemperature *>(dev), attributeMetadata->attributeId,
                                                  buffer, maxReadLength);
        }
        else
        {
            ChipLogError(DeviceLayer, "Unknown cluster ID: %d\n", clusterId);
        }
    }

    return ret;
}

EmberAfStatus emberAfExternalAttributeWriteCallback(EndpointId endpoint, ClusterId clusterId,
                                                    const EmberAfAttributeMetadata * attributeMetadata, uint8_t * buffer)
{
    uint16_t endpointIndex = emberAfGetDynamicIndexFromEndpoint(endpoint);

    EmberAfStatus ret = EMBER_ZCL_STATUS_FAILURE;

    if (endpointIndex < CHIP_DEVICE_CONFIG_DYNAMIC_ENDPOINT_COUNT)
    {
        Device * dev = gDevices[endpointIndex];

        if (!dev->IsReachable())
        {
            return ret;
        }

        if (clusterId == Identify::Id)
        {
            ret = HandleWriteIdentifyAttribute(static_cast<Device *>(dev), attributeMetadata->attributeId, buffer);
        }
        if (clusterId == OnOff::Id)
        {
            ret = HandleWriteOnOffAttribute(static_cast<DeviceOnOff *>(dev), attributeMetadata->attributeId, buffer);
        }
        if (clusterId == LevelControl::Id)
        {
            ret = HandleWriteLevelControlAttribute(static_cast<DeviceDimmable *>(dev), attributeMetadata->attributeId, buffer);
        }
        if (clusterId == ColorControl::Id)
        {
            ret = HandleWriteColorControlAttribute(static_cast<DeviceColorTemperature *>(dev), attributeMetadata->attributeId,
                                                   buffer);
        }
    }

    return ret;
}

void runOnOffRoomAction(Room * room, bool actionOn, EndpointId endpointId, uint16_t actionID, uint32_t invokeID, bool hasInvokeID)
{
    if (hasInvokeID)
    {
        Actions::Events::StateChanged::Type event{ actionID, invokeID, Actions::ActionStateEnum::kActive };
        EventNumber eventNumber;
        chip::app::LogEvent(event, endpointId, eventNumber);
    }

    if (hasInvokeID)
    {
        Actions::Events::StateChanged::Type event{ actionID, invokeID, Actions::ActionStateEnum::kInactive };
        EventNumber eventNumber;
        chip::app::LogEvent(event, endpointId, eventNumber);
    }
}

bool emberAfActionsClusterInstantActionCallback(app::CommandHandler * commandObj, const app::ConcreteCommandPath & commandPath,
                                                const Actions::Commands::InstantAction::DecodableType & commandData)
{
    bool hasInvokeID      = false;
    uint32_t invokeID     = 0;
    EndpointId endpointID = commandPath.mEndpointId;
    auto & actionID       = commandData.actionID;

    if (commandData.invokeID.HasValue())
    {
        hasInvokeID = true;
        invokeID    = commandData.invokeID.Value();
    }

    if (actionID == action1.getActionId() && action1.getIsVisible())
    {
        // Turn On Lights in Room 1
        runOnOffRoomAction(&room1, true, endpointID, actionID, invokeID, hasInvokeID);
        commandObj->AddStatus(commandPath, Protocols::InteractionModel::Status::Success);
        return true;
    }

    commandObj->AddStatus(commandPath, Protocols::InteractionModel::Status::NotFound);
    return true;
}

const EmberAfDeviceType gBridgedExtendedColorDeviceTypes[] = { { DEVICE_TYPE_LO_EXTENDED_COLOR_LIGHT, DEVICE_VERSION_DEFAULT },
                                                               { DEVICE_TYPE_BRIDGED_NODE, DEVICE_VERSION_DEFAULT } };

#define POLL_INTERVAL_MS (100)
uint8_t poll_prescale = 0;

bool kbhit()
{
    int byteswaiting;
    ioctl(0, FIONREAD, &byteswaiting);
    return byteswaiting > 0;
}

int wled_monitor_pipe[2];
int wled_fifo_in_fd;

bool add_wled_by_ip(std::string ip);
bool remove_wled_by_ip(std::string ip);

void * wled_monitoring_thread(void * context)
{
    int result;
    fd_set rfds;
    int nfds;

    while (true)
    {
        FD_ZERO(&rfds);
        FD_SET(wled_monitor_pipe[0], &rfds);
        nfds = wled_monitor_pipe[0];

        FD_SET(wled_fifo_in_fd, &rfds);
        nfds = std::max(nfds, wled_fifo_in_fd);

        for (auto & light : gLights)
        {
            if (light->IsReachable())
            {
                FD_SET(light->socket(), &rfds);
                nfds = std::max(nfds, light->socket());
            }
        }

        result = select(nfds + 1, &rfds, nullptr, nullptr, nullptr);
        if (result == -1)
        {
            perror("select");
            abort();
        }

        if (FD_ISSET(wled_monitor_pipe[0], &rfds))
        {
            char buf[1];
            // Don't care what it is, just breaking out of select
            read(wled_monitor_pipe[0], &buf, 1);
        }

        if (FD_ISSET(wled_fifo_in_fd, &rfds))
        {
            int wled_fifo_out_fd = open(WLED_FIFO_OUT, O_WRONLY);
            if (wled_fifo_out_fd == -1)
            {
                perror("open");
                exit(1);
            }

            ssize_t read_bytes = 0;

            char operation[1];
            read_bytes = read(wled_fifo_in_fd, operation, sizeof(operation));
            if (read_bytes < 0)
                ChipLogError(DeviceLayer, "Could not read from FIFO");

            char buf[100];
            read_bytes = read(wled_fifo_in_fd, buf, sizeof(buf));
            if (read_bytes < 0)
            {
                ChipLogError(DeviceLayer, "Could not read from FIFO");
                if (write(wled_fifo_out_fd, "1", 1) < 1)
                    ChipLogError(DeviceLayer, "Could not write!");
            }
            else
            {
                bool success = false;
                if (operation[0] == '1')
                {
                    ChipLogProgress(DeviceLayer, "Adding device: %s", buf);
                    success = add_wled_by_ip(std::string(buf));
                }
                else if (operation[0] == '2')
                {
                    ChipLogProgress(DeviceLayer, "Removing device: %s", buf);
                    success = remove_wled_by_ip(std::string(buf));
                }
                else
                {
                    ChipLogError(DeviceLayer, "Got unknown operation: %s", operation);
                }

                if (write(wled_fifo_out_fd, success ? "0" : "1", 1) < 1)
                    ChipLogError(DeviceLayer, "Could not write!");
            }

            close(wled_fifo_out_fd);
        }

        for (auto & light : gLights)
        {
            if (FD_ISSET(light->socket(), &rfds))
            {
                ChipLogProgress(DeviceLayer, "%s is ready to update!", light->GetName());
                light->update();
            }
        }
    }

    return nullptr;
}

bool add_wled(uint8_t index, WLED * device)
{
    if (index >= CHIP_DEVICE_CONFIG_DYNAMIC_ENDPOINT_COUNT - gFirstDynamicEndpointId)
    {
        ChipLogError(DeviceLayer, "Could not add WLED (%s)", device->GetIP().c_str());
        return false;
    }

    ChipLogProgress(DeviceLayer, "Adding WLED: %s (%s)", device->GetName(), device->GetIP().c_str());
    // TODO: Handle this a little more elegantly
    device->DeviceOnOff::SetChangeCallback(&HandleDeviceOnOffStatusChanged);
    device->DeviceDimmable::SetChangeCallback(&HandleDeviceDimmableStatusChanged);
    device->DeviceColorTemperature::SetChangeCallback(&HandleDeviceColorTemperatureStatusChanged);
    device->DeviceExtendedColor::SetChangeCallback(&HandleDeviceExtendedColorStatusChanged);
    gDataVersions[index] = { 0 };

    int ret =
        AddDeviceEndpoint(index, device, &bridgedLightEndpoint, Span<const EmberAfDeviceType>(gBridgedExtendedColorDeviceTypes),
                          Span<DataVersion>(gDataVersions[index]), 1);
    if (ret < 0)
        return false;

    kvs->store_wled(index, device);
    gLights.push_back(device);

    // Tell the monitoring thread there is a new WLED device
    char buf[1] = { 1 };
    write(wled_monitor_pipe[1], buf, 1);

    return true;
}

bool add_wled_by_ip(std::string ip)
{
    // Check if the IP is already known
    for (auto & device : gLights)
    {
        if (device->GetIP() == ip)
            return true;
    }

    for (auto & deny_entry : deny_list)
    {
        if (deny_entry == ip)
        {
            ChipLogError(DeviceLayer, "Not adding %s - it is in the deny list", ip.c_str());
            return false;
        }
    }

    uint8_t next_endpoint                             = CHIP_DEVICE_CONFIG_DYNAMIC_ENDPOINT_COUNT;
    DataVersion temp[ArraySize(bridgedLightClusters)] = {};

    for (uint8_t i = (uint8_t) gFirstDynamicEndpointId; i < CHIP_DEVICE_CONFIG_DYNAMIC_ENDPOINT_COUNT; i++)
    {
        if (memcmp(gDataVersions.at(i).data(), temp, sizeof(temp)) == 0)
        {
            next_endpoint = i;
            break;
        }
    }

    auto light = new WLED(ip, "Office");
    return add_wled(next_endpoint, light);
}

bool remove_wled_by_ip(std::string ip)
{
    size_t index         = 0;
    size_t devices_index = 0;
    WLED * target        = nullptr;

    // Check if the IP is already known
    for (; index < gLights.size(); index++)
    {
        if (gLights[index]->GetIP() == ip)
        {
            target = gLights[index];
            break;
        }
    }

    // Could not find it
    if (!target)
        return false;

    for (; devices_index < CHIP_DEVICE_CONFIG_DYNAMIC_ENDPOINT_COUNT; devices_index++)
    {
        if (gDevices[devices_index] == target)
        {
            break;
        }
    }

    int result = RemoveDeviceEndpoint(target);
    if (!result)
    {
        ChipLogError(DeviceLayer, "Could not remove endpoint: %s", ip.c_str());
        return false;
    }

    gLights.erase(gLights.begin() + (int) index);

    gDataVersions[devices_index] = { 0 };
    result                       = kvs->delete_wled((uint8_t) devices_index);

    // Tell the monitoring thread there is a new WLED device
    char buf[1] = { 1 };
    write(wled_monitor_pipe[1], buf, 1);

    return result;
}

void * mdns_monitoring_thread(void * context)
{
    mdns = new wled::MDNS();

    while (true)
    {
        ChipLogProgress(DeviceLayer, "Sending mDNS query");
        mdns->send_query();

        while (true)
        {
            fd_set rfds;
            FD_ZERO(&rfds);
            FD_SET(mdns->socket(), &rfds);
            struct timeval tv = { .tv_sec = MDNS_TIMEOUT, .tv_usec = 0 };

            int ret = select(mdns->socket() + 1, &rfds, NULL, NULL, &tv);
            if (ret < 0)
            {
                ChipLogError(DeviceLayer, "select issue");
                abort();
            }

            // Timeout
            if (ret == 0)
                break;

            std::string ip = mdns->recv_query();
            if (!ip.empty())
                add_wled_by_ip(ip);
        }
    }

    return nullptr;
}

void ApplicationInit()
{
    // Clear out the device database
    memset(gDevices, 0, sizeof(gDevices));

    // Set starting endpoint id where dynamic endpoints will be assigned, which
    // will be the next consecutive endpoint id after the last fixed endpoint.
    gFirstDynamicEndpointId = static_cast<chip::EndpointId>(
        static_cast<int>(emberAfEndpointFromIndex(static_cast<uint16_t>(emberAfFixedEndpointCount() - 1))) + 1);

    // Disable last fixed endpoint, which is used as a placeholder for all of the
    // supported clusters so that ZAP will generated the requisite code.
    emberAfEndpointEnableDisable(emberAfEndpointFromIndex(static_cast<uint16_t>(emberAfFixedEndpointCount() - 1)), false);

    gRooms.push_back(&room1);

    CURLcode code = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (code != CURLE_OK)
    {
        printf("%s\n", curl_easy_strerror(code));
        exit(1);
    }

    kvs = new wled::KVS(CHIP_DEVICE_CONFIG_DYNAMIC_ENDPOINT_COUNT);

    auto stored_devices = kvs->get_wleds();
    for (auto & [index, device] : stored_devices)
    {
        if (add_wled(index, device) == true)
        {
            ChipLogProgress(DeviceLayer, "Added WLED (%s) at index %d", device->GetName(), index);
        }
        else
        {
            ChipLogError(DeviceLayer, "Could not add WLED (%s) at index %d", device->GetName(), index);
        }
    }

    char * deny_string = std::getenv("WLED_DENY_LIST");
    if (deny_string)
    {
        char * p = strtok(deny_string, ",");
        while (p != NULL)
        {
            auto denied = std::string(p);
            deny_list.push_back(denied);
            ChipLogProgress(DeviceLayer, "Added %s to deny list", denied.c_str());
            p = strtok(NULL, ",");
        }
    }

    int res;

    res = pipe(wled_monitor_pipe);
    if (res)
    {
        perror("pipe");
        exit(1);
    }

    res = mkfifo(WLED_FIFO_IN, 0600);
    if (res && errno != EEXIST)
    {
        perror("mkfifo");
        exit(1);
    }

    res = mkfifo(WLED_FIFO_OUT, 0600);
    if (res && errno != EEXIST)
    {
        perror("mkfifo");
        exit(1);
    }

    wled_fifo_in_fd = open(WLED_FIFO_IN, O_RDWR | O_NONBLOCK); // Needs to be R/W or else select will always return
    if (wled_fifo_in_fd == -1)
    {
        perror("open");
        exit(1);
    }

    {
        pthread_t wled_thread;
        res = pthread_create(&wled_thread, nullptr, wled_monitoring_thread, nullptr);
        if (res)
        {
            printf("Error creating WLED thread: %d\n", res);
            exit(1);
        }
    }

#if ENABLE_MDNS
    {
        pthread_t mdns_thread;
        res = pthread_create(&mdns_thread, nullptr, mdns_monitoring_thread, nullptr);
        if (res)
        {
            printf("Error creating MDNS thread: %d\n", res);
            exit(1);
        }
    }
#else
    ChipLogProgress(DeviceLayer, "mDNS querying disabled!");
#endif
}

void ApplicationShutdown()
{
    printf("Shutting down...");
}

int main(int argc, char * argv[])
{
    if (ChipLinuxAppInit(argc, argv) != 0)
    {
        return -1;
    }
    ChipLinuxAppMainLoop();
    return 0;
}
