# PocketEngine Editor — yürütülebilir
file(GLOB_RECURSE EDITOR_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_executable(pocket_editor ${EDITOR_SOURCES})

target_link_libraries(pocket_editor
    PRIVATE
        pocket_engine
        imgui_gles3
)

target_include_directories(pocket_editor
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/third_party
        ${CMAKE_SOURCE_DIR}/third_party/imgui
        ${CMAKE_SOURCE_DIR}/third_party/imgui/backends
)

install(TARGETS pocket_editor RUNTIME DESTINATION bin)
