#pragma once
#include <eraser/shredder_file_info.h>
#include <winapi-helpers/sqlite3_helper.h>

#include <cstdint>
#include <utility>
#include <vector>
#include <string>

namespace shredder {


/// @brief 
class ShredderDatabaseWrapper {

    /// Column names without primary key
    enum FileTableColumnNames
    {
        PathColumn = 0,
        EntropyColumn = 1,
        FlagsColumn = 2
    };

public:

    /// @brief Singleton
    static ShredderDatabaseWrapper& instance();

    /// @brief Database file name
    static std::string database_name();

    /// @brief Read existing eraser database or create new if necessary
    void open_eraser_db();

    // /@brief Select eraser database data
    bool read_table(std::vector<ShredderFileInfo>& ret_table);

    /// @brief Insert new file path to the database
    bool insert_record(const std::string& hash, const std::wstring& path, int64_t flags);

    /// @brief Remove file path to the database
    bool remove_record(const std::string& hash);

    /// @brief Update entropy value
    bool update_record(const std::string& hash, double entropy);

    /// @brief Drop table with all records
    bool drop_table();

    /// @brief Clean user-added files only
    bool clean_user_files();

    /// Check error code and log if != SQLITE_OK
    bool check_sqlite_error() const;

private:

    /// Create empty database
    ShredderDatabaseWrapper() = default;

    /// SELECT callback called for every receiver row
    static int select_callback(void *raw_data, int column_count, char **column_values, char **column_name);

    /// Save record from database to memory
    void read_db_row(std::wstring&& path, double entropy, int64_t flags);

    //////////////////////////////////////////////////////////////////////////

    /// Temporary storage for 'filetable' (swapped in read_table() method)
    mutable std::vector<ShredderFileInfo> tmp_table_;

    /// Database
    helpers::sqlite3_helper eraser_db_;
};

} // namespace shredder
