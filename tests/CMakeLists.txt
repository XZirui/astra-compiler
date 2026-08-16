# `catch_discover_tests` is defined in Catch2's extras/Catch.cmake. The
# PARENT_SCOPE propagation of Catch2's CMAKE_MODULE_PATH cannot be relied
# upon under FetchContent, so add the module path explicitly.
list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
include(Catch)

add_subdirectory(support)
add_subdirectory(ast)
add_subdirectory(frontend)
add_subdirectory(basic)
