#pragma once

#include "Pch.h"

#include "Shell/InspectorRequestQueue.h"
#include "Shell/InspectorStatusPublisher.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IDiskInspectorHost
//
//  What the inspector window needs from the program that opened it: a way
//  to ask for copies of a disk and take the replies, how many drives there
//  are, each drive's head as last published, and word that the window
//  closed. Casso answers through the
//  emulation thread; NullDiskInspectorHost answers nothing, for a window
//  with no host yet.
//
////////////////////////////////////////////////////////////////////////////////

class IDiskInspectorHost
{
public:
    virtual ~IDiskInspectorHost() = default;

    virtual uint64_t  PostInspectorRequest (const InspectorRequest & request)  = 0;
    virtual void      TakeInspectorReplies (vector<InspectorReply> & outReplies) = 0;
    virtual int       GetDriveCount        () const                               = 0;
    virtual void      OnInspectorClosed    ()                                     = 0;

    //  Each drive's head and counts as last published, and whether the
    //  window is shown to want them (FR-062, FR-070).
    virtual std::shared_ptr<const DriveHeadState>  GetHeadState       (int drive) const = 0;
    virtual void                                   SetHeadStateWanted (bool isWanted)  = 0;
};


class NullDiskInspectorHost : public IDiskInspectorHost
{
public:
    uint64_t  PostInspectorRequest (const InspectorRequest & request)   override { (void) request; return 0; }
    void      TakeInspectorReplies (vector<InspectorReply> & outReplies) override { outReplies.clear(); }
    int       GetDriveCount        () const                               override { return 0; }
    void      OnInspectorClosed    ()                                     override {}

    std::shared_ptr<const DriveHeadState>  GetHeadState       (int drive) const override { (void) drive; return nullptr; }
    void                                   SetHeadStateWanted (bool isWanted)  override { (void) isWanted; }
};
