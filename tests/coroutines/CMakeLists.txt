# Copyright 2021 Pavel A. Lebedev
# Licensed under the Apache License, Version 2.0.
# (See accompanying file LICENSE.txt or copy at
#  http://www.apache.org/licenses/LICENSE-2.0)
# SPDX-License-Identifier: Apache-2.0

add_executable(test-coroutines
    src/main.cpp
)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

target_link_libraries(test-coroutines PRIVATE ampi::ampi Threads::Threads ampi::tests-common)

ntc_target(test-coroutines)

add_test(NAME coroutines COMMAND test-coroutines)
