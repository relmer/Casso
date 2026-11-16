#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  AcceleratorTableTests
//
//  The accelerator table in Casso.rc. TranslateAccelerator matches an entry
//  only when the Alt state matches its ALT flag, so a shortcut that takes Alt
//  as Open Apple has an entry with the flag and one without.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (AcceleratorTableTests)
{
public:

    //  Ctrl+Shift+R resets the machine, and with Alt down as Open Apple or
    //  Closed Apple it is a Ctrl-Open-Apple-Reset.
    TEST_METHOD (Reset_TakesCtrlShiftRWithOrWithoutAlt)
    {
        std::vector<std::string>  entries = ReadEntries ("IDM_MACHINE_RESET");



        Assert::IsTrue (HasEntry (entries, "\"R\"", false), L"Ctrl+Shift+R resets");
        Assert::IsTrue (HasEntry (entries, "\"R\"", true),  L"Ctrl+Alt+Shift+R resets");
    }


private:

    static std::string GetRcPath()
    {
        std::string  thisFile = __FILE__;
        size_t       lastSep  = thisFile.find_last_of ("\\/");
        std::string  emuTests = thisFile.substr (0, lastSep);



        return emuTests + "\\..\\..\\Casso\\Casso.rc";
    }


    //  The lines of the accelerator table that dispatch the command.
    static std::vector<std::string> ReadEntries (const std::string & command)
    {
        std::ifstream             file (GetRcPath());
        std::string               line;
        std::vector<std::string>  entries;
        bool                      inTable = false;



        Assert::IsTrue (file.is_open(), L"Casso.rc opens");

        while (std::getline (file, line))
        {
            if (line.find ("ACCELERATORS") != std::string::npos)
            {
                inTable = true;
            }
            else if (inTable && line.rfind ("END", 0) == 0)
            {
                inTable = false;
            }
            else if (inTable && line.find (command + ",") != std::string::npos)
            {
                entries.push_back (line);
            }
        }

        return entries;
    }


    static bool HasEntry (const std::vector<std::string> & entries, const std::string & key, bool alt)
    {
        bool  found = false;



        for (const std::string & entry : entries)
        {
            bool  isKey   = entry.find (key)       != std::string::npos;
            bool  isCtrl  = entry.find ("CONTROL") != std::string::npos;
            bool  isShift = entry.find ("SHIFT")   != std::string::npos;
            bool  isAlt   = entry.find ("ALT")     != std::string::npos;

            found = found || (isKey && isCtrl && isShift && isAlt == alt);
        }

        return found;
    }
};
