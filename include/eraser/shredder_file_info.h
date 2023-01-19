#pragma once
#include <string>
#include <cstdint>
#include <eraser/shredder_file_properties.h>


namespace shredder {

struct ShredderFileInfo
{
    /// @brief Construct 
    ShredderFileInfo(const std::wstring& p, double ent, int64_t flg) : 
        path(p), entropy(ent)
    {
        flags.set_flags(flg);
    }

    std::wstring path;
    double entropy;
    ShredderFileProperties flags;
};

} // namespace shredder

