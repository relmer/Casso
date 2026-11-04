#pragma once

#include "Pch.h"

#include "Machines/Apple2/Common/WozLoader.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WozProperties
//
//  What Disk properties offers for a WOZ image's INFO and META, from the WOZ
//  reference: META's standard keys in the reference's order, the machines of
//  INFO's compatible hardware and META's requires_machine, INFO's RAM sizes,
//  META's RAM and language values, and the conversion of typed text.
//
////////////////////////////////////////////////////////////////////////////////

class WozProperties
{
public:
    struct MetaKey
    {
        const char     * key   = nullptr;
        const wchar_t  * label = nullptr;
    };

    struct Machine
    {
        uint16_t         bit   = 0;         // INFO's compatible hardware
        const char     * code  = nullptr;   // META's requires_machine
        const wchar_t  * label = nullptr;
    };

    struct RamSize
    {
        uint16_t         kilobytes = 0;   // INFO's required RAM; 0 is unknown
        const wchar_t  * label     = nullptr;
    };

    static constexpr size_t        kMachineCount    = 9;
    static constexpr const char *  kTitle           = "title";
    static constexpr const char *  kRequiresRam     = "requires_ram";
    static constexpr const char *  kRequiresMachine = "requires_machine";
    static constexpr const char *  kLanguage        = "language";

    static const std::vector<MetaKey>       & GetMetaKeys         ();
    static const std::vector<Machine>       & GetMachines         ();
    static const std::vector<RamSize>       & GetRamSizes         ();
    static const std::vector<std::wstring>  & GetBootSectorLabels ();
    static const std::vector<std::wstring>  & GetMetaRamValues    ();
    static const std::vector<std::wstring>  & GetLanguages        ();

    static bool         IsStandardKey      (const std::string & key);
    static std::string  FormatMachineCodes (uint16_t machines);
    static uint16_t     ParseMachineCodes  (const std::string & value);

    //  A value as typed, as UTF-8, with each tab or line break as a space and
    //  the spaces at either end removed.
    static std::string  ToMetaValue (const std::wstring & typed);

    //  The standard keys in the reference's order, then the other keys in the
    //  order given. A key with no value is left out.
    static std::vector<WozLoader::MetaField>  MakeMeta (const std::vector<std::pair<std::string, std::wstring>> & typed);

    //  Why Disk properties' edits weren't saved, from WozLoader::ApplyEdits or a
    //  read or write of the file.
    static std::wstring  FormatEditError (HRESULT hr);

};
