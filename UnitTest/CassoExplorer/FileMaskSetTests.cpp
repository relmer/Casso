#include "Pch.h"

#include "CassoExplorer/Picker/FileMaskSet.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FileMaskSetTests
//
//  A file dialog filter's masks against file names, by the rules of the
//  Windows file dialogs.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (FileMaskSetTests)
{
public:

    TEST_METHOD (MasksAreSeparatedBySemicolons)
    {
        FileMaskSet  masks (L"*.po;*.2mg");



        Assert::AreEqual ((size_t) 2, masks.GetMasks().size());
        Assert::IsTrue  (masks.Matches (L"system.po"));
        Assert::IsTrue  (masks.Matches (L"games.2mg"));
        Assert::IsFalse (masks.Matches (L"karateka.dsk"));
        Assert::IsFalse (masks.Matches (L"po"));
    }



    TEST_METHOD (CaseIsIgnored)
    {
        FileMaskSet  masks (L"*.PO");



        Assert::IsTrue (masks.Matches (L"System.po"));
        Assert::IsTrue (masks.Matches (L"SYSTEM.Po"));
    }



    TEST_METHOD (QuestionMarkIsOneCharacter)
    {
        FileMaskSet  masks (L"disk?.dsk");



        Assert::IsTrue  (masks.Matches (L"disk1.dsk"));
        Assert::IsFalse (masks.Matches (L"disk.dsk"));
        Assert::IsFalse (masks.Matches (L"disk12.dsk"));
    }



    TEST_METHOD (StarMatchesAnyRun)
    {
        Assert::IsTrue  (FileMaskSet::MatchesMask (L"a*b*c", L"aXXbYYc"));
        Assert::IsTrue  (FileMaskSet::MatchesMask (L"a*b*c", L"abc"));
        Assert::IsTrue  (FileMaskSet::MatchesMask (L"*.tar.gz", L"x.tar.tar.gz"));
        Assert::IsFalse (FileMaskSet::MatchesMask (L"a*b*c", L"aXXbYY"));
        Assert::IsTrue  (FileMaskSet::MatchesMask (L"*", L""));
        Assert::IsFalse (FileMaskSet::MatchesMask (L"?", L""));
    }



    TEST_METHOD (AllFilesMatchesNamesWithoutAnExtension)
    {
        for (const wchar_t * pattern : { L"*.*", L"*", L"", L" ; ", L"*.po;*.*" })
        {
            FileMaskSet  masks (pattern);



            Assert::IsTrue (masks.Matches (L"README"),   pattern);
            Assert::IsTrue (masks.Matches (L"disk.dsk"), pattern);
        }
    }



    TEST_METHOD (EmptyMasksAndSpacesAreDropped)
    {
        FileMaskSet  masks (L" *.po ;; *.hdv ;");



        Assert::AreEqual ((size_t) 2, masks.GetMasks().size());
        Assert::AreEqual (std::wstring (L"*.po"),  masks.GetMasks()[0]);
        Assert::AreEqual (std::wstring (L"*.hdv"), masks.GetMasks()[1]);
    }



    TEST_METHOD (FirstExtensionIsThePlainOne)
    {
        Assert::AreEqual (std::wstring (L"woz"), FileMaskSet (L"*.woz").GetFirstExtension());
        Assert::AreEqual (std::wstring (L"dsk"), FileMaskSet (L"disk?.*;*.dsk;*.do").GetFirstExtension());
        Assert::AreEqual (std::wstring(),        FileMaskSet (L"*.*").GetFirstExtension());
        Assert::AreEqual (std::wstring(),        FileMaskSet (L"").GetFirstExtension());
    }

    TEST_METHOD (ApplyToName_SwapsKnownExtensions_AndLeavesABareNameForSeveral)
    {
        std::vector<FileMaskSet>  all = { FileMaskSet (L"*.dsk;*.po;*.woz"), FileMaskSet (L"*.dsk"), FileMaskSet (L"*.woz"), FileMaskSet (L"*.d13") };



        Assert::AreEqual (std::wstring (L"Slot 6.woz"),   FileMaskSet::ApplyToName (L"Slot 6.DSK",  all[2], all), L"a known extension is replaced");
        Assert::AreEqual (std::wstring (L"Slot 6.d13"),   FileMaskSet::ApplyToName (L"Slot 6",      all[3], all), L"a bare name gets the type's extension");
        Assert::AreEqual (std::wstring (L"v1.2.dsk"),     FileMaskSet::ApplyToName (L"v1.2",        all[1], all), L"an unknown extension is kept");
        Assert::AreEqual (std::wstring (L"Slot 6"),       FileMaskSet::ApplyToName (L"Slot 6.woz",  all[0], all), L"several extensions leave the name bare");
        Assert::AreEqual (std::wstring (L"Slot 6.txt"),   FileMaskSet::ApplyToName (L"Slot 6.txt",  all[0], all), L"an unknown extension stays with several");
        Assert::AreEqual (std::wstring (L"Slot 6.woz"),   FileMaskSet::ApplyToName (L"Slot 6.woz",  FileMaskSet (L"*.*"), all), L"all files leaves the name");
    }
};
