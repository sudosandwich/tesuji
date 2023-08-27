#pragma once

// This file is licensed under the Creative Commons Attribution 4.0 International Public License (CC
// BY 4.0).
// See https://creativecommons.org/licenses/by/4.0/legalcode for the full license text.
// github.com/sudosandwich/tesuji

#if defined(__has_include) && __has_include("version.hpp")
#    include "version.hpp"
#endif

#include <chrono>
#include <format>
#include <iostream>
#include <string>


namespace tesuji { namespace timed {

// Provides a function to format a duration in a human readable way.
//      std::string durationToHumanString(auto duration);
// Example:
//      cout << timed::durationToHumanString( 3h + 2min + 1s + 1ms + 1us + 1ns ) << endl;
//      cout << timed::durationToHumanString( 42ms + 103ns ) << endl;
// Possible output:
//      03:02:01.001
//      42.103ms
//
//
// Provides a class to measure the time between construction and destruction of that object. Blocks
// can be nested and will be indented accordingly.
//      struct block;
// Example:
//      {
//          timed::block b("do_stuff_block");
//          // do stuff
//          {
//              timed::block b("do_more_stuff_block");
//              // do more stuff
//          }
//      }
// Possible output:
//          do_more_stuff_block: 13ms
//      do_stuff_block: 42ms
//
//
// Provides a function to measure the time of a single function call, returning the result of the
// function. This way, this function can be used as a decorator.
//      auto call(std::string_view name, auto &&func);
// Example:
//      auto f = [&](){
//          // do stuff
//      };
//      auto result = timed::call("do_stuff_function", f);
// Possible output:
//      do_stuff_function: 42ms
//
//
// Provides a function to measure the time of multiple function calls, returning a struct with
// information about the calls. This struct can be printed to cout.
//      struct call_info;
//      call_info calls(std::string_view name, size_t count, auto &&func);
// Example:
//      std::mt19937_64 rng{random_device{}()};
//      auto f = [&](){
//          this_thread::sleep_for(
//              milliseconds{uniform_int_distribution<>{0, 100}(rng)});
//      };
//      cout << timed::calls("random_sleeper", 100, f) << endl;
// Possible output:
//      random_sleeper: total: 5.0575s avg: 55ms, min: 3700ns, max: 110ms
//


using namespace std::chrono_literals;

using std::chrono::duration_cast;
using std::chrono::high_resolution_clock;

using std::chrono::microseconds;
using std::chrono::milliseconds;
using std::chrono::nanoseconds;
using std::chrono::seconds;


std::string durationToHumanString(auto duration) {
    if(duration < 1us) {
        return std::format("{}", duration);
    } else if(duration < 1ms) {
        return std::format("{}", duration_cast<microseconds>(duration));
    } else if(duration < 1s) {
        return std::format("{}", duration_cast<milliseconds>(duration));
    } else if(duration < 1min) {
        // Unfortunately chrono's format doesn't support precision, and it will always add a padding
        // 0, so we have to do this manually. The milliseconds happen to be the 4 digits after the
        // comma in seconds. `{:0>4}` pads the milliseconds with zeros to the left, and we get the
        // rational part of duration.
        auto secPart   = duration_cast<seconds>(duration);
        auto milliPart = duration_cast<milliseconds>(duration - secPart);
        return std::format("{}.{:0>4}s", secPart.count(), milliPart.count());
    } else {
        return std::format("{:%T}", duration_cast<milliseconds>(duration));
    }
};


template<size_t IndentFactor = 4> struct block
{
    static inline size_t          indent        = 0;
    static constexpr const size_t indent_factor = IndentFactor;

    std::string                       name;
    high_resolution_clock::time_point start;

    block(std::string_view name = "local_block")
        : name(name)
        , start(high_resolution_clock::now()) {
        ++indent;
    }

    ~block() {
        auto end      = high_resolution_clock::now();
        auto duration = duration_cast<milliseconds>(end - start);
        std::cout << std::format("{}{}: {}\n", std::string(--indent * indent_factor, ' '), name,
                                 durationToHumanString(duration));
    }
};


auto call(std::string_view name, auto &&func) {
    block b(name);
    return func();
}


struct call_info
{
    using duration = high_resolution_clock::time_point::duration;

    std::string name;
    size_t      count{0};
    duration    total{0};
    duration    avg{0};
    duration    min{0};
    duration    max{0};
};


std::ostream &operator<<(std::ostream &os, const call_info &info) {
    return os << std::format("{}: total: {: >5} avg: {: >5}, min: {: >5}, max: {: >5}", info.name,
                             durationToHumanString(info.total), durationToHumanString(info.avg),
                             durationToHumanString(info.min), durationToHumanString(info.max));
}


call_info calls(std::string_view name, size_t count, auto &&func) {
    call_info info{std::string(name), count};

    if(count == 0) {
        return info;
    }

    // "warmup" to get some initial values
    {
        auto start = high_resolution_clock::now();
        (void)func();
        info.total = high_resolution_clock::now() - start;
        info.min   = info.total;
        info.max   = info.total;
    }

    // start at 1 because we already did one call
    for(size_t i = 1; i < count; ++i) {
        auto start = high_resolution_clock::now();
        (void)func();
        auto duration = high_resolution_clock::now() - start;
        info.total += duration;
        info.min = std::min(info.min, duration);
        info.max = std::max(info.max, duration);
    }

    info.avg = info.total / info.count;

    return info;
}


}} // namespace tesuji::timed
