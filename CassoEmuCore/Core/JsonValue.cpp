#include "Pch.h"

#include "JsonValue.h"





////////////////////////////////////////////////////////////////////////////////
//
//  JsonValue::Find
//
////////////////////////////////////////////////////////////////////////////////

const JsonValue * JsonValue::Find (const string & key) const
{
    for (const auto & entry : m_object)
    {
        if (entry.first == key)
        {
            return &entry.second;
        }
    }

    return nullptr;
}
