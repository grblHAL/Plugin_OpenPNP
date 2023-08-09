add_library(openpnp INTERFACE)

target_sources(openpnp INTERFACE
 ${CMAKE_CURRENT_LIST_DIR}/openpnp.c
)

target_include_directories(openpnp INTERFACE ${CMAKE_CURRENT_LIST_DIR})
