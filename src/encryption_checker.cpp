#include <eraser/encryption_checker.h>
#include <helpers/uint8_codecvt.h>
#include <sstream>
#include <fstream>
#include <cassert>
#include <stdexcept>

#include <boost/filesystem.hpp>

using namespace std;
using namespace shredder;

namespace fs = boost::filesystem;

bool ShannonEncryptionChecker::load_uint8_codecvt_;
bool ShannonEncryptionChecker::interrupt_all_ = false;


std::map<ShannonEncryptionChecker::InformationEntropyEstimation, std::string>
ShannonEncryptionChecker::entropy_string_description_ = {
    { Plain , "Plain" },
    { Binary , "Binary" },
    { Encrypted , "Encrypted" },
    { Unknown , "Unknown" } };


ShannonEncryptionChecker::ShannonEncryptionChecker()
{
    assert(entropy_string_description_.size() == EntropyLevelSize);
    if (false == load_uint8_codecvt_) {
        std::locale::global(std::locale(std::locale(), new std::codecvt<uint8_t, char, std::mbstate_t>));
        load_uint8_codecvt_ = true;
    }
}

void ShannonEncryptionChecker::set_callback(IShredderCallback* callback)
{
    callback_ = callback;
}

double ShannonEncryptionChecker::get_file_entropy(std::wstring file_path) const
{
    uintmax_t file_size = fs::file_size(file_path);
    std::vector<double> byte_probabilities = read_file_probabilities(file_path, file_size);
    if (byte_probabilities.empty()) {
        return -1.0;
    }
    return shannon_entropy(byte_probabilities.begin(), byte_probabilities.end());
}

double ShannonEncryptionChecker::get_sequence_entropy(const uint8_t* sequence_start, size_t sequence_size) const
{
    std::vector<double> byte_probabilities = read_stream_probabilities(sequence_start, sequence_size);
    if(byte_probabilities.empty()) {
        return -1.0;
    }
    return shannon_entropy(byte_probabilities.begin(), byte_probabilities.end());
}

ShannonEncryptionChecker::InformationEntropyEstimation
ShannonEncryptionChecker::information_entropy_estimation(double entropy, uintmax_t sequence_size)
{
    // known case, entropy calculation interrupted
    if (entropy == -1.0) {
        return Unknown;
    }

    double epsilon = estimated_epsilon(sequence_size);
    if ((8.0 - entropy) < epsilon) {
        return Encrypted;
    }
    else if (entropy > 6.0) {
        return Binary;
    }
    else if (entropy >= 0. && entropy <= 6.0) {
        return Plain;
    }
    // should not be here, entropy calculation error
    assert(false);
    return Unknown;
}

size_t ShannonEncryptionChecker::min_compressed_size(double entropy, size_t sequence_size) const
{
    return static_cast<size_t>((entropy * sequence_size) / 8);
}

std::string ShannonEncryptionChecker::get_information_description(InformationEntropyEstimation ent)
{
    std::string descr = entropy_string_description_[ent];

    // all descriptions must be provided!
    assert(!descr.empty());
    return descr;
}

void ShannonEncryptionChecker::interrupt(bool interrupt_flag)
{
    interrupt_all_ = interrupt_flag;
}

std::vector<double> ShannonEncryptionChecker::read_file_probabilities(const std::wstring& file_path, uintmax_t file_size) const
{
    // probability of every byte of zero-sized file is 0
    if (0 == file_size) {
        return std::vector<double>(256);
    }

    std::vector<double> bytes_frequencies(256);
    std::vector<size_t> bytes_distribution;
    if (callback_) {
        bytes_distribution = file_probabilities_observed(file_path, file_size);
    }
    else {
        bytes_distribution = file_probabilities_fast(file_path, file_size);
    }

    for (size_t i = 0; i != 256; ++i) {
        if (interrupt_all_) {
            return std::vector<double>{};
        }

        bytes_frequencies[i] = static_cast<double>(bytes_distribution[i]) / file_size;
    }

    return std::move(bytes_frequencies);
}

std::vector<size_t> ShannonEncryptionChecker::file_probabilities_fast(const std::wstring& file_path, uintmax_t file_size) const
{
    assert(callback_ == nullptr);
#if defined(_WIN32) || defined(_WIN64)
    std::basic_ifstream<uint8_t, std::char_traits<uint8_t>> file(file_path, std::ios::in | std::ios::binary);
#else
    std::basic_ifstream<uint8_t, std::char_traits<uint8_t>> file(helpers::wstring_to_string(file_path), std::ios::in | std::ios::binary);
#endif
    if (!file.is_open()) {
        return std::vector<size_t>{};
    }

    std::vector<size_t> bytes_distribution(256);
    uint8_t read_ahead_buffer[MAX_BUFFER_SIZE];
    file.rdbuf()->pubsetbuf(read_ahead_buffer, MAX_BUFFER_SIZE);

    uint8_t b{};
    while (file.good()) {

        if (interrupt_all_) {
            return std::vector<size_t>{};
        }

        file.read(reinterpret_cast<uint8_t*>(&b), sizeof(uint8_t));
        ++bytes_distribution[b];
    }
    return std::move(bytes_distribution);

}

std::vector<size_t> ShannonEncryptionChecker::file_probabilities_observed(const std::wstring& file_path, uintmax_t file_size) const
{
    assert(callback_);
#if defined(_WIN32) || defined(_WIN64)
    std::basic_ifstream<uint8_t, std::char_traits<uint8_t>> file(file_path, std::ios::in | std::ios::binary);
#else
    std::basic_ifstream<uint8_t, std::char_traits<uint8_t>> file(helpers::wstring_to_string(file_path), std::ios::in | std::ios::binary);
#endif
    if (!file.is_open()) {
        return std::vector<size_t>{};
    }

    std::vector<size_t> bytes_distribution(256);
    uint8_t read_ahead_buffer[MAX_BUFFER_SIZE];
    file.rdbuf()->pubsetbuf(read_ahead_buffer, MAX_BUFFER_SIZE);

    uintmax_t precent_size = 10;
    uintmax_t precent_counter = 0;
    if (file_size > 1024) {
        precent_size = file_size / 100;
    }
    
    callback_->init(file_size);


    uint8_t b{};
    uintmax_t counter{};
    while (file.good()) {

        if (interrupt_all_) {
            return std::vector<size_t>{};
        }

        file.read(reinterpret_cast<uint8_t*>(&b), sizeof(uint8_t));

        ++counter;
        ++precent_counter;
        if (callback_ && precent_counter == precent_size) {
            callback_->set_value(counter);
            precent_counter = 0;
        }

        ++bytes_distribution[b];
    }
    return std::move(bytes_distribution);
}

std::vector<double> ShannonEncryptionChecker::read_stream_probabilities(const uint8_t* sequence_start, uintmax_t sequence_size) const
{
    if (0 == sequence_size) {
        return std::move(std::vector<double>(256));
    }
    
    std::vector<double> bytes_frequencies(256);
    std::vector<size_t> bytes_distribution;
    if (callback_) {
        bytes_distribution = stream_probabilities_observed(sequence_start, sequence_size);
    }
    else {
        bytes_distribution = stream_probabilities_fast(sequence_start, sequence_size);
    }

    for (size_t i = 0; i != 256; ++i) {
        if (interrupt_all_) {
            return std::vector<double>{};
        }

        bytes_frequencies[i] = static_cast<double>(bytes_distribution[i]) / sequence_size;
    }

    return std::move(bytes_frequencies);
}

std::vector<size_t> ShannonEncryptionChecker::stream_probabilities_fast(const uint8_t* sequence_start, uintmax_t sequence_size) const
{
    assert(callback_ == nullptr);
    std::vector<size_t> bytes_distribution(256);

    for (size_t i = 0; i < sequence_size; ++i) {
        if (interrupt_all_) {
            return std::vector<size_t>{};
        }

        ++bytes_distribution[sequence_start[i]];
    }
    return std::move(bytes_distribution);
}

std::vector<size_t> ShannonEncryptionChecker::stream_probabilities_observed(const uint8_t* sequence_start, uintmax_t sequence_size) const
{
    assert(callback_);
    std::vector<size_t> bytes_distribution(256);

    if (callback_) {
        callback_->init(sequence_size);
    }

    uintmax_t counter{};
    for (size_t i = 0; i < sequence_size; ++i) {
        if (interrupt_all_) {
            return std::vector<size_t>{};
        }

        if (callback_) {
            callback_->set_value(++counter);
        }

        ++bytes_distribution[sequence_start[i]];
    }
    return std::move(bytes_distribution);
}

double ShannonEncryptionChecker::estimated_epsilon(uintmax_t sample_size)
{
    // Note: numbers based on very approximate estimations (several test calculations)
    // More reliable statistic should be collected for more exact results
    if (sample_size < (1024 * 1024)) {
        return 0.001;
    }
    else if (sample_size < (1024 * 1024 * 64)) {
        return 0.0001;
    }
    else if (sample_size < (1024 * 1024 * 512)) {
        return 0.00001;
    }
    return 0.000001;
}
