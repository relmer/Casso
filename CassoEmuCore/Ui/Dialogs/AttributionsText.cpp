#include "Pch.h"
#include "Ui/Dialogs/AttributionsText.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AttributionsText::BuildBody
//
//  Each work gets its title and author as prose, then its source and its
//  license as two links, then a blank line. The photograph's entry ends with
//  the sentence about the icons, which are drawn from it and take its terms
//  with them.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DialogTextRun> AttributionsText::BuildBody()
{
    std::vector<DialogTextRun>  runs;



    runs.push_back ({ L"Casso and Casso Explorer use these works under the licenses below." });
    runs.push_back ({ L"" });

    runs.push_back ({ L"Cassowary photograph, by Mr. Smiley / BunyipCo" });
    runs.push_back ({ L"Casso's and Casso Explorer's icons are drawn from it and are licensed under the same terms." });
    runs.push_back ({ L"The photograph", true, kPhotoUrl });
    runs.push_back ({ L"CC BY-NC-SA 3.0", true, kPhotoLicense });
    runs.push_back ({ L"" });

    runs.push_back ({ L"ImageWriter II printer sounds, by Scott Lawrence" });
    runs.push_back ({ L"The recordings", true, kSoundsUrl });
    runs.push_back ({ L"CC BY 4.0", true, kSoundsLicense });
    runs.push_back ({ L"" });

    //  MIT asks for the copyright and the permission notice with every copy;
    //  the notice is the license, which the link opens.
    runs.push_back ({ L"Casso Explorer's command bar icons: Fluent UI System Icons, by Microsoft" });
    runs.push_back ({ L"Copyright (c) 2020 Microsoft Corporation" });
    runs.push_back ({ L"The icons", true, kIconsUrl });
    runs.push_back ({ L"MIT License", true, kIconsLicense });

    return runs;
}
