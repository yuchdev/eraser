#include <eraser/shredder_datatbase.h>
#include <eraser/shredder_file_info.h>
#include <winapi-helpers/utilities.h>
#include <plog/Log.h>
#include <winapi-helpers/special_path_helper.h>

#include <boost/format.hpp>
#include <boost/filesystem.hpp>
#include <string>
#include <vector>
#include <cassert>


using namespace helpers;
using namespace shredder;


// static 
ShredderDatabaseWrapper& ShredderDatabaseWrapper::instance()
{
    static ShredderDatabaseWrapper s;
    return s;
}

// static 
int ShredderDatabaseWrapper::select_callback(void *raw_data, int column_count, char **column_values, char **column_name)
{
    assert(std::string(column_name[PathColumn]) == "filename");
    assert(std::string(column_name[EntropyColumn]) == "entropy");
    assert(std::string(column_name[FlagsColumn]) == "flags");
    std::wstring path = helpers::utf8_to_wstring(std::string(column_values[PathColumn]));
    double entropy = std::stod(std::string(column_values[EntropyColumn]));
    int64_t flags = std::stoll(std::string(column_values[FlagsColumn]));

    ShredderDatabaseWrapper::instance().read_db_row(std::move(path), entropy, flags);
    return 0;
}

// static 
std::string ShredderDatabaseWrapper::database_name()
{
    return std::string{ "eraser" };
}


void ShredderDatabaseWrapper::open_eraser_db()
{
    // sqlite3 library should be recompiled with thread-safety support
    // See https://www.sqlite.org/threadsafe.html for thread-safety options
    assert(sqlite3_helper::is_threadsafe());
    const char* create_table_sql = "CREATE TABLE IF NOT EXISTS filetable("
        "hash TEXT PRIMARY KEY,"
        "filename TEXT NOT NULL,"
        "entropy REAL NOT NULL,"
        "flags INT8 NOT NULL)";

    std::string database_name = ShredderDatabaseWrapper::database_name();

    eraser_db_.open(database_name.c_str());
    eraser_db_.exec(create_table_sql);

    // try twice to avoid sporadic issues like anti-virus
    // 1.
    if (!eraser_db_.is_valid()) {
        eraser_db_.open(database_name.c_str());
        eraser_db_.exec(create_table_sql);
    }

    // 2.
    if (!eraser_db_.is_valid()) {
        LOG_ERROR << "Unable to open eraser database";
        throw std::runtime_error("Unable to open eraser database");
    }
}

bool ShredderDatabaseWrapper::read_table(std::vector<ShredderFileInfo>& ret_table)
{
    eraser_db_.exec("SELECT filename, entropy, flags FROM filetable", &ShredderDatabaseWrapper::select_callback);
    if (eraser_db_.get_last_error() == 0) {
        LOG_DEBUG << "Returned table of " << tmp_table_.size() << " rows";
        ret_table.swap(tmp_table_);
        return true;
    }
    else {
        LOG_WARNING << "Error during SELECT";
        check_sqlite_error();
        return false;
    }
}

bool ShredderDatabaseWrapper::insert_record(const std::string& hash, const std::wstring& path, int64_t flags)
{
    std::string sql = boost::str(boost::format("INSERT INTO filetable(hash, filename, entropy, flags) VALUES ('%1%', '%2%', %3%, %4%)")
        % hash % helpers::wstring_to_utf8(path) % -1.0 % flags);

    eraser_db_.exec(sql.c_str());
    return (eraser_db_.get_last_error() == 0);
}

bool ShredderDatabaseWrapper::remove_record(const std::string& hash)
{
    std::string sql = boost::str(boost::format("DELETE FROM filetable WHERE hash='%1%'") % hash);

    eraser_db_.exec(sql.c_str());
    return (eraser_db_.get_last_error() == 0);
}

bool ShredderDatabaseWrapper::update_record(const std::string& hash, double entropy)
{
    std::string sql = boost::str(boost::format("UPDATE filetable SET entropy=%1% WHERE hash='%2%'") % entropy % hash);

    eraser_db_.exec(sql.c_str());
    return (eraser_db_.get_last_error() == 0);
}

bool ShredderDatabaseWrapper::drop_table()
{
    eraser_db_.exec("DROP TABLE filetable");
    return (eraser_db_.get_last_error() == 0);
}

bool ShredderDatabaseWrapper::clean_user_files()
{
    // SystemAdded flag is not set
    std::string sql = "DELETE FROM filetable WHERE flags IN (0, 2)";
    eraser_db_.exec(sql.c_str());
    return (eraser_db_.get_last_error() == 0);

}

void ShredderDatabaseWrapper::read_db_row(std::wstring&& path, double entropy, int64_t flags)
{
    tmp_table_.emplace_back(ShredderFileInfo(path, entropy, flags));
}

bool ShredderDatabaseWrapper::check_sqlite_error() const
{
    if (eraser_db_.get_last_error()) {
        LOG_ERROR << "Error executing SQL, code = " << eraser_db_.get_last_error()
            << "; description: " << eraser_db_.get_last_error_message();
        return false;
    }
    return true;
}
