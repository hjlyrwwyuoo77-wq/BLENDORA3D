cmake_minimum_required(VERSION 3.22.1)

project(blendora3d)

set(NATIVE_APP_GLUE_DIR
    "${ANDROID_NDK}/sources/android/native_app_glue"
)

add_library(native_app_glue STATIC
    "${NATIVE_APP_GLUE_DIR}/android_native_app_glue.c"
)

target_include_directories(native_app_glue PUBLIC
    "${NATIVE_APP_GLUE_DIR}"
)

target_link_libraries(native_app_glue
    android
    log
)

add_library(main SHARED
    main.cpp
    renderer.cpp
)

target_include_directories(main PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(main
    native_app_glue
    android
    log
    EGL
    GLESv3
)

target_link_options(main PRIVATE
    "-u"
    "ANativeActivity_onCreate"
)
