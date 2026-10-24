#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  INetworkChanges
//
//  A count that changes whenever one of the PC's addresses or interfaces is
//  added, removed or changed. A caller compares it with the count from its
//  last call instead of being called back.
//
////////////////////////////////////////////////////////////////////////////////

class INetworkChanges
{
public:
    virtual ~INetworkChanges () = default;

    virtual uint32_t  GetChangeCount () const = 0;
};
