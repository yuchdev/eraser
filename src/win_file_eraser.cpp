#if defined(_WIN32) || defined(_WIN64)
#define NOMINMAX
#include <eraser/win_file_eraser.h>

#include <algorithm>
#include <vector>
#include <cassert>

using namespace helpers;
using namespace shredder;

std::default_random_engine NativeFileEraser::generator_;

NativeFileEraser::NativeFileEraser(const std::wstring& filename, EntropyEstimation estimation, DiskType disk_type)
    : information_estimation_(estimation), disk_type_(disk_type)
{
    open(filename);
}

NativeFileEraser::~NativeFileEraser()
{
    close();
}

bool NativeFileEraser::open(const std::wstring& filename)
{
    file_attributes_ = GetFileAttributesW(filename.c_str());
    if ((file_attributes_ == INVALID_FILE_ATTRIBUTES) || (file_attributes_ & FILE_ATTRIBUTE_DIRECTORY)) {
        return false;
    }

    // remove read-only attribute
    if (file_attributes_ & FILE_ATTRIBUTE_READONLY) {
        ::SetFileAttributesW(filename.c_str(), file_attributes_ & ~FILE_ATTRIBUTE_READONLY);
    }

    // If the file is compressed, we have to go a different path
    is_file_compressed_ = is_file_compressed(file_attributes_);

    // try at least twice
    if (try_open(filename) || try_open(filename)) {
        // unable to open, do nothing
        return true;
    }
    return false;
}

void NativeFileEraser::close()
{
    if (INVALID_HANDLE_VALUE != file_handle_) {
        ::CloseHandle(file_handle_);
    }
    file_handle_ = INVALID_HANDLE_VALUE;
}

bool NativeFileEraser::erase_full(uint8_t* start_mask, size_t mask_length/* = 65536*/)
{
    // check mask length complaint to block size
    if (file_handle_ == INVALID_HANDLE_VALUE || (0 == file_size_)) {
        return false;
    }

    if (big_file_) {
        return false;
    }

    if (!prepared_to_erase_) {
        prepare();
    }

    // back to the file beginning
    last_pointer_ = SetFilePointer(file_handle_, 0, NULL, FILE_BEGIN);

    DWORD bytes_written{};
    for (size_t bytes_erased = 0; bytes_erased < file_size_; bytes_erased += bytes_written) {
        size_t erase_chunk = std::min<size_t>((file_size_ - bytes_erased), mask_length);
        if (!WriteFile(file_handle_, start_mask, erase_chunk, &bytes_written, NULL)) {
            return false;
        }
        bytes_erased += bytes_written;
    }

    return true;
}

bool NativeFileEraser::erase_random(uint8_t* start_mask, size_t mask_length)
{
    if (megabyte_ > file_size_) {
        return erase_full(start_mask, mask_length);
    }

    if (!prepared_to_erase_) {
        prepare();
    }

    size_t begin_offset = 0;
    size_t end_offset = file_size_ - mask_length;
    size_t erased_areas = file_size_ / (mask_length * 5);

    // add erase at begin, end and generate erase points in the middle (linearly distributed)
    std::vector<size_t> erase_points({ begin_offset });
    std::uniform_int_distribution<size_t> distribution(mask_length, end_offset - mask_length);
    for (int i = 0; i < erased_areas; ++i) {
        erase_points.push_back(distribution(generator_));
    }
    erase_points.push_back(end_offset);

    if (!std::is_sorted(erase_points.begin(), erase_points.end())) {
        std::sort(erase_points.begin(), erase_points.end());
    }
    
    DWORD bytes_written{};
    for (size_t erase_point : erase_points) {
        assert(erase_point <= file_size_ - mask_length);
        last_pointer_ = SetFilePointer(file_handle_, erase_point, NULL, FILE_BEGIN);
        if (!WriteFile(file_handle_, start_mask, mask_length, &bytes_written, NULL)) {
            return false;
        }
    }
    return true;
}

bool NativeFileEraser::erase_begin_end(uint8_t* start_mask, size_t mask_length)
{
    if (megabyte_ > file_size_) {
        return erase_full(start_mask, mask_length);
    }

    if (!prepared_to_erase_) {
        prepare();
    }

    DWORD bytes_written{};
    size_t begin_offset{};

    last_pointer_ = SetFilePointer(file_handle_, begin_offset, NULL, FILE_BEGIN);
    if (!WriteFile(file_handle_, start_mask, mask_length, &bytes_written, NULL)) {
        return false;
    }

    last_pointer_ = SetFilePointer(file_handle_, -mask_length, NULL, FILE_END);
    if (!WriteFile(file_handle_, start_mask, mask_length, &bytes_written, NULL)) {
        return false;
    }

    return true;
}

bool NativeFileEraser::erase_smart(uint8_t* start_mask, size_t mask_length)
{
    if (big_file_) {
        return erase_begin_end(start_mask, mask_length);
    }
    else if (false == big_file_ && information_estimation_ == ShannonEncryptionChecker::Encrypted) {
        return erase_begin_end(start_mask, mask_length);
    }
    else if (false == big_file_ && information_estimation_ == ShannonEncryptionChecker::Binary) {
        // TODO: erase_begin_end?
        return erase_full(start_mask, mask_length);
    }
    else if (false == big_file_ && information_estimation_ == ShannonEncryptionChecker::Plain) {
        return erase_full(start_mask, mask_length);
    }
    else if (information_estimation_ == ShannonEncryptionChecker::Unknown) {
        return erase_full(start_mask, mask_length);
    }

    // we did not covered something?
    assert(false);
    return false;
}

bool NativeFileEraser::is_file_compressed(DWORD file_attributes) const
{
    return (file_attributes_ & FILE_ATTRIBUTE_COMPRESSED ||
        file_attributes_ & FILE_ATTRIBUTE_ENCRYPTED ||
        file_attributes_ & FILE_ATTRIBUTE_SPARSE_FILE);
}

bool NativeFileEraser::prepare()
{
    constexpr BYTE anchor = 0xEF;
    LONG high_word_length{};
    DWORD bytes_written{};

    // set to last byte
    last_pointer_ = SetFilePointer(file_handle_, -1, NULL, FILE_END);
    
    if (last_pointer_ == INVALID_SET_FILE_POINTER) {
        return false;
    }

    if (!WriteFile(file_handle_, &anchor, sizeof(BYTE), &bytes_written, NULL)) {
        return false;
    }

    if (disk_type_ == helpers::PartititonInformation::SSD) {
        for (size_t ahchor_point = 0xFFFF; ahchor_point <= file_size_; ahchor_point += 0xFFFF) {
            last_pointer_ = SetFilePointer(file_handle_, ahchor_point, &high_word_length, FILE_BEGIN);
            if (!WriteFile(file_handle_, &anchor, sizeof(BYTE), &bytes_written, NULL)) {
                return false;
            }
        }
    }

    // we can start safe erase
    prepared_to_erase_ = true;

    return true;
}

bool NativeFileEraser::try_open(const std::wstring& filename)
{
    // First, open the file in overwrite mode
    file_handle_ = CreateFileW(filename.c_str(), GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING, 
        FILE_FLAG_WRITE_THROUGH,
        NULL);

    if (file_handle_ == INVALID_HANDLE_VALUE) {
        return false;
    }

    initial_filepath_ = filename;

    LARGE_INTEGER file_size{};
    GetFileSizeEx(file_handle_, &file_size);
    if (file_size.HighPart) {
        big_file_ = true;
    }
    file_size_ = file_size.QuadPart;

    return true;
}

// static
bool NativeFileEraser::clean_ntfs_journal(HANDLE volume_handle)
{
    // Function DeviceIoControl() params explanation:
    // DeviceIoControl(handle to volume, 
    // dwIoControlCode == FSCTL_DELETE_USN_JOURNAL, 
    // input struct DELETE_USN_JOURNAL_DATA, 
    // size of DELETE_USN_JOURNAL_DATA, 
    // lpOutBuffer == nullptr,
    // nOutBufferSize == 0,
    // number of bytes returned,
    // OVERLAPPED structure);
    if (INVALID_HANDLE_VALUE == volume_handle) {
        return false;
    }

    DWORD bytes_returned{};
    CREATE_USN_JOURNAL_DATA usn_create_journal = {};
    BOOL success = DeviceIoControl(volume_handle, 
        FSCTL_CREATE_USN_JOURNAL, 
        &usn_create_journal, 
        sizeof(usn_create_journal), 
        NULL, 0, 
        &bytes_returned, NULL);
    if (FALSE == success) {
        return false;
    }

    USN_JOURNAL_DATA usn_info;
    success = DeviceIoControl(volume_handle, 
        FSCTL_QUERY_USN_JOURNAL, 
        NULL, 0, 
        &usn_info, 
        sizeof(usn_info), 
        &bytes_returned, NULL);
    if (FALSE == success) {
        return false;
    }

    DELETE_USN_JOURNAL_DATA deleteUsn;
    deleteUsn.UsnJournalID = usn_info.UsnJournalID;
    deleteUsn.DeleteFlags = USN_DELETE_FLAG_DELETE | USN_DELETE_FLAG_NOTIFY;
    OVERLAPPED ov{};

    success = DeviceIoControl(volume_handle, FSCTL_DELETE_USN_JOURNAL, &deleteUsn, sizeof(deleteUsn), NULL, 0, NULL, &ov);

    if (FALSE == success) {
        return 1;
    }

    ::WaitForSingleObject(ov.hEvent, INFINITE);

    return (TRUE == success) ? true : false;
}

// static
HANDLE NativeFileEraser::get_volume_handle(const char drive_letter)
{
    static const size_t root_letter_position = 0;
    static const size_t volume_letter_position = 4;

    char volume_root_path[] = "X:\\";
    char volume_filename[] = "\\\\.\\X:";
    volume_root_path[root_letter_position] = ::toupper(drive_letter);
    volume_filename[volume_letter_position] = ::toupper(drive_letter);

    bool isNTFS = false;
    char filesystem_name[MAX_PATH] = { 0 };
    int status = GetVolumeInformationA(volume_root_path, NULL, 0, NULL, NULL, NULL, filesystem_name, MAX_PATH);

    if (0 != status) {
        return INVALID_HANDLE_VALUE;
    }

    HANDLE ret_handle = CreateFileA(volume_filename, 
        GENERIC_READ | GENERIC_WRITE, 
        FILE_SHARE_READ | FILE_SHARE_WRITE, 
        NULL, 
        OPEN_EXISTING, 
        FILE_ATTRIBUTE_READONLY, NULL);
    if (INVALID_HANDLE_VALUE == ret_handle) {
        return INVALID_HANDLE_VALUE;
    }
    return ret_handle;
}

// static
HANDLE NativeFileEraser::get_volume_handle(const wchar_t drive_letter)
{
    static const size_t root_letter_position = 0;
    static const size_t volume_letter_position = 4;

    wchar_t volume_root_path[] = L"X:\\";
    wchar_t volume_filename[] = L"\\\\.\\X:";
    volume_root_path[root_letter_position] = ::toupper(drive_letter);
    volume_filename[volume_letter_position] = ::toupper(drive_letter);

    bool isNTFS = false;
    wchar_t filesystem_name[MAX_PATH] = { 0 };
    BOOL status = GetVolumeInformationW(volume_root_path, NULL, 0, NULL, NULL, NULL, filesystem_name, MAX_PATH);

    if ((std::wstring(filesystem_name) != L"NTFS") || (TRUE != status)) {
        return INVALID_HANDLE_VALUE;
    }

    HANDLE ret_handle = CreateFileW(volume_filename,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_READONLY, NULL);
    if (INVALID_HANDLE_VALUE == ret_handle) {
        return INVALID_HANDLE_VALUE;
    }
    return ret_handle;

}

bool shredder::NativeFileEraser::clean_ntfs_journal(char drive_letter)
{
    return clean_ntfs_journal(get_volume_handle(drive_letter));
}

bool shredder::NativeFileEraser::clean_ntfs_journal(wchar_t drive_letter)
{
    return clean_ntfs_journal(get_volume_handle(drive_letter));
}

bool NativeFileEraser::clean_ntfs_journal(const std::wstring& drive_root)
{
    assert(drive_root.size() == 1);
    return clean_ntfs_journal(get_volume_handle(drive_root[0]));
}

#endif // defined(_WIN32) || defined(_WIN64)
