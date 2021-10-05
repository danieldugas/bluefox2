set(TARGET ContinuousCaptureFLTK)

set(SOURCES
    ${TARGET}.cpp
)

add_executable(${TARGET}
    ${SOURCES}
)
set_target_properties(${TARGET} PROPERTIES FOLDER "Applications(custom)")

if(FLTK_FOUND)
    target_include_directories(${TARGET} PRIVATE ${FLTK_INCLUDE_DIR})
endif()

target_link_libraries(${TARGET} PRIVATE mvIMPACT_Acquire::mvDeviceManager)
if(UNIX)
    target_link_libraries(${TARGET} PRIVATE ${FLTK_LIBRARIES})
endif()