#pragma once
#include <file_shredder/shredder_callback_interface.h>

#include <algorithm>
#include <vector>
#include <map>
#include <string>
#include <cmath>
#include <cassert>
namespace shredder {

/// @brief Accept range of probabilities per byte
/// Zero-probability in the sequence could be skipped
/// @return entropy if everything ok, -1.0 if probabilities range overflows one byte
/// Formula is here: https://en.wiktionary.org/wiki/Shannon_entropy
/// Possible valid result is from 0.0 (absolute order) to 8.0 (absolute chaos)
template <typename T>
double shannon_entropy(T first, T last)
{
    size_t frequencies_count{};
    double entropy{};

    std::for_each(first, last, [&entropy, &frequencies_count](auto item) mutable {

        if (0. == item) return;
        double fp_item = static_cast<double>(item);
        entropy += fp_item * log2(fp_item);
        ++frequencies_count;
    });

    if (frequencies_count > 256) {
        assert(false);
        return -1.0;
    }

    return -entropy;
}

/// @brief Detect whether some sequence (byte, block, memory, disk) is encrypted or highly compressed
class ShannonEncryptionChecker {
public:

    enum InformationEntropyEstimation {
        Plain,
        Binary,
        Encrypted,
        Unknown,
        EntropyLevelSize
    };

    /// @brief Set facet for unsigned char (boost binary reading twice)
    ShannonEncryptionChecker();

    /// @brief Make unique_ptr happy
    ~ShannonEncryptionChecker() = default;

    /// @brief Set callback function, accepting value of the bytes counter
    /// callback::init() is inside the checker, because we need to know the size,
    /// but callback::cleanup() can be elsewhere
    void set_callback(IShredderCallback* callback);

    /// @brief Detect whether file encrypted or very highly compressed with high enough probability
    /// @param file_path: full file path, passed by r-value to be executed in different thread 
    /// @param epsilon: estimated difference between absolute chaos (8.0) and actual entropy
    double get_file_entropy(std::wstring file_path) const;

    /// @brief Detect whether the bytes sequence (e.g. memory) is encrypted
    double get_sequence_entropy(const uint8_t* sequence_start, size_t sequence_size) const;

    /// @brief Get information encryption level using provided entropy and sequence size
    static InformationEntropyEstimation information_entropy_estimation(double entropy, uintmax_t sequence_size);

    /// @brief Min possible file size assuming max theoretical compression efficiency in bytes
    size_t min_compressed_size(double entropy, size_t sequence_size) const;

    /// @brief Provide readable properties of the information sequence
    static std::string get_information_description(InformationEntropyEstimation ent);

    /// @brief Interrupt all calculating threads
    static void interrupt(bool interrupt_flag);

private:

    /// do not make it atomic so that avoid cache ping-pong
    static bool interrupt_all_;

    /// Callback function called on every n-th iteration to observe calculation progress
    IShredderCallback* callback_{};

    /// Calculate probabilities to meet some byte in the file
    std::vector<double> read_file_probabilities(const std::wstring& file_path, uintmax_t file_size) const;

    /// Internal probabilities function for files without observing the progress, which is slightly faster
    std::vector<size_t> file_probabilities_fast(const std::wstring& file_path, uintmax_t file_size) const;

    /// Internal probabilities function for files with observing the progress with callback
    /// callback pointer in this function must not be zero
    std::vector<size_t> file_probabilities_observed(const std::wstring& file_path, uintmax_t file_size) const;

    /// Calculate probabilities to meet some byte in the sequence
    std::vector<double> read_stream_probabilities(const uint8_t* sequence_start, uintmax_t sequence_size) const;

    /// Internal probabilities function for generic sequences 
    /// without observing the progress, which is slightly faster
    std::vector<size_t> stream_probabilities_fast(const uint8_t* sequence_start, uintmax_t sequence_size) const;

    /// Internal probabilities function for generic sequences with observing the progress with callback
    /// callback pointer in this function must not be zero
    std::vector<size_t> stream_probabilities_observed(const uint8_t* sequence_start, uintmax_t sequence_size) const;

    /// Relate epsilon to checked file size
    /// Entropy of encrypted file very close to 8.0 (like 7.999998..)
    /// However estimation depends on the sample size
    /// Than bigger the sample than smaller the epsilon
    static double estimated_epsilon(uintmax_t sample_size);

    /// Static flag, set while we load uint8_t facet for the first time
    static bool load_uint8_codecvt_;

    /// Map information properties to string description
    static std::map<InformationEntropyEstimation, std::string> entropy_string_description_;

    /// Buffer size, should not be close to 1 MB as created on a thread stack
    static constexpr size_t MAX_BUFFER_SIZE = 1024 * 64;
};

} // namespace encryption
