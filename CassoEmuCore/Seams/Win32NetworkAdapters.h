#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  NetworkAdapter
//
//  One IPv4 address of one of the PC's network adapters, with its subnet
//  mask and the adapter's gateway, or zeros for none.
//
////////////////////////////////////////////////////////////////////////////////

struct NetworkAdapter
{
    std::array<Byte, 4>  address = {};
    std::array<Byte, 4>  mask    = {};
    std::array<Byte, 4>  gateway = {};
};





////////////////////////////////////////////////////////////////////////////////
//
//  Win32NetworkAdapters
//
//  The PC's IPv4 addresses, from the adapters that are up, loopback left out.
//
////////////////////////////////////////////////////////////////////////////////

class Win32NetworkAdapters
{
public:
    static HRESULT  List (std::vector<NetworkAdapter> & outAdapters);
};
