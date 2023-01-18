#pragma once
#include <cstdint>

namespace shredder {

class IShredderCallback
{
public:
    
    /// @brief Pure virtual base
    virtual ~IShredderCallback() {}

    /// @brief Method should be called to initialize counter 
    /// with total number of calculation iterations (size in bytes)
    virtual void init(uintmax_t opCount) = 0;

    /// @brief Method called to provide number of operations successfully performed 
    virtual void set_value(uintmax_t value) = 0;

    /// @brief Method called after the calculation is finished 
    // to inform it's safe to clean up the caller's side
    virtual void cleanup() = 0;
};

} // namespace shredder
