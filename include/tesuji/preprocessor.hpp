#pragma once

// This file is licensed under the Creative Commons Attribution 4.0 International Public License (CC
// BY 4.0).
// See https://creativecommons.org/licenses/by/4.0/legalcode for the full license text.
// github.com/sudosandwich/tesuji

#include "version.hpp"


#define TESUJIPP_STR_IMPL(X) #X
#define TESUJIPP_STR(X)      TESUJIPP_STR_IMPL(X)

#ifndef STRINGIFY
#  define STRINGIFY(X) TESUJIPP_STR(X)
#endif
