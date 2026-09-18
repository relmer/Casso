#include "Pch.h"

#include "Config/GlobalUserPrefs.h"
#include "Debugger/Source/SourceService.h"
#include "Sha1.h"
#include "UiTests/InMemoryFileSystem.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SourceServiceTests
//
//  Finding a debug file's sources (FR-058, FR-059, FR-060, R-032): the order
//  searched, the name and size filter ahead of the hash, what a mismatch or a
//  dropped file gives, and the folder lists remembered in the preferences.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    static const char * const  s_kSource   = "start   lda #1\r\n        rts\r\n";
    static const char * const  s_kEdited   = "start   lda #2\r\n        rts\r\n";
    static const char * const  s_kProgram  = "program key";



    TEST_CLASS (SourceServiceTests)
    {
    public:

        struct Rig
        {
            InMemoryFileSystem  files;
            GlobalUserPrefs     prefs;
            SourcePathList      paths   { prefs };
            SourceService       service { files, paths };
        };



        static DebugSourceFile Record (const char * name, const std::string & text)
        {
            DebugSourceFile  record;



            record.name = name;
            record.size = text.size();
            record.sha1 = Sha1::ComputeTextHex (text);
            return record;
        }



        TEST_METHOD (TheRecordedPathIsTriedFirst)
        {
            Rig           rig;
            SourceLookup  found;



            rig.files.WriteAllText (L"C:\\Work\\src\\main.a65", s_kSource);
            rig.files.WriteAllText (L"C:\\Other\\main.a65",     s_kSource);
            rig.paths.AddFound (s_kProgram, L"C:\\Other");

            found = rig.service.Find (Record ("../src/main.a65", s_kSource), L"C:\\Work\\bin\\main.dbg", s_kProgram);

            Assert::IsTrue   (found.match == SourceMatch::Exact);
            Assert::AreEqual (std::wstring (L"C:\\Work\\src\\main.a65"), found.path);
            Assert::AreEqual (std::string (s_kSource), found.text);
        }


        TEST_METHOD (TheProgramListComesBeforeTheGlobalOne)
        {
            Rig           rig;
            SourceLookup  found;



            rig.files.WriteAllText (L"C:\\Global\\main.a65",  s_kSource);
            rig.files.WriteAllText (L"C:\\Program\\main.a65", s_kSource);
            rig.paths.AddFound ("another program", L"C:\\Global");
            rig.paths.AddFound (s_kProgram,        L"C:\\Program");
            rig.paths.AddFound ("another program", L"C:\\Global");

            found = rig.service.Find (Record ("main.a65", s_kSource), L"C:\\Work\\main.dbg", s_kProgram);

            Assert::AreEqual (std::wstring (L"C:\\Program\\main.a65"), found.path, L"the global list's front is searched second");
        }


        TEST_METHOD (ASizeMismatchIsNotHashed)
        {
            Rig           rig;
            SourceLookup  found;



            rig.files.WriteAllText (L"C:\\Work\\main.a65",   std::string (s_kSource) + "; longer\r\n");
            rig.files.WriteAllText (L"C:\\Found\\main.a65",  s_kSource);
            rig.paths.AddFound (s_kProgram, L"C:\\Found");

            found = rig.service.Find (Record ("main.a65", s_kSource), L"C:\\Work\\main.dbg", s_kProgram);

            Assert::IsTrue   (found.match == SourceMatch::Exact);
            Assert::AreEqual (std::wstring (L"C:\\Found\\main.a65"), found.path, L"the recorded folder's file is the wrong size");
        }


        TEST_METHOD (ASizeMatchWithAnotherHashIsAMismatch)
        {
            Rig           rig;
            SourceLookup  found;



            rig.files.WriteAllText (L"C:\\Work\\main.a65", s_kEdited);

            found = rig.service.Find (Record ("main.a65", s_kSource), L"C:\\Work\\main.dbg", s_kProgram);

            Assert::IsTrue   (found.match == SourceMatch::Mismatch);
            Assert::AreEqual (std::string (s_kEdited), found.text);
            Assert::IsTrue   (rig.paths.GetGlobalFolders().empty(), L"a mismatch is not remembered as found");
        }


        TEST_METHOD (TheFirstMatchingHashWinsOverAnEarlierCandidate)
        {
            Rig           rig;
            SourceLookup  found;



            rig.files.WriteAllText (L"C:\\Work\\main.a65",  s_kEdited);
            rig.files.WriteAllText (L"C:\\Found\\main.a65", s_kSource);
            rig.paths.AddFound (s_kProgram, L"C:\\Found");

            found = rig.service.Find (Record ("main.a65", s_kSource), L"C:\\Work\\main.dbg", s_kProgram);

            Assert::IsTrue   (found.match == SourceMatch::Exact);
            Assert::AreEqual (std::wstring (L"C:\\Found\\main.a65"), found.path);
        }


        TEST_METHOD (TwoMismatchesAreBothOffered)
        {
            Rig           rig;
            SourceLookup  found;



            rig.files.WriteAllText (L"C:\\Work\\main.a65",  s_kEdited);
            rig.files.WriteAllText (L"C:\\Other\\main.a65", "start   lda #3\r\n        rts\r\n");
            rig.paths.AddFound (s_kProgram, L"C:\\Other");

            found = rig.service.Find (Record ("main.a65", s_kSource), L"C:\\Work\\main.dbg", s_kProgram);

            Assert::IsTrue   (found.match == SourceMatch::Mismatch);
            Assert::AreEqual ((size_t) 2, found.candidates.size());
        }


        TEST_METHOD (ARecordWithoutAHashMatchesOnSize)
        {
            Rig              rig;
            DebugSourceFile  record = Record ("main.a65", s_kSource);
            SourceLookup     found;



            record.sha1.clear();
            rig.files.WriteAllText (L"C:\\Work\\main.a65", s_kEdited);

            found = rig.service.Find (record, L"C:\\Work\\main.dbg", s_kProgram);

            Assert::IsTrue (found.match == SourceMatch::Unverified);
        }


        TEST_METHOD (NothingOfTheNameIsNotFound)
        {
            Rig  rig;



            rig.files.WriteAllText (L"C:\\Work\\other.a65", s_kSource);

            Assert::IsTrue (rig.service.Find (Record ("main.a65", s_kSource), L"C:\\Work\\main.dbg", s_kProgram).match == SourceMatch::NotFound);
        }


        TEST_METHOD (AFoundFolderGoesToTheFrontOfBothLists)
        {
            Rig  rig;



            rig.paths.AddFound (s_kProgram, L"C:\\Older");
            rig.files.WriteAllText (L"C:\\Work\\main.a65", s_kSource);
            rig.service.Find (Record ("main.a65", s_kSource), L"C:\\Work\\main.dbg", s_kProgram);

            Assert::AreEqual (std::wstring (L"C:\\Work"), rig.paths.GetGlobalFolders().at (0));
            Assert::AreEqual (std::wstring (L"C:\\Work"), rig.paths.GetProgramFolders (s_kProgram).at (0));
            Assert::AreEqual ((size_t) 2,                 rig.paths.GetProgramFolders (s_kProgram).size());
        }


        TEST_METHOD (AFolderFoundAgainIsNotListedTwice)
        {
            Rig  rig;



            rig.paths.AddFound (s_kProgram, L"C:\\Work");
            rig.paths.AddFound (s_kProgram, L"C:\\Other");
            rig.paths.AddFound (s_kProgram, L"c:/work/");

            Assert::AreEqual ((size_t) 2, rig.paths.GetGlobalFolders().size());
            Assert::AreEqual (std::wstring (L"c:/work/"), rig.paths.GetGlobalFolders().at (0));
        }


        TEST_METHOD (TheListsSurviveASaveAndLoad)
        {
            Rig              rig;
            GlobalUserPrefs  loaded;
            SourcePathList   reloaded { loaded };



            rig.paths.AddFound (s_kProgram, L"C:\\Work\\src");
            rig.paths.AddFound ("another",  L"C:\\Else");

            Assert::AreEqual (S_OK, rig.prefs.Save (L"C:\\Prefs", rig.files));
            Assert::AreEqual (S_OK, loaded.Load (L"C:\\Prefs", rig.files));

            Assert::AreEqual ((size_t) 2,                     reloaded.GetGlobalFolders().size());
            Assert::AreEqual (std::wstring (L"C:\\Else"),     reloaded.GetGlobalFolders().at (0));
            Assert::AreEqual (std::wstring (L"C:\\Work\\src"), reloaded.GetProgramFolders (s_kProgram).at (0));
        }


        TEST_METHOD (ADroppedFileIsMatchedByHash)
        {
            Rig        rig;
            DebugFile  file;
            int        index = -1;



            file.files.push_back (Record ("main.a65", s_kEdited));
            file.files.push_back (Record ("lib.a65",  s_kSource));
            rig.files.WriteAllText (L"C:\\Elsewhere\\renamed.a65", s_kSource);

            Assert::IsTrue   (rig.service.MatchDropped (file, L"C:\\Elsewhere\\renamed.a65", s_kProgram, index).match == SourceMatch::Exact);
            Assert::AreEqual (1, index, L"the hash, not the name, decides");
            Assert::AreEqual (std::wstring (L"C:\\Elsewhere"), rig.paths.GetProgramFolders (s_kProgram).at (0));
        }


        TEST_METHOD (ADroppedFileMatchingNoRecordIsPlainText)
        {
            Rig           rig;
            DebugFile     file;
            SourceLookup  found;
            int           index = 0;



            file.files.push_back (Record ("main.a65", s_kSource));
            rig.files.WriteAllText (L"C:\\Elsewhere\\notes.txt", "hello\r\n");

            found = rig.service.MatchDropped (file, L"C:\\Elsewhere\\notes.txt", s_kProgram, index);

            Assert::IsTrue   (found.match == SourceMatch::NotFound);
            Assert::AreEqual (-1, index);
            Assert::AreEqual (std::string ("hello\r\n"), found.text, L"shown as plain text");
            Assert::IsTrue   (rig.paths.GetGlobalFolders().empty());
        }


        TEST_METHOD (ADroppedEditOfARecordIsAMismatch)
        {
            Rig        rig;
            DebugFile  file;
            int        index = -1;



            file.files.push_back (Record ("main.a65", s_kSource));
            rig.files.WriteAllText (L"C:\\Elsewhere\\main.a65", s_kEdited);

            Assert::IsTrue   (rig.service.MatchDropped (file, L"C:\\Elsewhere\\main.a65", s_kProgram, index).match == SourceMatch::Mismatch);
            Assert::AreEqual (0, index);
        }


        TEST_METHOD (CombineResolvesDotDot)
        {
            Assert::AreEqual (std::wstring (L"C:\\Work\\src\\a.s"), SourceService::Combine (L"C:\\Work\\bin", L"../src/a.s"));
            Assert::AreEqual (std::wstring (L"D:\\x\\a.s"),        SourceService::Combine (L"C:\\Work",      L"D:/x/a.s"));
        }
    };
}
