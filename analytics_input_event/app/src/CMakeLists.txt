add_subdirectory(sample_component)

set(APP_DISPATCHER_SOURCE_DIR ${APP_SUPPORT_PACKAGE}/app_dispatcher)
set(LCM_SOURCE_DIR ${APP_SUPPORT_PACKAGE}/life_cycle_manager)
set(APP_BIN ${CMAKE_HOME_DIRECTORY}/bin)
set(APP_LIBS ${CMAKE_HOME_DIRECTORY}/libs)
set(APP_RES ${CMAKE_HOME_DIRECTORY}/res)
set(APP_STORAGE ${CMAKE_HOME_DIRECTORY}/storage)

# Get app_dispatcher binary and manifests from AppSupportPackage
file(MAKE_DIRECTORY ${APP_LIBS})
file(COPY ${APP_DISPATCHER_SOURCE_DIR} DESTINATION ${APP_LIBS})

# Create res, res/models
file(MAKE_DIRECTORY ${APP_RES})
file(MAKE_DIRECTORY ${APP_RES}/models)

# Create storage, storage/settings
file(MAKE_DIRECTORY ${APP_STORAGE})
file(MAKE_DIRECTORY ${APP_STORAGE}/settings)

# Get application execution binary and LCM manifests from AppSupportPackage
# - this operation is executed in cmake command running time, not build time
file(MAKE_DIRECTORY ${APP_BIN})
file(COPY ${LCM_SOURCE_DIR}/ DESTINATION ${APP_BIN}/)
file(RENAME ${APP_BIN}/life_cycle_manager ${APP_BIN}/opensdk_${PROJECT_NAME})
file(READ ${APP_BIN}/run_life_cycle_manager.sh RUN_LCM_SCRIPT)
set(ORIGINAL_RUN_LCM_SCRIPT "${RUN_LCM_SCRIPT}")
string(REPLACE "SCRIPT_DIR=$(CDPATH= cd -- \"$(dirname \"$0\")\" && pwd)\n"
               "SCRIPT_PATH=$(readlink -f \"$0\")\nSCRIPT_DIR=$(CDPATH= cd -- \"$(dirname \"$SCRIPT_PATH\")\" && pwd)\n"
               RUN_LCM_SCRIPT "${RUN_LCM_SCRIPT}")
if (RUN_LCM_SCRIPT STREQUAL ORIGINAL_RUN_LCM_SCRIPT)
  message(FATAL_ERROR "Failed to make launcher symlink-aware: expected SCRIPT_DIR pattern not found")
endif ()
string(REPLACE "./life_cycle_manager &" "./opensdk_${PROJECT_NAME} &" RUN_LCM_SCRIPT "${RUN_LCM_SCRIPT}")
file(RENAME ${APP_BIN}/run_life_cycle_manager.sh ${APP_BIN}/${PROJECT_NAME})
file(WRITE ${APP_BIN}/${PROJECT_NAME} "${RUN_LCM_SCRIPT}")
execute_process(COMMAND chmod 755 ${APP_BIN}/${PROJECT_NAME})

# (Optional) Get 3rd Party library from Hanwha Vision Open Platform Docker Image
#set(LIB_3RD_PARTY_DIR ${WISENET_PRODUCT_RELEASE_PATH}/3rd_party/lib)
#file(COPY ${LIB_3RD_PARTY_DIR}/libssl.so DESTINATION ${APP_LIBS})
#file(COPY ${LIB_3RD_PARTY_DIR}/libcrypto.so DESTINATION ${APP_LIBS})

# install PLifeCycleManagermanifest.json
install (FILES ${CMAKE_CURRENT_SOURCE_DIR}/PLifeCycleManagermanifest.json DESTINATION ${APP_BIN})
