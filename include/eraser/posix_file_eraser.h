#if defined (__unix__) || (defined (__APPLE__) && defined (__MACH__))
#pragma once
#include <string>
#include <random>
#include <Windows.h>

namespace boost{
namespace filesystem {
class path;
}
}


class NativeFileEraser {

    enum class ErasureMethod {
        Strong,
        Weak,
        Random,
        BeginEnd
    };

    enum class FileSpecific {
        Plain,
        Binary,
        Encrypted,
        Unknown
    };

public:

    NativeFileEraser() = default;

    // call open
    // if plain - strong erasure methods
    // if encrypted - restrict erasure methods
    NativeFileEraser(const std::wstring& filename, FileSpecific attributes = FileSpecific::Unknown);

    // @brief erase, close (change attributes)
    ~NativeFileEraser() = default;

    /// @brief
    // open file (try twice)
    // get size
    // if big - restrict erasure methods
    bool open(const std::wstring& filename);

    /// @brief
    void close();

    /// @brief
    // erase full
    bool erase_full(BYTE* start_mask, size_t mask_length = 65536);

    /// @brief
    // erase random
    bool erase_random(BYTE* start_mask, size_t mask_length, size_t areas = 0);

    /// @brief
    // erase begin and end
    bool erase_begin_end(BYTE* start_mask, size_t mask_length);

    /// @brief
    // erase smart (choose better way)
    bool erase_smart(BYTE* start_mask, size_t mask_length);

private:

    /// mark file anchor points
    bool prepare();

    /// do not throw, it's time-critical class
    bool try_open(const std::string& filename);

private:

    std::wstring initial_filepath_;

    bool prepared_to_erase_ = false;

    bool big_file_ = false;

    bool is_file_compressed_;

    FileSpecific information_estimation_ = FileSpecific::Unknown;

    LONGLONG file_size_ = 0;

    DWORD last_pointer_ = 0;

    DWORD file_attributes_ = INVALID_FILE_ATTRIBUTES;

    HANDLE file_handle_ = INVALID_HANDLE_VALUE;

    static constexpr size_t megabyte_ = 1024 * 1024;

    static std::vector<std::wstring> fake_extensions;

    static std::default_random_engine generator;
};
#endif // defined (__unix__) || (defined (__APPLE__) && defined (__MACH__))
