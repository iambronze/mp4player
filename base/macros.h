// Copyright 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_BASE_MACROS_H_
#define MINI_CHROMIUM_BASE_MACROS_H_

#include <string.h>
#include <sys/types.h>

#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
    TypeName(const TypeName&); \
    void operator=(const TypeName&)

#define DISALLOW_IMPLICIT_CONSTRUCTORS(TypeName) \
    TypeName(); \
    DISALLOW_COPY_AND_ASSIGN(TypeName)

template<typename T>
inline void ignore_result(const T &) {
}

#endif  // MINI_CHROMIUM_BASE_MACROS_H_
