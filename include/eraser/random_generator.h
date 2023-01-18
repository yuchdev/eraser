#pragma once
#include <cstdint>
#include <random>
#include <atomic>

/// @brief Storage for randomly generated sequence(s)
/// Class is thread-safe
class RandomGenerator {
public:

    /// @grief Generate byte sequence
    RandomGenerator();

    /// @brief Satisfy compiler
    ~RandomGenerator() = default;

    /// @brief Re-generate sequence
    void generate();

    /// @brief Get beginning of a random sequence
    uint8_t* random_sequence() const;

    /// @brief Get size of a random sequence
    size_t random_length() const;

private:

    /// STUB In case of multiple sequences point to actual
    std::atomic<uint8_t*> random_buffer_ = nullptr;

    /// Generating engine
    std::default_random_engine rnd_device;

    /// Size of a sequence
    static const size_t cleann_buffer_size = 0xFFFF;

    /// Array for storing the sequence
    static uint8_t clean_buffer1[cleann_buffer_size];
};