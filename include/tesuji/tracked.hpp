#pragma once

// This file is licensed under the Creative Commons Attribution 4.0 International Public License (CC
// BY 4.0).
// See https://creativecommons.org/licenses/by/4.0/legalcode for the full license text.
// github.com/sudosandwich/tesuji

#include "version.hpp"

#include <cassert>
#include <iostream>
#include <string>


namespace tesuji::tracked {

// Namespace tracked provides the following classes to aid understanding
// when objects are constructed and destroyed, copied, moved and so on.
// It is also important to see how memory leaks can occur, so at the end
// of the program a list of leaked objects is printed.
//
// The following classes are provided:
//    struct B;     // base class, virtual destructor
//    struct D : B; // derived class
//
//
// Example:
//    int main() {
//      auto* p = new tesuji::tracked::D;
//      auto* q = new tesuji::tracked::B;
//      *q = *p;
//      delete p;
//    }
//
// Possible output:
//    new(D) B0() D0() new(B) B1() B0=B1(&) ~D0() ~B0() delete(D) leaked
//    objects: B1(0x00000138012C0560)
//

namespace detail {
struct alloc_tracker
{
    alloc_tracker()                      = default;
    alloc_tracker(const alloc_tracker &) = delete;

    ~alloc_tracker() {
        for(const allocation &alloc: m_allocs) {
            if(!alloc.deleted) {
                static const auto execOnlyOnce = []() {
                    std::cout << "leaked objects: ";
                    return true;
                }();

                std::cout << alloc.classname << alloc.counter << "(0x" << alloc.address << ") "
                          << std::flush;
            }
        }
    }

    void new_(void *address) {
        m_allocs.emplace_back(allocation{address, "", -1, false});
    }

    bool delete_(void *address, const std::string &classname) {
        auto allocIt =
            std::find_if(m_allocs.rbegin(), m_allocs.rend(), [&](const allocation &alloc) {
                return alloc.address == address /*&& alloc.classname == classname*/;
            });

        if(allocIt == m_allocs.rend()) {
            std::cout << "delete of unkown object " << classname << "(0x" << address << ") "
                      << std::flush;
            return false;
        } else {
            if(allocIt->deleted) {
                std::cout << "double delete of " << allocIt->classname << "(0x" << address << ") "
                          << std::flush;
                return false;
            } else {
                allocIt->deleted = true;
                return false;
            }
        }
    }

    void construct_(void *address, const std::string &classname, int counter) {
        auto allocIt =
            std::find_if(m_allocs.begin(), m_allocs.end(), [address](const allocation &alloc) {
                return alloc.address == address;
            });

        if(allocIt != m_allocs.end()) {
            assert(allocIt->counter == -1 || allocIt->counter == counter);

            allocIt->classname = classname;
            allocIt->counter   = counter;
        }
    }

    struct allocation
    {
        void       *address;
        std::string classname;
        int         counter;
        bool        deleted;

        friend std::ostream &operator<<(std::ostream &os, const allocation &alloc) {
            os << alloc.classname << alloc.counter << "(0x" << alloc.address << ")["
               << (alloc.deleted ? "d" : "a") << "]";
            return os;
        }
    };

    std::vector<allocation> m_allocs;
};

struct tracked_base
{
    static size_t        currentCounter;
    static alloc_tracker allocs;

protected:
    tracked_base()
        : m_counter(currentCounter++) {}

protected:
    size_t m_counter;
};

size_t        tracked_base::currentCounter = 0;
alloc_tracker tracked_base::allocs;

} // namespace detail


#define TESUJI_TRACKED_MEMBER_FUNCS(C)                                                             \
private:                                                                                           \
    static constexpr const char *classname = #C;                                                   \
    friend class alloc_tracker;                                                                    \
                                                                                                   \
public:                                                                                            \
    /*construction*/                                                                               \
    C() {                                                                                          \
        allocs.construct_(this, classname, m_counter);                                             \
        std::cout << classname << m_counter << "() " << std::flush;                                \
    }                                                                                              \
                                                                                                   \
    C(const C &rhs) {                                                                              \
        std::cout << classname << m_counter << "(" << rhs.classname << rhs.m_counter << "&) "      \
                  << std::flush;                                                                   \
    }                                                                                              \
                                                                                                   \
    C(C &&rhs) {                                                                                   \
        std::cout << classname << m_counter << "(" << rhs.classname << rhs.m_counter << "&&) "     \
                  << std::flush;                                                                   \
    }                                                                                              \
                                                                                                   \
    void *operator new(size_t count) {                                                             \
        void *p = malloc(count);                                                                   \
        allocs.new_(p);                                                                            \
        std::cout << "new(" << classname << ") " << std::flush;                                    \
        return p;                                                                                  \
    }                                                                                              \
                                                                                                   \
    void *operator new[](size_t count) {                                                           \
        size_t numberOfObjects = count / sizeof(C); /*truncation is correct here*/                 \
        void  *p               = malloc(count);                                                    \
        allocs.new_(p);                                                                            \
        std::cout << "new[" << numberOfObjects << "](" << classname << ") " << std::flush;         \
        return p;                                                                                  \
    }                                                                                              \
                                                                                                   \
    /*destruction*/                                                                                \
    virtual ~C() {                                                                                 \
        std::cout << "~" << classname << m_counter << "() " << std::flush;                         \
    }                                                                                              \
                                                                                                   \
    void operator delete(void *p) {                                                                \
        const bool toDelete = allocs.delete_(p, classname);                                        \
        std::cout << "delete(" << classname << ") " << std::flush;                                 \
        if(toDelete)                                                                               \
            free(p);                                                                               \
    }                                                                                              \
                                                                                                   \
    void operator delete[](void *p) {                                                              \
        const bool toDelete = allocs.delete_(p, classname);                                        \
        std::cout << "delete[](" << classname << ") " << std::flush;                               \
        if(toDelete)                                                                               \
            free(p);                                                                               \
    }                                                                                              \
                                                                                                   \
    /*movement*/                                                                                   \
    const C &operator=(const C &rhs) {                                                             \
        std::cout << rhs.classname << rhs.m_counter << "=";                                        \
        std::cout << classname << m_counter << "(&) " << std::flush;                               \
        return *this;                                                                              \
    }                                                                                              \
                                                                                                   \
    C &operator=(C &&rhs) {                                                                        \
        std::cout << rhs.classname << rhs.m_counter << "=";                                        \
        std::cout << classname << m_counter << "(&&) " << std::flush;                              \
        return *this;                                                                              \
    }

struct B : detail::tracked_base
{
    TESUJI_TRACKED_MEMBER_FUNCS(B)
};

struct D : B
{
    TESUJI_TRACKED_MEMBER_FUNCS(D)
};


#undef TESUJI_TRACKED_MEMBER_FUNCS


} // namespace tesuji::tracked
