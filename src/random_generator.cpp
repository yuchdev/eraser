#include <functional>
#include <algorithm>
#include <ctime>
#include <thread>
#include <eraser/random_generator.h>


//static 
uint8_t RandomGenerator::clean_buffer1[cleann_buffer_size] = {};

RandomGenerator::RandomGenerator() : rnd_device(time(NULL))
{
    std::thread(&RandomGenerator::generate, this).detach();
}

void RandomGenerator::generate()
{
    std::mt19937 mersenne_engine(rnd_device());
    
    std::uniform_int_distribution<unsigned> dist(0, 255);
    auto gen = std::bind(dist, mersenne_engine);
    std::generate(clean_buffer1, clean_buffer1 + cleann_buffer_size, gen);
    random_buffer_.store(clean_buffer1);
}

uint8_t* RandomGenerator::random_sequence() const
{
    while (nullptr == random_buffer_.load()){}
    return clean_buffer1;
}

size_t RandomGenerator::random_length() const
{
    return cleann_buffer_size;
}
