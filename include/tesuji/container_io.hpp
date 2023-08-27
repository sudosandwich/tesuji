#pragma once

// This file is licensed under the Creative Commons Attribution 4.0 International Public License (CC
// BY 4.0).
// See https://creativecommons.org/licenses/by/4.0/legalcode for the full license text.
// github.com/sudosandwich/tesuji

#if defined(__has_include) && __has_include("version.hpp")
#include "version.hpp"
#endif

#include <ostream>
#include <regex>

#include <algorithm>
#include <array>
#include <deque>
#include <forward_list>
#include <list>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <version>


namespace tesuji {
    
///////////////////////////////////////////////////////////////////////
// ostream-output for containers
//
// The format is Python-like.
// - ordered homogeneous like std::vector [1, 2]
// - un-ordered homogeneous like std::set {1, 2}
// - un-ordered associative homogeneous like std::map {1: "foo", 2: "bar"}
// - heterogeneous like std::tuple (1, "foo", true)
//
// NB: Remember to `os << std::boolalpha;` if you want bools to use words.
//
// So `using namespace tesuji::container_io;` in functions where you want
// to use these operators.
//
// To extend functionality to your own containers, consider specialising
// `container_io::detail::decorate` if you need custom formatting. Otherwise
// overloading `operator<<` for your type is enough.
//
// NB: We use "'" as the string delimiter (which is fine in python). This makes
// writing things like `vector<int> v; stringstream("['2', '3', '4']") >> v;`
// easier as well as parsing. This loses the possibility to retrieve a `vector<char>`.
// This should really be a drawback, though, as std::string is a regular
// container.
//


namespace detail {


template<typename Char, typename Char2>
std::basic_string<Char> convert_string(const std::basic_string<Char2> &value) {
    return std::basic_string<Char>{value.cbegin(), value.cend()};
}


} // namespace detail


template<typename Char> std::basic_string<Char> regex_escape(const std::basic_string<Char> &value) {
    static const std::basic_regex<Char> specialChars{
        detail::convert_string<Char, char>(R"([-[\]{}()*+?.,\^$|#\s])")};
    static const std::basic_string<Char> replacement{detail::convert_string<Char, char>(R"(\$&)")};

    return std::regex_replace(value, specialChars, replacement);
}


} // namespace tesuji


namespace tesuji { namespace container_io {
namespace detail {


using tesuji::regex_escape;
using tesuji::detail::convert_string;

template<typename Char> using ostr_t = std::basic_ostream<Char>;


// clang-format off
template<typename T> struct is_string_like : public std::false_type{};
template<> struct is_string_like<char> : public std::true_type{};
template<> struct is_string_like<const char*> : public std::true_type{};
template<> struct is_string_like<wchar_t> : public std::true_type{};
template<> struct is_string_like<const wchar_t*> : public std::true_type{};
template<> struct is_string_like<std::string> : public std::true_type{};
template<> struct is_string_like<std::wstring> : public std::true_type{};

template<typename T>
static constexpr const bool 
is_string_like_v = is_string_like<std::remove_cvref_t<T>>::value;
// clang-format on


template<typename Char, typename Char2>
ostr_t<Char> &decorate_string(ostr_t<Char>            &os,
                              std::basic_string<Char2> value,
                              const std::string       &stringDelimiter) {
    static constexpr const bool needsConversion =
        (std::is_same_v<Char, char> && std::is_same_v<Char2, char>)
        || (std::is_same_v<Char2, wchar_t>);

    if constexpr(needsConversion) {
        // static_assert(needsConversion, "wchar_t to char needs a conversion");
    }

    auto escapedStringDelimiter = regex_escape(convert_string<Char2>(stringDelimiter));
    std::basic_regex<Char2>               delimRe{escapedStringDelimiter};
    static const std::basic_regex<Char2>  escapeRe(convert_string<Char2, char>(R"(\\)"));
    static const std::basic_string<Char2> replacement{convert_string<Char2, char>(R"(\$&)")};

    // this implementation is naive as stringDelimiter might contain escape (even though
    // that would be weird)
    value = std::regex_replace(value, escapeRe, replacement);
    value = std::regex_replace(value, delimRe, replacement);

    os << stringDelimiter.c_str();
    os << value.c_str();
    os << stringDelimiter.c_str();

    return os;
}


template<typename Char>
ostr_t<Char> &
decorate_string(ostr_t<Char> &os, const char *value, const std::string &stringDelimiter) {
    return decorate_string(os, std::string(value), stringDelimiter);
}


template<typename Char>
ostr_t<Char> &
decorate_string(ostr_t<Char> &os, const wchar_t *value, const std::string &stringDelimiter) {
    return decorate_string(os, std::wstring(value), stringDelimiter);
}


/////////////////////////////////////////////////////////////////////////////////////

template<typename Char, typename T, size_t N>
ostr_t<Char> &decorate(ostr_t<Char>                              &os,
                       const std::array<T, N>                    &container,
                       const std::pair<std::string, std::string> &containerDelimiters,
                       const std::string                         &stringDelimiter,
                       const std::string                         &valueSeparator);

template<size_t N, typename Char, typename... Args>
ostr_t<Char> &decorate_tuple(ostr_t<Char>              &os,
                             const std::tuple<Args...> &container,
                             const std::string         &stringDelimiter,
                             const std::string         &valueSeparator,
                             bool                       printValueSeparator = false);

template<typename Char, typename... Args>
ostr_t<Char> &decorate(ostr_t<Char>                              &os,
                       const std::tuple<Args...>                 &container,
                       const std::pair<std::string, std::string> &containerDelimiters,
                       const std::string                         &stringDelimiter,
                       const std::string                         &valueSeparator);

template<typename Char, typename Container>
ostr_t<Char> &decorate_assoc(ostr_t<Char>                              &os,
                             const Container                           &container,
                             const std::pair<std::string, std::string> &containerDelimiters,
                             const std::string                         &stringDelimiter,
                             const std::string                         &valueSeparator,
                             const std::string                         &keyValueSeparator);


/////////////////////////////////////////////////////////////////////////////////////


template<typename Char, typename Container>
ostr_t<Char> &decorate(ostr_t<Char>                              &os,
                       const Container                           &container,
                       const std::pair<std::string, std::string> &containerDelimiters,
                       const std::string                         &stringDelimiter,
                       const std::string                         &valueSeparator) {
    bool printValueSeparator = false;

    os << containerDelimiters.first.c_str();
    for(const auto &value: container) {
        if(printValueSeparator) {
            os << valueSeparator.c_str();
        }
        printValueSeparator = true;

        if constexpr(is_string_like_v<decltype(value)>) {
            decorate_string(os, value, stringDelimiter);
        } else {
            os << value;
        }
    }
    os << containerDelimiters.second.c_str();

    return os;
}


template<typename Char, typename T, size_t N>
ostr_t<Char> &decorate(ostr_t<Char>                              &os,
                       const std::array<T, N>                    &container,
                       const std::pair<std::string, std::string> &containerDelimiters,
                       const std::string                         &stringDelimiter,
                       const std::string                         &valueSeparator) {
    os << containerDelimiters.first.c_str();

    bool printValueSeparator = false;
    for(size_t i = 0; i < N; ++i) {
        if(printValueSeparator) {
            os << valueSeparator.c_str();
        }
        printValueSeparator = true;

        const auto &value = container.at(i);

        if constexpr(is_string_like_v<decltype(value)>) {
            decorate_string(os, value, stringDelimiter);
        } else {
            os << value;
        }
    }

    os << containerDelimiters.second.c_str();

    return os;
}


template<size_t N, typename Char, typename... Args>
ostr_t<Char> &decorate_tuple(ostr_t<Char>              &os,
                             const std::tuple<Args...> &container,
                             const std::string         &stringDelimiter,
                             const std::string         &valueSeparator,
                             bool                       printValueSeparator) {
    if(printValueSeparator) {
        os << valueSeparator.c_str();
    }

    const auto &value = std::get<N>(container);

    if constexpr(is_string_like_v<decltype(value)>) {
        decorate_string(os, value, stringDelimiter);
    } else {
        os << value;
    }

    if constexpr(N + 1 < sizeof...(Args)) {
        decorate_tuple<N + 1>(os, container, stringDelimiter, valueSeparator, true);
    }

    return os;
}


template<typename Char, typename... Args>
ostr_t<Char> &decorate(ostr_t<Char>                              &os,
                       const std::tuple<Args...>                 &container,
                       const std::pair<std::string, std::string> &containerDelimiters,
                       const std::string                         &stringDelimiter,
                       const std::string                         &valueSeparator) {
    os << containerDelimiters.first.c_str();
    decorate_tuple<0>(os, container, stringDelimiter, valueSeparator);
    os << containerDelimiters.second.c_str();

    return os;
}


template<typename Char, typename Container>
ostr_t<Char> &decorate_assoc(ostr_t<Char>                              &os,
                             const Container                           &container,
                             const std::pair<std::string, std::string> &containerDelimiters,
                             const std::string                         &stringDelimiter,
                             const std::string                         &valueSeparator,
                             const std::string                         &keyValueSeparator) {
    bool printValueSeparator = false;

    os << containerDelimiters.first.c_str();
    for(const auto &[key, value]: container) {
        if(printValueSeparator) {
            os << valueSeparator.c_str();
        }
        printValueSeparator = true;

        if constexpr(is_string_like_v<decltype(key)>) {
            decorate_string(os, key, stringDelimiter);
        } else {
            os << key;
        }

        os << keyValueSeparator.c_str();

        if constexpr(is_string_like_v<decltype(value)>) {
            decorate_string(os, value, stringDelimiter);
        } else {
            os << value;
        }
    }
    os << containerDelimiters.second.c_str();

    return os;
}


} // namespace detail


#define DEFINE_OSTREAM_OPERATOR(T)                                                                 \
    template<typename Char, typename... Args>                                                      \
    std::basic_ostream<Char> &operator<<(std::basic_ostream<Char> &os,                             \
                                         const T<Args...>         &container) {                            \
        return detail::decorate(os, container, {"[", "]"}, "'", ", ");                             \
    }

// DEFINE_OSTREAM_OPERATOR(std::array)
DEFINE_OSTREAM_OPERATOR(std::vector)
DEFINE_OSTREAM_OPERATOR(std::deque)
DEFINE_OSTREAM_OPERATOR(std::forward_list)
DEFINE_OSTREAM_OPERATOR(std::list)
DEFINE_OSTREAM_OPERATOR(std::set)
DEFINE_OSTREAM_OPERATOR(std::multiset)
DEFINE_OSTREAM_OPERATOR(std::unordered_set)
DEFINE_OSTREAM_OPERATOR(std::unordered_multiset)
#undef DEFINE_OSTREAM_OPERATOR


template<typename Char, typename T, size_t N>
std::basic_ostream<Char> &operator<<(std::basic_ostream<Char> &os,
                                     const std::array<T, N>   &container) {
    return detail::decorate(os, container, {"[", "]"}, "'", ", ");
}

template<typename Char, typename... Args>
std::basic_ostream<Char> &operator<<(std::basic_ostream<Char>  &os,
                                     const std::tuple<Args...> &container) {
    return detail::decorate(os, container, {"(", ")"}, "'", ", ");
}


template<typename Char, typename First, typename Second>
std::basic_ostream<Char> &operator<<(std::basic_ostream<Char>       &os,
                                     const std::pair<First, Second> &container) {
    return os << std::tuple<First, Second>{container};
}


#define DEFINE_OSTREAM_OPERATOR(T)                                                                 \
    template<typename Char, typename... Args>                                                      \
    std::basic_ostream<Char> &operator<<(std::basic_ostream<Char> &os,                             \
                                         const T<Args...>         &container) {                            \
        return detail::decorate_assoc(os, container, {"{", "}"}, "'", ", ", ": ");                 \
    }

DEFINE_OSTREAM_OPERATOR(std::map)
DEFINE_OSTREAM_OPERATOR(std::multimap)
DEFINE_OSTREAM_OPERATOR(std::unordered_map)
DEFINE_OSTREAM_OPERATOR(std::unordered_multimap)
#undef DEFINE_OSTREAM_OPERATOR


}} // namespace tesuji::container_io
