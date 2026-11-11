#include "Pch.h"

#include "Debugger/BinaryImageReader.h"
#include "Debugger/DebugFile.h"
#include "Debugger/DebugFileReader.h"
#include "Debugger/Project/DebugProject.h"
#include "Debugger/SymbolFileReader.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FileReaderRobustnessTests
//
//  The debugger's readers of files from outside: cc65 debug files, Merlin
//  listings, symbol files, binary images and debug projects. Each is given
//  empty text, random bytes, every prefix of a small valid file and copies of
//  one with bytes changed, and reads each as data or as an error; none
//  throws.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (FileReaderRobustnessTests)
    {
    public:

        TEST_METHOD (NoReaderThrowsOnBrokenFiles)
        {
            static const std::vector<std::string>  kSamples =
            {
                "version\tmajor=2,minor=0\n"
                "file\tid=0,name=\"a.s\",size=10,mtime=0x0,mod=0\n"
                "seg\tid=0,name=\"CODE\",start=0x000300,size=0x0003,addrsize=absolute,type=rw\n"
                "span\tid=0,seg=0,start=0,size=3\n"
                "line\tid=0,file=0,line=1,span=0\n"
                "sym\tid=0,name=\"start\",addrsize=absolute,size=3,scope=0,def=0,val=0x300,seg=0,type=lab\n",

                "0300: A9 00        1 START LDA #$00\n0302: 60           2       RTS\n",
                "$0300 START\n$0302 LOOP\nal C:0300 .start\nSTART=$0300\n",
                ":03030000A900608F\n:00000001FF\n",
                "S1060300A90060F2\nS9030000FC\n",
                std::string ("\x00\x05\x16\x00\x00\x02\x00\x00", 8) + std::string (16, '\0') + std::string ("\x00\x01", 2),
                std::string ("\x00\x03\x03\x00\xA9\x00\x60", 7),
                "{\"version\":1,\"engineScript\":[\"BP 0300\",\"BPD 0\"],\"breakpointNames\":{\"0\":\"x\"},\"logs\":{\"0\":\"A={A}\"}}",
            };

            std::vector<std::string>  inputs  = { std::string() };
            std::vector<std::string>  failures;
            std::mt19937              random (0xCA550035u);



            //  Random bytes, of lengths up to a few hundred.
            for (int i = 0; i < 400; i++)
            {
                std::string  bytes ((size_t) (random() % 400), '\0');

                for (char & ch : bytes)
                {
                    ch = (char) (random() & 0xFF);
                }

                inputs.push_back (bytes);
            }

            //  Every prefix of each sample, and copies with bytes changed.
            for (const std::string & sample : kSamples)
            {
                for (size_t length = 0; length <= sample.size(); length++)
                {
                    inputs.push_back (sample.substr (0, length));
                }

                for (int copy = 0; copy < 200 && !sample.empty(); copy++)
                {
                    std::string  changed = sample;

                    for (int change = 0; change < 3; change++)
                    {
                        changed[random() % changed.size()] = (char) (random() & 0xFF);
                    }

                    inputs.push_back (changed);
                }
            }

            for (const std::string & input : inputs)
            {
                Read (input, failures);
            }

            Logger::WriteMessage (std::format ("{} inputs read by each reader\n", inputs.size()).c_str());

            for (const std::string & failure : failures)
            {
                Logger::WriteMessage ((failure + "\n").c_str());
            }

            Assert::AreEqual ((size_t) 0, failures.size(), L"inputs a reader threw on: see the output");
        }

    private:

        //  Every reader on one input, each exception noted with the reader and
        //  the input's length.
        static void  Read (const std::string & input, std::vector<std::string> & failures)
        {
            static constexpr BinaryFormat  kFormats[] = { BinaryFormat::Raw, BinaryFormat::Dos33Binary, BinaryFormat::IntelHex, BinaryFormat::SRecord, BinaryFormat::AppleSingle };

            auto  attempt = [&failures, &input] (const char * reader, const std::function<void()> & read)
            {
                try
                {
                    read();
                }
                catch (const std::exception & e)
                {
                    failures.push_back (std::format ("{} on {} bytes: {}", reader, input.size(), e.what()));
                }
            };

            std::span<const Byte>  bytes ((const Byte *) input.data(), input.size());



            attempt ("DebugFileReader::Read", [&] { DebugFile out; std::string error; (void) DebugFileReader::Read (input, out, error); });
            attempt ("DebugFileReader::ReadMerlinListing", [&] { DebugFile out; std::string error; (void) DebugFileReader::ReadMerlinListing (input, "x.s", out, error); });
            attempt ("SymbolFileReader::Read", [&] { std::vector<SymbolFileEntry> out; SymbolFileFormat format; std::string error; (void) SymbolFileReader::Read (input, out, format, error); });
            attempt ("DebugProject::TryParse", [&] { DebugProject project; std::string error; (void) DebugProject::TryParse (input, L"C:\\Work", project, error); });
            attempt ("BinaryImageReader::Read", [&] { BinaryImage image; std::string error; (void) BinaryImageReader::Read (bytes, std::nullopt, std::nullopt, image, error); });

            for (BinaryFormat format : kFormats)
            {
                attempt (BinaryImageReader::GetFormatName (format), [&] { BinaryImage image; std::string error; (void) BinaryImageReader::Read (bytes, format, (Word) 0x0300, image, error); });
            }
        }
    };
}
