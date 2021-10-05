set(TARGET FirmwareUpgrade)

add_executable(${TARGET} "")
set_target_properties(${TARGET} PROPERTIES FOLDER "Applications(custom)")
mv_enable_and_enforce_cpp_standard_version(${TARGET} 11)

target_sources(${TARGET} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../Common/FirmwareUpdate_mvHYPERION/Epcs.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../Common/FirmwareUpdate_mvHYPERION/Epcs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../Common/FirmwareUpdate_mvHYPERION/Timeout.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../Common/FirmwareUpdate_mvHYPERION/Timeout.cpp
    ${TARGET}.cpp
)

target_link_libraries(${TARGET} PRIVATE mvIMPACT_Acquire::mvDeviceManager)