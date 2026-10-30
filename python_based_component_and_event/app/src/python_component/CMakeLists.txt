# Build the Python sample components as Cython extension modules.

# Shared, SOC-independent, native Python 3.13 interpreter staged into the
# SDK image by SDK/.update_common.sh (built once from OpenSource/Python, see
# that CMakeLists.txt for details). Used only to run `-m cython` at build
# time (Cythonizing .py -> .c is a text transform, so it does not need to
# match the target's architecture/ABI); never shipped to the camera.
set(PY_COMPONENT_HOST_PYTHON_DIR "${SDK_PATH}/common/opensource/host_python3.13")
set(Python_EXECUTABLE "${PY_COMPONENT_HOST_PYTHON_DIR}/bin/python3.13")

set(Python_INCLUDE_DIR "${SDK_PATH}/common/opensource/${SOC}/python3.13-embed/include/python3.13")
set(Python_LIBRARY "${SDK_PATH}/common/opensource/${SOC}/python3.13-embed/lib/libpython3.13.so")
set(PY_COMPONENT_PYTHON_LIBRARY_DIR "${SDK_PATH}/common/opensource/${SOC}/python3.13-embed/lib")

set(Python_FIND_STRATEGY LOCATION)
set(Python_FIND_FRAMEWORK NEVER)
set(Python_FIND_REGISTRY NEVER)
set(Python_FIND_IMPLEMENTATIONS CPython)

find_package(Python 3.13 EXACT REQUIRED COMPONENTS Development)

# add_py_component(<ComponentName> <install_subdir>)
#
# Cythonizes ${CMAKE_CURRENT_SOURCE_DIR}/<ComponentName>.py -> .c, builds it
# as a Python extension module, and installs it to
# ${CMAKE_HOME_DIRECTORY}/libs/<install_subdir>. One call per Python
# component; add a new call here when adding another Python component.
function(add_py_component COMPONENT_NAME INSTALL_SUBDIR)
  set(PY_COMPONENT_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/${COMPONENT_NAME}.py)
  set(PY_COMPONENT_CYTHON_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/${COMPONENT_NAME}.c)

  add_custom_command(
    OUTPUT ${PY_COMPONENT_CYTHON_SOURCE}
    COMMAND ${Python_EXECUTABLE} -m cython -3 -X c_string_type=str -X c_string_encoding=utf8 -E CYTHON_COMPRESS_STRINGS=0 --embed-positions -o ${PY_COMPONENT_CYTHON_SOURCE} ${PY_COMPONENT_SOURCE}
    DEPENDS ${PY_COMPONENT_SOURCE}
    COMMENT "Cythonizing ${COMPONENT_NAME}.py"
    VERBATIM
  )

  add_library(${COMPONENT_NAME} MODULE ${PY_COMPONENT_CYTHON_SOURCE})

  target_compile_definitions(${COMPONENT_NAME} PRIVATE
    CYTHON_COMPRESS_STRINGS=0
  )

  target_include_directories(${COMPONENT_NAME} PRIVATE
    ${Python_INCLUDE_DIRS}
  )

  target_link_directories(${COMPONENT_NAME} PRIVATE
    ${PY_COMPONENT_PYTHON_LIBRARY_DIR}
  )

  target_link_libraries(${COMPONENT_NAME} PRIVATE
    Python::Python
  )

  set_target_properties(${COMPONENT_NAME} PROPERTIES
    PREFIX ""
    OUTPUT_NAME "${COMPONENT_NAME}"
  )

  install(TARGETS ${COMPONENT_NAME} DESTINATION ${CMAKE_HOME_DIRECTORY}/libs/${INSTALL_SUBDIR})
endfunction()

add_py_component(HomeComponent home_component)
add_py_component(PyWriteEventLogComponent py_write_event_log_component)
add_py_component(PyAnalyticsInputEventComponent py_analytics_input_event_component)
add_py_component(SettingChangedInputEventComponent setting_changed_input_event_component)
add_py_component(DisplayImageComponent display_image_component)
add_py_component(EventTriggerComponent event_trigger_component)
add_py_component(MetadataSampleComponent metadata_sample_component)
add_py_component(SendMetadataComponent send_metadata_component)
add_py_component(DynamicEventComponent dynamic_event_component)
add_py_component(PyWebServiceComponent py_webservice_component)
add_py_component(SnapshotJpegComponent snapshot_jpeg_component)
add_py_component(SdcardOutputEventComponent sdcard_output_event_component)
add_py_component(SetOsdMessageComponent set_osd_message_component)
