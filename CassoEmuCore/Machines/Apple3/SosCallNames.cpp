#include "Pch.h"

#include "Machines/Apple3/SosCallNames.h"





////////////////////////////////////////////////////////////////////////////////
//
//  s_kpszFileCalls and the other call tables
//
//  Each table lists consecutive call numbers from its first.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr const char *  s_kpszFileCalls[] =
{
    "CREATE",     "DESTROY",    "RENAME",     "SET_FILE_INFO", "GET_FILE_INFO",
    "VOLUME",     "SET_PREFIX", "GET_PREFIX", "OPEN",          "NEWLINE",
    "READ",       "WRITE",      "CLOSE",      "FLUSH",         "SET_MARK",
    "GET_MARK",   "SET_EOF",    "GET_EOF",    "SET_LEVEL",     "GET_LEVEL",
};

static constexpr const char *  s_kpszDeviceCalls[] =
{
    "D_STATUS", "D_CONTROL", "GET_DEV_NUM", "D_INFO",
};

static constexpr const char *  s_kpszMemoryCalls[] =
{
    "REQUEST_SEG", "FIND_SEG", "CHANGE_SEG", "GET_SEG_INFO", "GET_SEG_NUM", "RELEASE_SEG",
};

static constexpr const char *  s_kpszUtilityCalls[] =
{
    "SET_FENCE", "GET_FENCE", "SET_TIME", "GET_TIME", "GET_ANALOG", "TERMINATE",
};

static constexpr Byte  s_kFirstFileCall    = 0xC0;
static constexpr Byte  s_kFirstDeviceCall  = 0x82;
static constexpr Byte  s_kFirstMemoryCall  = 0x40;
static constexpr Byte  s_kFirstUtilityCall = 0x60;





////////////////////////////////////////////////////////////////////////////////
//
//  SosCallNames::GetName
//
////////////////////////////////////////////////////////////////////////////////

const char * SosCallNames::GetName (Byte callNumber)
{
    struct Range
    {
        Byte                   first;
        const char * const   * names;
        size_t                 count;
    };

    static constexpr Range  s_kRanges[] =
    {
        { s_kFirstFileCall,    s_kpszFileCalls,    _countof (s_kpszFileCalls)    },
        { s_kFirstDeviceCall,  s_kpszDeviceCalls,  _countof (s_kpszDeviceCalls)  },
        { s_kFirstMemoryCall,  s_kpszMemoryCalls,  _countof (s_kpszMemoryCalls)  },
        { s_kFirstUtilityCall, s_kpszUtilityCalls, _countof (s_kpszUtilityCalls) },
    };

    const char  * name = nullptr;



    for (const Range & range : s_kRanges)
    {
        if (callNumber >= range.first && callNumber < range.first + range.count)
        {
            name = range.names[callNumber - range.first];
        }
    }

    return name;
}
