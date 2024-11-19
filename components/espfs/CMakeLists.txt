cmake_minimum_required(VERSION 3.5.2)

idf_build_get_property(target IDF_TARGET)

# Get a Python executable
# ---------------------------------------------------------
if (CMAKE_HOST_WIN32)
  find_package(Python3)
endif()

set(includes "include")
set(priv_includes "third-party/heatshrink")

list(APPEND sources
  "espfs/espfs.c"
  "espfs/espfs_vfs.c"
  "espfs/heatshrink_decoder.c"
  "third-party/heatshrink/heatshrink_decoder.c")

# FIXME: Make this dynamic
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
  "${PROJECT_DIR}/html/css/schedule.css"
  "${PROJECT_DIR}/html/css/signalbars.css"
  "${PROJECT_DIR}/html/css/siimple.min.css"
  "${PROJECT_DIR}/html/css/style.css"
  "${PROJECT_DIR}/html/css/themes.css"
  "${PROJECT_DIR}/html/js/140medley.js"
  "${PROJECT_DIR}/html/js/ansi_up.js"
  "${PROJECT_DIR}/html/js/common.js"
  "${PROJECT_DIR}/html/js/console.js"
  "${PROJECT_DIR}/html/js/flash.js"
  "${PROJECT_DIR}/html/js/index.js"
  "${PROJECT_DIR}/html/js/schedule.js"
  "${PROJECT_DIR}/html/js/smoothie.js"
  "${PROJECT_DIR}/html/js/tasks.js"
  "${PROJECT_DIR}/html/js/themes.js"
  "${PROJECT_DIR}/html/js/wifi.js"
  "${PROJECT_DIR}/html/img/ajax-spinner.gif"
  "${PROJECT_DIR}/html/img/arrow_down.png"
  "${PROJECT_DIR}/html/img/arrow_up.png"
  "${PROJECT_DIR}/html/img/bg.jpg"
  "${PROJECT_DIR}/html/img/favicon.png"
  "${PROJECT_DIR}/html/img/wifi_icons.png"
  )

# For ref
# ---------------------------------------------------------
if(AXRGB_DEV_ROCKET)
  list(APPEND weboages "${PROJECT_DIR}/html/wsa.html")
endif()

# Register components
# ---------------------------------------------------------
idf_component_register(SRCS ${sources}
                    INCLUDE_DIRS ${includes}
                    PRIV_INCLUDE_DIRS ${priv_includes}
                    PRIV_REQUIRES spi_flash spiffs vfs)

# Generate: "build/esp-idf/libespfs/webpages.espfs"
# ---------------------------------------------------------
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/webpages.espfs"
                    COMMAND ${python} ${COMPONENT_DIR}/extra/mkespfsimage.py ${PROJECT_DIR}/html ${CMAKE_CURRENT_BINARY_DIR}/webpages.espfs
                    DEPENDS "${COMPONENT_DIR}/extra/mkespfsimage.py" ${webpages}
                    VERBATIM)
# Get the above custom command to run...
# ---------------------------------------------------------
add_custom_target(espfs_image DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/webpages.espfs")
add_dependencies(${COMPONENT_LIB} espfs_image)

# Generate: "build/esp-idf/libespfs/libwebpages-espfs.a"
# ---------------------------------------------------------
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/libwebpages-espfs.a" ALL
                    COMMAND xtensa-esp32-elf-objcopy -I binary -O elf32-xtensa-le -B xtensa --rename-section .data=.rodata webpages.espfs webpages-espfs.o.tmp
                    COMMAND xtensa-esp32-elf-cc -nostdlib -Wl,-r webpages-espfs.o.tmp -o webpages-espfs.o -Wl,-T ${COMPONENT_DIR}/webpages/webpages.espfs.ld
                    COMMAND xtensa-esp32-elf-ar cr ${CMAKE_CURRENT_BINARY_DIR}/libwebpages-espfs.a webpages-espfs.o
                    DEPENDS espfs_image
                    VERBATIM)
# Get the above custom command to run...
# ---------------------------------------------------------
add_custom_target(espfs_lib DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/libwebpages-espfs.a")
add_dependencies(${COMPONENT_LIB} espfs_lib)

# Finally!:
# ---------------------------------------------------------
target_link_libraries(${COMPONENT_LIB} "-L${CMAKE_CURRENT_BINARY_DIR} -l:libwebpages-espfs.a -l:libespfs.a")
