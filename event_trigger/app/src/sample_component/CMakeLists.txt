set(TARGET_LIB sample_component)

add_library(${TARGET_LIB} MODULE sample_component.cc)

include_directories(${WISENET_PRODUCT_RELEASE_PATH}/includes/rule_manager)
include_directories(${WISENET_PRODUCT_RELEASE_PATH}/includes/dynamic_rule)

target_include_directories(${TARGET_LIB} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/includes)
target_link_libraries(${TARGET_LIB} event_rule)
target_link_libraries(${TARGET_LIB} component nstl)

install (TARGETS ${TARGET_LIB} DESTINATION ${CMAKE_HOME_DIRECTORY}/libs/${TARGET_LIB})
install (DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/manifests/ DESTINATION ${CMAKE_HOME_DIRECTORY}/libs/${TARGET_LIB} FILES_MATCHING PATTERN "*manifest.json")
install (DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/manifests/ DESTINATION ${CMAKE_HOME_DIRECTORY}/res/models FILES_MATCHING PATTERN "*instance*.json" PATTERN "*attribute*.json")
