#include "Pch.h"

#include "ControllerRig.h"
#include "Core/TextEncoding.h"

#include "CppUnitTest.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  RelativePathNoDirectoryTests
    //
    //  With no directory set by CD, a relative path is taken from the
    //  process's current directory, so a reply always gives the path it used.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (RelativePathNoDirectoryTests)
    {
    public:
        static std::wstring Expected (const wchar_t * name)
        {
            std::wstring  directory = std::filesystem::current_path().wstring();



            return directory + (directory.ends_with (L'\\') ? L"" : L"\\") + name;
        }



        TEST_METHOD (NoDirectory_ResolvesFromProcessDirectory)
        {
            ControllerRig  rig;



            Assert::IsTrue   (rig.controller.GetSession().GetCurrentDirectory().empty());
            Assert::AreEqual (Expected (L"out.bin"), rig.controller.GetSession().ResolvePath ("out.bin"));
            Assert::AreEqual (Expected (L"out.bin"), rig.controller.GetSession().ResolvePath ("\"out.bin\""));
        }

        TEST_METHOD (NoDirectory_ReplyGivesTheResolvedPath)
        {
            ControllerRig  rig;
            Reply          reply = rig.Run ("BSAVE out.bin 300:301");
            std::string    all;



            for (const std::string & line : reply.text)
            {
                all += line + "\n";
            }

            Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status, TextEncoding::NarrowToWide (reply.error.detail).c_str());
            Assert::IsTrue   (all.find (" to " + TextEncoding::WideToNarrow (Expected (L"out.bin"))) != std::string::npos,
                              TextEncoding::NarrowToWide (all).c_str());
        }
    };
}
