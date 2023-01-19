#if defined(_WIN32) || defined(_WIN64)
#pragma once
#include <string>
#include <random>
#include <Windows.h>
#include <eraser/encryption_checker.h>
#include <winapi-helpers/partition_information.h>

namespace shredder {

/// @brief Wrapper for whole or partial (smart) erase of the file under Windows
/// Single file eraser is not thread-safe, strongly advice using it in a single thread
class NativeFileEraser {

public:

    using EntropyEstimation = shredder::ShannonEncryptionChecker::InformationEntropyEstimation;
    using DiskType = helpers::PartititonInformation::DiskType;

    NativeFileEraser(const NativeFileEraser&) = delete;
    NativeFileEraser& operator=(const NativeFileEraser&) = delete;

    /// @brief Open file for write only
    // @param estimation: means encryption level.
    /// If plain - strong erasure methods applied,
    /// If encrypted - smart erasure methods
    /// @param disk_type: SSD or HDD, optimization of erasure process
    NativeFileEraser(const std::wstring& filename, EntropyEstimation estimation, DiskType disk_type);

    // erase, close (change attributes)
    ~NativeFileEraser();

    /// @brief Clean journal after all files are erased (ANSI version)
    static bool clean_ntfs_journal(char drive_letter);

    /// @brief Clean journal after all files are erased (Wide char version)
    static bool clean_ntfs_journal(wchar_t drive_letter);

    /// @brief Clean journal after all files are erased (Wide string version)
    static bool clean_ntfs_journal(const std::wstring& drive_root);

    /// @brief Open file (try twice), get size, if file is big (> 4 Gb) - set smart erasure method
    bool open(const std::wstring& filename);

    /// @brief Close file (should be dome before file node erasure)
    void close();

    /// @brief Erase the whole file from first to last byte
    bool erase_full(uint8_t* start_mask, size_t mask_length);

    /// @brief Erase beginning, end and random parts of the file
    bool erase_random(uint8_t* start_mask, size_t mask_length);

    /// @brief Erase begin and end only
    bool erase_begin_end(uint8_t* start_mask, size_t mask_length);

    /// @brief Smart erase (choose better way depending on file and drive type)
    bool erase_smart(uint8_t* start_mask, size_t mask_length);

private:

    /// mark file anchor points, it would increase probability of writing to the same blocks
    bool prepare();

    /// compressed, windows-encrypted or sparse file
    bool is_file_compressed(DWORD file_attributes) const;

    /// Try opening file. Does not throw, it's time-critical class
    bool try_open(const std::wstring& filename);

    /// Clean journal after all files are erased using volume handle
    static bool clean_ntfs_journal(HANDLE volume_handle);

    /// Volume handle by drive letter (ANSI version)
    static HANDLE get_volume_handle(const char drive_letter);

    /// Volume handle by drive letter (Wide char version)
    static HANDLE get_volume_handle(const wchar_t drive_letter);

private:

    // Canonical file path
    std::wstring initial_filepath_;

    // HDD/SSD/Unknown
    DiskType disk_type_ = helpers::PartititonInformation::UnknownType;

    // Make sure we performed some tricks that allows filesystem driver write to the same blocks (just probability)
    bool prepared_to_erase_ = false;

    // File > 4Gb
    bool big_file_ = false;

    // File is compressed of encrypted using NTFS encryption
    bool is_file_compressed_ = false;

    // Type of information. Plain/Binary/Encrypted/Unknown
    shredder::ShannonEncryptionChecker::InformationEntropyEstimation information_estimation_ =
        shredder::ShannonEncryptionChecker::Unknown;

    // Size of the file in a moment of eraser creation
    LONGLONG file_size_ = 0;

    // Erasure pointer this moment
    DWORD last_pointer_ = 0;

    // Windows file attributes
    DWORD file_attributes_ = INVALID_FILE_ATTRIBUTES;

    // Windows file handle
    HANDLE file_handle_ = INVALID_HANDLE_VALUE;

    // Just not to calculate every time
    static constexpr size_t megabyte_ = 1024 * 1024;

    // Choose random areas in a big file to erase
    static std::default_random_engine generator_;
};

} // namespace shredder

#endif // defined(_WIN32) || defined(_WIN64)
