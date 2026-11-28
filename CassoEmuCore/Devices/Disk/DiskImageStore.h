#pragma once

#include "Pch.h"

#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/MountDiagnosis.h"
#include "Devices/Disk/MountedImageState.h"
#include "Devices/Disk/BayChange.h"
#include "Devices/Disk/Store/BayFileAccess.h"
#include "Devices/Disk/Store/BayWatchRegistry.h"
#include "Devices/Disk/Store/DiskBayTable.h"
#include "Devices/Disk/Store/DiskFlusher.h"
#include "Devices/Disk/Store/DiskSalvager.h"
#include "Devices/Disk/Store/DiskStoreHooks.h"
#include "Devices/Disk/Store/DiskStoreInterfaces.h"
#include "Devices/Disk/Store/ExternalChangeActions.h"
#include "Devices/Disk/Store/ExternalChangeMonitor.h"

class IDiskFileIo;
class IImageWatcher;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore
//
//  Mount/Eject coordinator. Owns the DiskImage instances mounted across
//  every (slot, drive) pair, and the components that work on them:
//
//      DiskBayTable           the bays themselves
//      BayFileAccess          every read, stat and write of a file
//      DiskFlusher            writing dirty images back
//      DiskSalvager           salvaged copies of damaged disks
//      ExternalChangeMonitor  changes made to an image's file outside Casso
//      ExternalChangeActions  what such a change leads to
//      BayWatchRegistry       the folder watches behind the monitor
//
//  Auto-flush invariants (FR-025):
//      Eject(slot, drive) -- flush dirty image, then release.
//      FlushAll()         -- flush every dirty mount; called on machine
//                            switch and on emulator exit / PowerCycle.
//      SoftReset()        -- flush dirty, without unmounting.
//      PowerCycle()       -- unmount everything (auto-flush each).
//
////////////////////////////////////////////////////////////////////////////////

class DiskImageStore : private IBayRepoint, private IBayLifecycle
{
public:
    HRESULT                     Mount                     (int slot, int drive, const string & path);
    HRESULT                     MountFromBytes            (int slot, int drive, const string & virtualPath,
                                                           DiskFormat fmt, const vector<Byte> & bytes);

    //  The same two mounts, reporting why a refusal happened. The diagnosis is
    //  set to None on success.
    HRESULT                     Mount                     (int slot, int drive, const string & path,
                                                           MountDiagnosis & outDiagnosis);
    HRESULT                     MountFromBytes            (int slot, int drive, const string & virtualPath,
                                                           DiskFormat fmt, const vector<Byte> & bytes,
                                                           MountDiagnosis & outDiagnosis);
    void                        Eject                     (int slot, int drive) override;
    void                        SoftReset                 ();
    void                        PowerCycle                ();

    DiskFlusher               & GetFlusher                () { return m_flusher;       }
    DiskSalvager              & GetSalvager               () { return m_salvager;      }
    ExternalChangeActions     & GetChangeActions          () { return m_changeActions; }
    ExternalChangeMonitor     & GetChangeMonitor          () { return m_changeMonitor; }

    DiskImage                 * GetImage                  (int slot, int drive);
    uint64_t                    GetMediaId                (int slot, int drive) const;
    bool                        IsMounted                 (int slot, int drive) const;
    const string              & GetSourcePath             (int slot, int drive) const;

    //  Whether the machine has a drive at the bay: a Disk II connector with a
    //  drive on it, or the //c's disk port with a Disk IIc. A mount into a
    //  bay with no drive fails with MountFailure::NoDrive. Every bay has a
    //  drive until SetDriveAttached detaches it. Safe on any thread.
    void                        SetDriveAttached          (int slot, int drive, bool attached);
    bool                        IsDriveAttached           (int slot, int drive) const;

    //  Set once at mount. Free to call: no decoding.
    bool                        IsSalvageOffered          (int slot, int drive) const;

    //  A machine state was restored and a mounted disk's tracks may have
    //  changed under it (FR-069).
    void                        NoteStateRestored         (int slot, int drive);

    //  One mounted image's backing path with its bay.
    struct MountedSource
    {
        string  path;
        int     slot  = 0;
        int     drive = 0;
    };

    std::vector<MountedSource>  GetMountedSourcePaths     () const;

    //  Null for an out-of-range bay.
    MountedImageState         * GetSharedState            (int slot, int drive);
    const MountedImageState   * GetSharedState            (int slot, int drive) const;

    //  The fields of DiskStoreHooks, one setter each.
    void                        SetFlushSink              (DiskStoreHooks::FlushSink sink)          { m_hooks.flushSink       = std::move (sink);   }
    void                        SetImageReader            (DiskStoreHooks::ImageReader reader)      { m_hooks.imageReader     = std::move (reader); }
    void                        SetIdentityReader         (DiskStoreHooks::IdentityReader reader)   { m_hooks.identityReader  = std::move (reader); }
    void                        SetImageWatcher           (IImageWatcher * watcher)                 { m_hooks.watcher         = watcher;            }
    void                        SetFileIo                 (IDiskFileIo * fileIo)                    { m_hooks.fileIo          = fileIo;             }
    void                        SetMachineName            (const string & name)                     { m_hooks.machineName     = name;               }
    void                        SetMachineRestartCallback (std::function<void ()> cb)               { m_hooks.restartCallback = std::move (cb);     }
    void                        SetChangeReportSink       (DiskStoreHooks::ReportSink sink)         { m_hooks.reportSink      = std::move (sink);   }
    void                        SetAskSink                (DiskStoreHooks::AskSink sink)            { m_hooks.askSink         = std::move (sink);   }
    void                        SetRescueSink             (DiskStoreHooks::RescueSink sink)         { m_hooks.rescueSink      = std::move (sink);   }
    void                        SetBayChangeSink          (DiskStoreHooks::BayChangeSink sink)      { m_hooks.bayChangeSink   = std::move (sink);   }
    void                        SetDecisionSink           (DiskStoreHooks::DecisionSink sink)       { m_hooks.decisionSink    = std::move (sink);   }
    void                        SetClock                  (std::function<int64_t ()> clock)         { m_hooks.clock           = std::move (clock);  }
    void                        SetTimestampSource        (std::function<time_t ()> source)         { m_hooks.timestamp       = std::move (source); }

private:
    HRESULT                     RepointBayToFile            (int slot, int drive, const string & newPath) override;
    void                        EjectLostImage              (int slot, int drive) override;
    HRESULT                     MountExternallyModifiedDisk (int slot, int drive, const vector<Byte> & bytes) override;

    //  Loads bytes into a bay for MountFromBytes and Mount, which each call the
    //  bay-change sink after a successful load, in their own order.
    HRESULT                     LoadIntoBay                 (int slot, int drive, const string & virtualPath,
                                                             DiskFormat fmt, const vector<Byte> & bytes,
                                                             MountDiagnosis & outDiagnosis);

    //  Every path that changes a bay calls this, so the door and its sounds
    //  follow every change.
    void                        EmitBayChange               (int slot, int drive, BayChange change);

    DiskStoreHooks              m_hooks;
    DiskBayTable                m_bays;
    BayFileAccess               m_files         { m_hooks };
    DiskSalvager                m_salvager      { m_bays, m_hooks };
    DiskFlusher                 m_flusher       { m_bays, m_hooks, m_files, *this };
    ExternalChangeActions       m_changeActions { m_bays, m_hooks, m_files, *this, *this };
    ExternalChangeMonitor       m_changeMonitor { m_bays, m_hooks, m_files, m_changeActions };
    BayWatchRegistry            m_watches       { m_bays, m_hooks, m_changeMonitor };
    string                      m_emptyPath;

    std::array<std::array<std::atomic<bool>, DiskBayTable::kDriveCount>, DiskBayTable::kSlotCount>  m_detached {};
};
