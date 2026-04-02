function(ro_collect_qt6_candidate_roots out_var)
    set(_roots)

    if(RO_QT_ROOT)
        list(APPEND _roots "${RO_QT_ROOT}")
    endif()

    if(DEFINED ENV{RO_QT_ROOT} AND NOT "$ENV{RO_QT_ROOT}" STREQUAL "")
        list(APPEND _roots "$ENV{RO_QT_ROOT}")
    endif()

    if(DEFINED ENV{QTDIR} AND NOT "$ENV{QTDIR}" STREQUAL "")
        list(APPEND _roots "$ENV{QTDIR}")
    endif()

    if(WIN32 AND EXISTS "C:/Qt")
        if(MSVC)
            if(CMAKE_SIZEOF_VOID_P EQUAL 8)
                file(GLOB _qt_configs LIST_DIRECTORIES false "C:/Qt/*/msvc*_64/lib/cmake/Qt6/Qt6Config.cmake")
            else()
                file(GLOB _qt_configs LIST_DIRECTORIES false "C:/Qt/*/msvc*/lib/cmake/Qt6/Qt6Config.cmake")
            endif()
        elseif(MINGW)
            if(CMAKE_SIZEOF_VOID_P EQUAL 8)
                file(GLOB _qt_configs LIST_DIRECTORIES false "C:/Qt/*/mingw*/lib/cmake/Qt6/Qt6Config.cmake")
            else()
                file(GLOB _qt_configs LIST_DIRECTORIES false "C:/Qt/*/mingw*/lib/cmake/Qt6/Qt6Config.cmake")
            endif()
        else()
            file(GLOB _qt_configs LIST_DIRECTORIES false "C:/Qt/*/*/lib/cmake/Qt6/Qt6Config.cmake")
        endif()

        list(SORT _qt_configs ORDER DESCENDING)
        foreach(_qt_config IN LISTS _qt_configs)
            get_filename_component(_qt_prefix "${_qt_config}/../../.." ABSOLUTE)
            list(APPEND _roots "${_qt_prefix}")
        endforeach()
    endif()

    list(REMOVE_DUPLICATES _roots)
    set(${out_var} "${_roots}" PARENT_SCOPE)
endfunction()

function(ro_configure_qt6)
    if(NOT RO_ENABLE_QT6_UI)
        return()
    endif()

    if(NOT WIN32)
        message(STATUS "Qt 6 will be resolved from the active non-Windows toolchain")
        return()
    endif()

    ro_collect_qt6_candidate_roots(_qt_candidate_roots)
    set(_selected_qt_root "")
    foreach(_candidate IN LISTS _qt_candidate_roots)
        if(EXISTS "${_candidate}/lib/cmake/Qt6/Qt6Config.cmake")
            set(_selected_qt_root "${_candidate}")
            break()
        endif()
    endforeach()

    if(NOT _selected_qt_root)
        set(_expected_compiler "the active toolchain")
        if(MSVC)
            set(_expected_compiler "an MSVC Qt 6 desktop kit")
        elseif(MINGW)
            set(_expected_compiler "a MinGW Qt 6 desktop kit")
        endif()

        set(_hint "")
        if(WIN32 AND EXISTS "C:/Qt")
            file(GLOB _all_qt_configs LIST_DIRECTORIES false "C:/Qt/*/*/lib/cmake/Qt6/Qt6Config.cmake")
            list(SORT _all_qt_configs ORDER DESCENDING)
            if(_all_qt_configs)
                string(JOIN "\n  " _hint ${_all_qt_configs})
                set(_hint "\nDetected Qt configs:\n  ${_hint}")
            endif()
        endif()

        message(FATAL_ERROR
            "RO_ENABLE_QT6_UI=ON requires ${_expected_compiler}.\n"
            "Set RO_QT_ROOT to a compatible Qt kit root such as C:/Qt/6.11.0/msvc2022_64.\n"
            "${_hint}")
    endif()

    set(RO_QT_ROOT "${_selected_qt_root}" CACHE PATH "Compiler-compatible Qt 6 kit root" FORCE)
    list(PREPEND CMAKE_PREFIX_PATH "${RO_QT_ROOT}")

    if(RO_QT_WINDEPLOYQT_EXECUTABLE)
        set(_windeployqt "${RO_QT_WINDEPLOYQT_EXECUTABLE}")
    else()
        set(_windeployqt "${RO_QT_ROOT}/bin/windeployqt.exe")
    endif()

    if(WIN32 AND NOT EXISTS "${_windeployqt}")
        message(FATAL_ERROR
            "Qt deployment tool not found at '${_windeployqt}'.\n"
            "Set RO_QT_WINDEPLOYQT_EXECUTABLE to the matching windeployqt.exe path.")
    endif()

    set(RO_QT_WINDEPLOYQT_EXECUTABLE "${_windeployqt}" CACHE FILEPATH "Path to the matching Qt deployment tool" FORCE)
    message(STATUS "Qt 6 kit root: ${RO_QT_ROOT}")
    if(WIN32)
        message(STATUS "Qt deployment tool: ${RO_QT_WINDEPLOYQT_EXECUTABLE}")
    endif()
endfunction()

function(ro_add_qt_runtime_deploy target output_dir)
    if(NOT RO_ENABLE_QT6_UI)
        return()
    endif()

    if(NOT WIN32)
        return()
    endif()

    if(NOT EXISTS "${RO_QT_WINDEPLOYQT_EXECUTABLE}")
        message(FATAL_ERROR "Cannot deploy Qt runtime because windeployqt was not found.")
    endif()

    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            "-DQT_DEPLOY_TARGET_FILE=$<TARGET_FILE:${target}>"
            "-DQT_DEPLOY_OUTPUT_DIR=${output_dir}"
            "-DQT_DEPLOY_WINDEPLOYQT=${RO_QT_WINDEPLOYQT_EXECUTABLE}"
            "-DQT_DEPLOY_QML_DIR=${CMAKE_SOURCE_DIR}/src/qtui/qml"
            "-DQT_DEPLOY_LICENSES_DIR=${CMAKE_SOURCE_DIR}/third_party/qt"
            -P "${CMAKE_SOURCE_DIR}/cmake/DeployQtRuntime.cmake"
        VERBATIM
        COMMENT "Deploying Qt runtime and notices to ${output_dir}"
    )
endfunction()
