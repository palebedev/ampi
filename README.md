Asynchronous MessagePack Implementation (AMPI)
==============================================

This library provides serialization and deserialization facilities based internally on C++20 coroutines that implement MessagePack format (https://msgpack.org).

This is an early prototype - none of the interfaces, tests and docs are finished and complete.

License
-------
This library is licensed under the Apache License, Version 2.0. See LICENSE.txt.

Files under `patches` subdirectory come from external sources and are NOT covered by this license.

Documentation
-------------

See `tests/example/src/main.cpp` for example usage and basic concepts.

Dependencies
------------

- LLVM/Clang >= 11.1
  - `patches/llvm*` contains patches to version 11.1.0 to allow compilation to succeed.
    - `D96926+94834.patch` must be applied to LLVM during build. The rest are header-only libc++ patches that can be applied over already installed files without recompiling anything.
    - From version 12.0rc1 these patches are unnecessary, but git versions contain additional correctness and performance updates for C++20 coroutines.
- Boost >= 1.75.0
  - `patches/boost*` contain patches to version 1.75 to successfully build the library.
    - All of these patches are header-only and can be applied to installed files without recompiling Boost.
    - All patches except `polymorphic-allocator-noexcept.patch` are included in Boost >= 1.76.0.
- ntc-cmake from top of main branch.
- Boost.UT is required for testing, see https://github.com/boost-ext/ut.
