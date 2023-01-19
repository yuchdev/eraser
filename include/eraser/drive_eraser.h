#pragma once
#include <string>
#include <vector>
#include <map>
#include <random>
#include <memory>
#include <functional>
#include <chrono>
#include <cstdint>
#include <future>
#include <mutex>
#include <winapi-helpers/partition_information.h>
#include <winapi-helpers/dynamic_handler_map.h>
#include <eraser/random_generator.h>
#include <eraser/shredder_file_info.h>


namespace boost {
namespace filesystem {
    class path;
} // filesystem 
} // boost 

namespace encryption {
class ShannonEncryptionChecker;
}

namespace shredder {

class NativeFileEraser;

#ifdef ERASE_PROFILING
struct OutputInfo
{
    std::wstring path;
    std::wstring filename;
    std::string information_type;
    double msec;
    bool success;
};
#endif

// @brief Eraser for the one physical drive (HDD/SSD/Unknown drive)
class DriveEraser {

public:

    using DiskType = helpers::PartititonInformation::DiskType;

    // Full - all file, Random - begin, end and random areas in the middle, BeginEnd - only begin and End (suitable for excrypted)
    enum class ErasureMethod {
        Smart,
        Full,
        Random,
        BeginEnd
    };

    // @brief Also accept erasure type (Smart by default) and disk type
    DriveEraser(
        ErasureMethod erasure_method,
        DiskType disk_type,
        std::vector<helpers::PartititonInformation::PortablePartititon>& partitions);
#if 1
    DriveEraser(ErasureMethod erasure_method,
                int disk_type,
                std::vector<helpers::PartititonInformation::PortablePartititon>&
                partitions);
#endif
    // @brief Satisfy compiler
    ~DriveEraser() = default;

    /// @brief Shred files on this particular drive
    void shred_files();
    
    /// @brief Submit file root and path
    void submit(const std::wstring& root, const std::wstring& file_path, double entropy);
    
    /// @brief Remove file root and path
    void remove(const std::wstring& root, const std::wstring& file_path);

    /// @brief Submit directory path
    void submit_dir(const std::wstring& root, const std::wstring& file_path);

    /// @brief Remove directory path
    void remove_dir(const std::wstring& root, const std::wstring& file_path);

    /// @brief Check if record already in cache
    bool already_exist(const std::wstring& root, const std::wstring& file_path);

    /// @brief Cleanup erasure list
    void clean();

    /// @brief Return files prepared for erase this moment
    std::map<std::wstring, double> files_prepared() const;

    /// @brief Return directories prepared for erase this moment
    std::vector<std::wstring> directories_prepared() const;

private:

    /// pass by value so that handle std::move and async execution
    void erase_file(std::wstring file_path, double entropy);

    /// Multiple rename of the file
    bool cheat_file_node(const std::wstring& initial_path);

private:

    /// Lock submit-remove operations
    std::mutex files_lock_;

    /// List of drive partitions
    std::vector<helpers::PartititonInformation::PortablePartititon> partitions_;

    /// Save erase info in multimap so that keep sorted by the key (root path of the partition)
    /// Pair is <root, FileEraseInfo>
    std::multimap<std::wstring, shredder::ShredderFileInfo> shredded_files_;

    /// Save directories. Due to performance reasons they can't be shredded, just removed by OS function
    /// Pair is <root, direcory_path>
    std::multimap<std::wstring, std::wstring> shredded_directories_;

    /// Erasure method, see enum
    ErasureMethod erasure_method_ = ErasureMethod::Smart;

    /// Disk type SSD/HDD/Unknown
    DiskType disk_type_ = helpers::PartititonInformation::UnknownType;

    /// Save random generated sequence for erasure
    RandomGenerator gen_;

    /// Map installation response codes to handle actions
    helpers::HandlerMap <
        ErasureMethod,
        std::function<bool(shredder::NativeFileEraser*, uint8_t*, size_t)
        >>
        erasure_type_handler_;
};

} // namespace shredder
