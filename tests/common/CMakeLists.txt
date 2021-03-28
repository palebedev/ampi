# Copyright 2021 Pavel A. Lebedev
# Licensed under the Apache License, Version 2.0.
# (See accompanying file LICENSE.txt or copy at
#  http://www.apache.org/licenses/LICENSE-2.0)
# SPDX-License-Identifier: Apache-2.0

add_library(ampi-tests-common
    include/ampi/tests/ut_helpers.hpp
    src/main.cpp
)

find_package(ut REQUIRED)

target_link_libraries(ampi-tests-common INTERFACE boost::ut)

ntc_target(ampi-tests-common
    ALIAS_NAME ampi::tests-common
    HEADER_PREFIX ampi/tests/
)
