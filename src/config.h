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

#ifndef __CONFIG_H__
#define __CONFIG_H__

#if defined(_MSC_VER)
#define inline          __inline
#endif

#if defined(__GNUC__)
#define UNUSED(x)       UNUSED_##x __attribute__((unused))
#else
#define UNUSED(x)       x
#endif

#if defined(_WIN32) || defined(_WIN64)
#define strtok_r(str, delim, saveptr)   strtok_s(str, delim, saveptr)
#define usleep(microseconds)            Sleep((microseconds)/1000)
#endif

#endif /* __CONFIG_H__ */
