cmake_minimum_required (VERSION 2.8)

project (oscfile)

set (OSCFILE_VERSION "0.1.0")

# pkg-config
find_package (PkgConfig) # ${PKG_CONFIG_FOUND}

# liblo
pkg_search_module (LIBLO REQUIRED liblo)
include_directories (${LIBLO_INCLUDE_DIRS})
set (LIBS ${LIBLO_LDFLAGS})

# oscrec
add_executable (oscrec oscrec.c)
target_link_libraries (oscrec ${LIBS})
install (TARGETS oscrec DESTINATION bin)

# oscplay
add_executable (oscplay oscplay.c)
target_link_libraries (oscplay ${LIBS})
install (TARGETS oscplay DESTINATION bin)

# oscfile
install (PROGRAMS oscfile DESTINATION bin)
