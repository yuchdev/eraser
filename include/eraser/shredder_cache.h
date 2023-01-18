#pragma once
#include <file_shredder/shredder_file_info.h>
#include <file_shredder/drive_eraser.h>
#include <helpers/partition_information.h>

#include <map>
#include <set>
#include <vector>
#include <string>
#include <atomic>


namespace boost {
namespace filesystem {
    class path;
} // filesystem 
} // boost 

namespace shredder {

/// @brief File Shredder Cache
/// Warning: the cache itself is not thread-safe, thread-safety should be provided by the user class
/// (FileShredder in our case). If the cache is not consistent, it should be re-filled
/// This is also typically done in FileShredder
class ShredderCache {

public:

    /// @brief Compose two-directional key-value (partition-to-drive, drive-to-partition)
    ShredderCache();

    /// @brief Default
    ~ShredderCache() = default;

    ShredderCache(const ShredderCache&) = delete;
    ShredderCache& operator=(const ShredderCache&) = delete;

    /// @brief Submit file path for erasure
    /// @param file_path: Unicode path
    /// @param system_added: true if added by application, false is explicitly by the user
    void submit(const std::wstring& file_path, double entropy);

    /// @brief Remove file path from cache
    void remove(const std::wstring& file_path);

    /// @brief Cleanup cache
    /// @return: true if success, false otherwise
    void clean();

    /// @brief Check if record already in cache
    bool already_exist(const std::wstring& file_path);

    /// @brief Shred files if it's ready
    void erase_files();

    /// @brief Set the flag of cache coherence to the database
    void set_cache_ready(bool cache_ready);

    /// @brief True if the cache is coherent to the database
    bool is_cache_ready() const { return cache_ready_; }

    /// @brief Return files prepared for erase this moment
    std::map<std::wstring, double> files_prepared();

    /// @brief Return directories prepared for erase this moment
    std::vector<std::wstring> directories_prepared();

private:

    //////////////////////////////////////////////////////////////////////////

    /// Flag set if the data in file cache is coherent the data in database
    std::atomic_bool cache_ready_ = false;

    /// set of drives
    std::map<int, std::unique_ptr<shredder::DriveEraser>> erasible_drives_;

    /// Mapping drive root to physical drive index
    std::map<std::wstring, int> partition_to_drive_;
};

} // namespace shredder
