#include "Pch.h"

#include "Shell/Input/AppleKeyMapping.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleKeyMapping::TryMapVkToSpecialKey
//
//  The seven host keys an Apple keyboard identifies by the key rather than by
//  the character it produces.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleKeyMapping::TryMapVkToSpecialKey (WPARAM vk, AppleSpecialKey & outKey)
{
    bool  mapped = true;



    switch (vk)
    {
        case VK_LEFT:   outKey = AppleSpecialKey::Left;   break;
        case VK_RIGHT:  outKey = AppleSpecialKey::Right;  break;
        case VK_UP:     outKey = AppleSpecialKey::Up;     break;
        case VK_DOWN:   outKey = AppleSpecialKey::Down;   break;
        case VK_TAB:    outKey = AppleSpecialKey::Tab;    break;
        case VK_ESCAPE: outKey = AppleSpecialKey::Escape; break;
        case VK_DELETE: outKey = AppleSpecialKey::Delete; break;

        default:
            mapped = false;
            break;
    }

    return (mapped);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleKeyMapping::DoesSpecialKeySynthesizeChar
//
//  Only TAB ($09) and Escape ($1B) are character keys in Windows' eyes; the
//  arrows and DELETE produce a keydown and nothing else.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleKeyMapping::DoesSpecialKeySynthesizeChar (AppleSpecialKey key)
{
    bool  synthesizes = key == AppleSpecialKey::Tab || key == AppleSpecialKey::Escape;



    return (synthesizes);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleKeyMapping::IsArrowVk
//
////////////////////////////////////////////////////////////////////////////////

bool AppleKeyMapping::IsArrowVk (WPARAM vk)
{
    bool  isArrow = vk == VK_LEFT || vk == VK_RIGHT || vk == VK_UP || vk == VK_DOWN;



    return (isArrow);
}
