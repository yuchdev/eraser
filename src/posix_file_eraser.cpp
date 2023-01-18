#if defined (__unix__) || (defined (__APPLE__) && defined (__MACH__))
#include <win_file_eraser/destructabe_file.h>

#include <algorithm>
#include <vector>
#include <cassert>
#include <sstream>

#include <boost/filesystem.hpp>
#include <boost/system/error_code.hpp>
#include <boost/uuid/uuid.hpp>            // uuid class
#include <boost/uuid/uuid_generators.hpp> // generators
#include <boost/uuid/uuid_io.hpp>         // streaming operators etc.


namespace fs = boost::filesystem;
namespace bs = boost::system;

NativeFileEraser::NativeFileEraser(const std::wstring& filename, FileSpecific attributes/* = FileSpecific::Unknown*/)
{
}

bool NativeFileEraser::open(const std::wstring& filename)
{
    return false;
}

void NativeFileEraser::close()
{
}

bool NativeFileEraser::erase_full(uint8_t* start_mask, size_t mask_length/* = 65536*/)
{
    return false;
}

bool NativeFileEraser::erase_random(BYTE* start_mask, size_t mask_length, size_t areas/* = 0*/)
{
    if (megabyte_ > file_size_) {
        return erase_full(start_mask, mask_length);
    }

    if (!prepared_to_erase_) {
        prepare();
    }

    return false;
}

bool NativeFileEraser::erase_begin_end(BYTE* start_mask, size_t mask_length)
{
    if (megabyte_ > file_size_) {
        return erase_full(start_mask, mask_length);
    }

    if (!prepared_to_erase_) {
        prepare();
    }

    return false;
}

bool NativeFileEraser::erase_smart(BYTE* start_mask, size_t mask_length)
{
    if (big_file_) {
        return erase_random(start_mask, mask_length, 0x0F);
    }
    else if (false == big_file_ && information_estimation_ == FileSpecific::Encrypted) {
        return erase_begin_end(start_mask, mask_length);
    }
    else if (false == big_file_ && information_estimation_ == FileSpecific::Binary) {
        return erase_random(start_mask, mask_length);
    }
    else if (false == big_file_ && information_estimation_ == FileSpecific::Plain) {
        return erase_full(start_mask, mask_length);
    }
    else if (information_estimation_ == FileSpecific::Unknown) {
        return erase_random(start_mask, mask_length);
    }

    // we did not covered something?
    assert(false);
    return false;
}

bool NativeFileEraser::prepare()
{
}

bool NativeFileEraser::try_open(const std::wstring& filename)
{
    // First, open the file in overwrite mode
    return false;
}
#endif // defined (__unix__) || (defined (__APPLE__) && defined (__MACH__))
