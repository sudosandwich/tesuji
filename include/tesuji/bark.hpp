#pragma once

// This file is licensed under the Creative Commons Attribution 4.0 International Public License (CC
// BY 4.0).
// See https://creativecommons.org/licenses/by/4.0/legalcode for the full license text.
// github.com/sudosandwich/tesuji

#include "version.hpp"

#include <iostream>
#include <regex>
#include <string>


namespace tesuji {

// Provides a function to make types in compiler and type info output more readable.
//    string declutter(std::string name);
//
// Provides a macro to print the current function name and line.
//    #define BARK ...
//
// Provides a macro to print the type of an expression.
//    #define ETYPE(E) ...
//
// Example:
//    int main() {
//      BARK;
//      int i = 0;
//      cout << ETYPE(i) << "\n";
//      return 0;
//    }
//
// Possible output:
//      int main(int,char **)@25
//      int
//


namespace detail {
inline std::string declutter(std::string name) {
    using namespace std;

    // remove msvc annotations
    name = regex_replace(name, regex("\\(void\\)"), "()");

    name = regex_replace(name, regex("\\b(__cdecl|__stdcall|__fastcall)\\s+"), "");

    name = regex_replace(name, regex("\\b(struct|class)\\s+"), "");

    // remove extra spaces
    name = regex_replace(name, regex("\\s*> >\\s*"), ">>");
    name = regex_replace(name, regex("<\\s+"), "<");
    name = regex_replace(name, regex("(,|::)\\s+"), "$1");

    // types e.g. "unsigned __int64" -> "uint64_t"
    name = regex_replace(name, regex("unsigned __([a-z]+)(\\d+)"), "u$1$2_t");
    name = regex_replace(name, regex("(?:signed )?__([a-z]+)(\\d+)"), "$1$2_t");

    // strings
    name = regex_replace(name,
                         regex("std::basic_string<char,std::char_traits<char>,"
                               "std::allocator<char>>"),
                         "std::string");

    name = regex_replace(name,
                         regex("std::basic_string<wchar_t,std::char_traits<wchar_"
                               "t>,std::allocator<wchar_t>>"),
                         "std::wstring");

    // containers
    name = regex_replace(name,
                         regex("std::(vector|deque|forward_list|list|set|multiset|"
                               "unordered_set|unordered_multiset)<([^,]+),.*"),
                         "std::$1<$2>");

    name = regex_replace(name,
                         regex("std::(map|multimap|unordered_map|unordered_"
                               "multimap|pair)<([^,]+),([^,]+),.*"),
                         "std::$1<$2,$3>");

    return name;
}

} // namespace detail
} // namespace tesuji

#ifndef __PRETTY_FUNCTION__
#    define __PRETTY_FUNCTION__ __FUNCSIG__
#endif

#define BARK                                                                                       \
    std::cout << tesuji::detail::declutter(__PRETTY_FUNCTION__) << "@" << __LINE__ << "\n"         \
              << std::flush
#define ETYPE(E) tesuji::detail::declutter(typeid(decltype(E)).name())
