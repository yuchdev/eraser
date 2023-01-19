#include <eraser/shredder_cache.h>
#include <winapi-helpers/partition_information.h>
#include <plog/Log.h>

using namespace helpers;
using namespace shredder;

ShredderCache::ShredderCache()
{
    std::vector<int> physical_drives = PartititonInformation::instance().get_physical_drives();

    for (int drive_index : physical_drives) {

        std::vector<PartititonInformation::NativePartititon> parts =
            PartititonInformation::instance().enumerate_drive_partititons(drive_index);

        if (parts.empty()) {
            continue;
        }

        for (const PartititonInformation::NativePartititon& part : parts) {
#ifdef NATIVE
            partition_to_drive_[part.root] = drive_index;
#endif
        }
    }
}

void ShredderCache::submit(const std::wstring& file_path, double entropy)
{
    const size_t root_size = PartititonInformation::instance().root_string_size();
#if defined(_WIN32) || defined(_WIN64)
    std::wstring file_root = file_path.substr(0, root_size);
#else
    throw std::logic_error("Not implemented: FileShredder::submit");
#endif

    // Add record to cache
    auto it = partition_to_drive_.find(file_root);
    if (it != partition_to_drive_.end()) {
        int drive_index = (*it).second;
        erasible_drives_[drive_index]->submit(file_root, file_path, entropy);
    }
}

void ShredderCache::remove(const std::wstring& file_path)
{
    // Remove from cache
    const size_t root_size = PartititonInformation::instance().root_string_size();
    std::wstring file_root = file_path.substr(0, root_size);

    auto it = partition_to_drive_.find(file_root);
    if (it != partition_to_drive_.end()) {
        int drive_index = (*it).second;
        erasible_drives_[drive_index]->remove(file_root, file_path);
    }
}

void ShredderCache::clean()
{
    cache_ready_.store(false);
    std::for_each(erasible_drives_.begin(), erasible_drives_.end(), [](auto& drive) {
        drive.second->clean();
    });
}

void ShredderCache::erase_files()
{
    std::for_each(erasible_drives_.begin(), erasible_drives_.end(), [](auto& drive) {
        LOG_DEBUG << "Shred files on volume ID = " << drive.first;
        drive.second->shred_files();
    });
    cache_ready_.store(false);
}

bool ShredderCache::already_exist(const std::wstring& file_path)
{
    const size_t root_size = PartititonInformation::instance().root_string_size();
#if defined(_WIN32) || defined(_WIN64)
    std::wstring file_root = file_path.substr(0, root_size);
#else
    throw std::logic_error("Not implemented: FileShredder::submit");
#endif

    // Add record to cache
    auto it = partition_to_drive_.find(file_root);
    if (it != partition_to_drive_.end()) {
        int drive_index = (*it).second;
        return erasible_drives_[drive_index]->already_exist(file_root, file_path);
    }
    return false;
}

void ShredderCache::set_cache_ready(bool cache_ready)
{
    cache_ready_.store(cache_ready);
}

std::map<std::wstring, double> ShredderCache::files_prepared()
{
    std::map<std::wstring, double> files_prepared;
    for (auto& drive : erasible_drives_) {
        std::map<std::wstring, double> map_files = drive.second->files_prepared();

        for (auto& file_info : map_files) {
            files_prepared.emplace(std::make_pair(file_info.first, file_info.second));
        }
    }
    return std::move(files_prepared);
}

std::vector<std::wstring> ShredderCache::directories_prepared()
{
    std::vector<std::wstring> dirs_prepared;
    std::for_each(erasible_drives_.begin(), erasible_drives_.end(), [&dirs_prepared](auto& drive) {
        std::vector<std::wstring> drive_files = drive.second->directories_prepared();
        dirs_prepared.insert(std::end(dirs_prepared), std::begin(drive_files), std::end(drive_files));
    });

    return std::move(dirs_prepared);
}
