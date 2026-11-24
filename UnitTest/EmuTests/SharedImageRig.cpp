#include "Pch.h"

#include "../EhmTestHelper.h"
#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/MountedImageState.h"
#include "Devices/Disk/Store/ExternalChangeMonitor.h"
#include "Machines/Apple2/Common/NibbleFormat.h"
#include "SharedImageRig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SharedImageRig::SharedImageRig
//
////////////////////////////////////////////////////////////////////////////////

SharedImageRig::SharedImageRig()
{
    store.SetImageWatcher (&watcher);

    store.SetImageReader ([this] (const string & path, vector<Byte> & bytes) -> HRESULT
    {
        auto  found = files.find (path);

        reads++;

        if (found == files.end())
        {
            return E_FAIL;
        }

        bytes = found->second;

        return S_OK;
    });

    store.SetFlushSink ([this] (const string & path, const vector<Byte> & bytes) -> HRESULT
    {
        bool  isPreserved = (path != kImagePath);

        if (isPreserved)
        {
            if (refusePreserve)
            {
                return E_ACCESSDENIED;
            }

            preserved.push_back (path);
        }

        files[path] = bytes;
        Stamp (path);

        return S_OK;
    });

    store.SetIdentityReader ([this] (const string & path) -> ImageIdentity
    {
        auto  found = identities.find (path);

        return (found != identities.end()) ? found->second : ImageIdentity();
    });

    store.SetFileIo (&fileIo);

    store.SetClock ([this] () { return nowMs; });

    //  A wall clock that does not move unless a test moves it, which
    //  is what makes the same-second collision case reachable at all.
    store.SetTimestampSource ([this] () { return wallClock; });

    store.SetChangeReportSink ([this] (int, int, const ChangePrompt & prompt)
    {
        reports.push_back (prompt);
    });

    store.SetAskSink ([this] (int, int, const ChangePrompt & prompt) -> bool
    {
        questions.push_back (prompt);

        return askSinkDelivers;
    });

    //  The blocking last-chance dialog. Present from the start, the way
    //  the shell installs it, so a test that wants it declined simply
    //  leaves rescueChoice empty.
    store.SetRescueSink ([this] (const std::string &, std::string & outPath) -> bool
    {
        rescuesAsked++;

        if (rescueChoice.empty())
        {
            return false;
        }

        //  Picking somewhere else is what makes the write work: the
        //  folder this store chose for itself is the one that refused
        //  it. Without this the rig would refuse the user's choice too
        //  and the test would prove the opposite of what it says.
        refusePreserve = false;

        outPath = rescueChoice;

        return true;
    });

    store.SetMachineRestartCallback ([this] () { restarts++; });

    store.SetBayChangeSink ([this] (int, int, BayChange change)
    {
        bayChanges.push_back (change);
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  SharedImageRig::Stamp
//
//  Gives a path a fresh identity, as a real write would.
//
////////////////////////////////////////////////////////////////////////////////

void SharedImageRig::Stamp (
    const std::string  & path)
{
    ImageIdentity  identity;



    identity.recorded           = true;
    identity.stamp.sizeBytes    = files.count (path) ? files[path].size() : 0;
    identity.stamp.modifiedUnix = ++m_tick;

    identities[path] = identity;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SharedImageRig::WriteImage
//
//  Puts an image on "disk" with a distinct byte pattern.
//
////////////////////////////////////////////////////////////////////////////////

void SharedImageRig::WriteImage (
    const std::string  & path,
    Byte                 fill)
{
    files[path] = vector<Byte> (NibbleFormat::kImageByteSize, fill);
    Stamp (path);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SharedImageRig::FireAndSettle
//
//  A change arrives, settles, and the machine reaches a quiet moment.
//
//  THE INTENT IS OPTIONAL BECAUSE IT IS OPTIONAL IN LIFE. A watcher
//  reports every change; only this project's own command line also
//  says what it meant by one, and a real session that gets both gets
//  them in this order.
//
////////////////////////////////////////////////////////////////////////////////

void SharedImageRig::FireAndSettle (
    const std::string     & path,
    ExternalChangeIntent    intent,
    bool                    fromCassoExplorer)
{
    watcher.Fire (kDirectory, path);

    if (intent != ExternalChangeIntent::Unstated)
    {
        store.GetChangeMonitor().NoteExternalChange (path, intent, fromCassoExplorer);
    }

    nowMs += MountedImageState::kQuietPeriodMs;
    store.GetChangeMonitor().ApplyPendingReload();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SharedImageRig::FirstTrackByte
//
//  What the guest sees on a track, as a stand-in for "the disk changed".
//
////////////////////////////////////////////////////////////////////////////////

Byte SharedImageRig::FirstTrackByte (
    DiskImageStore  & store)
{
    DiskImage *  image = store.GetImage (kSlot, kDrive);



    Assert::IsNotNull (image, L"a mounted bay has an image");

    return image->GetTrackBits (0).empty() ? 0 : image->GetTrackBits (0)[0];
}





////////////////////////////////////////////////////////////////////////////////
//
//  SharedImageRig::FileIo::Exists
//
////////////////////////////////////////////////////////////////////////////////

bool SharedImageRig::FileIo::Exists (
    const std::string  & path)
{
    return rig.files.find (path) != rig.files.end();
}





