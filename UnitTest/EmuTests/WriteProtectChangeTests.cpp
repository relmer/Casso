#include "Pch.h"

#include "Devices/Disk/WriteProtectChange.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  WriteProtectChangeTests
//
//  Which mechanism the Disk menu's write-protect command changes, the label
//  it shows, and the notice it reports.
//
////////////////////////////////////////////////////////////////////////////////

namespace WriteProtectChangeTests
{
    TEST_CLASS (WriteProtectChangeTests)
    {
    public:

        TEST_METHOD (MakePlan_unprotectedWoz_setsFlagOnly)
        {
            WriteProtectChange  plan = WriteProtectChange::MakePlan (true, false, false, L"a.woz");

            Assert::IsTrue  (plan.protecting);
            Assert::IsTrue  (plan.changesImageFlag, L"a WOZ is protected by its own flag");
            Assert::IsFalse (plan.changesAttribute, L"and not also marked read-only");
        }



        TEST_METHOD (MakePlan_unprotectedDsk_setsAttributeOnly)
        {
            WriteProtectChange  plan = WriteProtectChange::MakePlan (false, false, false, L"a.dsk");

            Assert::IsTrue  (plan.protecting);
            Assert::IsTrue  (plan.changesAttribute, L"a DSK has no flag, so the attribute stands in");
            Assert::IsFalse (plan.changesImageFlag);
        }



        TEST_METHOD (MakePlan_readOnlyDsk_clearsAttribute)
        {
            WriteProtectChange  plan = WriteProtectChange::MakePlan (false, false, true, L"a.dsk");

            Assert::IsFalse (plan.protecting, L"the command write-enables what it write-protected");
            Assert::IsTrue  (plan.changesAttribute);
            Assert::IsFalse (plan.changesImageFlag);
        }



        TEST_METHOD (MakePlan_flaggedWoz_clearsFlagOnly)
        {
            WriteProtectChange  plan = WriteProtectChange::MakePlan (true, true, false, L"a.woz");

            Assert::IsFalse (plan.protecting);
            Assert::IsTrue  (plan.changesImageFlag);
            Assert::IsFalse (plan.changesAttribute, L"an attribute that is not set is left alone");
        }



        TEST_METHOD (MakePlan_readOnlyWoz_clearsAttributeOnly)
        {
            WriteProtectChange  plan = WriteProtectChange::MakePlan (true, false, true, L"a.woz");

            Assert::IsFalse (plan.protecting, L"a read-only WOZ is protected even with its flag clear");
            Assert::IsTrue  (plan.changesAttribute);
            Assert::IsFalse (plan.changesImageFlag);
        }



        TEST_METHOD (MakePlan_wozWithBoth_clearsBoth)
        {
            WriteProtectChange  plan = WriteProtectChange::MakePlan (true, true, true, L"a.woz");

            Assert::IsFalse (plan.protecting);
            Assert::IsTrue  (plan.changesImageFlag, L"leaving either set leaves the disk protected");
            Assert::IsTrue  (plan.changesAttribute);
        }



        TEST_METHOD (IsImageProtected_countsOnlyWhatTheCommandChanges)
        {
            WriteProtectInfo  wp;

            Assert::IsFalse (WriteProtectChange::IsImageProtected (wp));

            wp.userSetting      = true;
            wp.noPermission     = true;
            wp.checksumMismatch = true;

            Assert::IsFalse (WriteProtectChange::IsImageProtected (wp),
                L"the drive preference, a permission denial and damage do not flip the label");

            wp = WriteProtectInfo();
            wp.imageFlag = true;
            Assert::IsTrue (WriteProtectChange::IsImageProtected (wp));

            wp = WriteProtectInfo();
            wp.readOnlyFile = true;
            Assert::IsTrue (WriteProtectChange::IsImageProtected (wp));
        }



        TEST_METHOD (GetMenuLabel_givesTheActionAndFile)
        {
            Assert::AreEqual (wstring (L"Write-protect \"a.dsk\""), WriteProtectChange::GetMenuLabel (false, L"a.dsk"));
            Assert::AreEqual (wstring (L"Write-enable \"a.dsk\""),  WriteProtectChange::GetMenuLabel (true,  L"a.dsk"));
        }



        TEST_METHOD (GetMenuLabel_longNameShortenedInTheMiddle)
        {
            wstring  label = WriteProtectChange::GetMenuLabel (false, L"Karateka (1984)(Broderbund).woz");

            Assert::AreEqual (wstring (L"Write-protect \"Karateka (...nd).woz\""), label);
        }



        TEST_METHOD (DescribeResult_everyReachablePlan)
        {
            Assert::AreEqual (wstring (L"Set WOZ write-protect flag for \"a.woz\""),
                              WriteProtectChange::DescribeResult (WriteProtectChange::MakePlan (true,  false, false, L"a.woz")));
            Assert::AreEqual (wstring (L"Set read-only attribute for \"a.dsk\""),
                              WriteProtectChange::DescribeResult (WriteProtectChange::MakePlan (false, false, false, L"a.dsk")));
            Assert::AreEqual (wstring (L"Cleared read-only attribute for \"a.dsk\""),
                              WriteProtectChange::DescribeResult (WriteProtectChange::MakePlan (false, false, true,  L"a.dsk")));
            Assert::AreEqual (wstring (L"Cleared WOZ write-protect flag for \"a.woz\""),
                              WriteProtectChange::DescribeResult (WriteProtectChange::MakePlan (true,  true,  false, L"a.woz")));
            Assert::AreEqual (wstring (L"Cleared read-only attribute for \"a.woz\""),
                              WriteProtectChange::DescribeResult (WriteProtectChange::MakePlan (true,  false, true,  L"a.woz")));
            Assert::AreEqual (wstring (L"Cleared read-only attribute and WOZ write-protect flag for \"a.woz\""),
                              WriteProtectChange::DescribeResult (WriteProtectChange::MakePlan (true,  true,  true,  L"a.woz")));
        }
    };
}
