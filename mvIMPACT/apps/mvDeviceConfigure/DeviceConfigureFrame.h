//-----------------------------------------------------------------------------
#ifndef DeviceConfigureFrameH
#define DeviceConfigureFrameH DeviceConfigureFrameH
//-----------------------------------------------------------------------------
#include <apps/Common/wxIncluder.h>
#include "DebugFileParser.h"
#include <mvIMPACT_CPP/mvIMPACT_acquire_GenICam.h>
#include "ObjectFactory.h"
#include "DeviceHandler.h"
#ifdef BUILD_WITH_DIRECT_SHOW_SUPPORT
#   include "win32/DirectShowSupport.h"
#endif // #ifdef BUILD_WITH_DIRECT_SHOW_SUPPORT
#ifdef BUILD_WITH_PROCESSOR_POWER_STATE_CONFIGURATION_SUPPORT
#   include <common/ProcessorPowerStateConfiguration/ProcessorPowerStateConfiguration.h>
#endif // #ifdef BUILD_WITH_PROCESSOR_POWER_STATE_CONFIGURATION_SUPPORT
#include <map>

wxDECLARE_EVENT( latestChangesDownloaded, wxCommandEvent );

class DeviceListCtrl;

//-----------------------------------------------------------------------------
class DeviceConfigureFrame : public wxFrame
//-----------------------------------------------------------------------------
{
    typedef std::map<std::string, std::string> IgnoredInterfacesInfo;
    typedef std::vector<std::pair<std::string, std::string> > ProductUpdateInfoURLList;

public:
    explicit DeviceConfigureFrame( const wxString& title, const wxPoint& pos, const wxSize& size, int argc, wxChar** argv );
    ~DeviceConfigureFrame();
    typedef DeviceHandler* ( *CreateDeviceHandler )( mvIMPACT::acquire::Device* );
    typedef ObjectFactory<DeviceHandler, wxString, CreateDeviceHandler, mvIMPACT::acquire::Device*> DeviceHandlerFactory;
    void                                ActivateDeviceIn_wxPropView( int deviceIndex );
    bool                                ApplicationHasElevatedRights( void ) const
    {
        return m_boApplicationHasElevatedRights;
    }
    void                                CheckForOnlineUpdates( const bool boSilent );
    void                                DownloadFWUpdateFile( wxString& linkToFWFile );
    void                                GetDeviceDescription( wxString& descriptionFileName, wxString& newestVersionDateBF3, wxString lineToExtract, wxString packageDescriptionVersionPattern, wxString deviceDescriptionFilePattern );
    void                                GetDeviceFirmwareFileName( wxString& descriptionFileName, wxString lineToExtract, wxString firmwareFilePattern );
    ProductUpdateInfoURLList            GetProductUpdateInfoURLsFromXML( const wxString& updateInfoXML ) const;
    ProductUpdateURLs                   GetProductUpdateInfosFromXML( const wxString& deviceType, const wxString& productXML ) const;
    void                                ReportThatAdminRightsAreNeeded( void );
    int                                 SetID( int deviceIndex );
    int                                 UpdateFirmware( int deviceIndex );
    void                                UpdateFirmwareLocalLatest( int deviceIndex );
    void                                UpdateFirmwareFromInternetAllDevices( void );
    int                                 UpdateFirmwareFromInternet( int deviceIndex );
    void                                UpdateMenu( int deviceIndex );
    bool                                WeekPassedSinceLastUpdatesCheck( void ) const;
    void                                WriteErrorMessage( const wxString& msg );
    void                                WriteLogMessage( const wxString& msg, const wxTextAttr& style = wxTextAttr( wxColour( 0, 0, 0 ) ) );
    DeviceManager&                      GetDeviceManager( void )
    {
        return m_devMgr;
    }
    DeviceHandlerFactory&               GetHandlerFactory( void )
    {
        return m_deviceHandlerFactory;
    }
    int                                 UpdateDMABufferSize( int deviceIndex );
    static int                          GetUpdateResult( void )
    {
        return m_s_updateResult;
    }
    bool                                IsFirmwareUpdateForced( void )
    {
        return m_boForceFirmwareUpdate;
    }
    //-----------------------------------------------------------------------------
    struct UpdateableDevices
    //-----------------------------------------------------------------------------
    {
        wxString serial_;
        Version deviceFWVersion_;
        wxString product_;
        FileEntry fileEntry_;
        explicit UpdateableDevices( const wxString& serial, const Version& devFWversion, const wxString& product, const FileEntry& fileEntry ) :
            serial_( serial ), deviceFWVersion_( devFWversion ), product_( product ), fileEntry_( fileEntry ) {}
    };
    typedef std::vector<UpdateableDevices> UpdateableDevicesContainer;
    UpdateableDevicesContainer      m_updateableDevices;
protected:
    // event handlers (these functions should _not_ be virtual)
    void                                OnHelp_About( wxCommandEvent& e );
    void                                OnHelp_OnlineDocumentation( wxCommandEvent& )
    {
        ::wxLaunchDefaultBrowser( wxT( "http://www.matrix-vision.com/manuals/" ) );
    }
    void                                OnCheckForOnlineUpdatesNow( wxCommandEvent& );
    void                                OnConfigureLogOutput( wxCommandEvent& e );
    void                                OnQuit( wxCommandEvent& e );
    void                                OnSetID( wxCommandEvent& e );
    void                                OnTimer( wxTimerEvent& e );
    void                                OnUpdateDeviceList( wxCommandEvent& e );
    void                                OnUpdateFirmware( wxCommandEvent& e );
    void                                OnUpdateFirmwareLocalLatest( wxCommandEvent& e );
    void                                OnUpdateFirmwareFromInternet( wxCommandEvent& e );
    void                                OnUpdateFirmwareFromInternetAll( wxCommandEvent& )
    {
        UpdateFirmwareFromInternetAllDevices();
    }
    void                                OnUpdateDMABufferSize( wxCommandEvent& e );
    //-----------------------------------------------------------------------------
    enum TTimerEvent
    //-----------------------------------------------------------------------------
    {
        teListUpdate,
        teTimer
    };
    //-----------------------------------------------------------------------------
    enum
    //-----------------------------------------------------------------------------
    {
        TIMER_PERIOD = 500
    };
private:
    //-----------------------------------------------------------------------------
    enum TIgnoreInterfaceCommandLineParameter
    //-----------------------------------------------------------------------------
    {
        iiclpNotConfigured,
        iiclpEnableIgnore,
        iiclpDisableIgnore
    };
    void                                CollectOnlineFirmwareVersions( void );
    void                                FindFirmwarePacketsForPresentDevices( FileEntryContainer& m_onlineFirmwareVersions );
    void                                BuildList( void );
    bool                                CheckForDeviceConnectedToIgnoredInterfacesAndMarkGEVConfigurationProblems( Device* pDev, long index );
    static void                         RefreshApplicationExitCode( const int result );
    int                                 SetID( Device* pDev, int newID );
    void                                UpdateDeviceList( bool boShowProgressDialog = true );
    int                                 UpdateFirmware( Device* pDev, bool boSilentMode, bool boPersistentUserSets, bool boUseOnlineArchive, const wxString& customFirmwareFileName = wxEmptyString );
    int                                 UpdateFirmwareFromInternet( Device* pDev, bool boSilentMode );
    static int                          m_s_updateResult;
    mvIMPACT::acquire::DeviceManager    m_devMgr;
    bool                                m_boDevicesOnIgnoredInterfacesDetected;
    bool                                m_boCheckedForUpdates;
    TIgnoreInterfaceCommandLineParameter m_commandLineIgnoreGEV;
    TIgnoreInterfaceCommandLineParameter m_commandLineIgnorePCI;
    TIgnoreInterfaceCommandLineParameter m_commandLineIgnoreU3V;
    IgnoredInterfacesInfo               m_ignoredInterfacesInfo;
    DeviceListCtrl*                     m_pDevListCtrl;
    wxMenuItem*                         m_pMIActionSetID;
    wxMenuItem*                         m_pMIActionUpdateDMABufferSize;
    wxMenuItem*                         m_pMIActionUpdateDeviceList;
    wxMenuItem*                         m_pMIActionUpdateFW;
    wxMenuItem*                         m_pMIActionUpdateFWDefault;
    wxMenuItem*                         m_pMIActionFWUpdateFromInternet;
    wxMenuItem*                         m_pMIActionFWUpdateFromInternetAllDevices;
    wxMenuItem*                         m_pMIHelp_AutoCheckForOnlineUpdatesWeekly;
    wxDateTime                          m_lastCheckForNewerFWVersion;
    wxTextCtrl*                         m_pLogWindow;
    wxString                            m_logFileName;
    wxTimer                             m_timer;
    wxTimer                             m_listUpdateTimer;
    unsigned int                        m_lastDevMgrChangedCount;
    LogConfigurationVector              m_debugData;
    DeviceHandlerFactory                m_deviceHandlerFactory;
    wxString                            m_customFirmwareFile;
    wxString                            m_customFirmwarePath;
    wxString                            m_customGenICamFile;
    wxString                            m_downloadedFileContent;
    FileEntryContainer                  m_onlineFirmwareVersions;
    std::vector<ProductUpdateURLs>      m_productUpdateInfoListOnline;
    std::vector<std::string>            m_IPv4Masks;
    ProductToFileEntryMap               m_productToFirmwareFilesOnlineMap;
    ProductToFileEntryMap               m_productToFirmwareFilesLocalMap;
    //-----------------------------------------------------------------------------
    struct DeviceConfigurationData
    //-----------------------------------------------------------------------------
    {
        bool boUpdateFW_;
        wxString customFirmwareFileName_;
        bool boSetDeviceID_;
        int deviceID_;
        std::vector<std::string> serials_;
        explicit DeviceConfigurationData() :  boUpdateFW_( false ), customFirmwareFileName_(),
            boSetDeviceID_( false ), deviceID_( -1 ), serials_() {}
    };
    void                                GetConfigurationEntriesFromFile( const wxString& fileName );
    std::map<wxString, DeviceConfigurationData>::iterator GetConfigurationEntry( const wxString& value );
    bool                                PerformDeviceConfiguration( const std::string& searchToken, const DeviceConfigurationData& configurationData );
    void                                ReadIgnoredInterfacesInfoAndEnableAllInterfaces( void );
    void                                RevertIgnoredInterfaces( void );
    void                                RevertIgnoredInterfaceTechnology( const mvIMPACT::acquire::GenICam::SystemModule& sm, const std::string& technology, TIgnoreInterfaceCommandLineParameter commandLineParameter );
    void                                UnIgnoreInterfaceTechnology( const mvIMPACT::acquire::GenICam::SystemModule& sm, const std::string& technology );
    std::map<wxString, DeviceConfigurationData> m_devicesToConfigure;
    bool                                m_boPendingQuit;
    bool                                m_boForceFirmwareUpdate;
    bool                                m_boApplicationHasElevatedRights;
#ifdef BUILD_WITH_PROCESSOR_POWER_STATE_CONFIGURATION_SUPPORT
    bool                                m_boChangeProcessorIdleStates;
    bool                                m_boEnableIdleStates;
#endif // #ifdef BUILD_WITH_PROCESSOR_POWER_STATE_CONFIGURATION_SUPPORT
    // any class wishing to process wxWidgets events must use this macro
    DECLARE_EVENT_TABLE()
    //-----------------------------------------------------------------------------
    // IDs for the controls and the menu commands
    enum TMenuItem
    //-----------------------------------------------------------------------------
    {
        miAction_Quit = 1,
        miAction_SetID,
        miAction_UpdateFW,
        miAction_UpdateFWDefault,
        miAction_UpdateFWFromInternet,
        miAction_UpdateFWFromInternetAll,
        miAction_ConfigureLogOutput,
        miAction_UpdateDeviceList,
        miAction_UpdateDMABufferSize,
        miHelp_About,
        miHelp_CheckForOnlineUpdatesNow,
        miHelp_OnlineDocumentation,
        miCOMMON_LAST
    };
    //-----------------------------------------------------------------------------
    enum TDeviceIDLimits
    //-----------------------------------------------------------------------------
    {
        didlMin = 0,
        didlMax = 250
    };
#ifdef BUILD_WITH_DIRECT_SHOW_SUPPORT
    DirectShowDeviceManager             m_DSDevMgr;
    //-----------------------------------------------------------------------------
    enum TMenuItem_DirectShow
    //-----------------------------------------------------------------------------
    {
        miDirectShow_RegisterAllDevices = miCOMMON_LAST,
        miDirectShow_UnregisterAllDevices,
        miDirectShow_SetFriendlyName
    };
    wxMenuItem*                         m_pMIDirectShow_SetFriendlyName;
    void                                OnRegisterAllDevicesForDirectShow( wxCommandEvent& );
    void                                OnUnregisterAllDevicesForDirectShow( wxCommandEvent& );
    void                                OnSetFriendlyNameForDirectShow( wxCommandEvent& e );
public:
    int                                 SetDSFriendlyName( int deviceIndex );
#endif // #ifdef BUILD_WITH_DIRECT_SHOW_SUPPORT

    //-----------------------------------------------------------------------------
    enum TMenuItem_Settings
    //-----------------------------------------------------------------------------
    {
        miSettings_KeepUserSetSettingsAfterFirmwareUpdate = miCOMMON_LAST + 100
#ifdef BUILD_WITH_PROCESSOR_POWER_STATE_CONFIGURATION_SUPPORT
        , miSettings_CPUIdleStatesEnabled
#endif // #ifdef BUILD_WITH_PROCESSOR_POWER_STATE_CONFIGURATION_SUPPORT
    };

    wxMenuItem*                         m_pMISettings_KeepUserSetSettingsAfterFirmwareUpdate;

#ifdef BUILD_WITH_PROCESSOR_POWER_STATE_CONFIGURATION_SUPPORT
    wxMenuItem*                         m_pMISettings_CPUIdleStatesEnabled;
    void                                OnSettings_CPUIdleStatesEnabled( wxCommandEvent& e );
#endif // #ifdef BUILD_WITH_PROCESSOR_POWER_STATE_CONFIGURATION_SUPPORT
    void                                OnSettings_KeepUserSetSettingsAfterFirmwareUpdate( wxCommandEvent& e )
    {
        if( wxMessageBox( wxString::Format( wxT( "Are you sure you want to %s persistent UserSet settings?\n If you select YES, all UserSet settings of a GenICam device \n will be %s every time you update its' firmware!" ), e.IsChecked() ? wxT( "enable" ) : wxT( "disable" ), e.IsChecked() ? wxT( "preserved" ) : wxT( "deleted" ) ), wxT( "UserSet settings and Firmware Update" ), wxYES_NO | wxNO_DEFAULT | wxICON_EXCLAMATION, this ) != wxYES )
        {
            m_pMISettings_KeepUserSetSettingsAfterFirmwareUpdate->Check( !e.IsChecked() );
        }
    }
};

#endif // DeviceConfigureFrameH
