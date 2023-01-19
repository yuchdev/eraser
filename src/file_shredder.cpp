#include <eraser/file_shredder.h>
#include <eraser/shredder_cache.h>

#include <eraser/encryption_checker.h>
#include <winapi-helpers/md5.h>
#include <winapi-helpers/hardware_information.h>
#include <plog/Log.h>
#include <winapi-helpers/utilities.h>


#include <boost/filesystem.hpp>
#include <algorithm>
#include <memory>
#include <string>
#include <chrono>


using namespace helpers;
using namespace shredder;
using namespace encryption;
namespace fs = boost::filesystem;

namespace {

const char* get_md5(const char* message)
{
    static helpers::md5 hasher;
    return hasher.digest_string(message);
}

} // namespace

bool shredder::FileShredder::multithreaded_erase_(false);
bool shredder::FileShredder::ntfs_erase_(false);

bool shredder::FileShredderSettings::ntfs_erase = true;
bool shredder::FileShredderSettings::multithreaded_erase = false;
size_t shredder::FileShredderSettings::thread_number = 0;

FileShredder& FileShredder::instance(const FileShredderSettings& settings)
{
    static FileShredder s(settings);
    return s;
}

FileShredder::FileShredder(const FileShredderSettings& settings) :
    cache_(std::make_unique<shredder::ShredderCache>()),
    db_(ShredderDatabaseWrapper::instance()),
    calculation_pool(settings.thread_number)
{
    FileShredder::multithreaded_erase_ = settings.multithreaded_erase;
    std::string database_path = ShredderDatabaseWrapper::database_name();

    if (!fs::is_regular_file(database_path)) {
        LOG_WARNING << "Eraser file is not present, creating database may solve the problem";
    }
    db_.open_eraser_db();

    // Force NTFS journal cleanup
    FileShredder::ntfs_erase_ = settings.ntfs_erase;

    LOG_INFO << "FileShredder: NTFS_ERASE=" << FileShredder::ntfs_erase_;
    LOG_INFO << "FileShredder: System reported " << cores_number() << " CPU cores";
    LOG_INFO << "FileShredder: Shredder has " << threads_number() << " workers";
}

bool FileShredder::submit(const std::wstring& path, bool system_added, bool no_insert /*= false*/, IShredderCallback* callback /*= nullptr*/)
{
	std::wstring file_path = path;
#if defined(_WIN32) || defined(_WIN64)
	// case insensitive path
	if (!file_path.empty()) {
		std::transform(file_path.begin(), file_path.end(), file_path.begin(), ::towupper);
	}
#endif

    if (file_path.empty()) {
        return false;
    }

    if (!fs::is_regular_file(file_path) && !fs::is_directory(file_path)) {
        return false;
    }
    std::lock_guard<std::recursive_mutex> l(update_mutex_);

    std::string hash = get_md5(helpers::wstring_to_utf8(file_path).c_str());

    if (!no_insert) {
        if (cache_->is_cache_ready() && cache_->already_exist(file_path)) {
            LOG_DEBUG << "Trying to add already existing path: " << helpers::wstring_to_utf8(file_path);
            return false;
        }

        ShredderFileProperties p;
        p.set_system_added(system_added);
        p.set_is_file(fs::is_regular_file(file_path));


        if (!db_.insert_record(hash, path, p.get_flags())) {
            LOG_WARNING << "Unable to insert path " << helpers::wstring_to_utf8(file_path);
            db_.check_sqlite_error();
            return false;
        }

        cache_->submit(file_path, -1.0);
    }

    // last operation in the method
    calculation_pool.enqueue(&FileShredder::update_entropy, this, std::move(hash), std::move(file_path), callback);    
    return true;
}

bool FileShredder::remove(const std::wstring& path)
{
	std::wstring file_path = path;
#if defined(_WIN32) || defined(_WIN64)
	// case insensitive path
	if (!file_path.empty()) {
		std::transform(file_path.begin(), file_path.end(), file_path.begin(), ::towupper);
	}
#endif

    if (file_path.empty()) {
        return false;
    }

    std::string hash = get_md5(helpers::wstring_to_utf8(file_path).c_str());

    std::lock_guard<std::recursive_mutex> l(update_mutex_);
    if (!db_.remove_record(hash)) {
        LOG_WARNING << "Unable to insert path " << helpers::wstring_to_utf8(file_path);
        db_.check_sqlite_error();
        return false;
    }

    cache_->remove(file_path);
    return true;
}

void FileShredder::erase_files()
{
    LOG_DEBUG << "Interrupt current checks";
    interrupt_checks();

    if (!cache_->is_cache_ready()) {
        LOG_DEBUG << "Cache needs to be reset [shred_files]";
        reset_cache();
    }
    cache_->erase_files();

    db_.drop_table();
}

bool FileShredder::clean()
{
    std::lock_guard<std::recursive_mutex> l(update_mutex_);
    if (!db_.drop_table()) {
        LOG_WARNING << "Unable to clean files list";
        db_.check_sqlite_error();
        return false;
    }

    cache_->clean();
    return true;
}

bool FileShredder::clean_user_files()
{
    std::lock_guard<std::recursive_mutex> l(update_mutex_);
    if (!db_.clean_user_files()) {
        LOG_WARNING << "Unable to clean user added files";
        db_.check_sqlite_error();
        return false;
    }

    cache_->set_cache_ready(false);
    return true;
}


std::map<std::wstring, double> FileShredder::files_prepared()
{
    if (!cache_->is_cache_ready()) {
        LOG_DEBUG << "Cache needs to be reset [files_prepared]";
        reset_cache();
    }

    std::lock_guard<std::recursive_mutex> l(update_mutex_);
    return std::move(cache_->files_prepared());
}

std::vector<std::wstring> FileShredder::directories_prepared()
{
    if (!cache_->is_cache_ready()) {
        LOG_DEBUG << "Cache needs to be reset [directories_prepared]";
        reset_cache();
    }

    std::lock_guard<std::recursive_mutex> l(update_mutex_);
    return std::move(cache_->directories_prepared());
}

size_t FileShredder::cores_number() const
{
    return calculation_pool.cores_number();
}

size_t FileShredder::threads_number() const
{
    return calculation_pool.threads_number();
}

void FileShredder::interrupt_checks()
{
    ShannonEncryptionChecker::interrupt(true);
    calculation_pool.stop();
    calculation_pool.clear();

    // enough for finishing tasks
    while (!calculation_pool.stopped()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

bool FileShredder::read_table(std::vector<ShredderFileInfo>& ret_table)
{
    std::lock_guard<std::recursive_mutex> l(update_mutex_);
    if (db_.read_table(ret_table)) {
        std::for_each(ret_table.begin(), ret_table.end(), [this](const shredder::ShredderFileInfo& info) {
			std::wstring file_path = info.path;
#if defined(_WIN32) || defined(_WIN64)
			// case insensitive path
			if (!file_path.empty()) {
				std::transform(file_path.begin(), file_path.end(), file_path.begin(), ::towupper);
			}
#endif
            cache_->submit(file_path, info.entropy);
        });
        cache_->set_cache_ready(true);
        return true;
    }
    else {
        return false;
    }
}

void FileShredder::update_entropy(std::string hash, std::wstring file_path, IShredderCallback* callback)
{
    ShannonEncryptionChecker checker;
    
    if (callback) {
        checker.set_callback(callback);
    }

    double entropy = checker.get_file_entropy(file_path);

    if (!db_.update_record(hash, entropy)) {
        LOG_WARNING << "Unable to insert path " << helpers::wstring_to_utf8(file_path);
        db_.check_sqlite_error();
    }

    if (callback) {
        callback->cleanup();
    }

    cache_->set_cache_ready(false);
}


void FileShredder::reset_cache()
{
    LOG_DEBUG << "Reset cache";
    std::lock_guard<std::recursive_mutex> l(update_mutex_);
    cache_->clean();
    std::vector<ShredderFileInfo> files_prepared;
    read_table(files_prepared);
}

bool FileShredder::is_multithreaded_erase()
{
    return multithreaded_erase_;
}

bool FileShredder::is_ntfs_erase()
{
    return ntfs_erase_;
}
