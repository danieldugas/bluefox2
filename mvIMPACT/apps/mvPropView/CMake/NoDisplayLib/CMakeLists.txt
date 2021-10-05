# On Windows 'Default' will build wxPropView without extensions and this will use the mvDisplay-lib, which
# on other platforms is not available, which is why here this target is out 'Default'
if(WIN32)
    set(TARGET wxPropView.no.mvDisplay)
else()
    set(TARGET wxPropView)
endif()

set(PLATFORM_SPECIFIC_SOURCES
    ../../ImageCanvas.cpp
)

add_executable(${TARGET}
    $<TARGET_OBJECTS:wxPropView.Objects>
    ../Common.postTarget.cmake
    ${PLATFORM_SPECIFIC_SOURCES}
)
set_target_properties(${TARGET} PROPERTIES PROJECT_LABEL "wxPropView(no mvDisplay)")

target_compile_definitions(${TARGET} PRIVATE USE_RAW_BITMAP_SUPPORT)
include(../../../../CMake/openMP.compile.cmake)
include(../Common.postTarget.cmake)