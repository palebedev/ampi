# Copyright 2021 Pavel A. Lebedev
# Licensed under the Apache License, Version 2.0.
# (See accompanying file LICENSE.txt or copy at
#  http://www.apache.org/licenses/LICENSE-2.0)
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.19)

project(ampi
        VERSION 0.6.0
        DESCRIPTION "Asynchronous MessagePack Implementation"
        LANGUAGES CXX
)

find_package(ntc-cmake REQUIRED)
include(ntc-dev-build)

add_library(${PROJECT_NAME}
    include/ampi/asio/as_tuple.hpp
    include/ampi/asio/with_as_default_on.hpp
    include/ampi/async_msgpack.hpp
    include/ampi/async_msgunpack.hpp
    include/ampi/buffer.hpp
    include/ampi/buffer_sinks/async_stream_buffer_sink.hpp
    include/ampi/buffer_sinks/container_buffer_sink.hpp
    include/ampi/buffer_sinks/ostream_buffer_sink.hpp
    include/ampi/buffer_sources/async_stream_buffer_source.hpp
    include/ampi/buffer_sources/buffer_source.hpp
    include/ampi/buffer_sources/istream_buffer_source.hpp
    include/ampi/buffer_sources/one_buffer_source.hpp
    include/ampi/coro/awaiter_wrapper.hpp
    include/ampi/coro/coro_handle_owner.hpp
    include/ampi/coro/coroutine.hpp
    include/ampi/coro/stdcoro.hpp
    include/ampi/coro/traits.hpp
    include/ampi/coro/use_coroutine.hpp
    include/ampi/detail/fixed_msgpack_buffer_factory.hpp
    include/ampi/detail/hana_struct.hpp
    include/ampi/detail/msgpack_ctx_base.hpp
    include/ampi/detail/msgunpack_ctx_base.hpp
    include/ampi/detail/pfr_tuple.hpp
    include/ampi/event.hpp
    include/ampi/event_endpoints.hpp
    include/ampi/event_sinks/event_sink.hpp
    include/ampi/event_sinks/hana_struct.hpp
    include/ampi/event_sinks/pfr_tuple.hpp
    include/ampi/event_sources/event_source.hpp
    include/ampi/event_sources/hana_struct.hpp
    include/ampi/event_sources/pfr_tuple.hpp
    include/ampi/exception.hpp
    include/ampi/execution/executor.hpp
    include/ampi/execution/executor_wrapper.hpp
    include/ampi/execution/prefer.hpp
    include/ampi/execution/query.hpp
    include/ampi/execution/require.hpp
    include/ampi/execution/traits.hpp
    include/ampi/filters/emitter.hpp
    include/ampi/filters/parser.hpp
    include/ampi/hash/flat_map.hpp
    include/ampi/hash/span.hpp
    include/ampi/hash/time_point.hpp
    include/ampi/hash/vector.hpp
    include/ampi/istream.hpp
    include/ampi/manipulator.hpp
    include/ampi/msgpack.hpp
    include/ampi/piecewise_view.hpp
    include/ampi/pmr/detail/block_list_resource.hpp
    include/ampi/pmr/reusable_monotonic_buffer_resource.hpp
    include/ampi/pmr/segmented_stack_resource.hpp
    include/ampi/pmr/shared_polymorphic_allocator.hpp
    include/ampi/pmr/trivially_deallocatable_resource.hpp
    include/ampi/transmute.hpp
    include/ampi/utils/bit.hpp
    include/ampi/utils/empty_subobject.hpp
    include/ampi/utils/flags.hpp
    include/ampi/utils/format.hpp
    include/ampi/utils/ref_counted_base.hpp
    include/ampi/utils/repeated.hpp
    include/ampi/utils/stdtypes.hpp
    include/ampi/utils/tag_invoke.hpp
    include/ampi/utils/tagged_pointer.hpp
    include/ampi/value.hpp
    include/ampi/utf8_validator.hpp
    include/ampi/vocabulary.hpp
    src/event.cpp
    src/event_endpoints.cpp
    src/exception.cpp
    src/filters/parser.cpp
    src/piecewise_view.cpp
    src/pmr/reusable_monotonic_buffer_resource.cpp
    src/pmr/segmented_stack_resource.cpp
    src/pmr/shared_polymorphic_allocator.cpp
    src/pmr/trivially_deallocatable_resource.cpp
    src/utf8_validator.cpp
    src/value.cpp
)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

find_package(Boost 1.75 REQUIRED COMPONENTS container thread)
target_link_libraries(${PROJECT_NAME} PUBLIC Boost::container)

ntc_target(${PROJECT_NAME})

set(DOXYGEN_HTML_COLORSTYLE_HUE 40)

find_package(Doxygen OPTIONAL_COMPONENTS dot)
if(Doxygen_FOUND)
    set(DOXYGEN_BUILTIN_STL_SUPPORT YES)
    set(DOXYGEN_SHOW_INCLUDE_FILES NO)
    set(DOXYGEN_SORT_MEMBERS_CTORS_1ST YES)
    set(DOXYGEN_STRIP_FROM_PATH "${CMAKE_SOURCE_DIR}/include")
    set(DOXYGEN_DOT_IMAGE_FORMAT svg)
    set(DOXYGEN_INTERACTIVE_SVG YES)
    if(CMAKE_CXX_COMPILER_ID MATCHES Clang AND CMAKE_EXPORT_COMPILE_COMMANDS)
        set(DOXYGEN_CLANG_ASSISTED_PARSING YES)
        set(DOXYGEN_CLANG_DATABASE_PATH "${CMAKE_BINARY_DIR}")
    endif()
    doxygen_add_docs(${PROJECT_NAME}-docs
        "${CMAKE_SOURCE_DIR}/include"
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    )
    if(NOT TARGET docs)
        add_custom_target(docs)
    endif()
    add_dependencies(docs ${PROJECT_NAME}-docs)
endif()

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
