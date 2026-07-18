# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

idf_component_register(SRCS ${app_sources} REQUIRES esp_driver_i2s esp_adc mirf)

# Helpery walkie-talkie (mic.h/speaker.h/blinker.h) bywaja chwilowo nieuzywane -
# nie chcemy, zeby "unused" wywalalo build przez -Werror w komponencie aplikacji.
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-function -Wno-unused-variable)
