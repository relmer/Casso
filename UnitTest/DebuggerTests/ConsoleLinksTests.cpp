#include "Pch.h"

#include "Debugger/Handlers/SymbolHandlers.h"
#include "Debugger/ReplyJson.h"
#include "HandlerTestRig.h"
#include "Ui/Debugger/ConsoleLinks.h"
#include "Ui/Debugger/DebuggerViewState.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ConsoleLinksTests
    //
    //  A console line's links: a full path in the text becomes the file's
    //  name linked to the file, and the marks are not drawn.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ConsoleLinksTests)
    {
    public:

        TEST_METHOD (AFullPathBecomesTheFilesNameLinkedToTheFile)
        {
            std::string                      line;
            std::wstring                     shown;
            std::vector<ConsoleLinks::Link>  links;



            line  = ConsoleLinks::LinkFiles ("Loaded 1 symbols into user from C:\\Work\\prog.sym.", { "C:\\Work\\prog.sym" });
            shown = ConsoleLinks::Parse (line, links);

            Assert::AreEqual (std::wstring (L"Loaded 1 symbols into user from prog.sym."), shown);
            Assert::AreEqual (std::string ("Loaded 1 symbols into user from prog.sym."),   ConsoleLinks::GetShownText (line), L"the narrow text is the same");
            Assert::AreEqual ((size_t) 1,                       links.size());
            Assert::AreEqual (32,                               links[0].start, L"the link is the file's name");
            Assert::AreEqual (8,                                links[0].length);
            Assert::AreEqual (std::wstring (L"C:\\Work\\prog.sym"), links[0].path, L"for the file's full path");
        }


        //  A copy set aside under the project's name with more after it is
        //  another file, so the project's path inside it is not linked.
        TEST_METHOD (APathIsLinkedOnlyWhereTheTextHasItWhole)
        {
            const std::vector<std::string>   paths = { "C:\\P\\x.cassoproj" };
            std::string                      copied;
            std::vector<ConsoleLinks::Link>  links;



            copied = "It is copied to C:\\P\\x.cassoproj.unreadable-1, and an empty project is open.";

            Assert::AreEqual (copied, ConsoleLinks::LinkFiles (copied, paths), L"a longer path that starts with it");
            Assert::AreEqual (std::wstring (L"The debug project x.cassoproj could not be read: bad."),
                              ConsoleLinks::Parse (ConsoleLinks::LinkFiles ("The debug project C:\\P\\x.cassoproj could not be read: bad.", paths), links));
            Assert::AreEqual ((size_t) 1, links.size(), L"the path itself");
        }


        TEST_METHOD (LinkedTextIsNotLinkedAgain)
        {
            std::string                      once  = ConsoleLinks::LinkFiles ("from C:\\Work\\a.bin", { "C:\\Work\\a.bin" });
            std::vector<ConsoleLinks::Link>  links;



            Assert::AreEqual (once, ConsoleLinks::LinkFiles (once, { "C:\\Work\\a.bin" }));
            Assert::AreEqual (std::wstring (L"from a.bin"), ConsoleLinks::Parse (once, links));
            Assert::AreEqual ((size_t) 1, links.size());
        }


        //  A mark left open is no link, and none of its marks are drawn.
        TEST_METHOD (AnUnclosedMarkIsPlainText)
        {
            std::string                      line = std::string ("see ") + ConsoleLinks::kOpen + "C:\\x" + ConsoleLinks::kSplit + "x";
            std::vector<ConsoleLinks::Link>  links;



            Assert::AreEqual (std::wstring (L"see C:\\xx"), ConsoleLinks::Parse (line, links));
            Assert::AreEqual ((size_t) 0,                   links.size());
            Assert::AreEqual (std::string ("see C:\\xx"),   ConsoleLinks::GetShownText (line));
        }


        TEST_METHOD (AScopesLabelLinksTheFileItEndsWith)
        {
            std::vector<ConsoleLinks::Link>  links;



            Assert::AreEqual (std::wstring (L"Apple2e booting game.dsk"),
                              ConsoleLinks::Parse (ConsoleLinks::LinkFileName ("Apple2e booting game.dsk", "C:\\Disks\\game.dsk"), links));
            Assert::AreEqual ((size_t) 1, links.size());
            Assert::AreEqual (16,         links[0].start, L"only the disk's name");
            Assert::AreEqual (8,          links[0].length);

            Assert::AreEqual (std::string ("Apple2e"), ConsoleLinks::LinkFileName ("Apple2e", ""), L"the machine alone has no file");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ConsoleFileLinkTests
    //
    //  A command's reply has the full path of a file it read or wrote: the
    //  text the debug channel writes has it, and the console's line has the
    //  file's name, linked to it.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ConsoleFileLinkTests)
    {
    public:

        using Rig = HandlerRig<SymbolHandlers>;



        TEST_METHOD (SymLoadsReplyKeepsTheFullPath)
        {
            Rig          rig;
            Reply        reply;
            std::string  json;



            rig.files.WriteAllText (L"C:\\Work\\prog.sym", "0300 START\n");
            reply = rig.RunOk ("SYM LOAD prog.sym");
            json  = ReplyJson::WriteReply (reply, std::nullopt);

            Assert::AreEqual (std::string ("Loaded 1 symbols into user from C:\\Work\\prog.sym."), reply.text.at (0), L"the text, as batch writes it");
            Assert::IsTrue   (reply.files == std::vector<std::string> { "C:\\Work\\prog.sym" }, L"the file it writes out");
            Assert::IsTrue   (json.find ("C:\\\\Work\\\\prog.sym") != std::string::npos, L"the channel's text has the full path");
            Assert::IsTrue   (json.find (ConsoleLinks::kOpen) == std::string::npos, L"and no link");
        }


        TEST_METHOD (SymLoadsConsoleLineLinksTheFilesName)
        {
            Rig                              rig;
            DebuggerViewState                view;
            std::vector<std::string>         lines;
            std::vector<ConsoleLinks::Link>  links;



            rig.files.WriteAllText (L"C:\\Work\\prog.sym", "0300 START\n");
            lines = view.ExecuteConsoleLine (rig.session, "SYM LOAD prog.sym", std::nullopt);

            Assert::AreEqual ((size_t) 2, lines.size(), L"the command, then its reply");
            Assert::AreEqual (std::wstring (L"Loaded 1 symbols into user from prog.sym."), ConsoleLinks::Parse (lines[1], links));
            Assert::AreEqual ((size_t) 1, links.size());
            Assert::AreEqual (std::wstring (L"C:\\Work\\prog.sym"), links[0].path);
        }


        //  A path typed rooted is not echoed, so there is nothing to link.
        TEST_METHOD (ARootedPathIsNotLinked)
        {
            Rig                              rig;
            DebuggerViewState                view;
            std::vector<std::string>         lines;
            std::vector<ConsoleLinks::Link>  links;



            rig.files.WriteAllText (L"C:\\Work\\prog.sym", "0300 START\n");
            lines = view.ExecuteConsoleLine (rig.session, "SYM LOAD C:\\Work\\prog.sym", std::nullopt);

            Assert::AreEqual (std::wstring (L"Loaded 1 symbols into user."), ConsoleLinks::Parse (lines.at (1), links));
            Assert::AreEqual ((size_t) 0, links.size());
        }
    };
}
