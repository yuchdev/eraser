#include <eraser/drive_eraser.h>
#include <eraser/file_shredder.h>

#if defined(_WIN32) || defined(_WIN64)
#include <eraser/win_file_eraser.h>
#elif defined (__unix__) || (defined (__APPLE__) && defined (__MACH__))
#include <file_eraser/posix_file_eraser.h>
#endif

#include <plog/Log.h>
#include <winapi-helpers/utilities.h>
#include <eraser/encryption_checker.h>

#include <boost/filesystem.hpp>
#include <boost/system/error_code.hpp>

#include <algorithm>
#include <vector>
#include <string>
#include <cassert>
#include <sstream>
#include <map>
#include <set>
#include <iostream>
#include <iomanip>

using namespace shredder;
using namespace helpers;
namespace fs = boost::filesystem;
namespace bs = boost::system;

using std::string;
using std::wstring;
using helpers::thread_pool;
using encryption::ShannonEncryptionChecker;

DriveEraser::DriveEraser(ErasureMethod erasure_method, 
    DiskType disk_type,
    std::vector<PartititonInformation::PortablePartititon>& partitions)
    : erasure_method_(erasure_method), 
    disk_type_(disk_type),
    partitions_(partitions)
{
    erasure_type_handler_
        (ErasureMethod::Smart, &NativeFileEraser::erase_smart)
        (ErasureMethod::Full, &NativeFileEraser::erase_full)
        (ErasureMethod::Random, &NativeFileEraser::erase_random)
        (ErasureMethod::BeginEnd, &NativeFileEraser::erase_begin_end)
        ;
}

DriveEraser::DriveEraser(
    ErasureMethod erasure_method,
    int disk_type,
    std::vector<PartititonInformation::PortablePartititon>& partitions)
    : erasure_method_(erasure_method)
    , disk_type_(DriveEraser::DiskType::SSD)
    , partitions_(partitions)
{
}

void DriveEraser::submit(const std::wstring& root, const std::wstring& file_path, double entropy)
{
    std::lock_guard<std::mutex> l(files_lock_);
    fs::path fs_path(file_path);

    auto iter_pair = shredded_files_.equal_range(root);
    for (auto it = iter_pair.first; it != iter_pair.second; ++it) {
        if ((*it).second.path == fs_path.generic_wstring()) {
            // do not add doubles
            return;
        }
    }

    if (fs::is_directory(fs_path)) {

        auto iter_pair = shredded_directories_.equal_range(root);
        for (auto it = iter_pair.first; it != iter_pair.second; ++it) {
            if ((*it).second == fs_path) {
                // do not add doubles
                return;
            }
        }

        return this->submit_dir(root, fs_path.generic_wstring());
    }
    // further work only with regular files
    if (!fs::is_regular_file(fs_path)) {
        return;
    }

    // Flags (3rd param) are not important for cache, so pass 0LL
    ShredderFileInfo erase_file(file_path, entropy, 0LL);
    shredded_files_.emplace(std::make_pair(root, std::move(erase_file)));
}

void DriveEraser::remove(const std::wstring& root, const std::wstring& filepath)
{
    std::lock_guard<std::mutex> l(files_lock_);

    if (fs::is_directory(filepath)) {
        this->remove_dir(root, filepath);
    }

    auto iter_pair = shredded_files_.equal_range(root);
    for (auto it = iter_pair.first; it != iter_pair.second; ++it) {
        if ((*it).second.path == filepath) {
            shredded_files_.erase(it);
            return;
        }
    }
}

void DriveEraser::submit_dir(const std::wstring& root, const std::wstring& dirpath)
{
    shredded_directories_.emplace(std::make_pair(root, dirpath));
}

void DriveEraser::remove_dir(const std::wstring& root, const std::wstring& dirpath)
{
    auto iter_pair = shredded_directories_.equal_range(root);
    for (auto it = iter_pair.first; it != iter_pair.second; ++it) {
        if ((*it).second == dirpath) {
            shredded_directories_.erase(it);
            return;
        }
    }
}

void DriveEraser::clean()
{
    shredded_files_.clear();
    shredded_directories_.clear();
}

void DriveEraser::erase_file(std::wstring file_path, double entropy)
{
    uintmax_t file_size = fs::file_size(file_path);
    ShannonEncryptionChecker::InformationEntropyEstimation file_specific = 
        ShannonEncryptionChecker::information_entropy_estimation(entropy, file_size);

    // nothing to hide in zero-sized file
    if (file_size == 0) {
        cheat_file_node(file_path);
        return;
    }

    NativeFileEraser native_file_eraser(file_path, file_specific, disk_type_);
    erasure_type_handler_.call(erasure_method_, &native_file_eraser, gen_.random_sequence(), gen_.random_length());
    native_file_eraser.close();

    cheat_file_node(file_path);
}

bool DriveEraser::already_exist(const std::wstring& root, const std::wstring& file_path)
{
    fs::path fs_path(file_path);

    if (fs::is_regular_file(fs_path)) {
        auto iter_pair = shredded_files_.equal_range(root);
        for (auto it = iter_pair.first; it != iter_pair.second; ++it) {
            if ((*it).second.path == fs_path.generic_wstring()) {
                // do not add doubles
                return true;
            }
        }
    }

    if (fs::is_directory(fs_path)) {
        auto iter_pair = shredded_directories_.equal_range(root);
        for (auto it = iter_pair.first; it != iter_pair.second; ++it) {
            if ((*it).second == fs_path) {
                // do not add doubles
                return true;
            }
        }
    }

    return false;
}

void DriveEraser::shred_files()
{
    std::lock_guard<std::mutex> l(files_lock_);

    std::for_each(shredded_files_.begin(), shredded_files_.end(),
        [this](std::pair<const std::wstring, shredder::ShredderFileInfo>& erase_info) {

        helpers::thread_pool eraser;
        // debug switch between multi-thread and single-thread erasure
#ifdef DEBUG_MULTITHREAD_ERASE
        if (FileShredder::is_multithreaded_erase()) {

            if (disk_type_ == helpers::PartititonInformation::SSD) {
                eraser.enqueue(&DriveEraser::erase_file, this, erase_info.second.path, erase_info.second.entropy);
            }
            else {
                erase_file(erase_info.second.path, erase_info.second.entropy);
            }
        }
        else {
            erase_file(erase_info.second.path, erase_info.second.entropy);
        }
#else
        erase_file(erase_info.second.path, erase_info.second.entropy);
#endif
    });


    std::for_each(shredded_directories_.begin(), shredded_directories_.end(),
        [this](std::pair <const std::wstring, const std::wstring> erase_info) {

        // debug switch between multi-thread and single-thred erasure
        if (FileShredder::is_multithreaded_erase()) {
            helpers::thread_pool eraser;
            if (disk_type_ == helpers::PartititonInformation::SSD) {

                boost::system::error_code ec;
                boost::uintmax_t(*erase_func_ptr)(const fs::path&, boost::system::error_code&) = &fs::remove_all;
                eraser.enqueue(erase_func_ptr, fs::path(erase_info.second), ec);
            }
            else {
                boost::system::error_code ec;
                fs::remove_all(erase_info.second, ec);
            }
        }
        else {
            boost::system::error_code ec;
            fs::remove_all(erase_info.second, ec);

        }

    });
    
    /// Partitions to clean filesystem journal
    std::set<std::wstring> partitions_affected;
    for (auto it = shredded_files_.begin(), end = shredded_files_.end(); it != end; it = shredded_files_.upper_bound(it->first)) {

        auto htfs_part = std::find_if(std::begin(partitions_), std::end(partitions_), [&it](const PartititonInformation::PortablePartititon& p){
            return ((*it).first == p.root && p.filesystem_name == "NTFS");
        });
        
        if (htfs_part != partitions_.end()) {
            partitions_affected.insert(it->first);
        }
    }

    if (FileShredder::is_ntfs_erase()) {
        std::for_each(partitions_affected.begin(), partitions_affected.end(), [this](const wstring& c) { 
            NativeFileEraser::clean_ntfs_journal(c); 
        });
    }
}

bool shredder::DriveEraser::cheat_file_node(const std::wstring& file_path)
{
    fs::path old_path = file_path;
    fs::path directory_path = old_path.parent_path();
    fs::path file_name = old_path.filename();
    string pattern("abc");
    size_t name_length = file_name.size();

    for (char c : pattern) {
        string new_name(name_length, c);
        fs::path new_path = directory_path / new_name;
        bs::error_code ec;
        fs::rename(old_path, new_path, ec);
        if (ec) {
            LOG_DEBUG << "fs::rename returned err = " << ec.value() << " [" << ec.message() << "]";
            return false;
        }

        old_path = new_path;
    }


    fs::path recycle_bin = old_path.root_path() / "$Recycle.Bin";
    fs::path final_path = recycle_bin / "892F575F-DE37-4A0F-8A3E-427618C7D64C.tmp";
    bs::error_code ec;
    fs::rename(old_path, final_path, ec);
    if (ec) {
        LOG_DEBUG << "fs::rename returned err = " << ec.value() << " [" << ec.message() << "]";
        return false;
    }

    if (!fs::remove(final_path, ec)) {
        LOG_DEBUG << "fs::rename returned err = " << ec.value() << " [" << ec.message() << "]";
        return false;
    }

    return true;
}

std::map<std::wstring, double> DriveEraser::files_prepared() const
{
    std::map<std::wstring, double> files;
    for (const auto& erase_info : shredded_files_) {
        files.emplace(std::make_pair(erase_info.second.path, erase_info.second.entropy));
    }
    return std::move(files);
}

std::vector<std::wstring> DriveEraser::directories_prepared() const
{
    std::vector<std::wstring> dirs;
    for (auto& dir : shredded_directories_) {
        fs::path one_dir(dir.second);
        dirs.push_back(one_dir.generic_wstring());
    }
    return std::move(dirs);

}
