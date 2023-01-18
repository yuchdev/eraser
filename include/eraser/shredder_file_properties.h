#pragma once
#include <cstdint>

namespace shredder {


/// @brief Properties of shredded file
/// Set of binary flags, composed in a 64-bit value
class ShredderFileProperties {

public:

    ShredderFileProperties() = default;
    ~ShredderFileProperties() = default;

    /// @brief Binary flags for representing file properties
    enum FilePropertyFlags
    {
        SystemAdded = 0x01LL,
        IsFile = 0x02LL,
        Reserved1 = 0x04LL,
        Reserved2 = 0x08LL
    };

    /// @brief: If the file was added by application (browser or OS-related), not by user
    void set_system_added(bool system_added);

    /// @brief: If the file is regular (not directory, symlink etc)
    void set_is_file(bool is_file);

    /// @brief: If the file is regular (not directory, symlink etc)
    bool is_file() const;

    /// @brief: If the file was added by application (browser or OS-related), not by user
    bool is_system_added() const;

    /// @brief: Read all flags (usually reading from database)
    int64_t get_flags() const { return flags_; }

    /// @brief: Rewrite all flags (for writing to database)
    void set_flags(int64_t flags) { flags_ = flags; }

private:

    ///
    int64_t flags_{};
};

} // namespace shredder
