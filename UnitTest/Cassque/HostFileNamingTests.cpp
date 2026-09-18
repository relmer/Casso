#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Cassque/Model/HostFileNaming.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  HostFileNamingTests
//
//  Every row of the naming table in both styles, and the way back through
//  Parse for each of them.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (HostFileNamingTests)
{
public:

    using Style = HostFileNaming::Style;
    using Kind  = ParsedHostName::ConvertedKind;



    TEST_METHOD (Converted_ListingsAndText)
    {
        Assert::AreEqual (std::wstring (L"HELLO.Applesoft BASIC.txt"), HostFileNaming::ForConverted ("HELLO", Kind::ApplesoftListing));
        Assert::AreEqual (std::wstring (L"INTPROG.Integer BASIC.txt"), HostFileNaming::ForConverted ("INTPROG", Kind::IntegerListing));
        Assert::AreEqual (std::wstring (L"NOTES.Text.txt"),            HostFileNaming::ForConverted ("NOTES", Kind::Text));
    }



    TEST_METHOD (Raw_BinaryOnEitherFileSystem)
    {
        Assert::AreEqual (std::wstring (L"PIC.Binary.$2000.bin"),
                          HostFileNaming::ForRaw ("PIC", VolumeKind::ProDos, ProDosVolume::kTypeBinary, true, 0x2000, Style::Descriptive));
        Assert::AreEqual (std::wstring (L"PIC.Binary.$2000.bin"),
                          HostFileNaming::ForRaw ("PIC", VolumeKind::Dos33, Dos33Volume::kTypeBinary, true, 0x2000, Style::Descriptive));
        Assert::AreEqual (std::wstring (L"PIC#062000"),
                          HostFileNaming::ForRaw ("PIC", VolumeKind::ProDos, ProDosVolume::kTypeBinary, true, 0x2000, Style::CiderPress));
        Assert::AreEqual (std::wstring (L"PIC#062000"),
                          HostFileNaming::ForRaw ("PIC", VolumeKind::Dos33, Dos33Volume::kTypeBinary, true, 0x2000, Style::CiderPress));
    }



    TEST_METHOD (Raw_OtherProDosTypes)
    {
        //  A text file with a zero aux type shows no address; a system file
        //  with a nonzero one does.
        Assert::AreEqual (std::wstring (L"NOTES.ProDOS.$04.bin"),
                          HostFileNaming::ForRaw ("NOTES", VolumeKind::ProDos, ProDosVolume::kTypeText, true, 0, Style::Descriptive));
        Assert::AreEqual (std::wstring (L"START.ProDOS.$FF.$2000.bin"),
                          HostFileNaming::ForRaw ("START", VolumeKind::ProDos, ProDosVolume::kTypeSystem, true, 0x2000, Style::Descriptive));
        Assert::AreEqual (std::wstring (L"NOTES#040000"),
                          HostFileNaming::ForRaw ("NOTES", VolumeKind::ProDos, ProDosVolume::kTypeText, true, 0, Style::CiderPress));
        Assert::AreEqual (std::wstring (L"START#FF2000"),
                          HostFileNaming::ForRaw ("START", VolumeKind::ProDos, ProDosVolume::kTypeSystem, true, 0x2000, Style::CiderPress));
    }



    TEST_METHOD (Raw_OtherDos33Types)
    {
        Assert::AreEqual (std::wstring (L"HELLO.DOS.A.bin"),
                          HostFileNaming::ForRaw ("HELLO", VolumeKind::Dos33, Dos33Volume::kTypeApplesoft, false, 0, Style::Descriptive));
        Assert::AreEqual (std::wstring (L"INTPROG.DOS.I.bin"),
                          HostFileNaming::ForRaw ("INTPROG", VolumeKind::Dos33, Dos33Volume::kTypeInteger, false, 0, Style::Descriptive));
        Assert::AreEqual (std::wstring (L"NOTES.DOS.T.bin"),
                          HostFileNaming::ForRaw ("NOTES", VolumeKind::Dos33, Dos33Volume::kTypeText, false, 0, Style::Descriptive));

        //  The CiderPress form maps the letter onto ProDOS's type.
        Assert::AreEqual (std::wstring (L"HELLO#FC0000"),
                          HostFileNaming::ForRaw ("HELLO", VolumeKind::Dos33, Dos33Volume::kTypeApplesoft, false, 0, Style::CiderPress));
        Assert::AreEqual (std::wstring (L"INTPROG#FA0000"),
                          HostFileNaming::ForRaw ("INTPROG", VolumeKind::Dos33, Dos33Volume::kTypeInteger, false, 0, Style::CiderPress));
    }



    TEST_METHOD (Parse_RoundTripsEveryForm)
    {
        ParsedHostName  parsed;

        Assert::IsTrue   (HostFileNaming::Parse (L"HELLO.Applesoft BASIC.txt", parsed));
        Assert::AreEqual (std::string ("HELLO"), parsed.catalogName);
        Assert::IsTrue   (parsed.converted == Kind::ApplesoftListing);
        Assert::IsFalse  (parsed.hasType);

        Assert::IsTrue   (HostFileNaming::Parse (L"INTPROG.integer basic.TXT", parsed));
        Assert::IsTrue   (parsed.converted == Kind::IntegerListing);

        Assert::IsTrue   (HostFileNaming::Parse (L"NOTES.Text.txt", parsed));
        Assert::IsTrue   (parsed.converted == Kind::Text);

        Assert::IsTrue   (HostFileNaming::Parse (L"PIC.Binary.$2000.bin", parsed));
        Assert::AreEqual (std::string ("PIC"), parsed.catalogName);
        Assert::IsTrue   (parsed.hasType && parsed.type == ProDosVolume::kTypeBinary);
        Assert::IsTrue   (parsed.hasAux && parsed.aux == 0x2000);

        Assert::IsTrue   (HostFileNaming::Parse (L"START.ProDOS.$FF.$2000.bin", parsed));
        Assert::AreEqual (std::string ("START"), parsed.catalogName);
        Assert::IsTrue   (parsed.hasType && parsed.type == ProDosVolume::kTypeSystem);
        Assert::IsTrue   (parsed.hasAux && parsed.aux == 0x2000);

        Assert::IsTrue   (HostFileNaming::Parse (L"NOTES.ProDOS.$04.bin", parsed));
        Assert::IsTrue   (parsed.hasType && parsed.type == ProDosVolume::kTypeText);
        Assert::IsTrue   (parsed.hasAux && parsed.aux == 0);

        Assert::IsTrue   (HostFileNaming::Parse (L"HELLO.DOS.A.bin", parsed));
        Assert::AreEqual (std::string ("HELLO"), parsed.catalogName);
        Assert::IsTrue   (parsed.hasType && parsed.type == ProDosVolume::kTypeBasic);
        Assert::IsFalse  (parsed.hasAux);
    }



    TEST_METHOD (Parse_CiderPressAndBareNames)
    {
        ParsedHostName  parsed;

        Assert::IsTrue   (HostFileNaming::Parse (L"PIC#062000", parsed));
        Assert::AreEqual (std::string ("PIC"), parsed.catalogName);
        Assert::IsTrue   (parsed.hasType && parsed.type == 0x06);
        Assert::IsTrue   (parsed.hasAux && parsed.aux == 0x2000);

        //  A hash without six hex digits is part of the name.
        Assert::IsFalse  (HostFileNaming::Parse (L"NOTE#1", parsed));
        Assert::AreEqual (std::string ("NOTE#1"), parsed.catalogName);

        Assert::IsFalse  (HostFileNaming::Parse (L"readme.txt", parsed));
        Assert::AreEqual (std::string ("readme"), parsed.catalogName);
        Assert::IsFalse  (parsed.hasType);
        Assert::IsTrue   (parsed.converted == Kind::None);

        Assert::IsFalse  (HostFileNaming::Parse (L"PROGRAM", parsed));
        Assert::AreEqual (std::string ("PROGRAM"), parsed.catalogName);
    }



    TEST_METHOD (MakeHostLegal_SubstitutesAndReports)
    {
        bool          substituted = false;
        std::wstring  legal       = HostFileNaming::MakeHostLegal ("A/B:C?", substituted);

        Assert::AreEqual (std::wstring (L"A_B_C_"), legal);
        Assert::IsTrue   (substituted);

        legal = HostFileNaming::MakeHostLegal ("PLAIN.NAME", substituted);

        Assert::AreEqual (std::wstring (L"PLAIN.NAME"), legal);
        Assert::IsFalse  (substituted);
    }



    TEST_METHOD (Dos33TypeTable_MapsBothWays)
    {
        Byte  type = 0;

        Assert::AreEqual ('A', HostFileNaming::GetDos33TypeLetter (Dos33Volume::kTypeApplesoft));
        Assert::AreEqual ('B', HostFileNaming::GetDos33TypeLetter (0x40));
        Assert::AreEqual ('?', HostFileNaming::GetDos33TypeLetter (0x7F));

        Assert::IsTrue   (HostFileNaming::TryGetDos33TypeByte ('t', type));
        Assert::IsTrue   (type == Dos33Volume::kTypeText);
        Assert::IsFalse  (HostFileNaming::TryGetDos33TypeByte ('Z', type));

        Assert::IsTrue   (HostFileNaming::TryMapProDosToDos33 (ProDosVolume::kTypeBasic, type));
        Assert::IsTrue   (type == Dos33Volume::kTypeApplesoft);
        Assert::IsFalse  (HostFileNaming::TryMapProDosToDos33 (ProDosVolume::kTypeSystem, type));
    }
};
