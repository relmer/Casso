#include "Pch.h"
#include "Config/BackgroundFileWriter.h"
#include "InMemoryFileSystem.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BackgroundFileWriterTests
//
//  Saves written on a background thread: after a flush, or once the writer
//  is gone, each file has the last content written to it.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (BackgroundFileWriterTests)
{
public:

    TEST_METHOD (Flush_LeavesEachFileWithItsNewestContent)
    {
        InMemoryFileSystem    fs;
        BackgroundFileWriter  writer (fs);
        std::string           text;

        for (int i = 0; i < 50; i++)
        {
            writer.Write (L"C:\\Data\\Session.json", std::format ("session {}", i));
            writer.Write (L"C:\\Data\\Prefs.json",   std::format ("prefs {}", i));
        }

        writer.Flush();

        Assert::AreEqual (S_OK, fs.ReadAllText (L"C:\\Data\\Session.json", text));
        Assert::AreEqual (std::string ("session 49"), text);
        Assert::AreEqual (S_OK, fs.ReadAllText (L"C:\\Data\\Prefs.json", text));
        Assert::AreEqual (std::string ("prefs 49"), text);
    }


    TEST_METHOD (Destroy_WritesWhatIsQueued)
    {
        InMemoryFileSystem  fs;
        std::string         text;

        {
            BackgroundFileWriter  writer (fs);

            writer.Write (L"C:\\Data\\Session.json", "last");
        }

        Assert::AreEqual (S_OK, fs.ReadAllText (L"C:\\Data\\Session.json", text));
        Assert::AreEqual (std::string ("last"), text);
    }


    TEST_METHOD (Flush_WithNothingWritten_Returns)
    {
        InMemoryFileSystem    fs;
        BackgroundFileWriter  writer (fs);

        writer.Flush();

        Assert::IsFalse (fs.Exists (L"C:\\Data\\Session.json"));
    }
};
