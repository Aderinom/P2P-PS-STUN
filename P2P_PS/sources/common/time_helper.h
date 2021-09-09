#pragma once
#include <sys/time.h>
#include <stdint.h>

uint64_t get_time_in_ms(){
    struct timeval time_now{};
    gettimeofday(&time_now, nullptr);
    return (time_now.tv_sec * 1000) + (time_now.tv_usec / 1000);
}