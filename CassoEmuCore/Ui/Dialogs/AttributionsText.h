#pragma once

#include "Ui/Dialogs/DialogDefinition.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AttributionsText
//
//  The works Casso and Casso Explorer use under someone else's license, as
//  the lines an Attributions dialog shows. Both applications show the same
//  list from here, since they ship together and use the same works: a list
//  kept in two places is a list that will disagree.
//
//  The entries state the author, the license and where each came from. The
//  icons are not a work of their own -- they are drawn from the photograph,
//  so the line that says so belongs under the photograph's entry.
//
////////////////////////////////////////////////////////////////////////////////

class AttributionsText
{
public:
    static std::vector<DialogTextRun>  BuildBody ();

    static constexpr const wchar_t *  kPhotoUrl   = L"https://bunyipco.blogspot.com/2015/04/cassowary-update.html";
    static constexpr const wchar_t *  kPhotoLicense = L"https://creativecommons.org/licenses/by-nc-sa/3.0/";
    static constexpr const wchar_t *  kSoundsUrl  = L"https://github.com/BleuLlama/ImageWriterIISimulator";
    static constexpr const wchar_t *  kSoundsLicense = L"https://creativecommons.org/licenses/by/4.0/";
};
