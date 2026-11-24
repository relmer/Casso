#pragma once

#include "Pch.h"

#include "ExternalChangeIntent.h"
#include "FakeImageWatcher.h"
#include "Devices/Disk/BayChange.h"
#include "Devices/Disk/ChangePrompt.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/Disk/IDiskFileIo.h"
#include "Devices/Disk/ImageIdentity.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SharedImageRig
//
//  A store with every platform seam replaced: reads and writes land in
//  this object, identities come from a number this object controls, and
//  changes are fired by hand.
//
////////////////////////////////////////////////////////////////////////////////

class SharedImageRig
{
public:

    static constexpr int   kSlot  = 6;
    static constexpr int   kDrive = 0;

    static constexpr const char *  kDirectory = "C:\\work";
    static constexpr const char *  kImagePath = "C:\\work\\Loader.dsk";

    //
    //  Only the two questions the store asks the platform outside its read,
    //  write and identity seams: whether a path is taken, and whether somebody
    //  else is holding it.
    //
    //  IT ANSWERS OUT OF THE RIG'S OWN FILE MAP, which is what makes the
    //  preserved-name collision loop testable: a candidate name a test has
    //  already used has to read as taken.
    //
    struct FileIo : public IDiskFileIo
    {
        SharedImageRig &  rig;

        explicit FileIo (SharedImageRig & owner) : rig (owner) {}

        bool  Exists                 (const std::string & path) override;
        bool  IsHeldByAnotherProcess (const std::string &) override { return false; }

        //  Nothing below is reached: the store reads, writes and stats through
        //  its own seams, and this exists for the two questions above.
        HRESULT  ReadAllBytes                 (const std::string &, std::vector<Byte> &) override { return E_NOTIMPL; }
        HRESULT  ListFiles                    (const std::string &, std::vector<std::string> &) override { return E_NOTIMPL; }
        HRESULT  WriteAllBytes                (const std::string &, const std::vector<Byte> &) override { return E_NOTIMPL; }
        HRESULT  WriteAt                      (const std::string &, uint64_t, std::span<const Byte>) override { return E_NOTIMPL; }
        HRESULT  ReadAt                       (const std::string &, uint64_t, std::span<Byte>) override { return E_NOTIMPL; }
        HRESULT  Stat                         (const std::string &, FileStamp &) override { return E_NOTIMPL; }
        HRESULT  Remove                       (const std::string &) override { return E_NOTIMPL; }
        HRESULT  ReplaceAtomically            (const std::string &, const std::string &) override { return E_NOTIMPL; }
        HRESULT  FlushToStorage               (const std::string &) override { return E_NOTIMPL; }
        HRESULT  CopyFileMetadata             (const std::string &, const std::string &) override { return E_NOTIMPL; }
        HRESULT  RenameWithoutReplacing       (const std::string &, const std::string &) override { return E_NOTIMPL; }
        HRESULT  WritePayloadToStandardOutput (const std::vector<Byte> &) override { return E_NOTIMPL; }
    };

    SharedImageRig();

    void                              Stamp          (const std::string & path);
    const std::vector<std::string> &  PreservedPaths () const { return preserved; }
    void                              WriteImage     (const std::string & path, Byte fill);
    void                              FireAndSettle  (const std::string & path,
                                                      ExternalChangeIntent intent = ExternalChangeIntent::Unstated,
                                                      bool fromCassoExplorer = false);

    static Byte                       FirstTrackByte (DiskImageStore & store);

    DiskImageStore                                 store;
    FakeImageWatcher                               watcher;
    FileIo                                         fileIo { *this };
    std::unordered_map<std::string, vector<Byte>>  files;
    std::unordered_map<std::string, ImageIdentity> identities;
    int64_t                                        nowMs = 0;

    //  Fixed, so every preserved copy in one test is stamped alike unless
    //  the test says otherwise.
    time_t                                         wallClock = 1756500000;

    //  Makes every attempt to write a preserved copy fail, which is the
    //  only way to reach the rule that a version is never destroyed for
    //  want of somewhere to put the other one.
    bool                                           refusePreserve = false;

    //  Makes the ask sink report that the question never reached anywhere
    //  it could be answered -- what the shell returns when it has no
    //  window yet, or when the post fails.
    bool                                           askSinkDelivers = true;

    //  The path the shutdown rescue dialog returns, and how many times it
    //  was raised. An empty path stands for the user declining.
    std::string                                    rescueChoice;
    int                                            rescuesAsked = 0;

    //  What the store reported and did.
    std::vector<ChangePrompt>                      reports;
    std::vector<ChangePrompt>                      questions;
    int                                            restarts = 0;

    //  How many times the store has read an image file. Reading is the
    //  expensive half of noticing a change, so a test can hold this still
    //  to prove nothing is being re-read on every tick.
    int                                            reads = 0;

    //  Every bay change the store announced, in order, so a test can prove
    //  the one signal the shell reacts to fires on each path.
    std::vector<BayChange>         bayChanges;

    std::vector<std::string>  preserved;

private:
    int64_t  m_tick = 100;
};
