cmake_minimum_required(VERSION 3.22)

project(nanors
    VERSION     1.0.0
    DESCRIPTION "Nano Reed-Solomon erasure coding library with runtime SIMD dispatch"
    LANGUAGES   C
)

include(GNUInstallDirs)

# ── Options ────────────────────────────────────────────────────
set(NANORS_ISA "auto" CACHE STRING
    "SIMD ISA selection: auto (runtime dispatch), avx512, avx2, sse3, neon, tiny")
set_property(CACHE NANORS_ISA PROPERTY STRINGS auto avx512 avx2 sse3 neon tiny)

option(NANORS_BUILD_TESTS "Build test and benchmark executables"
    ${PROJECT_IS_TOP_LEVEL})
option(NANORS_INSTALL "Generate install rules"
    ${PROJECT_IS_TOP_LEVEL})

# ── Library target ─────────────────────────────────────────────
add_library(nanors rs_auto.c)
add_library(nanors::nanors ALIAS nanors)

# MSVC ARM64/ARM64EC: sse2neon.h requires C++ (uses lambdas in place
# of GCC statement expressions).  Compile the single translation unit
# as C++20 and let the register-keyword shim in rs.c handle the rest.
# /Zc:preprocessor is required by sse2neon.h for the conforming preprocessor.
if(MSVC AND (   CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64"
             OR CMAKE_GENERATOR_PLATFORM MATCHES "ARM64"))
    enable_language(CXX)
    set_source_files_properties(rs_auto.c PROPERTIES LANGUAGE CXX)
    target_compile_features(nanors PRIVATE cxx_std_20)
    target_compile_options(nanors PRIVATE /Zc:preprocessor)
endif()

target_compile_features(nanors PUBLIC c_std_11)

target_include_directories(nanors
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/deps/obl
)

set_target_properties(nanors PROPERTIES
    WINDOWS_EXPORT_ALL_SYMBOLS ON
    POSITION_INDEPENDENT_CODE  ON
    VERSION                    ${PROJECT_VERSION}
    SOVERSION                  ${PROJECT_VERSION_MAJOR}
)

# ── Compile-time ISA override ──────────────────────────────────
if(NOT NANORS_ISA STREQUAL "auto")
    string(TOUPPER "${NANORS_ISA}" _isa_upper)
    if(_isa_upper STREQUAL "AVX512")
        target_compile_definitions(nanors PRIVATE OBLAS_AVX512)
    elseif(_isa_upper STREQUAL "AVX2")
        target_compile_definitions(nanors PRIVATE OBLAS_AVX2)
    elseif(_isa_upper STREQUAL "SSE3")
        target_compile_definitions(nanors PRIVATE OBLAS_SSE3)
    elseif(_isa_upper STREQUAL "NEON")
        target_compile_definitions(nanors PRIVATE OBLAS_NEON)
    elseif(_isa_upper STREQUAL "TINY")
        target_compile_definitions(nanors PRIVATE OBLAS_TINY)
    else()
        message(FATAL_ERROR "Unknown NANORS_ISA value: ${NANORS_ISA}")
    endif()
    unset(_isa_upper)
endif()

# ── Tests / benchmarks ─────────────────────────────────────────
if(NANORS_BUILD_TESTS)
    enable_testing()

    add_executable(nanors_test  t/00util/test.c)
    add_executable(nanors_bench EXCLUDE_FROM_ALL t/00util/bench.c)

    target_link_libraries(nanors_test  PRIVATE nanors::nanors)
    target_link_libraries(nanors_bench PRIVATE nanors::nanors)

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_compile_definitions(nanors_test  PRIVATE _DEFAULT_SOURCE)
        target_compile_definitions(nanors_bench PRIVATE _DEFAULT_SOURCE)
    endif()

    add_test(NAME nanors_test COMMAND nanors_test)
endif()

# ── Install / export ──────────────────────────────────────────
if(NANORS_INSTALL)
    include(CMakePackageConfigHelpers)

    install(TARGETS nanors
        EXPORT nanorsTargets
        ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(FILES rs.h rs_auto.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(EXPORT nanorsTargets
        NAMESPACE  nanors::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/nanors
    )

    configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/nanorsConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/nanorsConfig.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/nanors
    )

    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/nanorsConfigVersion.cmake
        COMPATIBILITY SameMajorVersion
    )

    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/nanorsConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/nanorsConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/nanors
    )
endif()
