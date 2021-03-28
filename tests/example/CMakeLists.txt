# Copyright 2021 Pavel A. Lebedev
# Licensed under the Apache License, Version 2.0.
# (See accompanying file LICENSE.txt or copy at
#  http://www.apache.org/licenses/LICENSE-2.0)
# SPDX-License-Identifier: Apache-2.0

add_executable(test-example
    src/main.cpp
)

target_link_libraries(test-example PRIVATE ampi::ampi ampi::tests-common Boost::thread)

ntc_target(test-example)

add_test(NAME example COMMAND test-example)
