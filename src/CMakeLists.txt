# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

# Kazdy modul to osobny .c -> osobny .obj (main.c.obj, mic.c.obj, ...).
FILE(GLOB_RECURSE app_sources ${CMAKE_CURRENT_SOURCE_DIR}/*.c)

idf_component_register(SRCS ${app_sources}
                       INCLUDE_DIRS "."
                       REQUIRES esp_driver_i2s esp_adc mirf mbedtls)

# Helpery walkie-talkie (mic.h/speaker.h/blinker.h) bywaja chwilowo nieuzywane -
# nie chcemy, zeby "unused" wywalalo build przez -Werror w komponencie aplikacji.
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-maybe-uninitialized -Wno-unused-function -Wno-unused-variable)
