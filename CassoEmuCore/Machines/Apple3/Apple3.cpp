#include "Pch.h"

#include "Apple3.h"





////////////////////////////////////////////////////////////////////////////////
//
//  GetVideoModes
//
//  The seven modes the display switches select.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> Apple3::GetVideoModes() const
{
    return { "apple3-text40", "apple3-text40-color", "apple3-text80",
             "apple3-hires", "apple3-hires-color", "apple3-hires560", "apple3-color140" };
}
