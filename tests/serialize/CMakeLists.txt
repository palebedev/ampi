# Copyright 2021 Pavel A. Lebedev
# Licensed under the Apache License, Version 2.0.
# (See accompanying file LICENSE.txt or copy at
#  http://www.apache.org/licenses/LICENSE-2.0)
# SPDX-License-Identifier: Apache-2.0

add_executable(test-serialize
    src/main.cpp
)

target_link_libraries(test-serialize PRIVATE ampi::ampi ampi::tests-common Boost::thread)

ntc_target(test-serialize)

add_test(NAME serialize COMMAND test-serialize)
