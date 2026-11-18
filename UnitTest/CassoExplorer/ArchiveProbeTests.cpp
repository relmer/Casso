#include "Pch.h"
#include "CassoExplorer/Model/ArchiveProbe.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ArchiveProbeTests
//
//  Zips built byte by byte: a local header and its directory entry for each
//  file, then the end record.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ArchiveProbeTests)
{
public:

    static void  Put16 (std::vector<Byte> & out, uint32_t value)
    {
        out.push_back ((Byte) (value & 0xFF));
        out.push_back ((Byte) ((value >> 8) & 0xFF));
    }


    static void  Put32 (std::vector<Byte> & out, uint32_t value)
    {
        Put16 (out, value & 0xFFFF);
        Put16 (out, value >> 16);
    }


    //  A zip with one file, "A", for each flag given, the flag its
    //  general-purpose bits.
    static std::vector<Byte>  MakeZip (std::vector<uint16_t> flags)
    {
        std::vector<Byte>    zip;
        std::vector<Byte>    directory;
        std::vector<size_t>  offsets;
        uint32_t             size      = 0;

        for (uint16_t flag : flags)
        {
            offsets.push_back (zip.size());
            Put32 (zip, 0x04034B50);
            Put16 (zip, 20);
            Put16 (zip, flag);
            zip.insert (zip.end(), 22, 0);
            Put16 (zip, 1);
            Put16 (zip, 0);
            zip.push_back ('A');
        }

        for (size_t index = 0; index < flags.size(); index++)
        {
            Put32 (directory, 0x02014B50);
            Put16 (directory, 20);
            Put16 (directory, 20);
            Put16 (directory, flags[index]);
            directory.insert (directory.end(), 18, 0);
            Put16 (directory, 1);
            Put16 (directory, 0);
            Put16 (directory, 0);
            directory.insert (directory.end(), 8, 0);
            Put32 (directory, (uint32_t) offsets[index]);
            directory.push_back ('A');
        }

        size = (uint32_t) directory.size();

        Put32 (directory, 0x06054B50);
        Put16 (directory, 0);
        Put16 (directory, 0);
        Put16 (directory, (uint32_t) flags.size());
        Put16 (directory, (uint32_t) flags.size());
        Put32 (directory, size);
        Put32 (directory, (uint32_t) zip.size());
        Put16 (directory, 0);

        zip.insert (zip.end(), directory.begin(), directory.end());

        return zip;
    }


    TEST_METHOD (Zip_IsKnownByItsDirectory)
    {
        std::vector<Byte>  empty   = MakeZip ({});
        std::vector<Byte>  plain   = MakeZip ({ 0, 0 });
        std::vector<Byte>  locked  = MakeZip ({ 0, 1 });
        std::vector<Byte>  cut     = plain;
        std::vector<Byte>  other   = { 'M', 'S', 'C', 'F', 0, 0, 0, 0 };

        cut.resize (cut.size() / 2);

        Assert::IsTrue (ArchiveProbe::ProbeZip (empty)  == ArchiveProbe::Verdict::Empty,             L"The end record alone");
        Assert::IsTrue (ArchiveProbe::ProbeZip (plain)  == ArchiveProbe::Verdict::HasEntries);
        Assert::IsTrue (ArchiveProbe::ProbeZip (locked) == ArchiveProbe::Verdict::PasswordProtected, L"One encrypted entry locks it");
        Assert::IsTrue (ArchiveProbe::ProbeZip (cut)    == ArchiveProbe::Verdict::Damaged,           L"Cut short, as a failed download is");
        Assert::IsTrue (ArchiveProbe::ProbeZip (other)  == ArchiveProbe::Verdict::NotZip);
    }


    TEST_METHOD (Describe_SaysWhyByName)
    {
        Assert::AreEqual (std::wstring (L"a.zip can't be opened because it is damaged."),
                          ArchiveProbe::GetReason (L"a.zip", ArchiveProbe::Verdict::Damaged));
        Assert::AreEqual (std::wstring (L"a.zip can't be opened because it is protected by a password."),
                          ArchiveProbe::GetReason (L"a.zip", ArchiveProbe::Verdict::PasswordProtected));
        Assert::IsTrue   (ArchiveProbe::GetReason (L"a.zip", ArchiveProbe::Verdict::Empty).empty(), L"An empty zip is an empty folder");
    }
};
