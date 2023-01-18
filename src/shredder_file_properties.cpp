#include <eraser/shredder_file_properties.h>

using namespace shredder;

void ShredderFileProperties::set_system_added(bool system_added)
{
    if (system_added) {
        flags_ |= SystemAdded;
    }
    else {
        flags_ &= ~SystemAdded;
    }
}

void ShredderFileProperties::set_is_file(bool is_file)
{
    if (is_file) {
        flags_ |= IsFile;
    }
    else {
        flags_ &= ~IsFile;
    }
}

bool ShredderFileProperties::is_file() const
{
    return (flags_ & IsFile);
}

bool ShredderFileProperties::is_system_added() const
{
    return (flags_ & SystemAdded);
}
