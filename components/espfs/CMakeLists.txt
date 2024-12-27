cmake_minimum_required(VERSION 3.5.2)

idf_build_get_property(target IDF_TARGET)


# Get a Python executable
# ---------------------------------------------------------
if (CMAKE_HOST_WIN32)
  find_package(Python3)
endif()
# ---------------------------------------------------------

# ---------------------------------------------------------
set(INCLUDES "include")
set(PRIV_INCLUDES "third-party/heatshrink")

list(APPEND SOURCES
  "espfs/espfs.c"
  "espfs/espfs_vfs.c"
  "third-party/heatshrink/heatshrink_decoder.c")
# ---------------------------------------------------------

# https://stackoverflow.com/questions/1027247/is-it-better-to-specify-source-files-with-glob-or-each-file-individually-in-cmak
# (Keeping these as individually listed)
# ---------------------------------------------------------
list(APPEND webpages
  "${PROJECT_DIR}/html/404.html"
  "${PROJECT_DIR}/html/console.html"
  "${PROJECT_DIR}/html/favicon.ico"
  "${PROJECT_DIR}/html/flash.html"
  "${PROJECT_DIR}/html/graphs.html"
  "${PROJECT_DIR}/html/index.html"
  "${PROJECT_DIR}/html/mqttconfig.html"
  "${PROJECT_DIR}/html/patterns.html"
  "${PROJECT_DIR}/html/schedule.html"
  "${PROJECT_DIR}/html/tasks.html"
  "${PROJECT_DIR}/html/themes.html"
  "${PROJECT_DIR}/html/wifi.html"
  "${PROJECT_DIR}/html/wsa.html"
  "${PROJECT_DIR}/html/css/common.css"
  "${PROJECT_DIR}/html/css/schedule.css"
  "${PROJECT_DIR}/html/css/themes.css"
  "${PROJECT_DIR}/html/css/timesel.css"
  "${PROJECT_DIR}/html/css/wifi.css"
  "${PROJECT_DIR}/html/js/ansi_up.js"
  "${PROJECT_DIR}/html/js/common.js"
  "${PROJECT_DIR}/html/js/console.js"
  "${PROJECT_DIR}/html/js/dragable.js"
  "${PROJECT_DIR}/html/js/flash.js"
  "${PROJECT_DIR}/html/js/graphs.js"
  "${PROJECT_DIR}/html/js/index.js"
  "${PROJECT_DIR}/html/js/monthsel.js"
  "${PROJECT_DIR}/html/js/schedule.js"
  "${PROJECT_DIR}/html/js/smoothie.js"
  "${PROJECT_DIR}/html/js/tasks.js"
  "${PROJECT_DIR}/html/js/themes.js"
  "${PROJECT_DIR}/html/js/timesel.js"
  "${PROJECT_DIR}/html/js/wifi.js"
  "${PROJECT_DIR}/html/img/arrow_down.png"
  "${PROJECT_DIR}/html/img/arrow_up.png"
  "${PROJECT_DIR}/html/img/favicon.png"
  )
# ---------------------------------------------------------

# Relay control (a hack for November 5th)
# ---------------------------------------------------------
if(AEOLIAN_DEV_ROCKET)
  list(APPEND webpages "${PROJECT_DIR}/html/wsa.html")
endif()
# ---------------------------------------------------------

if (CONFIG_COMPRESS_ESPFS)
  set(ESPFS_CONFIG "espfs_defaults.yaml")
else()
  message(AUTHOR_WARNING "** Using debug espfs configuration **")
  set(ESPFS_CONFIG "espfs_debug.yaml")
endif()

# A list of things we require to work
# ---------------------------------------------------------
set(REQ_COMPONENTS spi_flash spiffs vfs)
# ---------------------------------------------------------

# Register components
# ---------------------------------------------------------
idf_component_register(SRCS               ${SOURCES}
                       INCLUDE_DIRS       ${INCLUDES}
                       PRIV_INCLUDE_DIRS  ${PRIV_INCLUDES}
                       PRIV_REQUIRES      ${REQ_COMPONENTS})
# ---------------------------------------------------------

# Generate: "build/esp-idf/espfs/webpages.espfs"
# ---------------------------------------------------------
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/webpages.espfs"
                    COMMAND ${python} ${COMPONENT_DIR}/extra/mkespfsimage.py ${ESPFS_CONFIG} ${PROJECT_DIR}/html ${CMAKE_CURRENT_BINARY_DIR}/webpages.espfs
                    DEPENDS "${COMPONENT_DIR}/extra/mkespfsimage.py" ${webpages}
                    VERBATIM)
# Get the above custom command to run...
# ---------------------------------------------------------
add_custom_target(webpages-espfs DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/webpages.espfs")
add_dependencies(${COMPONENT_LIB} webpages-espfs)
# ---------------------------------------------------------

# Generate: "build/esp-idf/espfs/webpages-espfs.o"
# ---------------------------------------------------------
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/webpages-espfs.o" ALL
                    COMMAND xtensa-esp32-elf-objcopy -I binary -O elf32-xtensa-le -B xtensa --rename-section .data=.rodata webpages.espfs webpages-espfs.o.tmp
                    COMMAND xtensa-esp32-elf-cc -nostdlib -Wl,-r webpages-espfs.o.tmp -o webpages-espfs.o -Wl,-T ${COMPONENT_DIR}/webpages/webpages.espfs.ld
                    DEPENDS webpages-espfs
                    VERBATIM)
# Get the above custom command to run...
# ---------------------------------------------------------
add_custom_target(libwebpages DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/webpages-espfs.o")
add_dependencies(${COMPONENT_LIB} libwebpages)
# ---------------------------------------------------------

# And, finally...
# ---------------------------------------------------------
target_link_libraries(${COMPONENT_LIB} "${CMAKE_CURRENT_BINARY_DIR}/webpages-espfs.o")
