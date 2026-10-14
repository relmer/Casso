#include "Pch.h"

#include "GuestSession.h"
#include "TestMachine.h"
#include "Machines/Apple2/Common/IDisk2EventSink.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace Disk2SectorEventTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SectorCounter
    //
    //  The sector events of a boot, counted by kind, and the bytes of the
    //  boot sector as the first read of T0 S0 decoded them.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class SectorCounter : public IDisk2EventSink
    {
    public:
        size_t             reads          = 0;
        size_t             addressErrors  = 0;
        size_t             dataErrors     = 0;
        size_t             cutOffs        = 0;
        size_t             orphans        = 0;
        size_t             unknownSectors = 0;
        std::vector<Byte>  bootSector;

        void OnMotorCommandOn() override                     {}
        void OnMotorEngaged() override                       {}
        void OnMotorCommandOff() override                    {}
        void OnMotorDisengaged() override                    {}
        void OnHeadStep        (int, int) override           {}
        void OnHeadBump        (int) override                {}
        void OnAddressMark     (int, int, int) override      {}
        void OnDataMarkWrite   (int, int, int, std::span<const Byte>) override {}
        void OnDriveSelect     (int) override                {}
        void OnDiskInserted    (int) override                {}
        void OnDiskEjected     (int) override                {}

        void OnDataMarkRead (int track, int sector, int, std::span<const Byte> bytes) override
        {
            reads++;
            unknownSectors += (sector < 0) ? 1 : 0;

            if (track == 0 && sector == 0 && bootSector.empty())
            {
                bootSector.assign (bytes.begin(), bytes.end());
            }
        }

        void OnAddressChecksumError (int, int, int) override                         { addressErrors++; }
        void OnDataChecksumError    (int, int, int, std::span<const Byte>) override  { dataErrors++; }
        void OnDataFieldCutOff      (int, int, int) override                         { cutOffs++; }
        void OnDataWithoutAddress   (std::span<const Byte>) override                 { orphans++; }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  Disk2SectorEventTests
    //
    //  A DOS 3.3 boot on the real master: every sector read decodes with a
    //  good checksum, the boot sector's bytes are the disk's, and a disk with
    //  nothing wrong with it raises no error or warning.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (Disk2SectorEventTests)
    {
    public:

        TEST_METHOD (ADos33BootReadsGoodSectorsWithNoErrors)
        {
            TestMachine        machine ("Apple2eEnhanced");
            SectorCounter      counter;
            std::vector<Byte>  master  = GuestSession::RequireDos33Master();



            machine.GetDiskStore().SetFlushSink ([] (const std::string &, const std::vector<Byte> &) { return S_OK; });
            machine.AttachObservers ({ &counter, nullptr }, ObserverSlot::History);

            GuestSession::BootToPrompt (machine, master);

            machine.AttachObservers ({}, ObserverSlot::History);

            Logger::WriteMessage (std::format ("{} sector reads, {} with the sector unknown\n", counter.reads, counter.unknownSectors).c_str());

            Assert::IsTrue   (counter.reads > 100, L"the boot read DOS");
            Assert::AreEqual ((size_t) 0, counter.addressErrors, L"no address field fails its checksum");
            Assert::AreEqual ((size_t) 0, counter.dataErrors,    L"no data field fails its checksum");
            Assert::AreEqual ((size_t) 0, counter.cutOffs,       L"no data field is cut off");
            Assert::AreEqual ((size_t) 0, counter.orphans,       L"every data field follows its address field");
            Assert::IsTrue   (std::equal (counter.bootSector.begin(), counter.bootSector.end(), master.begin()) && counter.bootSector.size() == 256,
                              L"T0 S0 decodes to the image's first 256 bytes");
        }
    };
}
