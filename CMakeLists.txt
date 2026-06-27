cmake_minimum_required(VERSION 3.16)
project(PocketEngine VERSION 0.1.0 LANGUAGES C CXX)

# ---- C++ standardı ----
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- Build tipi ----
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ---- Optimizasyon bayrakları (Termux/ARM için) ----
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG -ffast-math -fdata-sections -ffunction-sections")
    set(CMAKE_C_FLAGS_RELEASE   "-O2 -DNDEBUG -ffast-math -fdata-sections -ffunction-sections")
    if(NOT APPLE AND NOT WIN32)
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -flto")
        set(CMAKE_EXE_LINKER_FLAGS_RELEASE "-flto -Wl,--gc-sections")
    endif()
endif()

# ---- ARM/NEON optimizasyonu (telefon) ----
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|arm")
    add_compile_options(-mfpu=neon -funsafe-math-optimizations)
endif()

# ---- Pozisyon-bağımsız kod ----
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ---- Çıktı dizini ----
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ---- Modüller ----
add_subdirectory(third_party)
add_subdirectory(engine)
add_subdirectory(editor)

# ---- Install hedefleri ----
install(DIRECTORY assets/ DESTINATION share/pocketengine/assets)

# ---- Bilgi mesajı ----
message(STATUS "========================================")
message(STATUS "  PocketEngine ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Processor: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "========================================")
