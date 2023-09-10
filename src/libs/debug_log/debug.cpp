#pragma once
#include "iostream"

#ifndef enable_debug
    #define enable_debug 1
#endif

#if enable_debug
    #define debug(...) printf(__VA_ARGS__)
#else
    #define debug(...) /* Nothing */
#endif