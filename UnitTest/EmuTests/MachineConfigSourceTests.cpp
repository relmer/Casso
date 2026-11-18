#include "Pch.h"

#include "Core/MachineConfigSource.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  MachineConfigSourceTests
//
//  Which text a session uses for a machine: the file on disk, or the config
//  embedded in the build when the file was written by a newer build. The choice
//  is pure, so these tests pass text and read no file.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (MachineConfigSourceTests)
{
public:

    static constexpr const char * kpszEmbedded = "{ \"$cassoMachineVersion\": 5, \"name\": \"embedded\" }";



    ////////////////////////////////////////////////////////////////////////////
    //
    //  A lower or equal stamp keeps the file
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (ChooseText_LowerDiskStamp_UsesDiskText)
    {
        std::string          disk   = "{ \"$cassoMachineVersion\": 4, \"name\": \"disk\" }";
        MachineConfigChoice  choice = MachineConfigSource::ChooseText (disk, kpszEmbedded);



        Assert::IsFalse  (choice.useEmbedded);
        Assert::AreEqual (disk, choice.text);
        Assert::AreEqual (4,    choice.diskStamp);
        Assert::AreEqual (5,    choice.embeddedStamp);
    }


    TEST_METHOD (ChooseText_EqualStamp_UsesDiskText)
    {
        std::string          disk   = "{ \"$cassoMachineVersion\": 5, \"name\": \"edited\" }";
        MachineConfigChoice  choice = MachineConfigSource::ChooseText (disk, kpszEmbedded);



        Assert::IsFalse  (choice.useEmbedded);
        Assert::AreEqual (disk, choice.text);
    }


    TEST_METHOD (ChooseText_UnstampedDiskText_UsesDiskText)
    {
        std::string          disk   = "{ \"name\": \"unstamped\" }";
        MachineConfigChoice  choice = MachineConfigSource::ChooseText (disk, kpszEmbedded);



        Assert::IsFalse  (choice.useEmbedded);
        Assert::AreEqual (disk, choice.text);
        Assert::AreEqual (0,    choice.diskStamp);
    }


    TEST_METHOD (ChooseText_UnparseableDiskText_UsesDiskText)
    {
        std::string          disk   = "{ not json";
        MachineConfigChoice  choice = MachineConfigSource::ChooseText (disk, kpszEmbedded);



        Assert::IsFalse  (choice.useEmbedded);
        Assert::AreEqual (disk, choice.text);
    }





    ////////////////////////////////////////////////////////////////////////////
    //
    //  A higher stamp selects the embedded text
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (ChooseText_HigherDiskStamp_UsesEmbeddedText)
    {
        std::string          disk   = "{ \"$cassoMachineVersion\": 6, \"name\": \"newer\" }";
        MachineConfigChoice  choice = MachineConfigSource::ChooseText (disk, kpszEmbedded);



        Assert::IsTrue   (choice.useEmbedded);
        Assert::AreEqual (std::string (kpszEmbedded), choice.text);
        Assert::AreEqual (6, choice.diskStamp);
        Assert::AreEqual (5, choice.embeddedStamp);
    }


    TEST_METHOD (ChooseText_HigherLegacyStamp_UsesEmbeddedText)
    {
        std::string          disk   = "{ \"$cassoDefault\": 9, \"name\": \"legacy\" }";
        MachineConfigChoice  choice = MachineConfigSource::ChooseText (disk, kpszEmbedded);



        Assert::IsTrue   (choice.useEmbedded);
        Assert::AreEqual (std::string (kpszEmbedded), choice.text);
        Assert::AreEqual (9, choice.diskStamp);
    }


    TEST_METHOD (ChooseText_NoEmbeddedText_UsesDiskText)
    {
        std::string          disk   = "{ \"$cassoMachineVersion\": 6, \"name\": \"newer\" }";
        MachineConfigChoice  choice = MachineConfigSource::ChooseText (disk, std::string());



        Assert::IsFalse  (choice.useEmbedded);
        Assert::AreEqual (disk, choice.text);
    }
};
