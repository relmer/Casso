#include "Pch.h"

#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Shell/InspectorStatusPublisher.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorStatusPublisherTests
//
//  Each drive's published state after a frame matches the controller: the
//  turning drive and its head, what the head is doing, and the guest's write
//  and visit counts, with a generation that steps only when a count does.
//  Nothing is published while no window is shown.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (InspectorStatusPublisherTests)
{
public:

    static constexpr Word    kMotorOn  = 0xC0E9;
    static constexpr Word    kPhase1On = 0xC0E3;
    static constexpr Word    kQ6High   = 0xC0ED;
    static constexpr Word    kQ7High   = 0xC0EF;
    static constexpr size_t  kDskBytes = 143360;

    class FakeSource : public IInspectorDiskSource
    {
    public:
        DiskImage  image;

        const DiskImage * GetImage (int drive, InspectorDiskIdentity & outIdentity) override
        {
            outIdentity.mediaId = (drive == 0) ? 7 : 0;

            return (drive == 0) ? &image : nullptr;
        }
    };



    static void Setup (FakeSource & source, Disk2Controller & ctrl)
    {
        source.image.LoadFromBytes (DiskFormat::Dsk, vector<Byte> (kDskBytes, 0), "blank.dsk");
        ctrl.SetExternalDisk (0, &source.image);
        (void) ctrl.Read (kMotorOn);
        (void) ctrl.Read (kPhase1On);
    }



    TEST_METHOD (NothingIsPublishedWhileNoWindowIsShown)
    {
        auto                      source = std::make_unique<FakeSource>();
        auto                      ctrl   = std::make_unique<Disk2Controller> (6);
        InspectorStatusPublisher  publisher;



        Setup (*source, *ctrl);
        publisher.Publish (ctrl.get(), *source);

        Assert::IsNull (publisher.Get (0).get());
    }



    TEST_METHOD (TheTurningDriveShowsItsHeadAndWrites)
    {
        auto                      source = std::make_unique<FakeSource>();
        auto                      ctrl   = std::make_unique<Disk2Controller> (6);
        InspectorStatusPublisher  publisher;
        int                       slot   = 0;



        Setup (*source, *ctrl);
        slot = source->image.ResolveQuarterTrack (2);
        (void) ctrl->Read (kQ7High);
        ctrl->Write (kQ6High, 0xFF);
        ctrl->Write (kQ6High, 0xFF);

        publisher.SetActive (true);
        publisher.Publish (ctrl.get(), *source);

        auto  a = publisher.Get (0);
        auto  b = publisher.Get (1);

        Assert::IsNotNull (a.get());
        Assert::IsTrue  (a->hasDisk && a->isTurning && a->isEnabled);
        Assert::AreEqual (7ull, a->mediaId);
        Assert::AreEqual (2, a->headQuarterTrack);
        Assert::AreEqual (slot, a->trackRecord);
        Assert::IsTrue  (a->activity == HeadActivity::Writing);
        Assert::AreEqual (uint64_t (2), a->recordWriteCounts[slot]);
        Assert::AreEqual (1u, a->visitCounts[2]);

        Assert::IsFalse (b->hasDisk || b->isEnabled || b->isTurning);
        Assert::IsTrue  (b->activity == HeadActivity::Idle);
    }



    TEST_METHOD (ADroppedWriteShowsAsBlocked)
    {
        auto                      source = std::make_unique<FakeSource>();
        auto                      ctrl   = std::make_unique<Disk2Controller> (6);
        InspectorStatusPublisher  publisher;



        Setup (*source, *ctrl);
        source->image.SetUserWriteProtected (true);
        (void) ctrl->Read (kQ7High);
        ctrl->Write (kQ6High, 0xFF);

        publisher.SetActive (true);
        publisher.Publish (ctrl.get(), *source);

        Assert::IsTrue (publisher.Get (0)->activity == HeadActivity::WriteBlocked);
        Assert::IsTrue (publisher.Get (0)->writeProtect.userSetting);
    }



    TEST_METHOD (TheCountsGenerationStepsOnlyWhenACountChanges)
    {
        auto                      source = std::make_unique<FakeSource>();
        auto                      ctrl   = std::make_unique<Disk2Controller> (6);
        InspectorStatusPublisher  publisher;
        uint64_t                  first  = 0;



        Setup (*source, *ctrl);
        publisher.SetActive (true);
        publisher.Publish (ctrl.get(), *source);
        first = publisher.Get (0)->countsGeneration;

        publisher.Publish (ctrl.get(), *source);
        Assert::AreEqual (first, publisher.Get (0)->countsGeneration, L"nothing changed");

        (void) ctrl->Read (kQ7High);
        ctrl->Write (kQ6High, 0xFF);
        publisher.Publish (ctrl.get(), *source);
        Assert::AreEqual (first + 1, publisher.Get (0)->countsGeneration, L"a write");
    }



    //  A save and a change to the image's write-protect flag step the
    //  generation too, so the Image tab is rebuilt after either (FR-071).
    TEST_METHOD (ASaveAndAWriteProtectChangeStepTheCountsGeneration)
    {
        auto                      source = std::make_unique<FakeSource>();
        auto                      ctrl   = std::make_unique<Disk2Controller> (6);
        InspectorStatusPublisher  publisher;
        uint64_t                  first  = 0;



        Setup (*source, *ctrl);
        publisher.SetActive (true);
        publisher.Publish (ctrl.get(), *source);
        first = publisher.Get (0)->countsGeneration;

        source->image.NoteSaved();
        publisher.Publish (ctrl.get(), *source);
        Assert::AreEqual (first + 1, publisher.Get (0)->countsGeneration, L"a save");
        Assert::AreEqual (static_cast<uint64_t> (1), publisher.Get (0)->saveCount);

        source->image.SetImageWriteProtected (true);
        publisher.Publish (ctrl.get(), *source);
        Assert::AreEqual (first + 2, publisher.Get (0)->countsGeneration, L"the flag");
    }
};
