#include "Pch.h"

#include "Debugger/HeatMapRangeSets.h"
#include "Debugger/HeatMapSymbols.h"
#include "Debugger/SymbolTable.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapRangeSetsTests
//
//  The heat map's ranges as the user writes them, read by the console's
//  evaluator against the symbols; the built-in ranges every set holds and
//  cannot lose; the sets kept as text; and the symbols' copy the ranges are
//  read against, with sizes from a cc65 debug file and the span of what was
//  loaded from files.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (HeatMapRangeSetsTests)
    {
    public:

        using Field = HeatMapRangeSets::Field;

        //  Symbols as a file would leave them: one with a size, one without,
        //  and a constant.
        static HeatMapSymbols MakeSymbols()
        {
            HeatMapSymbols  symbols;



            symbols.Add ("ZP_VARS", 0x0083, 0x0010, true);
            symbols.Add ("SPRITES", 0x6000, 0,      true);
            symbols.Add ("SCREEN",  0x0400, 0,      false);
            symbols.Add ("LIMIT",   0x9F00, 0,      true, true);
            return symbols;
        }

        static HeatMapRange MakeRange (const std::string & start, const std::string & end)
        {
            HeatMapRange  range;



            range.start = start;
            range.end   = end;
            return range;
        }

        //  The span a range reads as, or a failed assert with why not.
        static std::pair<Word, Word> Resolve (const HeatMapRange & range, const HeatMapSymbols & symbols)
        {
            Word         first  = 0;
            Word         last   = 0;
            std::string  error;
            bool         isRead = HeatMapRangeSets::TryResolve (range, symbols, first, last, error);



            Assert::IsTrue (isRead, std::wstring (error.begin(), error.end()).c_str());
            return { first, last };
        }



        TEST_METHOD (ASpanTypedAsOneSplitsIntoAStartAndAnEnd)
        {
            std::string  start;
            std::string  end;



            Assert::IsTrue   (HeatMapRangeSets::TrySplit ("$6000-$95FF", start, end));
            Assert::AreEqual (std::string ("$6000"), start);
            Assert::AreEqual (std::string ("$95FF"), end);

            Assert::IsTrue   (HeatMapRangeSets::TrySplit ("ZP_VARS..+$20", start, end));
            Assert::AreEqual (std::string ("ZP_VARS"), start);
            Assert::AreEqual (std::string ("+$20"),    end);

            Assert::IsTrue   (HeatMapRangeSets::TrySplit ("$300:$3FF", start, end), L"the console's start:last");
            Assert::AreEqual (std::string ("$3FF"), end);

            Assert::IsTrue   (HeatMapRangeSets::TrySplit ("$300,$100", start, end), L"and its start,length");
            Assert::AreEqual (std::string ("+$100"), end);

            Assert::IsFalse  (HeatMapRangeSets::TrySplit ("BUF-1", start, end), L"a dash after a symbol is a subtraction");
            Assert::IsFalse  (HeatMapRangeSets::TrySplit ("ZP_VARS", start, end));
        }



        TEST_METHOD (ARangeReadsAsTheConsoleReadsAddressesAndSymbols)
        {
            HeatMapSymbols  symbols = MakeSymbols();



            Assert::IsTrue ((std::pair<Word, Word> { 0x6000, 0x95FF }) == Resolve (MakeRange ("$6000", "$95FF"), symbols), L"an address range");
            Assert::IsTrue ((std::pair<Word, Word> { 0x6000, 0x95FF }) == Resolve (MakeRange ("6000", "95FF"), symbols), L"bare numbers are hex, as in a command");
            Assert::IsTrue ((std::pair<Word, Word> { 0x0083, 0x00A2 }) == Resolve (MakeRange ("ZP_VARS", "+$20"), symbols), L"a symbol and a length");
            Assert::IsTrue ((std::pair<Word, Word> { 0x0083, 0x0092 }) == Resolve (MakeRange ("zp_vars", ""), symbols), L"a symbol alone takes its size, its case aside");
            Assert::IsTrue ((std::pair<Word, Word> { 0x6000, 0x9EFF }) == Resolve (MakeRange ("SPRITES", "LIMIT-1"), symbols), L"symbols and arithmetic at both ends");
        }



        TEST_METHOD (ARangeThatDoesNotReadSaysWhy)
        {
            HeatMapSymbols  symbols = MakeSymbols();
            Word            first   = 0;
            Word            last    = 0;
            std::string     error;



            Assert::IsFalse (HeatMapRangeSets::TryResolve (MakeRange ("SPRITES", ""), symbols, first, last, error));
            Assert::IsTrue  (error.find ("SPRITES has no size of its own") != std::string::npos, L"a symbol with no size needs an end");

            error.clear();
            Assert::IsFalse (HeatMapRangeSets::TryResolve (MakeRange ("NOSUCH", "+4"), symbols, first, last, error));
            Assert::IsFalse (error.empty(), L"the evaluator's own reason for a name it does not know");

            error.clear();
            Assert::IsFalse (HeatMapRangeSets::TryResolve (MakeRange ("$2000", "$1000"), symbols, first, last, error));
            Assert::IsTrue  (error.find ("before its start") != std::string::npos, L"the console's own words");

            error.clear();
            Assert::IsFalse (HeatMapRangeSets::TryResolve (MakeRange ("", "$1000"), symbols, first, last, error));
            Assert::IsFalse (error.empty());
        }



        TEST_METHOD (TheBuiltInRangesCoverTheMachinesRegions)
        {
            HeatMapSymbols  symbols;
            HeatMapRange    range;



            range.preset = "hires1";
            Assert::IsTrue ((std::pair<Word, Word> { 0x2000, 0x3FFF }) == Resolve (range, symbols));
            range.preset = "io";
            Assert::IsTrue ((std::pair<Word, Word> { 0xC000, 0xC0FF }) == Resolve (range, symbols));
            range.preset = "zeropage";
            Assert::IsTrue ((std::pair<Word, Word> { 0x0000, 0x00FF }) == Resolve (range, symbols));

            Assert::AreEqual ((size_t) 9, HeatMapRangeSets::GetPresets().size(), L"eight regions and the program");
            Assert::AreEqual (std::string ("Hi-res page 1"), HeatMapRangeSets::GetName ({ {}, {}, {}, "hires1", false }));
        }



        TEST_METHOD (MyProgramSpansTheLoadedSymbolsAndHidesWithoutThem)
        {
            HeatMapSymbols  none;
            HeatMapSymbols  symbols = MakeSymbols();
            HeatMapRange    program;



            program.preset = HeatMapRangeSets::kProgramPreset;

            Assert::IsTrue  (HeatMapRangeSets::IsHidden (program, none), L"no symbols loaded, no program");
            Assert::IsFalse (HeatMapRangeSets::IsHidden (program, symbols));
            Assert::IsTrue  ((std::pair<Word, Word> { 0x0083, 0x6000 }) == Resolve (program, symbols),
                             L"from ZP_VARS to SPRITES; SCREEN is a built-in symbol and LIMIT a constant");
        }



        TEST_METHOD (EverySetHoldsEveryBuiltInRangeLeftOut)
        {
            HeatMapRangeSet  set = HeatMapRangeSets::MakeSet ("Game");



            Assert::AreEqual (HeatMapRangeSets::GetPresets().size(), set.ranges.size());
            Assert::IsTrue   (std::ranges::all_of (set.ranges, [] (const HeatMapRange & range) { return HeatMapRangeSets::IsBuiltIn (range) && !range.included; }));
        }



        TEST_METHOD (ABuiltInRangeCannotBeDeletedOrChangedButMoves)
        {
            HeatMapRangeSet  set     = HeatMapRangeSets::MakeSet ("Game");
            HeatMapSymbols   symbols = MakeSymbols();
            HeatMapRange     first   = set.ranges[0];
            std::string      error;



            Assert::IsFalse (HeatMapRangeSets::TryRemoveRange (set, 0, error));
            Assert::IsTrue  (error.find ("cannot be deleted") != std::string::npos);
            Assert::AreEqual (HeatMapRangeSets::GetPresets().size(), set.ranges.size(), L"still there");

            error.clear();
            Assert::IsFalse (HeatMapRangeSets::TryEditRange (set, 0, Field::Start, "$1000-$1FFF", symbols, error));
            Assert::IsTrue  (error.find ("cannot be changed") != std::string::npos);
            Assert::IsTrue  (first == set.ranges[0], L"unchanged");

            Assert::IsTrue  (HeatMapRangeSets::TryMoveRange (set, 0, 1), L"but it moves");
            Assert::IsTrue  (first == set.ranges[1]);
            Assert::IsFalse (HeatMapRangeSets::TryMoveRange (set, 0, -1), L"not past the top");
        }



        TEST_METHOD (AUsersRangeEditsAndARefusedEditLeavesItAsItWas)
        {
            HeatMapRangeSet  set     = HeatMapRangeSets::MakeSet ("Game");
            HeatMapSymbols   symbols = MakeSymbols();
            size_t           index   = set.ranges.size();
            std::string      error;



            set.ranges.push_back ({});

            Assert::IsTrue   (HeatMapRangeSets::TryEditRange (set, index, Field::Start, "$6000-$95FF", symbols, error), L"a whole span in the start");
            Assert::AreEqual (std::string ("$6000"), set.ranges[index].start);
            Assert::AreEqual (std::string ("$95FF"), set.ranges[index].end);

            Assert::IsTrue   (HeatMapRangeSets::TryEditRange (set, index, Field::Size, "$100", symbols, error), L"a size is a length");
            Assert::AreEqual (std::string ("+$100"), set.ranges[index].end);

            Assert::IsTrue   (HeatMapRangeSets::TryEditRange (set, index, Field::Name, "Sprites", symbols, error));
            Assert::AreEqual (std::string ("Sprites"), HeatMapRangeSets::GetName (set.ranges[index]));

            Assert::IsFalse  (HeatMapRangeSets::TryEditRange (set, index, Field::End, "$5000", symbols, error), L"an end before the start");
            Assert::AreEqual (std::string ("+$100"), set.ranges[index].end, L"kept as it was");

            Assert::IsTrue   (HeatMapRangeSets::TryRemoveRange (set, index, error), L"the user's own range goes");
            Assert::AreEqual (HeatMapRangeSets::GetPresets().size(), set.ranges.size());
        }



        TEST_METHOD (TheSetsRoundTripThroughText)
        {
            HeatMapRangeSets  sets;
            HeatMapRangeSets  read;
            HeatMapRangeSet   game  = HeatMapRangeSets::MakeSet ("My \"best\" game");
            HeatMapRangeSet   other = HeatMapRangeSets::MakeSet ("Other");
            std::string       text;



            game.ranges[3].included = true;
            game.ranges.insert (game.ranges.begin(), HeatMapRange { "Sprite \\ tables", "SPRITES", "+$3600", {}, true });
            game.ranges.push_back (HeatMapRange { {}, "ZP_VARS", {}, {}, false });
            std::swap (game.ranges[1], game.ranges[4]);

            sets.sets  = { game, other };
            sets.shown = game.name;

            read = HeatMapRangeSets::FromText (sets.ToText());

            text = sets.ToText();
            Logger::WriteMessage (std::wstring (text.begin(), text.end()).c_str());
            text = read.ToText();
            Logger::WriteMessage (std::wstring (text.begin(), text.end()).c_str());

            Assert::IsTrue   (sets == read, L"every set, range, check and the set shown");
            Assert::IsFalse  (sets.ToText().empty());
            Assert::IsFalse  (HeatMapRangeSets().ToText().empty(), L"never empty, even with no sets");
        }



        TEST_METHOD (TextFromAnotherBuildStillGivesUsableSets)
        {
            HeatMapRangeSets  read = HeatMapRangeSets::FromText ("show \"Gone\"\n"
                                                                 "set \"Game\"\n"
                                                                 "preset io on\n"
                                                                 "preset future on\n"
                                                                 "preset io off\n"
                                                                 "range on \"Sprites\" \"$6000\" \"$95FF\"\n"
                                                                 "garbage\n"
                                                                 "set \"Game\"\n"
                                                                 "range on \"Lost\" \"$0\" \"$1\"\n");



            Assert::AreEqual ((size_t) 1, read.sets.size(), L"a second set of one name is dropped");
            Assert::IsTrue   (read.shown.empty(), L"a set shown that is gone shows all memory");
            Assert::AreEqual (std::string ("io"), read.sets[0].ranges[0].preset);
            Assert::IsTrue   (read.sets[0].ranges[0].included, L"a preset listed twice is kept where it first appears");
            Assert::AreEqual (std::string ("Sprites"), read.sets[0].ranges[1].name);
            Assert::AreEqual (HeatMapRangeSets::GetPresets().size() + 1, read.sets[0].ranges.size(), L"the presets it lacked are added, left out");
        }



        TEST_METHOD (SetsTakeNamesNoOtherSetHas)
        {
            HeatMapRangeSets  sets;
            std::string       error;



            Assert::AreEqual (std::string ("Set 1"), sets.GetNewSetName());
            Assert::IsTrue   (sets.TryAddSet ("Set 1", error));
            Assert::AreEqual (std::string ("Set 2"), sets.GetNewSetName());
            Assert::IsFalse  (sets.TryAddSet ("Set 1", error), L"taken");
            Assert::IsFalse  (sets.TryAddSet ("  ", error), L"empty");
            Assert::IsFalse  (sets.TryAddSet ("All memory", error), L"the map's own choice");

            sets.shown = "Set 1";
            Assert::IsTrue   (sets.TryRenameSet ("Set 1", " Game ", error));
            Assert::AreEqual (std::string ("Game"), sets.shown, L"the set shown follows its rename");
            Assert::IsTrue   (sets.TryRenameSet ("Game", "Game", error), L"its own name again");

            Assert::IsTrue   (sets.TryDeleteSet ("Game"));
            Assert::IsTrue   (sets.shown.empty(), L"all memory once the set shown is gone");
        }



        TEST_METHOD (TheMapShowsTheShownSetsIncludedRangesInOrder)
        {
            HeatMapRangeSets                      sets;
            HeatMapRangeSet                       game    = HeatMapRangeSets::MakeSet ("Game");
            HeatMapSymbols                        symbols = MakeSymbols();
            std::vector<HeatMapRangeSets::Span>   spans;



            game.ranges.insert (game.ranges.begin(), HeatMapRange { "Sprites", "$6000", "$95FF", {}, true });
            game.ranges.push_back (HeatMapRange { "Broken", "NOSUCH", "+1", {}, true });
            game.ranges.push_back (HeatMapRange { "Off", "$300", "$3FF", {}, false });
            game.ranges[1].included = true;     // the zero page
            game.ranges[9].included = true;     // My program

            sets.sets = { game };
            Assert::IsTrue (sets.GetShownSpans (symbols).empty(), L"all memory shows no set");

            sets.shown = "Game";
            spans      = sets.GetShownSpans (symbols);

            Assert::AreEqual ((size_t) 3, spans.size(), L"what does not read and what is left out are not shown");
            Assert::IsTrue   ((HeatMapRangeSets::Span { "Sprites",    0x6000, 0x95FF }) == spans[0]);
            Assert::IsTrue   ((HeatMapRangeSets::Span { "Zero page",  0x0000, 0x00FF }) == spans[1]);
            Assert::IsTrue   ((HeatMapRangeSets::Span { "My program", 0x0083, 0x6000 }) == spans[2]);

            spans = sets.GetShownSpans (HeatMapSymbols());
            Assert::AreEqual ((size_t) 2, spans.size(), L"no program without symbols");
        }



        TEST_METHOD (TheSymbolsCopyTakesTheEnabledTablesSizesAndLoadedSpan)
        {
            SymbolTable                            table;
            std::shared_ptr<const HeatMapSymbols>  copy;
            Word                                   address = 0;
            Word                                   size    = 0;
            Word                                   first   = 0;
            Word                                   last    = 0;
            uint64_t                               before  = table.GetRevision();



            table.Add (SymbolTableId::Main, "HOME",  0xFC58);
            table.Add (SymbolTableId::User, "START", 0x0800);
            table.Add (SymbolTableId::User, "TABLE", 0x0900, false, 0x40);
            table.AddOrigin (SymbolTableId::User, "game.sym");
            table.Add (SymbolTableId::Src,  "HIDDEN", 0x1234);

            Assert::AreNotEqual (before, table.GetRevision(), L"a change moves the revision");

            copy = HeatMapSymbols::Build (table);

            Assert::IsTrue   (copy->TryResolveSymbol ("home", address));
            Assert::AreEqual ((Word) 0xFC58, address);
            Assert::IsFalse  (copy->TryResolveSymbol ("HIDDEN", address), L"a table not enabled");
            Assert::IsTrue   (copy->TryGetSize ("TABLE", size));
            Assert::AreEqual ((Word) 0x40, size);
            Assert::IsFalse  (copy->TryGetSize ("START", size));
            Assert::IsTrue   (copy->TryGetProgramSpan (first, last));
            Assert::AreEqual ((Word) 0x0800, first, L"the loaded table, not Main");
            Assert::AreEqual ((Word) 0x093F, last,  L"through TABLE's size");
        }



        TEST_METHOD (ACc65DebugFileGivesALabelItsSize)
        {
            SymbolTable  table;
            size_t       loaded  = 0;
            std::string  error;
            Word         size    = 0;
            HRESULT      hr      = S_OK;
            std::string  content = "version\tmajor=2,minor=0\n"
                                   "seg\tid=0,name=\"DATA\",start=0x000083,size=0x0020,addrsize=zeropage,type=rw\n"
                                   "scope\tid=0,name=\"\",mod=0,size=32\n"
                                   "sym\tid=0,name=\"ZP_VARS\",addrsize=zeropage,size=32,scope=0,def=0,ref=0,val=0x83,seg=0,type=lab\n"
                                   "sym\tid=1,name=\"FLAG\",addrsize=zeropage,scope=0,def=0,ref=0,val=0x90,seg=0,type=lab\n";



            hr = table.LoadFrom (SymbolTableId::User, content, 0, loaded, error);

            Assert::IsTrue   (SUCCEEDED (hr), std::wstring (error.begin(), error.end()).c_str());
            Assert::IsTrue   (table.TryGetSize ("ZP_VARS", size), L"the file's size= for the label");
            Assert::AreEqual ((Word) 32, size);
            Assert::IsFalse  (table.TryGetSize ("FLAG", size), L"none given, none known");
        }
    };
}





