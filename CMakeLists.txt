cmake_minimum_required(VERSION 3.21)

project(Tonal VERSION 0.4.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt 6.8 LTS ou mais novo (usamos Shape.CurveRenderer e loadFromModule)
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Network Qml Quick QuickControls2 LinguistTools)

qt_standard_project_setup(REQUIRES 6.8)

set(TONAL_SOURCES
    src/main.cpp
    src/App.h               src/App.cpp
    src/AuthManager.h       src/AuthManager.cpp
    src/TokenStore.h        src/TokenStore.cpp
    src/SpotifyApi.h        src/SpotifyApi.cpp
    src/LibrespotProcess.h  src/LibrespotProcess.cpp
    src/Player.h            src/Player.cpp
    src/MediaControls.h     src/MediaControls.cpp
    src/Lyrics.h            src/Lyrics.cpp
    src/DeepL.h             src/DeepL.cpp
    src/GoogleTranslate.h   src/GoogleTranslate.cpp
    src/SetlistFm.h         src/SetlistFm.cpp
    src/NetworkFactory.h    src/NetworkFactory.cpp
)

qt_add_executable(Tonal ${TONAL_SOURCES})

# O registro gerado pelo qmltyperegistrar faz #include <App.h>: src/ precisa estar no include path.
target_include_directories(Tonal PRIVATE src)

# Ícone e versão do executável. Fica fora do TONAL_SOURCES: o /W4 de lá iria para o rc.exe.
if(WIN32)
    target_sources(Tonal PRIVATE resources/tonal.rc)
    set_source_files_properties(resources/tonal.rc PROPERTIES COMPILE_DEFINITIONS
        "TONAL_VERSION_MAJOR=${PROJECT_VERSION_MAJOR};TONAL_VERSION_MINOR=${PROJECT_VERSION_MINOR};TONAL_VERSION_PATCH=${PROJECT_VERSION_PATCH}")
    # Ativação das classes do Windows Runtime (controle de mídia do Windows, em MediaControls.cpp).
    target_link_libraries(Tonal PRIVATE runtimeobject)
endif()

set_source_files_properties(qml/Theme.qml qml/Icons.qml qml/Nav.qml qml/Playlists.qml qml/Setlists.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(Tonal
    URI Tonal
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/Icons.qml
        qml/Nav.qml
        qml/Playlists.qml
        qml/Setlists.qml
        qml/components/Icon.qml
        qml/components/IconButton.qml
        qml/components/PlayButton.qml
        qml/components/Cover.qml
        qml/components/MediaCard.qml
        qml/components/ShortcutTile.qml
        qml/components/TrackRow.qml
        qml/components/SeekBar.qml
        qml/components/Chip.qml
        qml/components/SectionHeader.qml
        qml/components/Logo.qml
        qml/components/LikedArt.qml
        qml/components/ThinScrollBar.qml
        qml/components/HorizontalShelf.qml
        qml/components/PillButton.qml
        qml/components/ModalDialog.qml
        qml/components/DarkMenu.qml
        qml/components/DarkMenuItem.qml
        qml/panels/TopBar.qml
        qml/panels/LibraryPanel.qml
        qml/panels/PlayerBar.qml
        qml/panels/RightPanel.qml
        qml/panels/DevicePopup.qml
        qml/pages/LoginPage.qml
        qml/pages/HomePage.qml
        qml/pages/SearchPage.qml
        qml/pages/CollectionPage.qml
        qml/pages/ArtistPage.qml
        qml/pages/LyricsPage.qml
        qml/pages/SetlistsPage.qml
)

target_link_libraries(Tonal PRIVATE
    Qt6::Core Qt6::Gui Qt6::Network Qt6::Qml Qt6::Quick Qt6::QuickControls2
)

target_compile_definitions(Tonal PRIVATE PROJECT_VERSION_STRING="${PROJECT_VERSION}")

set_target_properties(Tonal PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

if(MSVC)
    target_compile_options(Tonal PRIVATE /permissive- /utf-8)
    # /W4 só no código escrito à mão. O C++ que o qmlcachegen gera a partir do QML instancia
    # templates do Qt (qjsengine.h, qvariant.h) que disparam C4702 no MSVC, fora do nosso controle.
    set_source_files_properties(${TONAL_SOURCES} PROPERTIES COMPILE_OPTIONS /W4)
endif()

# No Windows, copia as DLLs do Qt para a pasta do executável depois de compilar,
# para dar F5 no Visual Studio sem configurar PATH.
option(TONAL_AUTO_DEPLOY "Rodar windeployqt depois do build" ON)
if(WIN32 AND TONAL_AUTO_DEPLOY)
    if(TARGET Qt6::windeployqt)
        set(TONAL_WINDEPLOYQT "$<TARGET_FILE:Qt6::windeployqt>")
    else()
        find_program(TONAL_WINDEPLOYQT_EXE windeployqt
            HINTS "${QT6_INSTALL_PREFIX}/bin" "$ENV{QTDIR}/bin")
        set(TONAL_WINDEPLOYQT "${TONAL_WINDEPLOYQT_EXE}")
    endif()
    if(TONAL_WINDEPLOYQT)
        add_custom_command(TARGET Tonal POST_BUILD
            COMMAND "${TONAL_WINDEPLOYQT}"
                --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/qml"
                --no-translations
                --no-system-d3d-compiler
                --no-opengl-sw
                "$<TARGET_FILE:Tonal>"
            COMMENT "windeployqt: copiando DLLs do Qt"
            VERBATIM)
    else()
        message(WARNING "windeployqt não encontrado: rode-o manualmente para copiar as DLLs do Qt.")
    endif()
endif()

# Interface em inglês: os textos do código (qsTr/tr) são em português. "cmake --build ... --target
# update_translations" extrai os textos novos para i18n/tonal_en.ts (traduza os que ficarem "unfinished"); o
# build compila o .ts e embute em :/i18n/tonal_en.qm.
qt_add_translations(Tonal
    TS_FILES i18n/tonal_en.ts
    RESOURCE_PREFIX /i18n
    NO_GENERATE_PLURALS_TS_FILE
    LUPDATE_OPTIONS -no-obsolete -locations none -source-language pt_BR -target-language en
)

include(GNUInstallDirs)
install(TARGETS Tonal
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
