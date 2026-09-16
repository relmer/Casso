#pragma once

#include "Debugger/Channel/IInstanceDirectory.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ListedInstance
//
//  One running Casso as its handshake described it.
//
////////////////////////////////////////////////////////////////////////////////

struct ListedInstance
{
    uint32_t                                  processId = 0;
    std::string                               title;
    std::string                               machine;
    std::vector<std::optional<std::string>>   disks;
};





////////////////////////////////////////////////////////////////////////////////
//
//  InstanceLister
//
//  What `CassoCli debug --list` prints: every instance that answers `hello`.
//
//  AN INSTANCE THAT DOES NOT ANSWER IS LEFT OUT RATHER THAN LISTED AS UNKNOWN.
//  One that refuses the connection belongs to another user and cannot be
//  attached to, and one that connects but never replies cannot be debugged
//  either; listing either would offer the reader a process id `--attach`
//  would then fail on.
//
////////////////////////////////////////////////////////////////////////////////

class InstanceLister
{
public:
    //  How long each instance has to answer `hello`.
    static constexpr DWORD  kHelloTimeoutMs = 2000;

    static std::vector<ListedInstance>  List   (IInstanceDirectory & directory, DWORD helloTimeoutMs = kHelloTimeoutMs);

    //  Columns `pid title machine disk1 disk2`, a header then one row each.
    //  An empty title or drive prints as `-`, so every row has five fields.
    static std::string                  Format (const std::vector<ListedInstance> & instances);

    //  The handshake reply read back into an instance. False for anything that
    //  is not a hello record.
    static bool  TryParseHello (const std::string & record, ListedInstance & instance);
};
