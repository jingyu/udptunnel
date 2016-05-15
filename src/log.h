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

#ifndef __LOG_H__
#define __LOG_H__

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UDPTUNNEL_LOG_ERROR         0
#define UDPTUNNEL_LOG_WARNING       1
#define UDPTUNNEL_LOG_INFO          2
#define UDPTUNNEL_LOG_DEBUG         3

extern int __log_level;

#define log_error(format, ...) \
    do { \
        if (__log_level >= UDPTUNNEL_LOG_ERROR) \
            utlog(UDPTUNNEL_LOG_ERROR, format, ##__VA_ARGS__); \
    } while(0)

#define log_warning(format, ...) \
    do { \
        if (__log_level >= UDPTUNNEL_LOG_WARNING) \
            utlog(UDPTUNNEL_LOG_WARNING, format, ##__VA_ARGS__); \
    } while(0)

#define log_info(format, ...) \
    do { \
        if (__log_level >= UDPTUNNEL_LOG_INFO) \
            utlog(UDPTUNNEL_LOG_INFO, format, ##__VA_ARGS__); \
    } while(0)

#define log_debug(format, ...) \
    do { \
        if (__log_level >= UDPTUNNEL_LOG_DEBUG) \
            utlog(UDPTUNNEL_LOG_DEBUG, format, ##__VA_ARGS__); \
    } while(0)

void utlog(int level, const char *format, ...);

void utlogv(int level, const char *format, va_list args);

void utlog_set_level(int level);

#ifdef __cplusplus
}
#endif

#endif /* __LOG_H__ */
