#include "../include/tesuji/timed.hpp"
using namespace tesuji;

#if defined(__has_include) && __has_include(<CLI/CLI.hpp>)
#    include <CLI/CLI.hpp>
#else
#    pragma message("Consider getting https://github.com/CLIUtils/CLI11")
#endif

#include <iostream>
#include <random>
#include <chrono>
using namespace std;


int main(int argc, char **argv) {
    timed::block main_block(__FUNCSIG__);

    size_t iterations = 1000000;

#if defined(CLI11_VERSION)
    CLI::App app{"comparing random number generation"};
    app.add_option("-i,--iterations", iterations, "number of iterations")
        ->default_str(std::to_string(iterations));
    CLI11_PARSE(app, argc, argv);
#endif

    random_device rd;

    cout << timed::calls("random_device        ", iterations, rd) << endl;

    auto mersenne = mt19937_64{rd()};
    cout << timed::calls("mt19937_64           ", iterations, mersenne) << endl;

    auto minstd = minstd_rand{rd()};
    cout << timed::calls("minstd_rand          ", iterations, minstd) << endl;

    auto ranlux48Engine = ranlux48{rd()};
    cout << timed::calls("ranlux48             ", iterations, ranlux48Engine) << endl;

    auto knuth_bEngine = knuth_b{rd()};
    cout << timed::calls("knuth_b              ", iterations, knuth_bEngine) << endl;

    auto defaultEngine = default_random_engine{rd()};
    cout << timed::calls("default_random_engine", iterations, defaultEngine) << endl;

    return 0;
}
