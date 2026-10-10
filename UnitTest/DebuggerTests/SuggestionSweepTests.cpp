#include "Pch.h"

#include "ControllerRig.h"
#include "Core/JsonParser.h"
#include "Debugger/AppleWinCommandTable.h"
#include "Debugger/CommandModeHelp.h"
#include "Debugger/CommandSuggestion.h"
#include "Debugger/ReplyJson.h"
#include "Core/JsonValue.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SuggestionSweepTests
//
//  SC-032 over every command of every mode: a word typed in a mode that does
//  not have it is answered with that mode's equivalent, or with a reply that
//  the mode has none. Also the channel's copy of a suggestion and a usage
//  line, and the removal confirmations FR-129 gives as its example.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (SuggestionSweepTests)
    {
    public:
        static std::wstring Widen (const std::string & text)
        {
            return std::wstring (text.begin(), text.end());
        }



        static std::vector<std::string> GetWords (CommandMode mode)
        {
            std::vector<std::string>  words;



            if (mode == CommandMode::AppleWin)
            {
                for (const AppleWinCommand & command : AppleWinCommandTable::GetAll())
                {
                    if (command.verb != DebugVerb::None)
                    {
                        words.push_back (command.name);
                    }
                }

                return words;
            }

            for (const CommandModeHelp::Entry & entry : CommandModeHelp::GetEntries (mode))
            {
                if (entry.word[0] != '\0')
                {
                    words.push_back (entry.word);
                }
            }

            return words;
        }



        static bool IsWordOf (CommandMode mode, const std::string & word)
        {
            if (mode == CommandMode::AppleWin || mode == CommandMode::Casso)
            {
                return AppleWinCommandTable::Find (word) != nullptr;
            }

            std::string  marker = CommandModeHelp::GetMarker (mode);



            if (!marker.empty() && word.size() > marker.size() && word.starts_with (marker))
            {
                return CommandModeHelp::IsCassoCommandReachable (mode, word.substr (marker.size()));
            }

            return CommandModeHelp::Find (mode, word) != nullptr || CommandModeHelp::IsCassoCommandReachable (mode, word);
        }



        TEST_METHOD (EveryCommandOfEveryMode_TypedInAnother_GetsItsEquivalentOrSaysThereIsNone)
        {
            static constexpr CommandMode  s_kSources[] = { CommandMode::AppleWin, CommandMode::Monitor, CommandMode::GSSquared, CommandMode::WinDbg };
            static constexpr CommandMode  s_kTargets[] = { CommandMode::AppleWin, CommandMode::Casso, CommandMode::GSSquared, CommandMode::WinDbg };
            ControllerRig                 rig;
            size_t                        checked      = 0;



            for (CommandMode source : s_kSources)
            {
                for (const std::string & word : GetWords (source))
                {
                    for (CommandMode target : s_kTargets)
                    {
                        Reply        reply;
                        std::string  where;



                        if (target == source || (source == CommandMode::AppleWin && target == CommandMode::Casso) || IsWordOf (target, word))
                        {
                            continue;
                        }

                        reply.status = CommandStatus::Unknown;
                        CommandSuggestion::Annotate (reply, word, target, rig.controller.GetSession());

                        where = std::format ("{} from {} typed in {}: {}", word, CommandModeHelp::GetTitle (source), CommandModeHelp::GetTitle (target), reply.error.detail);

                        Assert::IsTrue (reply.error.detail.starts_with (word + " is "), Widen (where).c_str());
                        Assert::IsTrue (reply.error.detail.find ("has no equivalent") != std::string::npos ||
                                        reply.error.detail.find ("'s is ") != std::string::npos ||
                                        reply.error.detail.find ("The closest is ") != std::string::npos,
                                        Widen (where).c_str());

                        if (!reply.suggestion.empty())
                        {
                            std::string  suggested = reply.suggestion.substr (0, reply.suggestion.find (' '));



                            Assert::IsTrue (IsWordOf (target, suggested), Widen (where).c_str());
                        }

                        ++checked;
                    }
                }
            }

            Assert::IsTrue (checked > 100, L"the sweep reached the tables");
        }



        TEST_METHOD (ChannelReply_CarriesTheSuggestion)
        {
            ControllerRig  rig;
            Reply          reply = rig.Run ("nobp 0", CommandMode::AppleWin);
            JsonValue      root;
            JsonParseError error;
            std::string    suggestion;



            Assert::AreEqual (S_OK, JsonParser::Parse (ReplyJson::WriteReply (reply, std::nullopt), root, error));
            Assert::IsTrue   (root.HasString ("suggestion", suggestion), L"the reply holds the suggestion");
            Assert::AreEqual (std::string ("bpc 0"), suggestion);
        }



        TEST_METHOD (ChannelReply_CarriesTheUsageLine)
        {
            Reply           reply;
            JsonValue       root;
            JsonParseError  error;
            std::string     usage;



            reply.command = "bpc";
            reply.SetError (CommandStatus::Error, "invalid arguments", "BPC needs an id.");
            reply.error.usage = "BPC # | *";

            Assert::AreEqual (S_OK, JsonParser::Parse (ReplyJson::WriteReply (reply, std::nullopt), root, error));
            Assert::IsTrue   (root.HasString ("usage", usage), L"the reply holds the usage line");
            Assert::AreEqual (std::string ("BPC # | *"), usage);
        }



        TEST_METHOD (RemovingABreakpoint_SaysWhichAndWhatItStoppedOn)
        {
            ControllerRig  rig;



            rig.Run ("bp c000");
            Assert::AreEqual (std::string ("Removed breakpoint 0 (exec C000)"), rig.Run ("bpc 0").text.at (0));
        }



        TEST_METHOD (RemovingAWatchpoint_SaysWhichAndWhatItStoppedOn)
        {
            ControllerRig  rig;



            rig.Run ("bpmw 400:4ff");
            Assert::AreEqual (std::string ("Removed breakpoint 0 (write 0400:04FF)"), rig.Run ("bpc 0").text.at (0));
        }



        TEST_METHOD (RemovingAWatch_SaysWhichAndItsAddress)
        {
            ControllerRig  rig;



            rig.Run ("watch 400", CommandMode::GSSquared);
            Assert::AreEqual (std::string ("Removed watch 0 (0400)"), rig.Run ("nowatch 0", CommandMode::GSSquared).text.at (0));
        }
    };
}
