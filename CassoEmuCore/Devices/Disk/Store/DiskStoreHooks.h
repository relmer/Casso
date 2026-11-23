#pragma once

#include "Pch.h"

#include "Devices/Disk/BayChange.h"
#include "Devices/Disk/ChangePrompt.h"
#include "Devices/Disk/ExternalChangePolicy.h"
#include "Devices/Disk/ImageIdentity.h"

class IDiskFileIo;
class IImageWatcher;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreHooks
//
//  The seams and sinks a host installs on a DiskImageStore. Every store
//  component reads them from here; each is empty or null until set.
//
////////////////////////////////////////////////////////////////////////////////

struct DiskStoreHooks
{
    using FlushSink      = std::function<HRESULT (const string &, const vector<Byte> &)>;
    using ImageReader    = std::function<HRESULT (const string &, vector<Byte> &)>;
    using IdentityReader = std::function<ImageIdentity (const string &)>;
    using ReportSink     = std::function<void (int slot, int drive, const ChangePrompt &)>;
    using AskSink        = std::function<bool (int slot, int drive, const ChangePrompt &)>;
    using RescueSink     = std::function<bool (const string & imagePath, string & outPath)>;
    using BayChangeSink  = std::function<void (int slot, int drive, BayChange change)>;
    using DecisionSink   = std::function<void (const string & path, ChangeAction action,
                                               bool guestCopyPreserved, const string & preservedPath)>;

    FlushSink                  flushSink;
    ImageReader                imageReader;
    IdentityReader             identityReader;

    IImageWatcher            * watcher = nullptr;
    IDiskFileIo              * fileIo  = nullptr;

    std::function<void ()>     restartCallback;
    string                     machineName;
    ReportSink                 reportSink;
    AskSink                    askSink;
    RescueSink                 rescueSink;
    BayChangeSink              bayChangeSink;
    DecisionSink               decisionSink;
    std::function<int64_t ()>  clock;
    std::function<time_t ()>   timestamp;
};
