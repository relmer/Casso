#pragma once

#include "Pch.h"

#include "Core/MachineConfig.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SerialPortId
//
//  The serial connectors Settings stages: the Super Serial Card's, and the
//  //c's two.
//
////////////////////////////////////////////////////////////////////////////////

enum class SerialPortId
{
    Card,
    Port1,
    Port2,
};





////////////////////////////////////////////////////////////////////////////////
//
//  SerialAttachmentChoice
//
//  What a serial connector has attached and its options: the device string
//  (empty for nothing, or on a //c port for the loopback plug), the host
//  file of the file attachments, and the COM port, TCP and modem options.
//
////////////////////////////////////////////////////////////////////////////////

struct SerialAttachmentChoice
{
    std::string              device;
    std::string              path;
    SerialAttachmentOptions  options;

    bool operator== (const SerialAttachmentChoice & other) const = default;
};
