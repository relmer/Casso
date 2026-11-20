#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/IDiskInspectorHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FakeDrivelessHost
//
//  A host of the disk inspector with no drives, as Casso Explorer is.
//
////////////////////////////////////////////////////////////////////////////////

class FakeDrivelessHost : public IDiskInspectorHost
{
public:
    uint64_t  PostInspectorRequest (const InspectorRequest &) override         { return 0; }
    void      TakeInspectorReplies (vector<InspectorReply> & outReplies) override { outReplies.clear(); }
    int       GetDriveCount        () const override                         { return 0; }
    void      OnInspectorClosed    () override                               {}

    std::shared_ptr<const DriveHeadState>  GetHeadState       (int) const override { return nullptr; }
    void                                   SetHeadStateWanted (bool) override      {}
};
