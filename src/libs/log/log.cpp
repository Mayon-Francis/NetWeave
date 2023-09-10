#pragma once

#include "stdio.h"
class Logger
{

public:
    Logger()
    {
    }

    template <typename... Args>
    void info(Args... args)
    {
        printf("\n[info]: ");
        printf(args...);
    }
    template <typename... Args>
    void error(Args... args)
    {
        printf("\n[error]: ");
        perror(args...);
        printf("\n");
    }
    template <typename... Args>
    void crash(Args... args)
    {
        printf("\n[crash]: ");
        perror(args...);
        printf("\n");
        exit(1);
    }
};