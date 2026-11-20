#pragma once
#include <BinaryData.h>
#include <cstring>

// Finds a resource built into the plug-in by its original file name (e.g. "knob.png").
inline const char* getResource (const char* originalName, int& size)
{
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        const char* name = BinaryData::namedResourceList[i];
        if (const char* orig = BinaryData::getNamedResourceOriginalFilename (name))
            if (std::strcmp (orig, originalName) == 0)
                return BinaryData::getNamedResource (name, size);
    }
    size = 0;
    return nullptr;
}
