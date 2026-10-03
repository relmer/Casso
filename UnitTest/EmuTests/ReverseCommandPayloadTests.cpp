#include "Pch.h"

#include "Shell/CpuCommandDispatcher.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


static constexpr uint64_t  s_kPayloadSeekPosition = 1234567890123ull;





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseCommandPayloadTests
//
//  The text an IDM_DEBUG_REVERSE command carries from the UI thread to the
//  CPU thread: every command survives the trip, seek with its position, and
//  text that does not parse asks for nothing.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ReverseCommandPayloadTests)
{
public:

    TEST_METHOD (EveryCommandSurvivesTheTrip)
    {
        static constexpr ReverseCommand  kCommands[] =
        {
            ReverseCommand::StepBack,
            ReverseCommand::StepBackOver,
            ReverseCommand::StepBackOut,
            ReverseCommand::ReverseContinue,
            ReverseCommand::StepForward,
            ReverseCommand::Seek,
            ReverseCommand::GoLive,
        };
        ReverseCommand  parsed   = ReverseCommand::StepBack;
        uint64_t        argument = 0;
        std::string     payload;
        bool            isParsed = false;



        for (ReverseCommand command : kCommands)
        {
            payload  = CpuCommandDispatcher::FormatReversePayload (command, s_kPayloadSeekPosition);
            isParsed = CpuCommandDispatcher::TryParseReversePayload (payload, parsed, argument);

            Assert::IsTrue (isParsed, std::format (L"parsed command {}", static_cast<int> (command)).c_str());
            Assert::IsTrue (parsed == command, std::format (L"the same command {}", static_cast<int> (command)).c_str());
            Assert::AreEqual<uint64_t> ((command == ReverseCommand::Seek) ? s_kPayloadSeekPosition : 0, argument, L"the position goes with seek alone");
        }
    }


    TEST_METHOD (TextThatDoesNotParseAsksForNothing)
    {
        static constexpr const char *  kBad[] =
        {
            "",
            "backward",
            "back 5",
            "seek",
            "seek ",
            "seek 12a",
            "seek -3",
            "seek 99999999999999999999999",
            "live now",
        };
        ReverseCommand  parsed   = ReverseCommand::StepBack;
        uint64_t        argument = 0;
        bool            isParsed = false;



        for (const char * text : kBad)
        {
            isParsed = CpuCommandDispatcher::TryParseReversePayload (text, parsed, argument);

            Assert::IsFalse (isParsed, std::format (L"\"{}\" is refused", std::wstring (text, text + strlen (text))).c_str());
        }
    }
};
