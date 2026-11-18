#include "Pch.h"

#include "../EhmTestHelper.h"
#include "DiskCheck/GivenNames.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  GivenNamesTests
//
//  The user's name for each Full Send image, by fingerprint, in given.json
//  in a folder of the test's in the temporary folder.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (GivenNamesTests)
{
public:

    TEST_METHOD (TheGivenPart_IsTheNameAfterTheNumber)
    {
        Assert::AreEqual (std::string ("Archon"),          GivenNames::GetGivenPart ("0024 Archon.woz"));
        Assert::AreEqual (std::string ("Archon II Adept"), GivenNames::GetGivenPart ("0024 Archon II Adept.dsk"));
        Assert::AreEqual (std::string ("Archon"),          GivenNames::GetGivenPart ("Archon.woz"), L"No number");
        Assert::IsTrue (GivenNames::GetGivenPart ("0024.woz").empty(), L"A number alone");
        Assert::AreEqual (std::string ("0024Archon"),      GivenNames::GetGivenPart ("0024Archon.woz"), L"No space after the number");
    }

    //  A name set for a fingerprint is found by another instance over the
    //  same folder, a second name replaces it, and an empty one removes it.
    TEST_METHOD (AName_IsStoredByFingerprint_InTheFile)
    {
        std::filesystem::path  root = std::filesystem::temp_directory_path() / std::format (L"CassoGiven{}", ::GetCurrentProcessId());
        std::string            name;



        std::filesystem::remove_all (root);

        {
            GivenNames  given (root);

            Assert::IsFalse (given.TryGet ("abc", name), L"No file");
            AssertSucceeded (given.Set ("abc", "Archon"));
            AssertSucceeded (given.Set ("def", "One-on-One"));
        }

        Assert::IsTrue (std::filesystem::exists (root / GivenNames::kFileName));

        {
            GivenNames  given (root);

            Assert::IsTrue (given.TryGet ("abc", name));
            Assert::AreEqual (std::string ("Archon"), name);

            AssertSucceeded (given.Set ("abc", "Archon (side A)"));
            Assert::IsTrue (given.TryGet ("abc", name));
            Assert::AreEqual (std::string ("Archon (side A)"), name);

            AssertSucceeded (given.Set ("def", ""));
            Assert::IsFalse (given.TryGet ("def", name), L"An empty name removes it");
            Assert::IsFalse (given.TryGet ("", name));
        }

        std::filesystem::remove_all (root);
    }
};
