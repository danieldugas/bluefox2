set(TARGET mvDeviceConfigure)

if(WIN32)
    set(PLATFORM_SPECIFIC_HEADERS
        ${CMAKE_CURRENT_SOURCE_DIR}/win32/DirectShowSupport.h
        ../Common/win32/ProxyResolver.h
        resource.h
    )
    set(RESOURCE_FILES
        ${TARGET}.ico
        ${TARGET}.rc
        ${TARGET}.icon.xpm
    )
    source_group("Resource Files" FILES ${RESOURCE_FILES})
    set(PLATFORM_SPECIFIC_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/win32/DirectShowSupport.cpp
        ../Common/win32/ProxyResolver.cpp
        ../Common/win32/ProxyResolverContext.cpp
    )
elseif(UNIX)
    set(PLATFORM_SPECIFIC_SOURCES
        ../Common/linux/ProxyResolverContext.cpp
    )
endif()

set(HEADERS
    CheckedListCtrl.h
    ../Common/CommonGUIFunctions.h
    DebugFileParser.h
    DeviceConfigureFrame.h
    DeviceHandler.h
    DeviceHandlerBlueDevice.h
    DeviceHandlerBlueFOX.h
    DeviceHandlerHYPERION.h
    DeviceListCtrl.h
    ../Common/FirmwareUpdate_mvHYPERION/Epcs.h
    ${CMAKE_CURRENT_LIST_DIR}/../icons.h
    LogOutputConfigurationDlg.h
    LogOutputHandlerDlg.h
    ObjectFactory.h
    PackageDescriptionParser.h
    ../Common/ProxyResolverContext.h
    ../Common/FirmwareUpdate_mvHYPERION/Timeout.h
    ValuesFromUserDlg.h
    ../Common/wxAbstraction.h
    ${PLATFORM_SPECIFIC_HEADERS}
)
set_source_files_properties(../icons.h PROPERTIES GENERATED TRUE)

set(SOURCES
    CheckedListCtrl.cpp  
    ../Common/CommonGUIFunctions.cpp
    DebugFileParser.cpp
    DeviceConfigureFrame.cpp
    DeviceHandler.cpp
    DeviceHandlerBlueDevice.cpp
    DeviceHandlerBlueFOX.cpp
    DeviceHandlerHYPERION.cpp
    DeviceListCtrl.cpp
    ../Common/FirmwareUpdate_mvHYPERION/Epcs.cpp
    LogOutputConfigurationDlg.cpp
    LogOutputHandlerDlg.cpp
    PackageDescriptionParser.cpp
    ../Common/FirmwareUpdate_mvHYPERION/Timeout.cpp
    ValuesFromUserDlg.cpp
    ${PLATFORM_SPECIFIC_SOURCES}
)

add_executable(${TARGET}
    ${HEADERS}
    ${SOURCES}
    ${RESOURCE_FILES}
)
set_target_properties(${TARGET} PROPERTIES FOLDER "Applications(GUI)/mvDeviceConfigure")
if(MVIMPACT_ACQUIRE_BUILD)
    add_dependencies(${TARGET} mvDeviceConfigure_IconsToHeader mvDeviceConfigure_SplashScreenToHeader)
endif()

if(WIN32)
    target_include_directories(${TARGET} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/win32)
endif()
include(../../CMake/wxWidgets.includes.cmake)
include(../../CMake/wxWidgets.compile.cmake)
if(WIN32)
    target_compile_definitions(${TARGET} PRIVATE BUILD_WITH_DIRECT_SHOW_SUPPORT;BUILD_WITH_PROCESSOR_POWER_STATE_CONFIGURATION_SUPPORT;XML_STATIC)
endif()

if(WIN32)
    target_link_libraries(${TARGET} PRIVATE mvIMPACT_Acquire::ProcessorPowerStateConfiguration comctl32.lib rpcrt4.lib)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MANIFEST:NO")
endif()
include(../../CMake/libexpat.link.cmake)
target_link_libraries(${TARGET} PRIVATE mvIMPACT_Acquire::mvDeviceManager)
if(WIN32)
    target_link_libraries(${TARGET} PRIVATE winhttp.lib Ws2_32.lib)
    if(NOT wxWidgets_FOUND)
        target_link_libraries(${TARGET} PRIVATE
            ${WX_WIDGETS_LIB_DIR}/wxbase30u$<$<CONFIG:Debug>:d>_net.lib
            ${WX_WIDGETS_LIB_DIR}/wxregexu$<$<CONFIG:Debug>:d>.lib
        )
    endif()
endif()
include(../../CMake/wxWidgets.link.cmake)