# PocketEngine — engine statik kütüphane
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

# Header include
set(ENGINE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

# ---- pkg-config ile bağımlılıkları bul ----
find_package(PkgConfig QUIET)

# SDL2
if(PkgConfig_FOUND)
    pkg_check_modules(SDL2 IMPORTED_TARGET sdl2)
    pkg_check_modules(SDL2_IMAGE IMPORTED_TARGET SDL2_image)
    pkg_check_modules(SDL2_TTF IMPORTED_TARGET SDL2_ttf)
    pkg_check_modules(OPENGLES IMPORTED_TARGET glesv2)
    pkg_check_modules(EGL IMPORTED_TARGET egl)
    pkg_check_modules(BOX2D IMPORTED_TARGET box2d)
    pkg_check_modules(LUA IMPORTED_TARGET lua5.4)
    pkg_check_modules(SQLITE3 IMPORTED_TARGET sqlite3)
    pkg_check_modules(OPENAL IMPORTED_TARGET openal)
endif()

add_library(pocket_engine STATIC ${ENGINE_SOURCES})

target_include_directories(pocket_engine
    PUBLIC
        ${ENGINE_INCLUDE_DIR}
        ${CMAKE_SOURCE_DIR}/third_party
)

# ---- Bağlantılar ----
if(TARGET PkgConfig::SDL2)
    target_link_libraries(pocket_engine PUBLIC PkgConfig::SDL2 PkgConfig::SDL2_IMAGE PkgConfig::SDL2_TTF)
endif()
if(TARGET PkgConfig::OPENGLES)
    target_link_libraries(pocket_engine PUBLIC PkgConfig::OPENGLES PkgConfig::EGL)
else()
    find_library(GLESV2_LIB NAMES GLESv2 REQUIRED)
    target_link_libraries(pocket_engine PUBLIC ${GLESV2_LIB})
endif()
if(TARGET PkgConfig::BOX2D)
    target_link_libraries(pocket_engine PUBLIC PkgConfig::BOX2D)
endif()
if(TARGET PkgConfig::LUA)
    target_link_libraries(pocket_engine PUBLIC PkgConfig::LUA)
endif()
if(TARGET PkgConfig::SQLITE3)
    target_link_libraries(pocket_engine PUBLIC PkgConfig::SQLITE3)
endif()
if(TARGET PkgConfig::OPENAL)
    target_link_libraries(pocket_engine PUBLIC PkgConfig::OPENAL)
endif()

# pthread (threading)
find_library(PTHREAD_LIB pthread)
if(PTHREAD_LIB)
    target_link_libraries(pocket_engine PUBLIC ${PTHREAD_LIB})
endif()

target_link_libraries(pocket_engine PUBLIC dl)
