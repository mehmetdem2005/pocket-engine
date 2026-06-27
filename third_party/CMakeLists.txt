# third_party — ImGui ve yardımcı kütüphaneler kaynakdan derlenir
# (Termux'ta pkg ile ImGui paketi olmadığı için vendored kaynak kullanıyoruz)

# ---- ImGui ----
# Kaynaklar setup-termux.sh tarafından third_party/imgui altına klonlanır
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/imgui)

if(EXISTS ${IMGUI_DIR}/imgui.cpp)
    set(IMGUI_SOURCES
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/imgui_demo.cpp
        # Backends
        ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
        ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
    )

    add_library(imgui_gles3 STATIC ${IMGUI_SOURCES})

    target_include_directories(imgui_gles3 PUBLIC
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
    )

    target_compile_definitions(imgui_gles3 PUBLIC
        IMGUI_DISABLE_OBSOLETE_FUNCTIONS
        IMGUI_IMPL_OPENGL_ES3
    )

    if(TARGET PkgConfig::SDL2)
        target_link_libraries(imgui_gles3 PUBLIC PkgConfig::SDL2)
    endif()
    if(TARGET PkgConfig::OPENGLES)
        target_link_libraries(imgui_gles3 PUBLIC PkgConfig::OPENGLES)
    else()
        find_library(GLESV2_LIB NAMES GLESv2)
        if(GLESV2_LIB)
            target_link_libraries(imgui_gles3 PUBLIC ${GLESV2_LIB})
        endif()
    endif()
else()
    message(WARNING "ImGui bulunamadı! setup-termux.sh çalıştırın: third_party/imgui klonlanacak.")
endif()
