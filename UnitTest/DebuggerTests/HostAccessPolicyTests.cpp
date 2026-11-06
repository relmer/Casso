#include "Pch.h"

#include "Debugger/Channel/HostAccessPolicy.h"
#include "FakeHostPathResolver.h"

#include "CppUnitTest.h"
#include "Core/TextEncoding.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  HostAccessPolicyTests
    //
    //  Each rule of the debug channel's host file access policy, over a fake
    //  file system: C:\Work is the allowed folder, C:\Elsewhere is not, and
    //  the known folders are under C:\Users\Tester.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (HostAccessPolicyTests)
    {
    public:
        struct Rig
        {
            InMemoryFileSystem    files;
            FakeHostPathResolver  resolver { &files };



            Rig()
            {
                resolver.AddFolder (L"C:\\Work\\Sub");
                resolver.AddFolder (L"C:\\Elsewhere");
                resolver.AddFolder (L"C:\\Users\\Tester\\Documents\\Casso");
            }

            HostAccessVerdict Check (const std::wstring & path, HostAccess access = HostAccess::Read, bool overwrite = false,
                                     std::vector<std::wstring> folders = { L"C:\\Work" })
            {
                HostAccessPolicy  policy (resolver, std::move (folders));



                return policy.Check (path, access, overwrite);
            }
        };



        static std::wstring Widen (const std::string & text)
        {
            return TextEncoding::NarrowToWide (text);
        }



        static void AssertFails (const HostAccessVerdict & verdict, const std::string & reasonPart)
        {
            Assert::IsFalse (verdict.isAllowed, Widen (verdict.reason).c_str());
            Assert::IsTrue  (verdict.reason.find (reasonPart) != std::string::npos, Widen (verdict.reason).c_str());
        }



        static void AssertAllowed (const HostAccessVerdict & verdict)
        {
            Assert::IsTrue (verdict.isAllowed, Widen (verdict.reason).c_str());
        }





        //  A UNC path, the long form of one, and one with forward slashes,
        //  for a read and a write alike, even with the share an allowed folder.
        TEST_METHOD (UncAndNetworkPaths_Fail)
        {
            Rig  rig;



            for (const wchar_t * path : { L"\\\\server\\share\\x.bin", L"\\\\?\\UNC\\server\\share\\x.bin", L"//server/share/x.bin" })
            {
                AssertFails (rig.Check (path, HostAccess::Read,  false, { L"C:\\Work", L"\\\\server\\share" }), "is a network path");
                AssertFails (rig.Check (path, HostAccess::Write, true,  { L"C:\\Work", L"\\\\server\\share" }), "is a network path");
            }
        }





        //  A drive letter whose folder resolves to a share, as a mapped
        //  drive's does.
        TEST_METHOD (APathThatResolvesToTheNetwork_Fails)
        {
            Rig  rig;



            rig.resolver.AddFolder (L"\\\\server\\share");
            rig.resolver.AddLink   (L"Z:\\Share", L"\\\\server\\share");

            AssertFails (rig.Check (L"Z:\\Share\\x.bin", HostAccess::Read, false, { L"C:\\Work", L"Z:\\Share" }), "is a network path");
        }





        TEST_METHOD (DevicePaths_Fail)
        {
            Rig  rig;



            AssertFails (rig.Check (L"\\\\.\\PhysicalDrive0"), "is a device path");
            AssertFails (rig.Check (L"\\\\?\\C:\\Work\\x.bin"), "is a device path");
        }





        //  Relative, drive-relative and stream paths are not resolved here;
        //  the session makes a path full before it is checked.
        TEST_METHOD (APathThatIsNotAPlainFullPath_Fails)
        {
            Rig  rig;



            AssertFails (rig.Check (L"Work\\x.bin"),              "isn't a full path");
            AssertFails (rig.Check (L"C:x.bin"),                  "isn't a full path");
            AssertFails (rig.Check (L"\\Work\\x.bin"),            "isn't a full path");
            AssertFails (rig.Check (L"C:\\Work\\x.txt:run.exe", HostAccess::Write), "isn't a plain file path");
        }





        TEST_METHOD (APathUnderAnAllowedFolder_IsAllowed)
        {
            Rig  rig;



            AssertAllowed (rig.Check (L"C:\\Work\\x.bin"));
            AssertAllowed (rig.Check (L"c:/work/sub/new.bin", HostAccess::Write));
            AssertAllowed (rig.Check (L"C:\\Work\\Sub\\NotYet\\new.bin", HostAccess::Write));
        }





        TEST_METHOD (APathOutsideEveryAllowedFolder_Fails)
        {
            Rig  rig;



            AssertFails (rig.Check (L"C:\\Elsewhere\\x.bin"),                    "C:\\Elsewhere\\x.bin is outside the allowed folders");
            AssertFails (rig.Check (L"C:\\Elsewhere\\x.bin", HostAccess::Write), "is outside the allowed folders");

            //  A folder that starts with an allowed one's name is not under it.
            AssertFails (rig.Check (L"C:\\Workshop\\x.bin"), "is outside the allowed folders");
        }





        TEST_METHOD (DotDotIsResolvedBeforeTheFolderCheck)
        {
            Rig  rig;



            AssertFails   (rig.Check (L"C:\\Work\\..\\Elsewhere\\x.bin"),        "C:\\Elsewhere\\x.bin is outside");
            AssertFails   (rig.Check (L"C:\\Work\\Sub\\..\\..\\Elsewhere\\x.bin"), "is outside");
            AssertFails   (rig.Check (L"C:\\Work\\..\\..\\..\\x.bin"),           "is outside");
            AssertAllowed (rig.Check (L"C:\\Work\\Sub\\..\\.\\x.bin"));
        }





        //  A junction inside an allowed folder that points out of it.
        TEST_METHOD (ALinkOutOfAnAllowedFolder_Fails)
        {
            Rig  rig;



            rig.resolver.AddLink (L"C:\\Work\\Escape", L"C:\\Elsewhere");

            AssertFails (rig.Check (L"C:\\Work\\Escape\\x.bin"),                    "C:\\Elsewhere\\x.bin is outside");
            AssertFails (rig.Check (L"C:\\Work\\Escape\\new.bin", HostAccess::Write), "C:\\Elsewhere\\new.bin is outside");
        }





        //  The reverse: a path through a link elsewhere to an allowed folder is
        //  allowed, and
        //  an allowed folder given through a link is the folder it points to.
        TEST_METHOD (ALinkIntoAnAllowedFolder_IsAllowed)
        {
            Rig  rig;



            rig.resolver.AddLink (L"C:\\Alias", L"C:\\Work");

            AssertAllowed (rig.Check (L"C:\\Alias\\x.bin"));
            AssertAllowed (rig.Check (L"C:\\Work\\x.bin", HostAccess::Read, false, { L"C:\\Alias" }));
            AssertFails   (rig.Check (L"C:\\Alias\\x.bin", HostAccess::Read, false, { L"C:\\Elsewhere" }), "C:\\Work\\x.bin is outside");
        }





        TEST_METHOD (DocumentsCasso_IsAlwaysAllowed)
        {
            Rig  rig;



            AssertAllowed (rig.Check (L"C:\\Users\\Tester\\Documents\\Casso\\x.bin", HostAccess::Read, false, {}));
            AssertFails   (rig.Check (L"C:\\Users\\Tester\\Documents\\x.bin",        HostAccess::Read, false, {}), "is outside");
        }





        TEST_METHOD (AWriteOverAnExistingFile_NeedsOverwrite)
        {
            Rig  rig;
            HRESULT  hr = rig.files.WriteAllText (L"C:\\Work\\x.bin", "old");



            Assert::AreEqual (S_OK, hr);

            AssertFails   (rig.Check (L"C:\\Work\\x.bin", HostAccess::Write),  "C:\\Work\\x.bin already exists. Set the overwrite option to replace it.");
            AssertFails   (rig.Check (L"C:\\Work\\x.bin", HostAccess::Delete), "Set the overwrite option to delete it.");
            AssertAllowed (rig.Check (L"C:\\Work\\x.bin", HostAccess::Write, true));
            AssertAllowed (rig.Check (L"C:\\Work\\x.bin", HostAccess::Read));
            AssertAllowed (rig.Check (L"C:\\Work\\y.bin", HostAccess::Write));
        }





        //  Every blocked extension, in any case and with the trailing dots and
        //  spaces Windows drops, fails for a write even with overwrite set
        //  and in an allowed folder; a read of one is allowed.
        TEST_METHOD (AWriteOfABlockedExtension_Fails)
        {
            Rig  rig;



            for (const wchar_t * extension : { L".exe", L".dll", L".bat", L".cmd", L".ps1", L".vbs", L".js", L".lnk", L".scr" })
            {
                std::wstring  path = std::wstring (L"C:\\Work\\run") + extension;



                AssertFails (rig.Check (path, HostAccess::Write, true), "Writing " + TextEncoding::WideToNarrow (extension) + " files is not allowed.");
            }

            AssertFails   (rig.Check (L"C:\\Work\\RUN.BAT",   HostAccess::Write), "Writing .bat files");
            AssertFails   (rig.Check (L"C:\\Work\\run.cmd. ", HostAccess::Write), "Writing .cmd files");
            AssertFails   (rig.Check (L"C:\\Work\\run.exe",   HostAccess::Delete, true), "Writing .exe files");
            AssertAllowed (rig.Check (L"C:\\Work\\run.exe",   HostAccess::Read));
            AssertAllowed (rig.Check (L"C:\\Work\\run.exe.txt", HostAccess::Write));
        }





        //  A link with a plain name to a file with a blocked one is checked
        //  where it resolves.
        TEST_METHOD (AWriteThroughALinkToABlockedExtension_Fails)
        {
            Rig      rig;
            HRESULT  hr = rig.files.WriteAllText (L"C:\\Work\\run.bat", "echo");



            Assert::AreEqual (S_OK, hr);
            rig.resolver.AddLink (L"C:\\Work\\notes.txt", L"C:\\Work\\run.bat");

            AssertFails (rig.Check (L"C:\\Work\\notes.txt", HostAccess::Write, true), "Writing .bat files");
        }





        //  Both Startup folders fail for a write even when an allowed folder
        //  covers them.
        TEST_METHOD (AWriteIntoAStartupFolder_Fails)
        {
            Rig  rig;



            rig.resolver.AddFolder (rig.resolver.knownFolders[HostKnownFolder::Startup]);
            rig.resolver.AddFolder (rig.resolver.knownFolders[HostKnownFolder::CommonStartup]);

            AssertFails   (rig.Check (rig.resolver.knownFolders[HostKnownFolder::Startup] + L"\\x.txt", HostAccess::Write, true, { L"C:\\Users" }),
                           "is in a Startup folder.");
            AssertFails   (rig.Check (rig.resolver.knownFolders[HostKnownFolder::CommonStartup] + L"\\x.txt", HostAccess::Write, true, { L"C:\\ProgramData" }),
                           "is in a Startup folder.");
            AssertAllowed (rig.Check (rig.resolver.knownFolders[HostKnownFolder::Startup] + L"\\x.txt", HostAccess::Read, false, { L"C:\\Users" }));
        }
    };
}
