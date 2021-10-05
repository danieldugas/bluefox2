set(TARGET wxPropView.no.mvDisplay.no.raw.bitmap)

set(PLATFORM_SPECIFIC_SOURCES
    ../../ImageCanvas.cpp
)

add_executable(${TARGET}
    $<TARGET_OBJECTS:wxPropView.Objects>
    ../Common.postTarget.cmake
    ${PLATFORM_SPECIFIC_SOURCES}
)
set_target_properties(${TARGET} PROPERTIES PROJECT_LABEL "wxPropView(no mvDisplay, no raw bitmap support)")

include(../../../../CMake/openMP.compile.cmake)
include(../Common.postTarget.cmake)