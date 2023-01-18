#pragma once
#include <file_shredder/shredder_callback_interface.h>
#include <file_shredder/shredder_datatbase.h>
#include <file_shredder/shredder_file_info.h>
#include <helpers/thread_pool.h>
#include <helpers/partition_information.h>

//#if (_MSC_VER > 1900)

#include <vector>
#include <string>
#include <mutex>

namespace encryption {
class ShannonEncryptionChecker;
}

namespace boost {
    namespace filesystem {
        class path;
    } // filesystem 
} // boost 

namespace shredder {

class ShredderCache;
class ShredderDatabaseWrapper;

/// @brief
struct FileShredderSettings
{
    /// Number of threads in calculation pool
    static size_t thread_number;

    /// Use all possible cores for file erase
    static bool multithreaded_erase;

    /// Erase NTFS file journal
    static bool ntfs_erase;
};

static FileShredderSettings default_settings;

/// @brief The only class instance that performs files erasure in the system
/// Owns the erased files list sorted out by physical drives (HDD/SSD/External)
class FileShredder {

    friend class DriveEraser;

public:

    ~FileShredder() = default;

    FileShredder(const FileShredder&) = delete;
    FileShredder& operator=(const FileShredder&) = delete;

    /// @brief The only instance, Meyers singleton
    static FileShredder& instance(const FileShredderSettings& settings = default_settings);

    /// @brief Do we use all possible cores for file erase
    static bool is_multithreaded_erase();

    /// @brief Erase NTFS file journal
    static bool is_ntfs_erase();

    /// @brief Submit file path for erasure
    /// @param file_path: Unicode path
    /// @param system_added: true if added by application, false is explicitly by the user
    /// @param no_insert: if true, DO NOT perform INSERT INTO operation, since the item is supposed to be there,
    /// just it's calculation is not finished and estimation is not performed
    /// @return: true if success, false otherwise
    bool submit(const std::wstring& file_path, bool system_added, bool no_insert = false, IShredderCallback* callback = nullptr);

    /// @brief Remove file path from erasure list
    /// @return: true if success, false otherwise
    bool remove(const std::wstring& file_path);

    /// @brief Cleanup user-added 
    /// @return: true if success, false otherwise
    bool clean_user_files();

    /// @brief Cleanup erasure list
    /// @return: true if success, false otherwise
    bool clean();

    /// @brief Erase files
    void erase_files();

    /// @brief Interrupt all encryption checks and empty the tasks queue
    void interrupt_checks();

    /// @brief Read from database table to shredder
    bool read_table(std::vector<ShredderFileInfo>& ret_table);

    /// @brief Return files prepared for erase this moment
    std::map<std::wstring, double> files_prepared();

    /// @brief Return directories prepared for erase this moment
    std::vector<std::wstring> directories_prepared();

    /// @brief CPU cores as reported by the system
    size_t cores_number() const;

    /// @brief Thread workers in the calculation pool
    size_t threads_number() const;


private:

    /// Private constructor (use "virtual c-tor")
    FileShredder(const FileShredderSettings& settings);

    /// @brief Enqueue file path and entropy if known, 
    /// and let the caller know about the progress (may slow it down)
    /// param hash:
    /// param file_path:
    /// param callback:
    void update_entropy(std::string hash, std::wstring file_path, IShredderCallback* callback);

    /// @brief Reset cache
    void reset_cache();

    //////////////////////////////////////////////////////////////////////////

    /// Lock complete re-read operation and reset cache
    std::recursive_mutex update_mutex_;

    /// Wrapper for 'eraser' database
    ShredderDatabaseWrapper& db_;

    /// Shredder cache for faster processing
    mutable std::unique_ptr<ShredderCache> cache_;

    /// Thread pool created only for entropy calculation (interrupted upon panic)
    helpers::thread_pool calculation_pool;

    /// Use all possible cores for file erase
    static bool multithreaded_erase_;

    /// Erase NTFS file journal
    static bool ntfs_erase_;
};

} // namespace shredder
