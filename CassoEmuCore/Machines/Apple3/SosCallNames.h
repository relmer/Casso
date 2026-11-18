#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SosCallNames
//
//  A SOS call is a BRK, the call number and a two-byte pointer to the
//  parameter list. Each call's name and number is from the SOS Reference
//  Manual, volume 2: file calls $C0-$D3, device calls $82-$85, memory calls
//  $40-$45 and utility calls $60-$65.
//
////////////////////////////////////////////////////////////////////////////////

class SosCallNames
{
public:
    static constexpr int  kCallBytes = 4;

    //  The call's name, or nullptr for a number that is not a SOS call.
    static const char *  GetName (Byte callNumber);
};
