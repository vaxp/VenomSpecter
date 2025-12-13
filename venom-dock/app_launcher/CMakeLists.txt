add_library(app_launcher SHARED
  app_launcher.c
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK REQUIRED gtk+-3.0)

target_include_directories(app_launcher PRIVATE ${GTK_INCLUDE_DIRS})
target_link_libraries(app_launcher PRIVATE ${GTK_LIBRARIES})

set_target_properties(app_launcher PROPERTIES
  OUTPUT_NAME "app_launcher"
)
