/*
 * udptunnel : Lightweight TCP over UDP Tunneling
 *
 * Copyright (C) 2014 Jingyu jingyu.niu@gmail.com
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* #define USE_ANDROID_LOG */
/* #define USE_SYSLOG */

#if defined(USE_SYSLOG)
#include <syslog.h>
#elif defined(USE_ANDROID_LOG)
#include <android/log.h>
#else
#include <stdio.h>
#include <time.h>

#define TIME_FORMAT     "%Y-%m-%d %H:%M:%S"
#endif

#include <stdarg.h>

#include "log.h"

#define LOG_TAG         "udptunnel"

int __log_level = UDPTUNNEL_LOG_DEBUG;

static char* level_names[] = {
    "ERROR",
    "WARNING",
    "INFO",
    "DEBUG"
};

#if !defined(USE_SYSLOG) && !defined(USE_ANDROID_LOG)

#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>

static HANDLE hConsole = NULL;

static void set_color(int level)
{
    WORD attr;

    if (!hConsole)
        hConsole = GetStdHandle(STD_ERROR_HANDLE);

    if (level == UDPTUNNEL_LOG_ERROR)
        attr = FOREGROUND_RED;
    else if (level == UDPTUNNEL_LOG_WARNING)
        attr = FOREGROUND_RED | FOREGROUND_GREEN;
    else if (level == UDPTUNNEL_LOG_INFO)
        attr = FOREGROUND_GREEN;
    else if (level == UDPTUNNEL_LOG_DEBUG)
        attr = FOREGROUND_INTENSITY;

    SetConsoleTextAttribute(hConsole, attr);
}

static void reset_color()
{
    SetConsoleTextAttribute(hConsole, FOREGROUND_BLUE 
                            | FOREGROUND_GREEN | FOREGROUND_RED);
}
#else
static void set_color(int level)
{
    char *color = "";

    if (level == UDPTUNNEL_LOG_ERROR)
        color = "\e[0;31m";
    else if (level == UDPTUNNEL_LOG_WARNING)
        color = "\e[0;33m";
    else if (level == UDPTUNNEL_LOG_INFO)
        color = "\e[0;32m";
    else if (level == UDPTUNNEL_LOG_DEBUG)
        color = "\e[1;30m";

    fprintf(stderr, color);
}

static void reset_color()
{
    fprintf(stderr, "\e[0m");
}
#endif

#endif /* !defined(USE_SYSLOG) && !defined(USE_ANDROID_LOG) */


void utlog(int level, const char *format, ...)
{
    va_list args;
    va_start(args, format);

    utlogv(level, format, args);
}

void utlogv(int level, const char *format, va_list args)
{
    if (level > __log_level)
        return;

#if defined(USE_SYSLOG)
   switch (level) {
   case UDPTUNNEL_LOG_ERROR:
       level = LOG_ERR;
       break;
   case UDPTUNNEL_LOG_WARNING:
       level = LOG_WARNING;
       break;
   case UDPTUNNEL_LOG_INFO:
       level = LOG_INFO;
       break;
   case UDPTUNNEL_LOG_DEBUG:
       level = LOG_DEBUG;
       break;
   default:
       level = LOG_DEBUG;
       break;
   }

   vsyslog(level, format, args);
#elif defined(USE_ANDROID_LOG)
   switch (level) {
   case UDPTUNNEL_LOG_ERROR:
       level = ANDROID_LOG_ERROR;
       break;
   case UDPTUNNEL_LOG_WARNING:
       level = ANDROID_LOG_WARN;
       break;
   case UDPTUNNEL_LOG_INFO:
       level = ANDROID_LOG_INFO;
       break;
   case UDPTUNNEL_LOG_DEBUG:
       level = ANDROID_LOG_DEBUG;
       break;
   default:
       level = ANDROID_LOG_DEBUG;
       break;
   }

   __android_log_vprint(level, LOG_TAG, format, args);
#else
    if (level > UDPTUNNEL_LOG_DEBUG)
        level = UDPTUNNEL_LOG_DEBUG;

    {
        time_t now = time(NULL);
        char timestr[20];
        strftime(timestr, 20, TIME_FORMAT, localtime(&now));

        set_color(level);
        fprintf(stderr, "%s - %-7s : ", timestr, level_names[level]);
        vfprintf(stderr, format, args);
        fprintf(stderr, "\n");
        reset_color();
        fflush(stderr);
    }
#endif
}

void utlog_set_level(int level)
{
    if (level > UDPTUNNEL_LOG_DEBUG)
        level = UDPTUNNEL_LOG_DEBUG;

    __log_level = level;
}
