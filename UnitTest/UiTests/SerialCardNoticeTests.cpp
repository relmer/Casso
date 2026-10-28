#include "Pch.h"

#include "Core/MachineConfig.h"
#include "Shell/SerialCardNotice.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;




namespace SerialCardNoticeTestNs
{
    ////////////////////////////////////////////////////////////////////////////
    //
    //  SerialCardNoticeTests
    //
    //  What the Super Serial Card's infobar reports for each firmware and
    //  attachment state, and which buttons it offers.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (SerialCardNoticeTests)
    {
    public:
        TEST_METHOD (NoEnabledCardReportsNothing)
        {
            MachineConfig     config = MakeConfig (false, SlotFirmwareStatus::Missing);
            SerialCardNotice  notice = SerialCardNotice::MakeForMachine (config, false, "");



            Assert::IsTrue (notice.kind == SerialCardNoticeKind::None);
        }


        TEST_METHOD (MissingFirmwareGivesTheSlotAndPathAndOffersDownload)
        {
            MachineConfig     config = MakeConfig (true, SlotFirmwareStatus::Missing);
            SerialCardNotice  notice = SerialCardNotice::MakeForMachine (config, false, "");



            Assert::IsTrue (notice.kind == SerialCardNoticeKind::FirmwareMissing);
            Assert::IsTrue (notice.text.find (L"slot 2") != wstring::npos);
            Assert::IsTrue (notice.text.find (L"Devices/SuperSerialCard/SSC.rom") != wstring::npos);
            Assert::AreEqual (wstring (SerialCardNotice::kpszDownload), notice.actions.front());
        }


        TEST_METHOD (WrongSizeFirmwareIsInvalid)
        {
            MachineConfig     config = MakeConfig (true, SlotFirmwareStatus::WrongSize);
            SerialCardNotice  notice = SerialCardNotice::MakeForMachine (config, false, "");



            Assert::IsTrue (notice.kind == SerialCardNoticeKind::FirmwareInvalid);
            Assert::AreEqual (wstring (SerialCardNotice::kpszDownload), notice.actions.front());
        }


        TEST_METHOD (PresentFirmwareThatDidNotLoadIsInvalid)
        {
            MachineConfig     config = MakeConfig (true, SlotFirmwareStatus::Present);
            SerialCardNotice  notice = SerialCardNotice::MakeForMachine (config, false, "");



            Assert::IsTrue (notice.kind == SerialCardNoticeKind::FirmwareInvalid);
        }


        TEST_METHOD (LoadedFirmwareReportsNothing)
        {
            MachineConfig     config = MakeConfig (true, SlotFirmwareStatus::Present);
            SerialCardNotice  notice = SerialCardNotice::MakeForMachine (config, true, "");



            Assert::IsTrue (notice.kind == SerialCardNoticeKind::None);
        }


        TEST_METHOD (AttachmentProblemIsReportedWhenFirmwareIsFine)
        {
            MachineConfig     config  = MakeConfig (true, SlotFirmwareStatus::Present);
            SerialCardNotice  fine    = SerialCardNotice::MakeForMachine (config, true, "Cannot open C:\\x.txt.");
            MachineConfig     missing = MakeConfig (true, SlotFirmwareStatus::Missing);
            SerialCardNotice  both    = SerialCardNotice::MakeForMachine (missing, false, "Cannot open C:\\x.txt.");



            Assert::IsTrue (fine.kind == SerialCardNoticeKind::AttachmentProblem);
            Assert::AreEqual (wstring (L"Cannot open C:\\x.txt."), fine.text);
            Assert::IsTrue (both.kind == SerialCardNoticeKind::FirmwareMissing, L"firmware first");
        }


        TEST_METHOD (DownloadOutcomesOfferRestartOrRetry)
        {
            SerialCardNotice  installed = SerialCardNotice::MakeInstalled();
            SerialCardNotice  failed    = SerialCardNotice::MakeDownloadFailed ("HTTP 404");



            Assert::AreEqual (wstring (SerialCardNotice::kpszRestart), installed.actions.front());
            Assert::AreEqual (wstring (SerialCardNotice::kpszRetry),   failed.actions.front());
            Assert::IsTrue (failed.text.find (L"HTTP 404") != wstring::npos);
        }

        TEST_METHOD (NoLinkReportsShowNothing)
        {
            Assert::IsTrue (SerialCardNoticeKind::None == SerialCardNotice::MakeForLinks ({}).kind);
        }


        TEST_METHOD (TheNewestLinkReportIsShownWithItsPort)
        {
            SerialCardNotice  notice = SerialCardNotice::MakeForLinks ({ { "Serial port 1", "COM3 was disconnected." },
                                                                         { "Serial port 2", "Port 1977 is in use by another program." } });



            Assert::IsTrue (SerialCardNoticeKind::LinkProblem == notice.kind);
            Assert::AreEqual (wstring (L"Serial port 2: Port 1977 is in use by another program."), notice.text);
            Assert::AreEqual (wstring (SerialCardNotice::kpszDismiss), notice.actions.front());
        }

    private:
        static MachineConfig MakeConfig (bool enabled, SlotFirmwareStatus status)
        {
            MachineConfig  config;
            SlotConfig     slot;



            slot.slot           = 2;
            slot.device         = kpszSuperSerialCardDevice;
            slot.enabled        = enabled;
            slot.firmware       = "SSC.rom";
            slot.firmwarePath   = "Devices/SuperSerialCard/SSC.rom";
            slot.firmwareStatus = status;
            config.slots.push_back (slot);

            return config;
        }
    };
}
