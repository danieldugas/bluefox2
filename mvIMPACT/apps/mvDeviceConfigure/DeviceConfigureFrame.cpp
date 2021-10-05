#include <apps/Common/CommonGUIFunctions.h>
#include <apps/Common/exampleHelper.h>
#include <apps/Common/ProxyResolverContext.h>
#include <apps/Common/wxAbstraction.h>
#include <common/STLHelper.h>
#include "DeviceConfigureFrame.h"
#include "DeviceHandlerBlueDevice.h"
#include "DeviceHandlerBlueFOX.h"
#include "DeviceHandlerHYPERION.h"
#include "DeviceListCtrl.h"
#include <fstream>
#include <limits>
#include "LogOutputHandlerDlg.h"
#include "mvDeviceConfigure.icon.xpm"
#include "SplashScreen/SplashScreen.h"
#include <string>
#include "ValuesFromUserDlg.h"
#include <apps/Common/wxIncludePrologue.h>
#include <wx/config.h>
#include <wx/dir.h>
#include <wx/ffile.h>
#include <wx/filefn.h>
#include <wx/image.h>
#if wxCHECK_VERSION(2, 7, 1)
#   include <wx/platinfo.h>
#endif // #if wxCHECK_VERSION(2, 7, 1)
#include <wx/filename.h>
#include <wx/progdlg.h>
#include <wx/regex.h>
#include <wx/socket.h>
#include <wx/splitter.h>
#include <wx/sstream.h>
#include <wx/tokenzr.h>
#include <wx/url.h>
#include <wx/utils.h>
#include <wx/wfstream.h>
#include <wx/xml/xml.h>
#include <wx/tooltip.h>
#include <apps/Common/wxIncludeEpilogue.h>

using namespace mvIMPACT::acquire;
using namespace std;

wxDEFINE_EVENT( latestChangesDownloaded, wxCommandEvent );

//=============================================================================
//======================= Internal helper functions ===========================
//=============================================================================
//-----------------------------------------------------------------------------
bool DownloadContentHasErrors( const wxString& downloadedContent, const bool boSilent, DeviceConfigureFrame* pDeviceConfigureFrame )
//-----------------------------------------------------------------------------
{
    if( downloadedContent.empty() ||
        downloadedContent.StartsWith( wxT( "wxURL Error:" ) ) )
    {
        const wxString msg( wxString::Format( wxT( "An error occurred while trying to contact MATRIX VISION server:\n%s\n" ), downloadedContent.empty() ? "Server not reachable!" : downloadedContent.mb_str() ) );
        ShowMessageToUser( pDeviceConfigureFrame, wxT( "Connection Error!" ), msg, boSilent, wxOK | wxICON_ERROR, true );
        return true;
    }
    return false;
}

//=============================================================================
//================= Implementation MyApp ======================================
//=============================================================================
//-----------------------------------------------------------------------------
class MyApp : public wxApp
//-----------------------------------------------------------------------------
{
    DeviceConfigureFrame* pFrame_;
    void CreateApplicationInstance( void )
    {
        pFrame_ = new DeviceConfigureFrame( wxString::Format( wxT( "mvDeviceConfigure - Tool For %s Devices(Firmware Updates, Log-Output Configuration, etc.)(%s)" ), COMPANY_NAME, VERSION_STRING ), wxDefaultPosition, wxDefaultSize, argc, argv );
    }
public:
#ifdef __clang_analyzer__ // The analyzer doesn't understand the wxWidgets memory management thus assumes that the 'pFrame_' pointer is never freed :-(
    ~MyApp()
    {
        delete pFrame_;
    }
#endif // #ifdef __clang_analyzer__
    virtual bool OnInit( void )
    {
        wxImage::AddHandler( new wxPNGHandler );

        bool boShowWindow = true;
        for( int i = 1; i < argc; i++ )
        {
            wxString param( wxApp::argv[i].MakeLower() );
            const wxString key( param.BeforeFirst( wxT( '=' ) ) );
            if( key == wxT( "hidden" ) )
            {
                boShowWindow = false;
            }
        }

        if( boShowWindow )
        {
            SplashScreenScope splashScreenScope( wxBitmap::NewFromPNGData( splashScreen_png, sizeof( splashScreen_png ) ), wxBitmap( mvDeviceConfigure_icon_xpm ) );
            // Create the main application window
            CreateApplicationInstance();
            pFrame_->Show( true );
            SetTopWindow( pFrame_ );
            // success: wxApp::OnRun() will be called which will enter the main message
            // loop and the application will run. If we returned false here, the
            // application would exit immediately.
            // Workaround for the refreshing of the Log wxTextCtrl
            pFrame_->WriteLogMessage( wxT( "" ) );
        }
        else
        {
            CreateApplicationInstance();
        }
        return true;
    }
    virtual int OnRun( void )
    {
        wxApp::OnRun();
        return DeviceConfigureFrame::GetUpdateResult();
    }
};

IMPLEMENT_APP( MyApp )

//=============================================================================
//=========== Implementation Contact Matrix Vision Server Thread===============
//=============================================================================
//------------------------------------------------------------------------------
class ContactMatrixVisionServerThread : public wxThread
//------------------------------------------------------------------------------
{
    wxString urlString_;
    wxString& fileContents_;
    wxEvtHandler* pEventHandler_;
    int parentWindowID_;
    unsigned int connectionTimeout_s_;
protected:
    void* Entry()
    {
        wxURL url( urlString_ );
        if( url.GetError() == wxURL_NOERR )
        {
            url.GetProtocol().SetDefaultTimeout( connectionTimeout_s_ );
            wxPlatformInfo platformInfo;
            if( ( platformInfo.GetOperatingSystemId() & wxOS_WINDOWS ) != 0 )
            {
                ProxyResolverContext proxyResolverContext( wxT( "mvDeviceConfigure" ), wxT( "http://www.matrix-vision.de" ) );
                if( proxyResolverContext.GetProxy( 0 ).length() > 0 )
                {
                    url.SetProxy( wxString( proxyResolverContext.GetProxy( 0 ).c_str() ) + wxT( ":" ) + wxString::Format( wxT( "%u" ), proxyResolverContext.GetProxyPort( 0 ) ) );
                }
            }

            wxInputStream* pInStream = url.GetInputStream();
            if( pInStream )
            {
                if( pInStream->IsOk() )
                {
                    wxString tUrl = urlString_.AfterLast( wxT( '.' ) );
                    if( tUrl != wxString( wxT( "mvu" ) ) )
                    {
                        auto_array_ptr<unsigned char> pBuf( pInStream->GetSize() );
                        wxStringOutputStream outStream( &fileContents_ );
                        while( pInStream->CanRead() && !pInStream->Eof() )
                        {
                            pInStream->Read( pBuf.get(), pBuf.parCnt() );
                            outStream.WriteAll( pBuf.get(), pInStream->LastRead() );
                        }
                    }
                    else
                    {
                        wxString fileName = wxT( "" );
                        wxString pathSuffix = wxT( "" );
                        if( urlString_.Find( "mvBlueCOUGAR-X_Update" ) != wxNOT_FOUND )
                        {
                            pathSuffix = wxT( "FirmwareUpdates/mvBlueCOUGAR/" );
                        }
                        else if( urlString_.Find( "mvBlueFOX3_Update" ) != wxNOT_FOUND )
                        {
                            pathSuffix = wxT( "FirmwareUpdates/mvBlueFOX/" );
                        }
                        else if( urlString_.Find( "mvBlueCOUGAR-XT_Update" ) != wxNOT_FOUND )
                        {
                            pathSuffix = wxT( "FirmwareUpdates/mvBlueCOUGAR-XT/" );
                        }
                        fileName = GetPlatformSpecificDownloadPath( pathSuffix.Append( urlString_.AfterLast( wxT( '/' ) ) ) );

                        if( !wxDirExists( fileName.BeforeLast( wxT( '/' ) ) ) )
                        {
                            const wxString dirName = fileName.BeforeLast( wxT( '/' ) );
                            if( !wxFileName::Mkdir( dirName, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL ) )
                            {
                                fileName = wxString( wxT( "Temporary directory could not be created!\n" ) );
                            }
                        }
                        if( !wxFileExists( fileName ) )
                        {
                            wxFileOutputStream localFile( fileName );
                            auto_array_ptr<unsigned char> pBuf( pInStream->GetSize() );
                            while( pInStream->CanRead() && !pInStream->Eof() )
                            {
                                pInStream->Read( pBuf.get(), pBuf.parCnt() );
                                localFile.WriteAll( pBuf.get(), pInStream->LastRead() );
                            }
                        }
                        fileContents_ = fileName;
                    }
                }
                delete pInStream;
            }
        }
        else
        {
            // An error occurred while trying to contact the MATRIX VISION server...
            fileContents_ = wxString::Format( wxT( "wxURL Error: %d\n" ), url.GetError() );
        }
        if( pEventHandler_ && ( GetKind() == wxTHREAD_DETACHED ) )
        {
            wxQueueEvent( pEventHandler_, new wxCommandEvent( latestChangesDownloaded, parentWindowID_ ) );
        }
        return 0;
    }
public:
    explicit ContactMatrixVisionServerThread( const wxString& urlString, wxString& fileContents, wxThreadKind kind, unsigned int connectionTimeout_s = 5 ) : wxThread( kind ),
        urlString_( urlString ), fileContents_( fileContents ), pEventHandler_( 0 ), parentWindowID_( wxID_ANY ), connectionTimeout_s_( connectionTimeout_s ) {}
    void AttachEventHandler( wxEvtHandler* pEventHandler, int parentWindowID )
    {
        pEventHandler_ = pEventHandler;
        parentWindowID_ = parentWindowID;
    }
};

//=============================================================================
//================= Implementation DeviceConfigureFrame =======================
//=============================================================================
BEGIN_EVENT_TABLE( DeviceConfigureFrame, wxFrame )
    EVT_MENU( miHelp_CheckForOnlineUpdatesNow, DeviceConfigureFrame::OnCheckForOnlineUpdatesNow )
    EVT_MENU( miHelp_About, DeviceConfigureFrame::OnHelp_About )
    EVT_MENU( miHelp_OnlineDocumentation, DeviceConfigureFrame::OnHelp_OnlineDocumentation )
    EVT_MENU( miAction_Quit, DeviceConfigureFrame::OnQuit )
    EVT_MENU( miAction_SetID, DeviceConfigureFrame::OnSetID )
    EVT_MENU( miAction_UpdateFW, DeviceConfigureFrame::OnUpdateFirmware )
    EVT_MENU( miAction_UpdateFWDefault, DeviceConfigureFrame::OnUpdateFirmwareLocalLatest )
    EVT_MENU( miAction_UpdateFWFromInternet, DeviceConfigureFrame::OnUpdateFirmwareFromInternet )
    EVT_MENU( miAction_UpdateFWFromInternetAll, DeviceConfigureFrame::OnUpdateFirmwareFromInternetAll )
    EVT_MENU( miAction_ConfigureLogOutput, DeviceConfigureFrame::OnConfigureLogOutput )
    EVT_MENU( miAction_UpdateDeviceList, DeviceConfigureFrame::OnUpdateDeviceList )
    EVT_MENU( miAction_UpdateDMABufferSize, DeviceConfigureFrame::OnUpdateDMABufferSize )
#ifdef BUILD_WITH_DIRECT_SHOW_SUPPORT
    EVT_MENU( miDirectShow_RegisterAllDevices, DeviceConfigureFrame::OnRegisterAllDevicesForDirectShow )
    EVT_MENU( miDirectShow_UnregisterAllDevices, DeviceConfigureFrame::OnUnregisterAllDevicesForDirectShow )
    EVT_MENU( miDirectShow_SetFriendlyName, DeviceConfigureFrame::OnSetFriendlyNameForDirectShow )
#endif // #ifdef BUILD_WITH_DIRECT_SHOW_SUPPORT
#ifdef BUILD_WITH_PROCESSOR_POWER_STATE_CONFIGURATION_SUPPORT
    EVT_MENU( miSettings_CPUIdleStatesEnabled, DeviceConfigureFrame::OnSettings_CPUIdleStatesEnabled )
#endif // #ifdef BUILD_WITH_PROCESSOR_POWER_STATE_CONFIGURATION_SUPPORT
    EVT_MENU( miSettings_KeepUserSetSettingsAfterFirmwareUpdate, DeviceConfigureFrame::OnSettings_KeepUserSetSettingsAfterFirmwareUpdate )
    EVT_TIMER( wxID_ANY, DeviceConfigureFrame::OnTimer )
END_EVENT_TABLE()

int DeviceConfigureFrame::m_s_updateResult = 0;

//-----------------------------------------------------------------------------
DeviceConfigureFrame::DeviceConfigureFrame( const wxString& title, const wxPoint& pos, const wxSize& size, int argc, wxChar** argv )
    : wxFrame( ( wxFrame* )NULL, wxID_ANY, title, pos, size ), m_updateableDevices(), m_boDevicesOnIgnoredInterfacesDetected( false ), m_boCheckedForUpdates( false ),
      m_commandLineIgnoreGEV( iiclpNotConfigured ), m_commandLineIgnorePCI( iiclpNotConfigured ), m_commandLineIgnoreU3V( iiclpNotConfigured ), m_ignoredInterfacesInfo(), m_pDevListCtrl( 0 ),
      m_pLogWindow( 0 ), m_logFileName(), m_lastDevMgrChangedCount( numeric_limits<unsigned int>::max() ), m_customFirmwareFile(),
      m_customFirmwarePath(), m_customGenICamFile(), m_onlineFirmwareVersions(), m_productUpdateInfoListOnline(), m_IPv4Masks(), m_productToFirmwareFilesOnlineMap(), m_productToFirmwareFilesLocalMap(),
      m_devicesToConfigure(), m_boPendingQuit( false ), m_boForceFirmwareUpdate( false ), m_boApplicationHasElevatedRights( false )
#ifdef BUILD_WITH_PROCESSOR_POWER_STATE_CONFIGURATION_SUPPORT
    , m_boChangeProcessorIdleStates( false ), m_boEnableIdleStates( false ), m_pMISettings_CPUIdleStatesEnabled( 0 )
#endif // #ifdef BUILD_WITH_PROCESSOR_POWER_STATE_CONFIGURATION_SUPPORT
//-----------------------------------------------------------------------------
{
    m_deviceHandlerFactory.Register( wxString( wxT( "GigEVisionDevice" ) ), DeviceHandler3rdParty::Create );
    m_deviceHandlerFactory.Register( wxString( wxT( "USB3VisionDevice" ) ), DeviceHandler3rdParty::Create );
    m_deviceHandlerFactory.Register( wxString( wxT( "mvBlueCOUGAR" ) ), DeviceHandlerBlueCOUGAR::Create );
    m_deviceHandlerFactory.Register( wxString( wxT( "mvBlueFOX" ) ), DeviceHandlerBlueFOX::Create );
    m_deviceHandlerFactory.Register( wxString( wxT( "mvBlueFOX3" ) ), DeviceHandlerBlueFOX3::Create );
    m_deviceHandlerFactory.Register( wxString( wxT( "mvBlueNAOS" ) ), DeviceHandlerBlueNAOS::Create );
    m_deviceHandlerFactory.Register( wxString( wxT( "mvBlueSIRIUS" ) ), DeviceHandlerBlueCOUGAR::Create );
    m_deviceHandlerFactory.Register( wxString( wxT( "mvHYPERION" ) ), DeviceHandlerHYPERION::Create );
    m_deviceHandlerFactory.Register( wxString( wxT( "BVS CA" ) ), BVSCADeviceHandlerCreator::Create );

    wxToolTip::Enable( true );
    wxToolTip::SetAutoPop( 12000 );

    // create the menu
    wxMenu* pMenuAction = new wxMenu;
    m_pMIActionSetID = pMenuAction->Append( miAction_SetID, wxT( "&Set ID\tCTRL+S" ) );
    wxMenu* pMenuFWUpdate = new wxMenu( wxT( "" ) );
    pMenuAction->AppendSubMenu( pMenuFWUpdate, wxT( "Update Firmware" ) );
    m_pMIActionUpdateFWDefault = pMenuFWUpdate->Append( miAction_UpdateFWDefault, wxT( "From Latest Local &Firmware Package" ) );
    m_pMIActionUpdateFW = pMenuFWUpdate->Append( miAction_UpdateFW, wxT( "From Specific Local &Firmware Package\tCTRL+F" ) );
    m_pMIActionFWUpdateFromInternet = pMenuFWUpdate->Append( miAction_UpdateFWFromInternet, wxT( "From &Online Firmware Package\tCTRL+O" ) );
    m_pMIActionFWUpdateFromInternetAllDevices = pMenuFWUpdate->Append( miAction_UpdateFWFromInternetAll, wxT( "From &Online Firmware Package On All Connected Devices\tCTRL+A" ) );
    m_pMIActionUpdateDMABufferSize = pMenuAction->Append( miAction_UpdateDMABufferSize, wxT( "Update Permanent &DMA Buffer Size\tCTRL+D" ) );

    pMenuAction->AppendSeparator();
    pMenuAction->Append( miAction_ConfigureLogOutput, wxT( "&Configure Log Output\tCTRL+C" ) );
    m_pMIActionUpdateDeviceList = pMenuAction->Append( miAction_UpdateDeviceList, wxT( "&Update Device List\tF5" ) );
    pMenuAction->AppendSeparator();

    pMenuAction->Append( miAction_Quit, wxT( "E&xit\tALT+X" ) );

    wxMenu* pMenuHelp = new wxMenu;
    pMenuHelp->Append( miHelp_OnlineDocumentation, wxT( "Online Documentation...\tF12" ) );
    pMenuHelp->AppendSeparator();
    pMenuHelp->Append( miHelp_CheckForOnlineUpdatesNow, wxT( "Check For Firmware Updates Now\tCTRL+E" ) );
    m_pMIHelp_AutoCheckForOnlineUpdatesWeekly = pMenuHelp->Append( wxID_ANY, wxT( "&Auto-Check For Firmware Updates Every Week...\tALT+CTRL+E" ), wxT( "" ), wxITEM_CHECK );
    pMenuHelp->AppendSeparator();
    pMenuHelp->Append( miHelp_About, wxT( "About...\tF1" ) );

    // create a menu bar...
    wxMenuBar* pMenuBar = new wxMenuBar;
    // ... add all the menu items...
    pMenuBar->Append( pMenuAction, wxT( "&Action" ) );
#ifdef BUILD_WITH_DIRECT_SHOW_SUPPORT
    wxMenu* menuDirectShow = new wxMenu;
    menuDirectShow->Append( miDirectShow_RegisterAllDevices, wxT( "&Register All Devices\tCTRL+R" ) );
    menuDirectShow->Append( miDirectShow_UnregisterAllDevices, wxT( "&Unregister All Devices\tCTRL+U" ) );
    menuDirectShow->AppendSeparator();
    m_pMIDirectShow_SetFriendlyName = menuDirectShow->Append( miDirectShow_SetFriendlyName, wxT( "Set Friendly Name\tALT+CTRL+F" ) );
    pMenuBar->Append( menuDirectShow, wxT( "&DirectShow" ) );
    m_DSDevMgr.create( this );
#endif // #ifdef BUILD_WITH_DIRECT_SHOW_SUPPORT
    wxMenu* pMenuSettings = new wxMenu;
    m_pMISettings_KeepUserSetSettingsAfterFirmwareUpdate = pMenuSettings->Append( miSettings_KeepUserSetSettingsAfterFirmwareUpdate, wxT( "&Persistent UserSet Settings\tCTRL+P" ), wxT( "" ), wxITEM_CHECK );
#ifdef BUILD_WITH_PROCESSOR_POWER_STATE_CONFIGURATION_SUPPORT
    bool boAllowCPUIdleStateConfiguration = true;
#   if wxCHECK_VERSION(2, 7, 1)
    wxPlatformInfo platformInfo;
    if( platformInfo.GetOperatingSystemId() == wxOS_WINDOWS_NT )
    {
        if( ( platformInfo.GetOSMajorVersion() > 6 ) ||
            ( ( platformInfo.GetOSMajorVersion() == 6 ) && ( platformInfo.GetOSMinorVersion() >= 2 ) ) )
        {
            boAllowCPUIdleStateConfiguration = false;
        }
    }
#   endif // #if wxCHECK_VERSION(2, 7, 1)
    if( boAllowCPUIdleStateConfiguration )
    {
        m_pMISettings_CPUIdleStatesEnabled = pMenuSettings->Append( miSettings_CPUIdleStatesEnabled, wxT( "CPU &Idle States Enabled\tCTRL+I" ), wxT( "" ), wxITEM_CHECK );
        bool boValue = false;
        GetPowerState( boValue );
        m_pMISettings_CPUIdleStatesEnabled->Check( boValue );
    }
#endif // #ifdef BUILD_WITH_PROCESSOR_POWER_STATE_CONFIGURATION_SUPPORT
    pMenuBar->Append( pMenuSettings, wxT( "&Settings" ) );
    pMenuBar->Append( pMenuHelp, wxT( "&Help" ) );
    // ... and attach this menu bar to the frame
    SetMenuBar( pMenuBar );

    // define the applications icon
    wxIcon icon( mvDeviceConfigure_icon_xpm );
    SetIcon( icon );

    wxPanel* pPanel = new wxPanel( this, wxID_ANY );
    wxSplitterWindow* pHorizontalSplitter = new wxSplitterWindow( pPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSIMPLE_BORDER );
    wxBoxSizer* pSizer = new wxBoxSizer( wxVERTICAL );
    pSizer->Add( pHorizontalSplitter, wxSizerFlags( 5 ).Expand() );

    m_pDevListCtrl = new DeviceListCtrl( pHorizontalSplitter, LIST_CTRL, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL | wxBORDER_NONE, this );
    m_pDevListCtrl->InsertColumn( lcFamily, wxT( "Family" ) );
    m_pDevListCtrl->InsertColumn( lcProduct, wxT( "Product" ) );
    m_pDevListCtrl->InsertColumn( lcSerial, wxT( "Serial" ) );
    m_pDevListCtrl->InsertColumn( lcState, wxT( "State" ) );
    m_pDevListCtrl->InsertColumn( lcFWVersion, wxT( "Firmware Version" ) );
    m_pDevListCtrl->InsertColumn( lcDeviceID, wxT( "Device ID" ) );
    m_pDevListCtrl->InsertColumn( lcAllocatedDMABuffer, wxT( "Allocated DMA Buffer(KB)" ) );
#ifdef BUILD_WITH_DIRECT_SHOW_SUPPORT
    m_pDevListCtrl->InsertColumn( lcDSRegistered, wxT( "Registered For DirectShow" ) );
    m_pDevListCtrl->InsertColumn( lcDSFriendlyName, wxT( "DirectShow Friendly Name" ) );
#endif // #ifdef BUILD_WITH_DIRECT_SHOW_SUPPORT

    m_pLogWindow = new wxTextCtrl( pHorizontalSplitter, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxBORDER_NONE | wxTE_RICH | wxTE_READONLY );

    // splitter...
    pHorizontalSplitter->SetSashGravity( 0.8 );
    pHorizontalSplitter->SetMinimumPaneSize( 100 );

    ReadIgnoredInterfacesInfoAndEnableAllInterfaces();

    RevertIgnoredInterfaces();

    // restore previous state
    const wxRect defaultRect( 0, 0, 640, 480 );
    bool boMaximized = false;
    const wxRect rect = FramePositionStorage::Load( defaultRect, boMaximized );
    wxConfigBase* pConfig = wxConfigBase::Get();
    bool boUserSetPersistence = pConfig->Read( wxT( "/MainFrame/Settings/PersistentUserSetSettings" ), 1l ) != 0;
    const bool boAutoCheckForOnlineUpdatesWeekly = pConfig->Read( wxT( "/MainFrame/Settings/AutoCheckForOnlineUpdatesWeekly" ), 1l ) != 0;
    m_lastCheckForNewerFWVersion.ParseFormat( pConfig->Read( wxT( "/MainFrame/Help/LastCheck" ), wxT( "2000-01-01" ) ), "%Y-%m-%d" );
    if( boAutoCheckForOnlineUpdatesWeekly )
    {
        m_pMIHelp_AutoCheckForOnlineUpdatesWeekly->Check( true );
    }

    m_boApplicationHasElevatedRights = IsCurrentUserLocalAdministrator();
    BuildList();

    wxString processedCommandLineParameters;
    wxString commandLineErrors;
    for( int i = 1; i < argc; i++ )
    {
        const wxString param( argv[i] );
        const wxString key = param.BeforeFirst( wxT( '=' ) );
        const wxString value = param.AfterFirst( wxT( '=' ) );
        if( key.IsEmpty() )
        {
            commandLineErrors.Append( wxString::Format( wxT( "Invalid command line parameter: '%s'.\n" ), param.c_str() ) );
        }
        else
        {
            if( ( key == wxT( "update_fw" ) ) || ( key == wxT( "ufw" ) ) )
            {
                GetConfigurationEntry( value )->second.boUpdateFW_ = true;
            }
            else if( ( key == wxT( "update_fw_file" ) ) || ( key == wxT( "ufwf" ) ) )
            {
                GetConfigurationEntriesFromFile( value );
            }
            else if( ( key == wxT( "custom_genicam_file" ) ) || ( key == wxT( "cgf" ) ) )
            {
                m_customGenICamFile = value;
            }
            else if( key == wxT( "fw_path" ) )
            {
                m_customFirmwarePath = value;
            }
            else if( key == wxT( "fw_file" ) )
            {
                m_customFirmwareFile = value;
            }
            else if( key == wxT( "ipv4_mask" ) )
            {
                wxStringTokenizer tokenizer( value, wxString( wxT( ";" ) ), wxTOKEN_STRTOK );
                while( tokenizer.HasMoreTokens() )
                {
                    std::string token( tokenizer.GetNextToken().mb_str() );
                    m_IPv4Masks.push_back( token );
                }
            }
            else if( ( key == wxT( "ignore_gev" ) ) || ( key == wxT( "igev" ) ) )
            {
                m_commandLineIgnoreGEV = ( value == wxString( "1" ) ) ? iiclpEnableIgnore : iiclpDisableIgnore;
            }
            else if( ( key == wxT( "ignore_pci" ) ) || ( key == wxT( "ipci" ) ) )
            {
                m_commandLineIgnorePCI = ( value == wxString( "1" ) ) ? iiclpEnableIgnore : iiclpDisableIgnore;
            }
            else if( ( key == wxT( "ignore_u3v" ) ) || ( key == wxT( "iu3v" ) ) )
            {
                m_commandLineIgnoreU3V = ( value == wxString( "1" ) ) ? iiclpEnableIgnore : iiclpDisableIgnore;
            }
            else if( ( key == wxT( "log_file" ) ) || ( key == wxT( "lf" ) ) )
            {
                m_logFileName = value;
            }
#ifdef BUILD_WITH_PROCESSOR_POWER_STATE_CONFIGURATION_SUPPORT
            else if( ( key == wxT( "set_processor_idle_states" ) ) || ( key == wxT( "spis" ) ) )
            {
                unsigned long conversionResult = 0;
                if( value.ToULong( &conversionResult ) )
                {
                    m_boChangeProcessorIdleStates = true;
                    m_boEnableIdleStates = conversionResult != 0;
                }
            }
#endif // #ifdef BUILD_WITH_PROCESSOR_POWER_STATE_CONFIGURATION_SUPPORT
            else if( ( key == wxT( "setid" ) ) || ( key == wxT( "id" ) ) )
            {
                wxString serial( value.BeforeFirst( wxT( '.' ) ) );
                wxString idString( value.AfterFirst( wxT( '.' ) ) );
                if( serial.IsEmpty() || idString.IsEmpty() )
                {
                    commandLineErrors.Append( wxString::Format( wxT( "Invalid command line parameter: '%s'.\n" ), param.c_str() ) );
                }
                else
                {
                    long id;
                    if( !idString.ToLong( &id ) )
                    {
                        commandLineErrors.Append( wxString::Format( wxT( "Invalid command line parameter: '%s'.\n" ), param.c_str() ) );
                    }
                    else
                    {
                        GetConfigurationEntry( serial )->second.boSetDeviceID_ = true;
                        GetConfigurationEntry( serial )->second.deviceID_ = static_cast<int>( id );
                    }
                }
            }
            else if( ( key == wxT( "quit" ) ) || ( key == wxT( "q" ) ) )
            {
                m_boPendingQuit = true;
            }
            else if( ( key == wxT( "force" ) ) || ( key == wxT( "f" ) ) )
            {
                m_boForceFirmwareUpdate = true;
            }
            else if( key == wxT( "set_userset_persistence" ) || key == wxT( "sup" ) )
            {
                unsigned long conversionResult = 0;
                if( value.ToULong( &conversionResult ) )
                {
                    // overwrite the value stored by the application (Registry on Windows, a .<nameOfTheApplication> file on other platforms
                    boUserSetPersistence = ( conversionResult != 0 ) ? true : false;
                }
            }
            else if( key != wxT( "hidden" ) )
            {
                commandLineErrors.Append( wxString::Format( wxT( "Invalid command line parameter: '%s'.\n" ), param.c_str() ) );
            }
        }
        processedCommandLineParameters += param;
        processedCommandLineParameters.Append( wxT( ' ' ) );
    }

    DisplayCommandLineProcessingInformation( m_pLogWindow, processedCommandLineParameters, commandLineErrors, GetBoldUnderlinedStyle( m_pLogWindow ) );
    if( !commandLineErrors.empty() )
    {
        m_s_updateResult = DeviceHandler::urInvalidParameter;
    }
    if( !m_devicesToConfigure.empty() )
    {
        WriteLogMessage( wxT( "Devices that await configuration:\n" ), GetBoldUnderlinedStyle( m_pLogWindow ) );
        std::map<wxString, DeviceConfigurationData>::const_iterator it = m_devicesToConfigure.begin();
        const std::map<wxString, DeviceConfigurationData>::const_iterator itEND = m_devicesToConfigure.end();
        while( it != itEND )
        {
            const wxString IDString( it->second.boSetDeviceID_ ? wxString::Format( wxT( "yes(%d)" ), it->second.deviceID_ ) : wxT( "no" ) );
            WriteLogMessage( wxString::Format( wxT( "%s(firmware update: %s, assigning device ID: %s).\n" ),
                                               it->first.c_str(),
                                               it->second.boUpdateFW_ ? wxT( "yes" ) : wxT( "no" ),
                                               IDString.c_str() ) );
            ++it;
        }
        WriteLogMessage( wxT( "\n" ) );
    }

    m_pMISettings_KeepUserSetSettingsAfterFirmwareUpdate->Check( boUserSetPersistence );

    if( m_pMIHelp_AutoCheckForOnlineUpdatesWeekly->IsChecked() &&
        WeekPassedSinceLastUpdatesCheck() && !m_boPendingQuit )
    {
        CheckForOnlineUpdates( true );
    }

    if( !m_devicesToConfigure.empty() ||
#ifdef BUILD_WITH_PROCESSOR_POWER_STATE_CONFIGURATION_SUPPORT
        m_boChangeProcessorIdleStates ||
#endif // #ifdef BUILD_WITH_PROCESSOR_POWER_STATE_CONFIGURATION_SUPPORT
        m_boPendingQuit )
    {
        m_timer.SetOwner( this, teTimer );
        m_timer.Start( TIMER_PERIOD );
    }

    SetSizeHints( defaultRect.GetWidth(), defaultRect.GetHeight() );
    pPanel->SetSizer( pSizer );

    SetSize( rect );
    Maximize( boMaximized );
    UpdateMenu( -1 );
    UpdateDeviceList( false );

    pHorizontalSplitter->SplitHorizontally( m_pDevListCtrl, m_pLogWindow );

    m_listUpdateTimer.SetOwner( this, teListUpdate );
    m_listUpdateTimer.Start( TIMER_PERIOD );

}

//-----------------------------------------------------------------------------
DeviceConfigureFrame::~DeviceConfigureFrame()
//-----------------------------------------------------------------------------
{
    // store the current state of the application
    FramePositionStorage::Save( this );
    if( m_listUpdateTimer.IsRunning() )
    {
        m_listUpdateTimer.Stop();
    }
    if( m_timer.IsRunning() )
    {
        m_timer.Stop();
    }

    if( !m_logFileName.IsEmpty() )
    {
        wxFFile file( m_logFileName, wxT( "w" ) );
        if( file.IsOpened() )
        {
            file.Write( m_pLogWindow->GetValue() );
        }
    }
    // when we e.g. try to write config stuff on a read-only file system the result can
    // be an annoying message box. Therefore we switch off logging now, as otherwise higher level
    // clean up code might produce error messages
    wxLog::EnableLogging( false );
    wxConfigBase* pConfig = wxConfigBase::Get();
    pConfig->Write( wxT( "/MainFrame/Settings/PersistentUserSetSettings" ), m_pMISettings_KeepUserSetSettingsAfterFirmwareUpdate->IsChecked() );
    // make sure the update cycle settings will be stored
    pConfig->Write( wxT( "/MainFrame/Settings/AutoCheckForOnlineUpdatesWeekly" ), m_pMIHelp_AutoCheckForOnlineUpdatesWeekly->IsChecked() );
    if( m_boCheckedForUpdates )
    {
        pConfig->Write( wxT( "/MainFrame/Help/LastCheck" ), wxDateTime::Today().FormatISODate() );
    }
}

//-----------------------------------------------------------------------------
void DeviceConfigureFrame::ActivateDeviceIn_wxPropView( int deviceIndex )
//-----------------------------------------------------------------------------
{
    if( m_pDevListCtrl->GetItemBackgroundColour( deviceIndex ) == acGreyPastel )
    {
        const wxString commandString( wxT( "wxPropView interfaceConfiguration=1" ) );
        ::wxExecute( commandString );
        m_boPendingQuit = true;
        m_timer.SetOwner( this, teTimer );
        m_timer.Start( TIMER_PERIOD );
    }
    else
    {
        const wxString commandString( wxT( "wxPropView device=" ) + ConvertedString( m_devMgr.getDevice( deviceIndex )->serial.read() ) + wxT( " live=1" ) );
        ::wxExecute( commandString );
    }
}

//-----------------------------------------------------------------------------
void DeviceConfigureFrame::BuildList( void )
//-----------------------------------------------------------------------------
{
    typedef map<int, int> IntIntMap;
    typedef pair<int, int> IntIntPair;
    typedef map<string, IntIntMap> String_IntIntMap_Map;
    typedef pair<string, IntIntMap> String_IntIntMap_Pair;
    String_IntIntMap_Map devMap;

    m_lastDevMgrChangedCount = m_devMgr.changedCount();
    m_pDevListCtrl->DeleteAllItems();

#ifdef BUILD_WITH_DIRECT_SHOW_SUPPORT
    DirectShowDeviceManager::DSDeviceList registeredDSDevices;
    m_DSDevMgr.getDeviceList( registeredDSDevices );
#endif // #ifdef BUILD_WITH_DIRECT_SHOW_SUPPORT

    unsigned int devCnt = m_devMgr.deviceCount();
    map<string, int> productFirmwareTable;
    for( unsigned int i = 0; i < devCnt; i++ )
    {
        const string product( m_devMgr.getDevice( i )->product.read() );
        const int firmwareVersion( m_devMgr.getDevice( i )->firmwareVersion.read() );
        map<string, int>::iterator it = productFirmwareTable.find( product );
        if( it == productFirmwareTable.end() )
        {
            productFirmwareTable.insert( make_pair( product, firmwareVersion ) );
        }
        else if( it->second < firmwareVersion )
        {
            it->second = firmwareVersion;
        }
    }

    m_boDevicesOnIgnoredInterfacesDetected = false;
    m_productToFirmwareFilesLocalMap.clear();
    m_updateableDevices.clear();
    for( unsigned int i = 0; i < devCnt; i++ )
    {
        Device* pDev = m_devMgr[i];
        long index = m_pDevListCtrl->InsertItem( i, ConvertedString( pDev->family.read() ) );
        pair<String_IntIntMap_Map::iterator, bool> p = devMap.insert( String_IntIntMap_Pair( pDev->family.read(), IntIntMap() ) );
        if( !p.second )
        {
            IntIntMap::iterator it = p.first->second.find( pDev->deviceID.read() );
            if( it != p.first->second.end() )
            {
                // mark items that have a conflicting device ID
                m_pDevListCtrl->SetItemTextColour( index, *wxRED );
                m_pDevListCtrl->SetItemTextColour( it->second, *wxRED );
                WriteErrorMessage( wxString::Format( wxT( "WARNING: The device in row %ld(%s) has the same ID as the device in row %d(%s) belonging to the same device family. This should be resolved.\n" ), index, ConvertedString( pDev->serial.read() ).c_str(), it->second, ConvertedString( m_devMgr[it->second]->serial.read() ).c_str() ) );
            }
            else
            {
                p.first->second.insert( IntIntPair( pDev->deviceID.read(), index ) );
            }
        }
        else
        {
            p.first->second.insert( IntIntPair( pDev->deviceID.read(), index ) );
        }
        const string product( pDev->product.read() );
        m_pDevListCtrl->SetItem( index, lcProduct, ConvertedString( product ) );
        m_pDevListCtrl->SetItem( index, lcSerial, ConvertedString( pDev->serial.read() ) );
        m_pDevListCtrl->SetItem( index, lcState, ConvertedString( pDev->state.readS() ) );
#ifdef BUILD_WITH_DIRECT_SHOW_SUPPORT
        {
            DirectShowDeviceManager::DSDeviceList::const_iterator it = registeredDSDevices.find( pDev->serial.read() );
            if( it == registeredDSDevices.end() )
            {
                m_pDevListCtrl->SetItem( index, lcDSRegistered, wxString( m_boApplicationHasElevatedRights ? wxT( "no" ) : wxT( "unknown" ) ) );
                m_pDevListCtrl->SetItem( index, lcDSFriendlyName, wxString( m_boApplicationHasElevatedRights ? wxT( "not registered" ) : wxT( "Restart with elevated rights to see this" ) ) );
            }
            else
            {
                m_pDevListCtrl->SetItem( index, lcDSRegistered, wxString( wxT( "yes" ) ) );
                m_pDevListCtrl->SetItem( index, lcDSFriendlyName, ConvertedString( it->second ) );
            }
        }
#endif // #ifdef BUILD_WITH_DIRECT_SHOW_SUPPORT
        m_pDevListCtrl->SetItemData( index, i );
        if( pDev->state.read() != dsPresent )
        {
            m_pDevListCtrl->SetItemBackgroundColour( index, acRedPastel );
        }

        ObjectDeleter<DeviceHandler> pHandler( m_deviceHandlerFactory.CreateObject( ConvertedString( pDev->family.read() ), pDev ) );

        bool boUpdateAvailable = false;
        Version latestFirmwareLocal;
        Version deviceFirmwareVersion = VersionFromString( ConvertedString( pDev->firmwareVersion.readS() ) );
        wxString latestFirmwareS;
        wxString updateNotification;
        if( pHandler.get() )
        {
            int result = DeviceHandler::urFeatureUnsupported;
            if( pHandler->SupportsFirmwareUpdateFromMVU() )
            {
                result = pHandler->GetLatestLocalFirmwareVersionToMap( latestFirmwareLocal, m_productToFirmwareFilesLocalMap );
            }
            else
            {
                result = pHandler->GetLatestLocalFirmwareVersion( latestFirmwareLocal );
            }
            if( result == DeviceHandler::urOperationSuccessful )
            {
                latestFirmwareS = wxString::Format( wxT( "(Version %s)" ), pHandler->GetStringRepresentationOfFirmwareVersion( latestFirmwareLocal ).c_str() );
                updateNotification = wxString::Format( wxT( " (UPDATE AVAILABLE FROM LOCAL ARCHIVE %s)" ), latestFirmwareS.c_str() );
                boUpdateAvailable = true;
            }
            else if( result == DeviceHandler::urFileIOError )
            {
                updateNotification = wxString( wxT( " (No firmware found on local system. Try to check for available online updates.)" ) );
            }
            else
            {
                updateNotification = wxString( wxT( " (Firmware is up to date with local package. Try to check for available online updates.)" ) );
            }

            if( m_boCheckedForUpdates && pHandler->SupportsFirmwareUpdateFromMVU() )
            {
                const map<string, FileEntry>::const_iterator itLatestOnlineFirmwareAvailable = m_productToFirmwareFilesOnlineMap.find( pDev->product.read() );
                const map<string, FileEntry>::const_iterator itLatestOnlineFirmwareAvailableEND = m_productToFirmwareFilesOnlineMap.end();
                if( itLatestOnlineFirmwareAvailable != itLatestOnlineFirmwareAvailableEND )
                {
                    const Version& latestOnlineVersion( itLatestOnlineFirmwareAvailable->second.version_ );
                    if( ( latestOnlineVersion > latestFirmwareLocal ) && pHandler->SupportsFirmwareUpdate() && ( latestOnlineVersion > deviceFirmwareVersion ) )
                    {
                        latestFirmwareS = wxString::Format( wxT( "(Version %s)" ), VersionToString( latestOnlineVersion ).c_str() );
                        updateNotification = wxString::Format( wxT( " (UPDATE AVAILABLE ONLINE %s)" ), latestFirmwareS.c_str() );
                        boUpdateAvailable = true;
                        const UpdateableDevices deviceInfoForUpdateDialog( pDev->serial.read(), deviceFirmwareVersion, pDev->product.read(), itLatestOnlineFirmwareAvailable->second );
                        m_updateableDevices.push_back( deviceInfoForUpdateDialog );
                    }
                    else if( deviceFirmwareVersion >= latestOnlineVersion )
                    {
                        updateNotification = wxString( wxT( " (Latest firmware installed.)" ) );
                        boUpdateAvailable = false;
                    }
                }
                else
                {
                    updateNotification = wxString( wxT( " (Latest firmware installed.)" ) );
                    boUpdateAvailable = false;
                }
            }
            if( deviceFirmwareVersion.major_ == 0 )
            {
                if( DeviceHandlerBlueDevice::IsBlueCOUGAR_XT( pDev ) )
                {
                    updateNotification = wxString::Format( wxT( " (RESCUE MODE FIRMWARE -> UPDATE REQUIRED)" ) );
                    wxTextAttr redStyle( *wxRED );
                    WriteLogMessage( wxString::Format( wxT( "WARNING: The device in row %ld(%s, %s) is running in rescue mode.\n" ), index, ConvertedString( pDev->serial.read() ).c_str(), ConvertedString( pDev->product.read() ).c_str() ), redStyle );
                    WriteLogMessage( wxString::Format( wxT( "Most likely the previous firmware update was interrupted (e.g. due to a power failure). In order to use this device's full feature set, another firmware update must be applied. Until then image acquisition will not be possible!\n%s\n" ), pHandler->GetAdditionalUpdateInformation().c_str() ) );
                }
                else if( DeviceHandlerBlueDevice::IsBlueFOX3( pDev ) )
                {
                    updateNotification = wxString::Format( wxT( " (BOOTPROGRAMMER FIRMWARE -> UPDATE REQUIRED)" ) );
                    wxTextAttr redStyle( *wxRED );
                    WriteLogMessage( wxString::Format( wxT( "WARNING: The device in row %ld(%s, %s) uses a backup firmware.\n" ), index, ConvertedString( pDev->serial.read() ).c_str(), ConvertedString( pDev->product.read() ).c_str() ), redStyle );
                    WriteLogMessage( wxString::Format( wxT( "Most likely the previous firmware update was interrupted (e.g. due to a power failure). In order to use this device's full feature set, another firmware update must be applied. Until then image acquisition will not be possible!\n%s\n" ), pHandler->GetAdditionalUpdateInformation().c_str() ) );
                }
            }
        }

        if( !boUpdateAvailable )
        {
            map<string, int>::const_iterator it = productFirmwareTable.find( product );
            if( it != productFirmwareTable.end() )
            {
                const int firmwareVersion( pDev->firmwareVersion.read() );
                if( ( firmwareVersion < it->second ) && ( makeLowerCase( pDev->firmwareVersion.readS() ) != "unknown" ) )
                {
                    m_pDevListCtrl->SetItemTextColour( index, *wxRED );
                    boUpdateAvailable = true;
                    updateNotification = wxString::Format( wxT( "(UPDATE AVAILABLE BASED ON THE PRESENCE OF A MORE RECENT DEVICE IN THE SYSTEM %s)" ), latestFirmwareS.c_str() );
                }
            }
        }

        m_pDevListCtrl->SetItem( index, lcFWVersion, wxString::Format( wxT( "%s%s" ), ConvertedString( pDev->firmwareVersion.readS() ).c_str(), updateNotification.c_str() ) );
        if( boUpdateAvailable )
        {
            wxTextAttr blueStyle( acDarkBluePastel );
            WriteLogMessage( wxString::Format( wxT( "NOTE: The device in row %ld(%s, %s) uses an outdated firmware version. %s\n" ), index, ConvertedString( pDev->serial.read() ).c_str(), ConvertedString( pDev->product.read() ).c_str(), pHandler->GetAdditionalUpdateInformation().c_str() ), blueStyle );
            m_pDevListCtrl->SetItemBackgroundColour( index, wxColour( acBluePastel ) );
        }

        int currentDMABufferSize_kB = 0;
        if( pHandler.get() && pHandler->SupportsDMABufferSizeUpdate( &currentDMABufferSize_kB ) )
        {
            m_pDevListCtrl->SetItem( index, lcAllocatedDMABuffer, wxString::Format( wxT( "%d kB" ), currentDMABufferSize_kB ) );
        }
        else
        {
            m_pDevListCtrl->SetItem( index, lcAllocatedDMABuffer, wxString( wxT( "unsupported" ) ) );
        }
        if( pDev->state.read() == dsUnreachable )
        {
            m_pDevListCtrl->SetItemTextColour( index, *wxRED );
            WriteErrorMessage( wxString::Format( wxT( "WARNING: device in row %ld(%s, %s)' is currently reported as 'unreachable'. This indicates a compliant device has been detected but for some reasons cannot be used at the moment. For GEV devices this might be a result of an incorrect network configuration (run 'mvIPConfigure' with 'Advanced Device Discovery' enabled to get more information on this). For U3V devices this might be a result of the device being bound to a different (third party) U3V device driver. Refer to the documentation in section 'Troubleshooting' for additional information regarding this issue.\n" ),
                                                 index, ConvertedString( pDev->serial.read() ).c_str(), ConvertedString( pDev->product.read() ).c_str() ) );
        }

        if( supportsValue( pDev->interfaceLayout, dilGenICam ) )
        {
            m_boDevicesOnIgnoredInterfacesDetected |= CheckForDeviceConnectedToIgnoredInterfacesAndMarkGEVConfigurationProblems( pDev, index );
        }
        m_pDevListCtrl->SetItem( index, lcDeviceID, ConvertedString( pDev->deviceID.readS() ) );
    }

    m_pDevListCtrl->SetColumnWidth( lcFamily, ( ( devCnt > 0 ) ? wxLIST_AUTOSIZE : wxLIST_AUTOSIZE_USEHEADER ) );
    m_pDevListCtrl->SetColumnWidth( lcProduct, ( ( devCnt > 0 ) ? wxLIST_AUTOSIZE : wxLIST_AUTOSIZE_USEHEADER ) );
    m_pDevListCtrl->SetColumnWidth( lcSerial, ( ( devCnt > 0 ) ? wxLIST_AUTOSIZE : wxLIST_AUTOSIZE_USEHEADER ) );
    m_pDevListCtrl->SetColumnWidth( lcState, ( ( devCnt > 0 ) ? wxLIST_AUTOSIZE : wxLIST_AUTOSIZE_USEHEADER ) );
    m_pDevListCtrl->SetColumnWidth( lcFWVersion, wxLIST_AUTOSIZE_USEHEADER );
    m_pDevListCtrl->SetColumnWidth( lcDeviceID, wxLIST_AUTOSIZE_USEHEADER );
    m_pDevListCtrl->SetColumnWidth( lcAllocatedDMABuffer, wxLIST_AUTOSIZE_USEHEADER );
#ifdef BUILD_WITH_DIRECT_SHOW_SUPPORT
    m_pDevListCtrl->SetColumnWidth( lcDSRegistered, wxLIST_AUTOSIZE_USEHEADER );
    m_pDevListCtrl->SetColumnWidth( lcDSFriendlyName, wxLIST_AUTOSIZE_USEHEADER );
#endif // #ifdef BUILD_WITH_DIRECT_SHOW_SUPPORT
    WriteLogMessage( wxT( "\n" ) );
}

//-----------------------------------------------------------------------------
bool DeviceConfigureFrame::CheckForDeviceConnectedToIgnoredInterfacesAndMarkGEVConfigurationProblems( Device* pDev, long index )
//-----------------------------------------------------------------------------
{
    const bool boIgnoreGEVInterfaces = m_ignoredInterfacesInfo[string( "GEV" )] == string( "1" );
    const bool boIgnoreU3VInterfaces = m_ignoredInterfacesInfo[string( "U3V" )] == string( "1" );
    int devicesOnIgnoredInterfacesCnt = 0;
    // find the GenTL interface this device claims to be connected to
    PropertyI64 interfaceID;
    PropertyI64 deviceMACAddress;
    DeviceComponentLocator locator( pDev->hDev() );
    locator.bindComponent( interfaceID, "InterfaceID" );
    locator.bindComponent( deviceMACAddress, "DeviceMACAddress" );
    if( interfaceID.isValid() && deviceMACAddress.isValid() )
    {
        const string interfaceDeviceHasBeenFoundOn( interfaceID.readS() );
        const int64_type devMACAddress( deviceMACAddress.read() );
        const int64_type systemModuleCount = mvIMPACT::acquire::GenICam::SystemModule::getSystemModuleCount();
        for( int64_type systemModuleIndex = 0; systemModuleIndex < systemModuleCount; systemModuleIndex++ )
        {
            try
            {
                mvIMPACT::acquire::GenICam::SystemModule sm( systemModuleIndex );
                // always run this code as new interfaces might appear at runtime e.g. when plugging in a network cable...
                const int64_type interfaceCount( sm.getInterfaceModuleCount() );
                for( int64_type interfaceIndex = 0; interfaceIndex < interfaceCount; interfaceIndex++ )
                {
                    mvIMPACT::acquire::GenICam::InterfaceModule im( sm, interfaceIndex );
                    if( !im.interfaceID.isValid() ||
                        !im.interfaceType.isValid() ||
                        !im.deviceSelector.isValid() )
                    {
                        // most likely this is a 3rd party producer. As we cannot obtain the
                        // information we are after just skip this interface!
                        continue;
                    }

                    if( interfaceDeviceHasBeenFoundOn != im.interfaceID.readS() )
                    {
                        continue;
                    }

                    const string interfaceDeviceUpdateListBehaviour = m_ignoredInterfacesInfo[interfaceDeviceHasBeenFoundOn];
                    const string interfaceType( im.interfaceType.readS() );
                    if( ( boIgnoreGEVInterfaces && ( ( interfaceType == "GEV" ) || ( interfaceType == "GigEVision" ) ) && ( interfaceDeviceUpdateListBehaviour != "ForceEnumerate" ) ) ||
                        ( boIgnoreU3VInterfaces && ( ( interfaceType == "U3V" ) || ( interfaceType == "USB3Vision" ) ) && ( interfaceDeviceUpdateListBehaviour != "ForceEnumerate" ) ) ||
                        ( interfaceDeviceUpdateListBehaviour == "ForceIgnore" ) )
                    {
                        m_pDevListCtrl->SetItemBackgroundColour( index, wxColour( acGreyPastel ) );
                        m_pDevListCtrl->SetItem( index, lcState, ConvertedString( pDev->state.readS() + string( "(Interface Ignored)" ) ) );
                        devicesOnIgnoredInterfacesCnt++;
                    }

                    if( ( interfaceType != "GEV" ) &&
                        ( interfaceType != "GigEVision" ) )
                    {
                        continue;
                    }

                    if( !im.gevInterfaceSubnetSelector.isValid() ||
                        !im.gevDeviceMACAddress.isValid() ||
                        !im.gevDeviceSubnetMask.isValid() ||
                        !im.gevDeviceIPAddress.isValid() ||
                        !im.gevInterfaceSubnetIPAddress.isValid() )
                    {
                        // most likely this is a 3rd party producer. As we cannot obtain the
                        // information we are after just skip this interface!
                        continue;
                    }

                    // this is the interface we are looking for
                    const int64_type deviceCount( im.deviceSelector.getMaxValue() + 1 );
                    const int64_type interfaceSubnetCount( im.gevInterfaceSubnetSelector.getMaxValue() + 1 );
                    for( int64_type deviceIndex = 0; deviceIndex < deviceCount; deviceIndex++ )
                    {
                        im.deviceSelector.write( deviceIndex );
                        if( im.gevDeviceMACAddress.read() != devMACAddress )
                        {
                            continue;
                        }

                        // this is the device we are looking for
                        bool boDeviceCorrectlyConfigured = false;
                        const int64_type netMask1 = im.gevDeviceSubnetMask.read();
                        const int64_type net1 = im.gevDeviceIPAddress.read();
                        for( int64_type interfaceSubnetIndex = 0; interfaceSubnetIndex < interfaceSubnetCount; interfaceSubnetIndex++ )
                        {
                            im.gevInterfaceSubnetSelector.write( interfaceSubnetIndex );
                            const int64_type netMask2 = im.gevInterfaceSubnetMask.read();
                            const int64_type net2 = im.gevInterfaceSubnetIPAddress.read();
                            if( ( netMask1 == netMask2 ) &&
                                ( ( net1 & netMask1 ) == ( net2 & netMask2 ) ) )
                            {
                                boDeviceCorrectlyConfigured = true;
                                break;
                            }
                        }
                        if( !boDeviceCorrectlyConfigured )
                        {
                            m_pDevListCtrl->SetItemTextColour( index, *wxRED );
                            WriteErrorMessage( wxString::Format( wxT( "WARNING: GigE Vision device '%s' in row %ld will currently not work properly as its network configuration is invalid in the current setup. Run 'mvIPConfigure' with 'Advanced Device Discovery' enabled to get more information on this).\n" ),
                                                                 ConvertedString( im.deviceID.readS() ).c_str(), index ) );
                        }
                        break;
                    }
                    break;
                }
            }
            catch( const ImpactAcquireException& e )
            {
                WriteErrorMessage( wxString::Format( wxT( "Failed to configure GEV interfaces of system module %u for 'advanced device discovery'. %s(numerical error representation: %d (%s)). The associated GenTL producer might not support a GenICam file for the System-module and therefore the required properties are not available.\n" ), static_cast<unsigned int>( systemModuleIndex ), ConvertedString( e.getErrorString() ).c_str(), e.getErrorCode(), ConvertedString( e.getErrorCodeAsString() ).c_str() ) );
            }
        }
    }
    return devicesOnIgnoredInterfacesCnt != 0;
}

//-----------------------------------------------------------------------------
void DeviceConfigureFrame::CheckForOnlineUpdates( const bool boSilent )
//-----------------------------------------------------------------------------
{
    SocketInitializeScope socketInitializeScope;
    // Contact MATRIX VISION and download some xml files on a separate thread
    static const int MAX_UPDATE_TIME_MILLISECONDS = 5000;

    wxProgressDialog dlg( wxT( "Checking For New Releases" ),
                          wxT( "Contacting MATRIX VISION Servers..." ),
                          MAX_UPDATE_TIME_MILLISECONDS, // range
                          this,     // parent
                          wxPD_AUTO_HIDE | wxPD_APP_MODAL | wxPD_ELAPSED_TIME );
    dlg.Show();
    wxString updateInfoXML;
    ContactMatrixVisionServerThread getProductXmlListThread( wxString( wxT( "http://static.matrix-vision.com/Firmware/UpdateInformation/UpdateInfo.xml" ) ), updateInfoXML, wxTHREAD_JOINABLE, static_cast< unsigned int >( MAX_UPDATE_TIME_MILLISECONDS / 1000 ) );
    getProductXmlListThread.Run();
    getProductXmlListThread.Wait();
    if( DownloadContentHasErrors( updateInfoXML, boSilent, this ) )
    {
        return;
    }
    ProductUpdateInfoURLList allAvailableUpdateURLs = GetProductUpdateInfoURLsFromXML( updateInfoXML );
    ProductUpdateInfoURLList::const_iterator it = allAvailableUpdateURLs.begin();
    const ProductUpdateInfoURLList::const_iterator itEND = allAvailableUpdateURLs.end();
    while( it != itEND )
    {
        dlg.Pulse();
        wxString productInfoXML;
        ContactMatrixVisionServerThread getProductInfoXmlThread( wxString( it->second ), productInfoXML, wxTHREAD_JOINABLE, static_cast< unsigned int >( MAX_UPDATE_TIME_MILLISECONDS / 1000 ) );
        getProductInfoXmlThread.Run();
        getProductInfoXmlThread.Wait();
        if( DownloadContentHasErrors( productInfoXML, boSilent, this ) )
        {
            continue;
        }
        m_productUpdateInfoListOnline.push_back( GetProductUpdateInfosFromXML( it->first, productInfoXML ) );
        it++;
    }
    m_onlineFirmwareVersions.clear();
    CollectOnlineFirmwareVersions();
    FindFirmwarePacketsForPresentDevices( m_onlineFirmwareVersions );
    m_boCheckedForUpdates = true;
    dlg.Update( MAX_UPDATE_TIME_MILLISECONDS );
    UpdateDeviceList();
}

//-----------------------------------------------------------------------------
void DeviceConfigureFrame::CollectOnlineFirmwareVersions( void )
//-----------------------------------------------------------------------------
{
    static const int MAX_UPDATE_TIME_MILLISECONDS = 50000;
    vector<ProductUpdateURLs>::const_iterator itUpdateInfoList = m_productUpdateInfoListOnline.begin();
    const vector<ProductUpdateURLs>::const_iterator itUpdateInfoListEnd = m_productUpdateInfoListOnline.end();
    while( itUpdateInfoList != itUpdateInfoListEnd )
    {
        wxString XmlContent;
        {
            SocketInitializeScope socketInitializeScope;
            ContactMatrixVisionServerThread fwDownloadThread( itUpdateInfoList->packageDescriptionUrl_, XmlContent, wxTHREAD_JOINABLE, static_cast< unsigned int >( MAX_UPDATE_TIME_MILLISECONDS / 1000 ) );
            fwDownloadThread.Run();
            fwDownloadThread.Wait();
        }
        PackageDescriptionFileParser parser;
        const string downloadedFileContentANSI( XmlContent.mb_str() );
        if( DeviceHandlerBlueDevice::ParsePackageDescriptionFromBuffer( parser, downloadedFileContentANSI.c_str(), downloadedFileContentANSI.length(), this ) == DeviceHandlerBlueDevice::urOperationSuccessful )
        {
            FileEntryContainer resultContainer = parser.GetResults();
            if( !resultContainer.empty() )
            {
                FileEntryContainer::const_iterator it = resultContainer.begin();
                const FileEntryContainer::const_iterator itEND = resultContainer.end();
                while( it != itEND )
                {
                    FileEntry entryToAdd( *it );
                    entryToAdd.boFromOnline_ = true;
                    entryToAdd.onlineUpdateInfos_ = *itUpdateInfoList;
                    m_onlineFirmwareVersions.push_back( entryToAdd );
                    it++;
                }
            }
        }
        itUpdateInfoList++;
    }
}

//-----------------------------------------------------------------------------
void DeviceConfigureFrame::DownloadFWUpdateFile( wxString& linkToFWFile )
//-----------------------------------------------------------------------------
{
    WriteLogMessage( wxString::Format( wxT( "Starting download of firmware file %s.\n" ), linkToFWFile.AfterLast( wxT( '/' ) ) ) );
    {
        SocketInitializeScope socketInitializeScope;

        // Contact MATRIX VISION and download the latest firmware file for the selected device on a separate thread
        static const int MAX_UPDATE_TIME_MILLISECONDS = 60000;
        static const int UPDATE_THREAD_PROGRESS_INTERVAL_MILLISECONDS = 1000;

        wxProgressDialog dlg( wxT( "Downloading New Firmware Release" ),
                              wxT( "Contacting MATRIX VISION Servers..." ),
                              MAX_UPDATE_TIME_MILLISECONDS, // range
                              this,     // parent
                              wxPD_AUTO_HIDE | wxPD_APP_MODAL | wxPD_ELAPSED_TIME );

        ContactMatrixVisionServerThread fwDownloadThread( linkToFWFile, m_downloadedFileContent, wxTHREAD_JOINABLE, static_cast< unsigned int >( MAX_UPDATE_TIME_MILLISECONDS / 1000 ) );
        fwDownloadThread.Run();
        while( fwDownloadThread.IsRunning() )
        {
            wxMilliSleep( UPDATE_THREAD_PROGRESS_INTERVAL_MILLISECONDS );
            dlg.Pulse( wxT( "Downloading firmware file... " ) );
        }
        fwDownloadThread.Wait();
        dlg.Update( MAX_UPDATE_TIME_MILLISECONDS );
    }
    WriteLogMessage( wxString::Format( wxT( "Done.\n" ) ) );
}

//-----------------------------------------------------------------------------
void DeviceConfigureFrame::FindFirmwarePacketsForPresentDevices( FileEntryContainer& m_onlineFirmwareVersions )
//-----------------------------------------------------------------------------
{
    for( unsigned int i = 0; i < m_devMgr.deviceCount(); i++ )
    {
        Device* pDev = m_devMgr.getDevice( i );
        ObjectDeleter<DeviceHandler> pHandler( m_deviceHandlerFactory.CreateObject( ConvertedString( pDev->family.read() ), pDev ) );
        if( pHandler.get() && !m_onlineFirmwareVersions.empty() )
        {
            Version latestFWVersionAvailableOnline;
            size_t fileEntryPosition = 0;
            if( pHandler->GetLatestFirmwareFromList( latestFWVersionAvailableOnline, m_onlineFirmwareVersions, fileEntryPosition ) != DeviceHandler::urInvalidFileSelection )
            {
                m_productToFirmwareFilesOnlineMap.insert( make_pair( pDev->product.read(), m_onlineFirmwareVersions[fileEntryPosition] ) );
            }
        }
    }
}

//-----------------------------------------------------------------------------
void DeviceConfigureFrame::GetConfigurationEntriesFromFile( const wxString& fileName )
//-----------------------------------------------------------------------------
{
    std::ifstream file( fileName.mb_str() );
    if( !file.good() )
    {
        WriteErrorMessage( wxString::Format( wxT( "Could not open file '%s' for obtaining device update information." ), fileName.c_str() ) );
        return;
    }

    while( !file.eof() )
    {
        std::string l;
        getline( file, l );
        // remove all white-spaces, TABs and newlines and also just consider everything until the first ',' to get the product name as the file
        // format also supports to specify additional parameters after a ',' (e.g. 'mvBlueCOUGAR-XD126aC, serial=GX200376, linkspeed=1000000000, firmwareFile=mvBlueCOUGAR-X.mvu')
        const wxString line = ConvertedString( l ).Strip( wxString::both );
        if( line.StartsWith( wxT( "//" ) ) || line.StartsWith( wxT( "#" ) ) )
        {
            continue;
        }

        const wxString product = line.BeforeFirst( wxT( ',' ) );
        if( product.IsEmpty() )
        {
            continue;
        }

        if( !product.Contains( wxT( "mvBlueCOUGAR-S" ) ) )
        {
            std::map<wxString, DeviceConfigurationData>::iterator it = GetConfigurationEntry( product );
            it->second.boUpdateFW_ = true;
            wxStringTokenizer tokenizer( line.AfterFirst( wxT( ',' ) ), wxString( wxT( "," ) ), wxTOKEN_STRTOK );
            while( tokenizer.HasMoreTokens() )
            {
                const wxString token( tokenizer.GetNextToken().Strip( wxString::both ) );
                const wxString key = token.BeforeFirst( wxT( '=' ) ).MakeLower();
                if( key == wxT( "firmwarefile" ) )
                {
                    it->second.customFirmwareFileName_ = token.AfterFirst( wxT( '=' ) );
                }
                else if( key == wxT( "serial" ) )
                {
                    it->second.serials_.push_back( string( token.AfterFirst( wxT( '=' ) ).mb_str() ) );
                }
            }
        }
        else if( product.StartsWith( wxT( "mvBlueCOUGAR-S" ) ) == true )
        {
            WriteErrorMessage( wxString::Format( wxT( "Devices of type '%s' do not support this update mechanism as there is no way to check if the firmware on the device is the same as the one in the update archive and this could result in a lot of redundant update cycles in an automatic environment.\n" ), product.c_str() ) );
        }
    }
}

//-----------------------------------------------------------------------------
std::map<wxString, DeviceConfigureFrame::DeviceConfigurationData>::iterator DeviceConfigureFrame::GetConfigurationEntry( const wxString& value )
//-----------------------------------------------------------------------------
{
    std::map<wxString, DeviceConfigurationData>::iterator it = m_devicesToConfigure.find( value );
    if( it == m_devicesToConfigure.end() )
    {
        it = m_devicesToConfigure.insert( make_pair( value, DeviceConfigurationData() ) ).first;
    }
    return it;
}

//-----------------------------------------------------------------------------
void DeviceConfigureFrame::GetDeviceDescription( wxString& descriptionFileName, wxString& newestVersionDate, wxString lineToExtract, wxString packageDescriptionVersionPattern, wxString deviceDescriptionFilePattern )
//-----------------------------------------------------------------------------
{
    wxRegEx reVersionLine( lineToExtract );
    if( reVersionLine.Matches( m_downloadedFileContent ) )
    {
        wxString versionLine = reVersionLine.GetMatch( m_downloadedFileContent );
        wxRegEx reVersion( packageDescriptionVersionPattern );
        if( reVersion.Matches( versionLine ) )
        {
            newestVersionDate = reVersion.GetMatch( versionLine );
        }
        wxRegEx reVersionPackageName( deviceDescriptionFilePattern );
        if( reVersionPackageName.Matches( versionLine ) )
        {
            descriptionFileName = reVersionPackageName.GetMatch( versionLine );
        }
    }
}
//-----------------------------------------------------------------------------
void DeviceConfigureFrame::GetDeviceFirmwareFileName( wxString& devcieFirmwareFileName, wxString lineToExtract, wxString firmwareFilePattern )
//-----------------------------------------------------------------------------
{
    wxRegEx reFirmwareFileNameLine( lineToExtract );
    if( reFirmwareFileNameLine.Matches( m_downloadedFileContent ) )
    {
        wxString firmwareFileNameLine = reFirmwareFileNameLine.GetMatch( m_downloadedFileContent );
        wxRegEx reFirmwareFileName( firmwareFilePattern );
        if( reFirmwareFileName.Matches( firmwareFileNameLine ) )
        {
            devcieFirmwareFileName = reFirmwareFileName.GetMatch( firmwareFileNameLine );
        }
    }
}

//-----------------------------------------------------------------------------
vector<pair<std::string, std::string> > DeviceConfigureFrame::GetProductUpdateInfoURLsFromXML( const wxString& updateInfoXML ) const
//-----------------------------------------------------------------------------
{
    vector<pair<std::string, std::string> > descriptionURLList;
    wxStringInputStream xmlStream( updateInfoXML );
    wxXmlDocument xmlFile( xmlStream );
    if( xmlFile.GetRoot()->GetName() != "UpdateInfo" )
    {
        return descriptionURLList;
    }
    wxXmlNode* include = xmlFile.GetRoot()->GetChildren();
    while( include )
    {
        if( include->GetName() == "Include" )
        {
            descriptionURLList.push_back( make_pair( include->GetAttribute( "deviceFamily" ).ToStdString(), include->GetAttribute( "url" ).ToStdString() ) );
        }
        include = include->GetNext();
    }
    return descriptionURLList;
}

//-----------------------------------------------------------------------------
ProductUpdateURLs DeviceConfigureFrame::GetProductUpdateInfosFromXML( const wxString& productFamily, const wxString& productXML ) const
//-----------------------------------------------------------------------------
{
    ProductUpdateURLs productInfos;
    wxStringInputStream xmlStream( productXML );
    wxXmlDocument xmlFile( xmlStream );
    if( xmlFile.GetRoot()->GetName() != "Product" )
    {
        return productInfos;
    }
    wxXmlNode* update = xmlFile.GetRoot()->GetChildren();
    if( update->GetName() == "Update" )
    {
        productInfos.timestamp_ = update->GetAttribute( wxT( "timestamp" ) );
        productInfos.productFamily_ = productFamily;
        wxXmlNode* urls = update->GetChildren();
        while( urls )
        {
            if( urls->GetName() == "UpdateFile" )
            {
                productInfos.updateArchiveUrl_ = urls->GetAttribute( wxT( "url" ) );
                urls = urls->GetNext();
                continue;
            }
            if( urls->GetName() == "ReleaseNotes" )
            {
                productInfos.releaseNotesUrl_ = urls->GetAttribute( wxT( "url" ) );
                urls = urls->GetNext();
                continue;
            }
            if( urls->GetName() == "PackageDescription" )
            {
                productInfos.packageDescriptionUrl_ = urls->GetAttribute( wxT( "url" ) );
            }
            urls = urls->GetNext();
        }
    }
    return productInfos;
}

//-----------------------------------------------------------------------------
void DeviceConfigureFrame::OnHelp_About( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    AboutDialogInformation info( GetIcon(), this );
    info.applicationName_ = wxT( "mvDeviceConfigure" );
    info.briefDescription_ = wxString::Format( wxT( "Configuration tool for %s devices(firmware updates, log-output configuration, etc.)" ), COMPANY_NAME );
    info.yearOfInitialRelease_ = 2005;
    info.boAddExpatInfo_ = true;

    const wxTextAttr boldStyle( GetBoldUnderlinedStyle( m_pLogWindow ) );
    info.usageHints_.push_back( make_pair( wxT( "Right click on a device entry to get a menu with available options for the selected device\n" ), wxTextAttr( *wxBLACK ) ) );
    info.usageHints_.push_back( make_pair( wxT( "If a menu entry is disabled the underlying feature is not available for the selected device.\n" ), wxTextAttr( *wxBLACK ) ) );
    info.usageHints_.push_back( make_pair( wxT( "\n" ), wxTextAttr( *wxBLACK ) ) );
    info.usageHints_.push_back( make_pair( wxT( "Double click on a list entry to start live acquisition from this device in a new instance of wxPropView (wxPropView must be locatable via " ), wxTextAttr( *wxBLACK ) ) );
    info.usageHints_.push_back( make_pair( wxT( "the systems path or must reside in the same directory as mvDeviceConfigure!)\n" ), wxTextAttr( *wxBLACK ) ) );
    info.usageHints_.push_back( make_pair( wxT( "\n" ), wxTextAttr( *wxBLACK ) ) );
    info.usageHints_.push_back( make_pair( wxT( "Menu entries under 'Action' will be enabled and disabled whenever the currently selected device changes.\n" ), wxTextAttr( *wxBLACK ) ) );
    info.usageHints_.push_back( make_pair( wxT( "\n" ), wxTextAttr( *wxBLACK ) ) );
    info.usageHints_.push_back( make_pair( wxT( "To modify the way log output is generated select 'Action -> Configure Log Output'.\n" ), wxTextAttr( *wxBLACK ) ) );
    info.usageHints_.push_back( make_pair( wxT( "\n" ), wxTextAttr( *wxBLACK ) ) );
    info.usageHints_.push_back( make_pair( wxT( "The number of commands that can be passed to the application is not limited.\n" ), wxTextAttr( *wxBLACK ) ) );
    info.usageHints_.push_back( make_pair( wxT( "'*' can be used as a wildcard, devices will be searched by serial number AND by product. The application will first try to locate a device with a serial number matching the specified string and then (if no suitable device is found) a device with a matching product string.\n" ), wxTextAttr( *wxBLACK ) ) );
    info.usageHints_.push_back( make_pair( wxT( "\n" ), wxTextAttr( *wxBLACK ) ) );
    info.usageHints_.push_back( make_pair( wxT( "Usage examples:\n" ), boldStyle ) );
    info.usageHints_.push_back( make_pair( wxT( "mvDeviceConfigure ufw=BF000666 (will update the firmware of a mvBlueFOX with the serial number BF000666)\n" ), wxTextAttr( *wxBLACK ) ) );
    info.usageHints_.push_back( make_pair( wxT( "mvDeviceConfigure update_fw=BF* (will update the firmware of ALL mvBlueFOX devices in the current system)\n" ), wxTextAttr( *wxBLACK ) ) );
    info.usageHints_.push_back( make_pair( wxT( "mvDeviceConfigure update_fw=mvBlueFOX-2* lf=output.txt quit (will update the firmware of ALL mvBlueFOX-2 devices in the current system, then will store a log file of the executed operations and afterwards will terminate the application.)\n" ), wxTextAttr( *wxBLACK ) ) );
    info.usageHints_.push_back( make_pair( wxT( "mvDeviceConfigure setid=BF000666.5 (will assign the device ID '5' to a mvBlueFOX with the serial number BF000666)\n" ), wxTextAttr( *wxBLACK ) ) );
    info.usageHints_.push_back( make_pair( wxT( "mvDeviceConfigure ufw=* (will update the firmware of every device in the system)\n" ), wxTextAttr( *wxBLACK ) ) );
    info.usageHints_.push_back( make_pair( wxT( "mvDeviceConfigure ufw=BF000666 ufw=BF000667 (will update the firmware of 2 mvBlueFOX cameras)\n" ), wxTextAttr( *wxBLACK ) ) );
    info.usageHints_.push_back( make_pair( wxT( "mvDeviceConfigure ipv4_mask=169.254.*;192.168.100* update_fw=GX* (will update the firmware of all mvBlueCOUGAR-X devices with a valid IPv4 address that starts with '169.254.' or '192.168.100.')\n" ), wxTextAttr( *wxBLACK ) ) );
    info.usageHints_.push_back( make_pair( wxT( "mvDeviceConfigure igev=1 (will disable enumerating devices on all GEV interfaces of the system)\n" ), wxTextAttr( *wxBLACK ) ) );

    info.keyboardShortcuts_.push_back( make_pair( wxT( "CTRL+A" ), wxT( "Update Firmware From Online Source For All Devices" ) ) );
    info.keyboardShortcuts_.push_back( make_pair( wxT( "CTRL+C" ), wxT( "Configure Log Output" ) ) );
    info.keyboardShortcuts_.push_back( make_pair( wxT( "CTRL+D" ), wxT( "Update Permanent DMA Buffer" ) ) );
    info.keyboardShortcuts_.push_back( make_pair( wxT( "CTRL+E" ), wxT( "Get Latest Firmware Information From Over Internet" ) ) );
    info.keyboardShortcuts_.push_back( make_pair( wxT( "CTRL+F" ), wxT( "Update Firmware" ) ) );
#ifdef BUILD_WITH_DIRECT_SHOW_SUPPORT
    info.keyboardShortcuts_.push_back( make_pair( wxT( "ALT+CTRL+F" ), wxT( "Set Friendly Name" ) ) );
#endif // #ifdef BUILD_WITH_DIRECT_SHOW_SUPPORT
#ifdef BUILD_WITH_PROCESSOR_POWER_STATE_CONFIGURATION_SUPPORT
    if( m_pMISettings_CPUIdleStatesEnabled )
    {
        info.keyboardShortcuts_.push_back( make_pair( wxT( "CTRL+I" ), wxT( "Enable/Disable CPU Idle states" ) ) );
    }
#endif // #ifdef BUILD_WITH_PROCESSOR_POWER_STATE_CONFIGURATION_SUPPORT
    info.keyboardShortcuts_.push_back( make_pair( wxT( "CTRL+O" ), wxT( "Update Firmware From Online Source" ) ) );
    info.keyboardShortcuts_.push_back( make_pair( wxT( "CTRL+P" ), wxT( "Enable/Disable Persistent UserSet Settings" ) ) );
#ifdef BUILD_WITH_DIRECT_SHOW_SUPPORT
    info.keyboardShortcuts_.push_back( make_pair( wxT( "CTRL+R" ), wxT( "Register All Devices(for DirectShow)" ) ) );
#endif // #ifdef BUILD_WITH_DIRECT_SHOW_SUPPORT
    info.keyboardShortcuts_.push_back( make_pair( wxT( "CTRL+S" ), wxT( "Set ID" ) ) );
#ifdef BUILD_WITH_DIRECT_SHOW_SUPPORT
    info.keyboardShortcuts_.push_back( make_pair( wxT( "CTRL+U" ), wxT( "Unregister All Devices(for DirectShow)" ) ) );
#endif // #ifdef BUILD_WITH_DIRECT_SHOW_SUPPORT
    info.keyboardShortcuts_.push_back( make_pair( wxT( "ALT+X" ), wxT( "Exit" ) ) );
    info.keyboardShortcuts_.push_back( make_pair( wxT( "F1" ), wxT( "Display 'About' dialog" ) ) );
    info.keyboardShortcuts_.push_back( make_pair( wxT( "F5" ), wxT( "Update device list" ) ) );
    info.keyboardShortcuts_.push_back( make_pair( wxT( "F12" ), wxT( "Online Documentation" ) ) );

    info.availableCommandLineOptions_.push_back( make_pair( wxT( "'custom_genicam_file' or 'cgf'" ), wxT( "Specifies a custom GenICam file to be used to open devices for firmware updates. This can be useful when the actual XML on the device is damaged/invalid" ) ) );
    info.availableCommandLineOptions_.push_back( make_pair( wxT( "'force' or 'f'" ), wxT( "Will force a firmware update in unattended mode, even if it isn't a newer version" ) ) );
    info.availableCommandLineOptions_.push_back( make_pair( wxT( "'fw_file'" ), wxT( "Specifies a custom name for the firmware file to use( you can pass 'online' to use the latest version from the MATRIX VISION website(fw_file=online))" ) ) );
    info.availableCommandLineOptions_.push_back( make_pair( wxT( "'fw_path'" ), wxT( "Specifies a custom path for the firmware files" ) ) );
    info.availableCommandLineOptions_.push_back( make_pair( wxT( "'hidden'" ), wxT( "Will start the application without showing the splash screen or the application window. This might be useful when doing a silent configuration in combination with the 'quit' parameter" ) ) );
    info.availableCommandLineOptions_.push_back( make_pair( wxT( "'ignore_gev' or 'igev'" ), wxT( "Disables enumerating devices on all GEV interfaces of the system, for all applications using a MATRIX VISION GenTL producer(syntax: 'igev=1' or 'igev=0')" ) ) );
    info.availableCommandLineOptions_.push_back( make_pair( wxT( "'ignore_pci' or 'ipci'" ), wxT( "Disables enumerating devices on all PCI/PCIe interfaces of the system, for all applications using a MATRIX VISION GenTL producer(syntax: 'ipci=1' or 'ipci=0')" ) ) );
    info.availableCommandLineOptions_.push_back( make_pair( wxT( "'ignore_u3v' or 'iu3v'" ), wxT( "Disables enumerating devices on all U3V interfaces of the system, for all applications using a MATRIX VISION GenTL Producer(syntax: 'iu3v=1' or 'iu3v=0')" ) ) );
    info.availableCommandLineOptions_.push_back( make_pair( wxT( "'ipv4_mask'" ), wxT( "Specifies an IPv4 address mask to use as a filter for the selected update operations. Multiple masks can be passed here separated by semicolons" ) ) );
    info.availableCommandLineOptions_.push_back( make_pair( wxT( "'log_file' or 'lf'" ), wxT( "Specifies a log file storing the content of this text control upon application shutdown" ) ) );
    info.availableCommandLineOptions_.push_back( make_pair( wxT( "'quit' or 'q'" ), wxT( "Will end the application automatically after all updates have been applied" ) ) );
#ifdef BUILD_WITH_PROCESSOR_POWER_STATE_CONFIGURATION_SUPPORT
    info.availableCommandLineOptions_.push_back( make_pair ( wxT( "'set_processor_idle_states' or 'spis'" ), wxT( "to change the C1, C2 and C3 states for ALL processors in the current system(syntax: 'spis=1' or 'spis=0')" ) ) );
#endif // #ifdef BUILD_WITH_PROCESSOR_POWER_STATE_CONFIGURATION_SUPPORT
    info.availableCommandLineOptions_.push_back( make_pair( wxT( "'set_userset_persistence' or 'sup' " ), wxT( "Sets the persistency of UserSet settings during firmware updates (syntax: 'sup=1' or 'sup=0')" ) ) );
    info.availableCommandLineOptions_.push_back( make_pair( wxT( "'setid' or 'id'" ), wxT( "to update the firmware of one or many devices(syntax: 'id=<serial>.<id>' or 'id=<product>.<id>')" ) ) );
    info.availableCommandLineOptions_.push_back( make_pair( wxT( "'update_fw' or 'ufw'" ), wxT( "Updates the firmware of one or many devices(syntax: ufw=<serial> of ufw=<product>, e.g.: ufw=mvBlueFO*" ) ) );
    info.availableCommandLineOptions_.push_back( make_pair( wxT( "'update_fw_file' or 'ufwf'" ), wxT( "Updates the firmware of one or many devices. Pass a full path to a text file that contains a serial number or a product type per line" ) ) );

    DisplayCommonAboutDialog( info );
}

//-----------------------------------------------------------------------------
void DeviceConfigureFrame::OnCheckForOnlineUpdatesNow( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    CheckForOnlineUpdates( false );
    if( m_updateableDevices.size() > 0 )
    {
        UpdatesInformationDlg dlg( this, wxT( "Update Info" ), m_updateableDevices, m_pMIHelp_AutoCheckForOnlineUpdatesWeekly->IsChecked() );
        dlg.ShowModal();
        m_pMIHelp_AutoCheckForOnlineUpdatesWeekly->Check( dlg.IsWeeklyAutoCheckForOnlineUpdatesActive() );
    }
    else
    {
        wxMessageDialog selectDlg( NULL, wxT( "The latest firmware package is already available on this system. It can be used to update the corresponding devices using the 'Action'->'UpdateFirmware' menu." ), wxT( "No new firmware available" ), wxOK | wxICON_INFORMATION );
        selectDlg.ShowModal();
    }
    UpdateDeviceList();
}

//-----------------------------------------------------------------------------
void DeviceConfigureFrame::OnConfigureLogOutput( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    LogOutputHandlerDlg dlg( this, m_devMgr, m_debugData );
    dlg.ShowModal();
}

//-----------------------------------------------------------------------------
void DeviceConfigureFrame::OnQuit( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    // true is to force the frame to close
    Close( true );
}

//-----------------------------------------------------------------------------
void DeviceConfigureFrame::OnSetID( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    SetID( m_pDevListCtrl->GetCurrentItemIndex() );
}

//-----------------------------------------------------------------------------
void DeviceConfigureFrame::OnTimer( wxTimerEvent& e )
//-----------------------------------------------------------------------------
{
    switch( e.GetId() )
    {
    case teListUpdate:
        if( m_lastDevMgrChangedCount == m_devMgr.changedCount() )
        {
            return;
        }
        BuildList();
        break;
    case teTimer:
        m_timer.Stop();
        if( !m_devicesToConfigure.empty() )
        {
            std::map<wxString, DeviceConfigurationData>::const_iterator it = m_devicesToConfigure.begin();
            const std::map<wxString, DeviceConfigurationData>::const_iterator itEND = m_devicesToConfigure.end();
            while( it != itEND )
            {
                const string searchTokenANSI( it->first.mb_str() );
                if( it->second.serials_.empty() )
                {
                    if( !PerformDeviceConfiguration( searchTokenANSI, it->second ) )
                    {
                        WriteErrorMessage( wxString::Format( wxT( "No device found, that matches the search criterion %s.\n" ), it->first.c_str() ) );
                    }
                }
                else
                {
                    const vector<string>::size_type serialCount = it->second.serials_.size();
                    for( vector<string>::size_type i = 0; i < serialCount; i++ )
                    {
                        Device* pDev = m_devMgr.getDeviceBySerial( it->second.serials_[i] );
                        if( pDev &&
                            ( match( searchTokenANSI, pDev->product.read(), '*' ) == 0 ) )
                        {
                            if( !PerformDeviceConfiguration( it->second.serials_[i], it->second ) )
                            {
                                WriteErrorMessage( wxString::Format( wxT( "No device found, that matches the search criterion %s.\n" ), it->first.c_str() ) );
                            }
                        }
                        else
                        {
                            WriteErrorMessage( wxString::Format( wxT( "No device found, that matches the serial number search token %s in configuration entry %s(Be sure the product type matches the given serial number).\n" ), ConvertedString( it->second.serials_[i] ).c_str(), it->first.c_str() ) );
                        }
                    }
                }
                ++it;
            }
        }
#ifdef BUILD_WITH_PROCESSOR_POWER_STATE_CONFIGURATION_SUPPORT
        if( m_boChangeProcessorIdleStates )
        {
            if( m_pMISettings_CPUIdleStatesEnabled )
            {
                if( SetPowerState( m_boEnableIdleStates ) )
                {
                    m_pMISettings_CPUIdleStatesEnabled->Check( m_boEnableIdleStates );
                }
            }
            else
            {
                WriteErrorMessage( wxT( "Changing the processor idle state configuration is not supported on this platform.\n" ) );
            }
        }
#endif // #ifdef BUILD_WITH_PROCESSOR_POWER_STATE_CONFIGURATION_SUPPORT
        if( m_boPendingQuit )
        {
            Close( true );
        }
        break;
    default:
        break;
    }
    e.Skip();
}

//-----------------------------------------------------------------------------
bool DeviceConfigureFrame::PerformDeviceConfiguration( const string& searchToken, const DeviceConfigurationData& configurationData )
//-----------------------------------------------------------------------------
{
    const vector<string>::size_type IPv4MaskCnt = m_IPv4Masks.size();
    int i = 0;
    Device* pDev = 0;
    while( ( ( pDev = m_devMgr.getDeviceBySerial( searchToken, i ) ) != 0 ) ||
           ( ( pDev = m_devMgr.getDeviceByProduct( searchToken, i ) ) != 0 ) )
    {
        if( !m_IPv4Masks.empty() )
        {
            DeviceComponentLocator locator( pDev->hDev() );
            PropertyI deviceIPAddress;
            locator.bindComponent( deviceIPAddress, "DeviceIPAddress" );
            if( deviceIPAddress.isValid() )
            {
                const string IPAddress( deviceIPAddress.readS() );
                if( IPAddress != string( "Unavailable" ) )
                {
                    bool boIgnore = true;
                    for( vector<string>::size_type j = 0; j < IPv4MaskCnt; j++ )
                    {
                        if( match( IPAddress, m_IPv4Masks[j], '*' ) == 0 )
                        {
                            boIgnore = false;
                            break;
                        }
                    }
                    if( boIgnore == true )
                    {
                        WriteLogMessage( wxString::Format( wxT( "Device '%s'(%s) will not be configured as it IPv4 address(%s) does not match any of the specified masks.\n" ), ConvertedString( pDev->serial.read() ).c_str(), ConvertedString( pDev->product.read() ).c_str(), ConvertedString( IPAddress ).c_str() ) );
                        ++i;
                        continue;
                    }
                }
            }
        }
        if( configurationData.boSetDeviceID_ )
        {
            RefreshApplicationExitCode( SetID( pDev, configurationData.deviceID_ ) );
        }
        if( configurationData.boUpdateFW_ )
        {
            if( configurationData.customFirmwareFileName_.CmpNoCase( wxT( "online" ) ) == 0 )
            {
                RefreshApplicationExitCode( UpdateFirmwareFromInternet( pDev, true ) );
            }
            else
            {
                RefreshApplicationExitCode( UpdateFirmware( pDev, true, m_pMISettings_KeepUserSetSettingsAfterFirmwareUpdate->IsChecked(), false, configurationData.customFirmwareFileName_ ) );
            }
        }
        ++i;
    }
    return i > 0;
}


//-----------------------------------------------------------------------------
void DeviceConfigureFrame::OnUpdateFirmware( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    UpdateFirmware( m_pDevListCtrl->GetCurrentItemIndex() );
}

//-----------------------------------------------------------------------------
void DeviceConfigureFrame::OnUpdateFirmwareLocalLatest( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    UpdateFirmwareLocalLatest( m_pDevListCtrl->GetCurrentItemIndex() );
}

//-----------------------------------------------------------------------------
void DeviceConfigureFrame::OnUpdateFirmwareFromInternet( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    UpdateFirmwareFromInternet( m_pDevListCtrl->GetCurrentItemIndex() );
}

//-----------------------------------------------------------------------------
void DeviceConfigureFrame::OnUpdateDMABufferSize( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    UpdateDMABufferSize( m_pDevListCtrl->GetCurrentItemIndex() );
}

//-----------------------------------------------------------------------------
bool DeviceConfigureFrame::WeekPassedSinceLastUpdatesCheck( void ) const
//-----------------------------------------------------------------------------
{
    return wxDateTime::Today() >= m_lastCheckForNewerFWVersion + wxDateSpan::Week();
}

//-----------------------------------------------------------------------------
int DeviceConfigureFrame::SetID( Device* pDev, int newID )
//-----------------------------------------------------------------------------
{
    int result = 0;
    if( newID < didlMin )
    {
        result = -4;
        WriteLogMessage( wxT( "Operation canceled by the user.\n" ) );

    }
    else if( newID > didlMax )
    {
        result = -5;
        WriteErrorMessage( wxString::Format( wxT( "Invalid ID. Valid IDs range from %d to %d.\n" ), didlMin, didlMax ) );
    }
    else
    {
        Device* pDevByFamily = 0;
        string family = pDev->family.read();
        int devIndex = 0;
        while( ( pDevByFamily = m_devMgr.getDeviceByFamily( family, devIndex++ ) ) != 0 )
        {
            if( ( pDev != pDevByFamily ) && ( pDevByFamily->deviceID.read() == newID ) )
            {
                WriteErrorMessage( wxString::Format( wxT( "WARNING: The new ID(%d) is already assigned to at least one other device belonging to the same family(%s)(%s)????.\n" ), newID, ConvertedString( pDevByFamily->serial.read() ).c_str(), ConvertedString( family ).c_str() ) );
                break;
            }
        }
    }

    if( result == 0 )
    {
        WriteLogMessage( wxString::Format( wxT( "Trying to assign ID %d to %s(%s).\n" ), newID, ConvertedString( pDev->serial.read() ).c_str(), ConvertedString( pDev->product.read() ).c_str() ) );
        switch( pDev->setID( newID ) )
        {
        case DMR_FEATURE_NOT_AVAILABLE:
            WriteErrorMessage( wxString::Format( wxT( "Device %s(%s) doesn't support setting the device ID.\n" ), ConvertedString( pDev->serial.read() ).c_str(), ConvertedString( pDev->product.read() ).c_str() ) );
            break;
        case DMR_NO_ERROR:
            WriteLogMessage( wxString::Format( wxT( "%s.\n" ), ConvertedString( pDev->HWUpdateResult.readS() ).c_str() ) );
            break;
        default:
            WriteErrorMessage( wxString::Format( wxT( "An error occurred while setting the device ID for device %s(%s): %s(please refer to the manual for this error code).\n" ), ConvertedString( pDev->serial.read() ).c_str(), ConvertedString( pDev->product.read() ).c_str(), ConvertedString( ImpactAcquireException::getErrorCodeAsString( result ) ).c_str() ) );
            break;
        }
    }
    return result;
}

//-----------------------------------------------------------------------------
int DeviceConfigureFrame::SetID( int deviceIndex )
//-----------------------------------------------------------------------------
{
    int result = 0;
    if( deviceIndex >= 0 )
    {
        Device* pDev = m_devMgr[deviceIndex];
        if( pDev )
        {
            ObjectDeleter<DeviceHandler> pHandler( m_deviceHandlerFactory.CreateObject( ConvertedString( pDev->family.read() ), pDev ) );
            if( pHandler.get() )
            {
                pHandler->AttachParent( this );
                //pHandler->ConfigureProgressDisplay( IsShown() ); // It doesn't make any sense to call this here, since we have to ask the user a question anyway!
                long newID = 0;
                if( pHandler->GetIDFromUser( newID, didlMin, didlMax ) )
                {
                    result = SetID( pDev, static_cast<int>( newID ) );
                }
            }
        }
        else
        {
            WriteErrorMessage( wxString::Format( wxT( "Invalid item selection(index: %d).\n" ), deviceIndex ) );
            result = -2;
        }
    }
    else
    {
        wxMessageDialog selectDlg( NULL, wxT( "Select a device." ), wxT( "Error" ), wxOK | wxICON_INFORMATION );
        selectDlg.ShowModal();
        result = -1;
    }
    return result;
}

//-----------------------------------------------------------------------------
void DeviceConfigureFrame::OnUpdateDeviceList( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    UpdateDeviceList();
}

//-----------------------------------------------------------------------------
void DeviceConfigureFrame::UpdateDeviceList( bool boShowProgressDialog /* = true */ )
//-----------------------------------------------------------------------------
{
    wxBusyCursor busyCursorScope;
    m_pLogWindow->SetInsertionPointEnd();
    WriteLogMessage( wxString( wxT( "Updating device list...\n" ) ) );
    const int64_type systemModuleCount = mvIMPACT::acquire::GenICam::SystemModule::getSystemModuleCount();
    for( int64_type i = 0; i < systemModuleCount; i++ )
    {
        try
        {
            mvIMPACT::acquire::GenICam::SystemModule sm( i );
            // always run this code as new interfaces might appear at runtime e.g. when plugging in a network cable...
            const int64_type interfaceCount( sm.getInterfaceModuleCount() );
            for( int64_type interfaceIndex = 0; interfaceIndex < interfaceCount; interfaceIndex++ )
            {
                mvIMPACT::acquire::GenICam::InterfaceModule im( sm, interfaceIndex );
                if( im.mvGevAdvancedDeviceDiscoveryEnable.isValid() &&
                    im.mvGevAdvancedDeviceDiscoveryEnable.isWriteable() )
                {
                    im.mvGevAdvancedDeviceDiscoveryEnable.write( bTrue );
                }
            }
        }
        catch( const ImpactAcquireException& e )
        {
            WriteErrorMessage( wxString::Format( wxT( "Failed to configure GEV interfaces of system module %u for 'advanced device discovery'. %s(numerical error representation: %d (%s)). The associated GenTL producer might not support a GenICam file for the System-module and therefore the required properties are not available.\n" ), static_cast<unsigned int>( i ), ConvertedString( e.getErrorString() ).c_str(), e.getErrorCode(), ConvertedString( e.getErrorCodeAsString() ).c_str() ) );
        }
    }
    ReadIgnoredInterfacesInfoAndEnableAllInterfaces();
    if( boShowProgressDialog )
    {
        UpdateDeviceListWithProgressMessage( this, m_devMgr );
    }
    else
    {
        m_devMgr.updateDeviceList();
    }
    BuildList();
    RevertIgnoredInterfaces();

    WriteLogMessage( wxString( wxT( "Done.\n" ) ) );

    if( m_boDevicesOnIgnoredInterfacesDetected )
    {
        WriteLogMessage( wxString( wxT( "\nWARNING: Some interfaces are being ignored during device enumeration!!! Devices connected to those interfaces will appear in grey in the device list above. To fix this, double click on the device to configure those interfaces in wxPropView\n" ) ) );
    }
}

//-----------------------------------------------------------------------------
int DeviceConfigureFrame::UpdateFirmware( Device* pDev, bool boSilentMode, bool boPersistentUserSets, bool boUseOnlineArchive, const wxString& customFirmwareFileName /* = wxEmptyString */ )
//-----------------------------------------------------------------------------
{
    int result = 0;
    ObjectDeleter<DeviceHandler> pHandler( m_deviceHandlerFactory.CreateObject( ConvertedString( pDev->family.read() ), pDev ) );
    if( pHandler.get() )
    {
        wxBusyCursor busyCursorScope;
        m_listUpdateTimer.Stop();
        pHandler->AttachParent( this );
        pHandler->ConfigureProgressDisplay( IsShown() );
        // Prefer using the more specific custom firmware file name
        if( !customFirmwareFileName.IsEmpty() )
        {
            pHandler->SetCustomFirmwareFile( customFirmwareFileName );
        }
        else
        {
            pHandler->SetCustomFirmwareFile( m_customFirmwareFile );
        }
        pHandler->SetCustomFirmwarePath( m_customFirmwarePath );
        pHandler->SetCustomGenICamFile( m_customGenICamFile );
        result = pHandler->UpdateFirmware( boSilentMode, boPersistentUserSets, boUseOnlineArchive );
        m_listUpdateTimer.Start( TIMER_PERIOD );
        UpdateDeviceList();
    }
    else
    {
        WriteErrorMessage( wxString::Format( wxT( "Device %s doesn't support firmware updates.\n" ), ConvertedString( pDev->serial.read() ).c_str() ) );
        result = -3;
    }
    return result;
}

//-----------------------------------------------------------------------------
int DeviceConfigureFrame::UpdateFirmware( int deviceIndex )
//-----------------------------------------------------------------------------
{
    int result = 0;
    if( deviceIndex >= 0 )
    {
        Device* pDev = m_devMgr[deviceIndex];
        if( pDev )
        {
            // De-select the list item (so that the background color is visible) and color the background
            // light green to highlight the device on which a firmware update is performed.
            m_pDevListCtrl->SetItemState( deviceIndex, 0, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED );
            m_pDevListCtrl->SetItemBackgroundColour( deviceIndex, wxColour( acGreenPastel ) );
            result = UpdateFirmware( pDev, false, m_pMISettings_KeepUserSetSettingsAfterFirmwareUpdate->IsChecked(), false );
        }
        else
        {
            WriteErrorMessage( wxString::Format( wxT( "Invalid item selection(index: %d).\n" ), deviceIndex ) );
            result = -2;
        }
    }
    else
    {
        wxMessageDialog selectDlg( NULL, wxT( "Select a device." ), wxT( "Error" ), wxOK | wxICON_INFORMATION );
        selectDlg.ShowModal();
        result = -1;
    }
    return result;
}

//-----------------------------------------------------------------------------
void DeviceConfigureFrame::UpdateFirmwareLocalLatest( int deviceIndex )
//-----------------------------------------------------------------------------
{
    if( deviceIndex >= 0 )
    {
        Device* pDev = m_devMgr[deviceIndex];
        if( pDev )
        {
            // De-select the list item (so that the background color is visible) and color the background
            // light green to highlight the device on which a firmware update is performed.
            m_pDevListCtrl->SetItemState( deviceIndex, 0, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED );
            m_pDevListCtrl->SetItemBackgroundColour( deviceIndex, wxColour( acGreenPastel ) );
            ObjectDeleter<DeviceHandler> pHandler( m_deviceHandlerFactory.CreateObject( ConvertedString( pDev->family.read() ), pDev ) );
            if( pHandler.get() && pHandler->SupportsFirmwareUpdateFromMVU() )
            {
                const ProductToFileEntryMap::const_iterator productIT = m_productToFirmwareFilesLocalMap.find( pDev->product.read() );
                if( productIT == m_productToFirmwareFilesLocalMap.end() )
                {
                    WriteErrorMessage( wxString::Format( wxT( "No local update archive found for device %s (index: %d).\n" ), pDev->product.read(), deviceIndex ) );
                    return;
                }
                const wxString firmwareFileName = productIT->second.localMVUFileName_;
                UpdateFirmware( pDev, false, m_pMISettings_KeepUserSetSettingsAfterFirmwareUpdate->IsChecked(), false, firmwareFileName );
            }
            else
            {
                UpdateFirmware( pDev, false, m_pMISettings_KeepUserSetSettingsAfterFirmwareUpdate->IsChecked(), false );
            }

        }
        else
        {
            WriteErrorMessage( wxString::Format( wxT( "Invalid item selection(index: %d).\n" ), deviceIndex ) );
        }
    }
    else
    {
        wxMessageDialog selectDlg( NULL, wxT( "Select a device." ), wxT( "Error" ), wxOK | wxICON_INFORMATION );
        selectDlg.ShowModal();
    }
}

//-----------------------------------------------------------------------------
void DeviceConfigureFrame::UpdateFirmwareFromInternetAllDevices( void )
//-----------------------------------------------------------------------------
{
    const unsigned int devCnt = m_devMgr.deviceCount();
    for( unsigned int i = 0; i < devCnt; i++ )
    {
        UpdateFirmwareFromInternet( i );
    }
}

//-----------------------------------------------------------------------------
int DeviceConfigureFrame::UpdateFirmwareFromInternet( Device* pDev, bool boSilentMode )
//-----------------------------------------------------------------------------
{
    int result = 0;
    if( DeviceHandlerBlueDevice::IsBlueCOUGAR_X( pDev ) ||
        DeviceHandlerBlueDevice::IsBlueFOX3( pDev ) ||
        DeviceHandlerBlueDevice::IsBlueCOUGAR_XT( pDev ) )
    {
        ObjectDeleter<DeviceHandler> pHandler( m_deviceHandlerFactory.CreateObject( ConvertedString( pDev->family.read() ), pDev ) );
        if( pHandler.get() )
        {
            wxString fwfilePath;
            result = pHandler->GetFirmwareUpdateFolder( fwfilePath );
            if( result != DeviceHandler::urOperationSuccessful )
            {
                WriteErrorMessage( wxString::Format( wxT( "\nCould not locate the firmware download folder for device %s(%s).\n" ), ConvertedString( pDev->serial.readS() ).c_str(), ConvertedString( pDev->product.readS() ).c_str() ) );
                return result;
            }
            AppendPathSeparatorIfNeeded( fwfilePath );
            WriteLogMessage( wxString::Format( wxT( "\nChecking firmware update information from MATRIX VISION website...\n" ) ) );
            CheckForOnlineUpdates( true );
            ProductToFileEntryMap::const_iterator fileEntryIT = m_productToFirmwareFilesOnlineMap.find( pDev->product.readS() );
            if( fileEntryIT == m_productToFirmwareFilesOnlineMap.end() )
            {
                WriteErrorMessage( wxString::Format( wxT( "For device %s are no firmware updates available from Internet.\n" ), ConvertedString( pDev->serial.read() ).c_str() ) );
                return -42;
            }
            WriteLogMessage( wxString::Format( wxT( "\nStarting firmware update using the latest firmware version available online for device %s(%s).\n" ), ConvertedString( pDev->serial.readS() ).c_str(), ConvertedString( pDev->product.readS() ).c_str() ) );
            wxString mvuFileUrl( fileEntryIT->second.onlineUpdateInfos_.updateArchiveUrl_ );
            wxString mvuFileName( mvuFileUrl.AfterLast( wxT( '/' ) ) );
            if( !wxFileExists( fwfilePath.Append( mvuFileName ) ) )
            {
                DownloadFWUpdateFile( mvuFileUrl );
            }
            result = UpdateFirmware( pDev, boSilentMode, m_pMISettings_KeepUserSetSettingsAfterFirmwareUpdate->IsChecked(), true, mvuFileName );
        }
    }
    else
    {
        WriteErrorMessage( wxString::Format( wxT( "Device %s doesn't support firmware updates from Internet.\n" ), ConvertedString( pDev->serial.read() ).c_str() ) );
    }
    return result;
}

//-----------------------------------------------------------------------------
int DeviceConfigureFrame::UpdateFirmwareFromInternet( int deviceIndex )
//-----------------------------------------------------------------------------
{
    int result = 0;
    if( deviceIndex >= 0 )
    {
        Device* pDev = m_devMgr[deviceIndex];
        if( pDev )
        {
            // De-select the list item (so that the background color is visible) and color the background
            // light green to highlight the device on which a firmware update is performed.
            m_pDevListCtrl->SetItemState( deviceIndex, 0, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED );
            m_pDevListCtrl->SetItemBackgroundColour( deviceIndex, wxColour( acGreenPastel ) );
            result = UpdateFirmwareFromInternet( pDev, false );
        }
        else
        {
            WriteErrorMessage( wxString::Format( wxT( "Invalid item selection(index: %d).\n" ), deviceIndex ) );
            result = -2;
        }
    }
    else
    {
        wxMessageDialog selectDlg( NULL, wxT( "Select a device." ), wxT( "Error" ), wxOK | wxICON_INFORMATION );
        selectDlg.ShowModal();
        result = -1;
    }
    return result;
}

//-----------------------------------------------------------------------------
void DeviceConfigureFrame::ReadIgnoredInterfacesInfoAndEnableAllInterfaces( void )
//-----------------------------------------------------------------------------
{
    m_ignoredInterfacesInfo.clear();
    const int64_type systemModuleCount = mvIMPACT::acquire::GenICam::SystemModule::getSystemModuleCount();
    for ( int64_type systemModuleIndex = 0; systemModuleIndex < systemModuleCount; systemModuleIndex++ )
    {
        try
        {
            mvIMPACT::acquire::GenICam::SystemModule sm( systemModuleIndex );
            if( sm.mvDeviceUpdateListBehaviour.isValid() )
            {
                UnIgnoreInterfaceTechnology( sm, "GEV" );
                UnIgnoreInterfaceTechnology( sm, "GigEVision" );
                UnIgnoreInterfaceTechnology( sm, "U3V" );
                UnIgnoreInterfaceTechnology( sm, "USB3Vision" );
                UnIgnoreInterfaceTechnology( sm, "PCI" );
                const int64_type interfaceCount( sm.getInterfaceModuleCount() );
                for( int64_type i = 0; i < interfaceCount; i++ )
                {
                    sm.interfaceSelector.write( i );
                    m_ignoredInterfacesInfo.insert( make_pair( sm.interfaceID.readS(), sm.mvDeviceUpdateListBehaviour.readS() ) );
                    sm.mvDeviceUpdateListBehaviour.writeS( "NotConfigured" );
                }
            }
        }
        catch( const ImpactAcquireException& ) {}
    }
}

//-----------------------------------------------------------------------------
void DeviceConfigureFrame::ReportThatAdminRightsAreNeeded( void )
//-----------------------------------------------------------------------------
{
    wxMessageDialog dlg( NULL, wxT( "In order for this operation to succeed the application needs to be restarted with elevated privileges. On Windows you will need to restart it either 'as Administrator' or when being logged on with a user that is part of the local administrators. If you want to terminate this application now press 'Ok'!" ), wxT( "Administrator Rights Needed" ), wxOK | wxCANCEL | wxICON_INFORMATION );
    if( dlg.ShowModal() == wxID_OK )
    {
        Close( true );
    }
}

//-----------------------------------------------------------------------------
void DeviceConfigureFrame::RevertIgnoredInterfaces( void )
//-----------------------------------------------------------------------------
{
    const int64_type systemModuleCount = mvIMPACT::acquire::GenICam::SystemModule::getSystemModuleCount();
    for( int64_type systemModuleIndex = 0; systemModuleIndex < systemModuleCount; systemModuleIndex++ )
    {
        try
        {
            mvIMPACT::acquire::GenICam::SystemModule sm( systemModuleIndex );
            if( sm.mvInterfaceTechnologyToIgnoreSelector.isValid() &&
                sm.mvInterfaceTechnologyToIgnoreEnable.isValid() &&
                sm.mvDeviceUpdateListBehaviour.isValid() )
            {
                RevertIgnoredInterfaceTechnology( sm, "GEV", m_commandLineIgnoreGEV );
                RevertIgnoredInterfaceTechnology( sm, "U3V", m_commandLineIgnoreU3V );
                RevertIgnoredInterfaceTechnology( sm, "PCI", m_commandLineIgnorePCI );

                const int64_type interfaceCount = sm.interfaceSelector.getMaxValue() + 1;
                for( int64_type i = 0; i < interfaceCount; i++ )
                {
                    sm.interfaceSelector.write( i );
                    if( !m_ignoredInterfacesInfo[sm.interfaceID.readS()].empty() )
                    {
                        sm.mvDeviceUpdateListBehaviour.writeS( m_ignoredInterfacesInfo[sm.interfaceID.readS()] );
                    }
                }
            }
        }
        catch( const ImpactAcquireException& ) {}
    }
}

//-----------------------------------------------------------------------------
void DeviceConfigureFrame::RevertIgnoredInterfaceTechnology( const mvIMPACT::acquire::GenICam::SystemModule& sm, const string& technology, TIgnoreInterfaceCommandLineParameter commandLineParameter )
//-----------------------------------------------------------------------------
{
    if( supportsEnumStringValue( sm.mvInterfaceTechnologyToIgnoreEnable, technology ) )
    {
        sm.mvInterfaceTechnologyToIgnoreSelector.writeS( technology );
        if( commandLineParameter == iiclpNotConfigured )
        {
            sm.mvInterfaceTechnologyToIgnoreEnable.write( ( m_ignoredInterfacesInfo[technology] == string( "1" ) ) ? bTrue : bFalse );
        }
        else
        {
            sm.mvInterfaceTechnologyToIgnoreEnable.write( commandLineParameter == iiclpEnableIgnore ? bTrue : bFalse );
        }
    }
}

//-----------------------------------------------------------------------------
void DeviceConfigureFrame::RefreshApplicationExitCode( const int result )
//-----------------------------------------------------------------------------
{
    if( ( result != DeviceHandler::urOperationSuccessful ) &&
        ( result != DeviceHandler::urFeatureUnsupported ) ) // this might be returned e.g. from 3rd party devices but during a script or command line based updates an should not be treated as an error
    {
        m_s_updateResult = result;
    }
}

//-----------------------------------------------------------------------------
void DeviceConfigureFrame::UnIgnoreInterfaceTechnology( const mvIMPACT::acquire::GenICam::SystemModule& sm, const string& technology )
//-----------------------------------------------------------------------------
{
    if( sm.mvInterfaceTechnologyToIgnoreSelector.isValid() &&
        supportsEnumStringValue( sm.mvInterfaceTechnologyToIgnoreSelector, technology ) &&
        sm.mvInterfaceTechnologyToIgnoreEnable.isValid() &&
        supportsEnumStringValue( sm.mvInterfaceTechnologyToIgnoreSelector, technology ) )
    {
        sm.mvInterfaceTechnologyToIgnoreSelector.writeS( technology );
        if( sm.mvInterfaceTechnologyToIgnoreEnable.read() == bTrue )
        {
            m_ignoredInterfacesInfo.insert( make_pair( technology, sm.mvInterfaceTechnologyToIgnoreEnable.readS() ) );
        }
        sm.mvInterfaceTechnologyToIgnoreEnable.write( bFalse );
    }
}

//-----------------------------------------------------------------------------
int DeviceConfigureFrame::UpdateDMABufferSize( int deviceIndex )
//-----------------------------------------------------------------------------
{
    int result = 0;
    if( deviceIndex >= 0 )
    {
        Device* pDev = m_devMgr[deviceIndex];
        if( pDev )
        {
            ObjectDeleter<DeviceHandler> pHandler( m_deviceHandlerFactory.CreateObject( ConvertedString( pDev->family.read() ), pDev ) );
            if( pHandler.get() )
            {
                wxBusyCursor busyCursorScope;
                pHandler->AttachParent( this );
                //pHandler->ConfigureProgressDisplay( IsShown() ); // It doesn't make any sense to call this here, since we have to ask the user a question anyway!
                result = pHandler->UpdatePermanentDMABufferSize( false );
            }
            else
            {
                WriteErrorMessage( wxString::Format( wxT( "Device %s doesn't support firmware updates.\n" ), ConvertedString( pDev->serial.read() ).c_str() ) );
                result = -3;
            }
        }
        else
        {
            WriteErrorMessage( wxString::Format( wxT( "Invalid item selection(index: %d).\n" ), deviceIndex ) );
            result = -2;
        }
    }
    else
    {
        wxMessageDialog selectDlg( NULL, wxT( "Select a device." ), wxT( "Error" ), wxOK | wxICON_INFORMATION );
        selectDlg.ShowModal();
        result = -1;
    }
    return result;
}

#ifdef BUILD_WITH_DIRECT_SHOW_SUPPORT
//-----------------------------------------------------------------------------
void DeviceConfigureFrame::OnRegisterAllDevicesForDirectShow( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    if( m_boApplicationHasElevatedRights )
    {
        m_DSDevMgr.registerAllDevices();
        BuildList();
    }
    else
    {
        ReportThatAdminRightsAreNeeded();
    }
}

//-----------------------------------------------------------------------------
void DeviceConfigureFrame::OnUnregisterAllDevicesForDirectShow( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    if( m_boApplicationHasElevatedRights )
    {
        m_DSDevMgr.unregisterAllDevices();
        BuildList();
    }
    else
    {
        ReportThatAdminRightsAreNeeded();
    }
}

//-----------------------------------------------------------------------------
void DeviceConfigureFrame::OnSetFriendlyNameForDirectShow( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    SetDSFriendlyName( m_pDevListCtrl->GetCurrentItemIndex() );
}

//-----------------------------------------------------------------------------
int DeviceConfigureFrame::SetDSFriendlyName( int deviceIndex )
//-----------------------------------------------------------------------------
{
    int result = 0;
    if( deviceIndex >= 0 )
    {
        Device* pDev = m_devMgr[deviceIndex];
        if( pDev )
        {
            wxListItem info;
            info.m_itemId = deviceIndex;
            info.m_col = lcDSFriendlyName;
            info.m_mask = wxLIST_MASK_TEXT;
            if( !m_pDevListCtrl->GetItem( info ) )
            {
                WriteErrorMessage( wxT( "Failed to obtain item info.\n" ) );
            }

            string devSerial = pDev->serial.read();
            string devProduct = pDev->product.read();
            WriteLogMessage( wxString::Format( wxT( "Trying to set new DirectShow friendly name for %s(%s). Current friendly name: %s\n" ), ConvertedString( devSerial ).c_str(), ConvertedString( devProduct ).c_str(), info.m_text.c_str() ) );
            wxString newFriendlyName = wxGetTextFromUser( wxString::Format( wxT( "Enter the new DirectShow friendly name for device %s.\nMake sure that no other device is using this name already.\n" ), ConvertedString( devSerial ).c_str() ),
                                       wxT( "New friendly name:" ),
                                       wxString::Format( wxT( "%s_%s" ), ConvertedString( devProduct ).c_str(), ConvertedString( devSerial ).c_str() ),
                                       this );
            if( newFriendlyName == wxEmptyString )
            {
                result = -4;
                WriteErrorMessage( wxT( "Operation canceled by the user or invalid (empty) input.\n" ) );
            }
            else
            {
                // check if this name is already in use
                int itemCount = m_pDevListCtrl->GetItemCount();
                for( int i = 0; i < itemCount; i++ )
                {
                    info.m_itemId = i;
                    info.m_col = lcDSFriendlyName;
                    info.m_mask = wxLIST_MASK_TEXT;
                    if( !m_pDevListCtrl->GetItem( info ) )
                    {
                        WriteErrorMessage( wxString::Format( wxT( "Failed to obtain item info for item %d.\n" ), i ) );
                        continue;
                    }
                    if( info.m_text == newFriendlyName )
                    {
                        if( i == deviceIndex )
                        {
                            WriteLogMessage( wxT( "The friendly name for this device has not been changed.\n" ) );
                            result = -5;
                        }
                        else
                        {
                            WriteErrorMessage( wxT( "WARNING: Another device is using this friendly name already. Operation skipped.\n" ) );
                            result = -6;
                        }
                    }
                }

                if( result == 0 )
                {
                    if( m_boApplicationHasElevatedRights )
                    {
                        WriteLogMessage( wxString::Format( wxT( "Trying to assign %s as a friendly name to device %s.\n" ), newFriendlyName.c_str(), ConvertedString( devSerial ).c_str() ) );
                        string friendlyNameANSI( newFriendlyName.mb_str() );
                        result = m_DSDevMgr.registerDevice( pDev->deviceID.read(), pDev->product.read().c_str(), friendlyNameANSI.c_str(), pDev->serial.read().c_str(), pDev->family.read().c_str() );
                        BuildList();
                    }
                    else
                    {
                        ReportThatAdminRightsAreNeeded();
                    }
                }
            }
        }
        else
        {
            WriteErrorMessage( wxString::Format( wxT( "Invalid item selection(index: %d).\n" ), deviceIndex ) );
            result = -2;
        }
    }
    else
    {
        wxMessageDialog selectDlg( NULL, wxT( "Select a device." ), wxT( "Error" ), wxOK | wxICON_INFORMATION );
        selectDlg.ShowModal();
        result = -1;
    }
    return result;
}
#endif // #ifdef BUILD_WITH_DIRECT_SHOW_SUPPORT

#ifdef BUILD_WITH_PROCESSOR_POWER_STATE_CONFIGURATION_SUPPORT
//-----------------------------------------------------------------------------
void DeviceConfigureFrame::OnSettings_CPUIdleStatesEnabled( wxCommandEvent& e )
//-----------------------------------------------------------------------------
{
    if( m_boApplicationHasElevatedRights )
    {
        if( wxMessageBox( wxString::Format( wxT( "Are you sure you want to %s the C1, C2 and C3 states for ALL processors in this system?" ), e.IsChecked() ? wxT( "enable" ) : wxT( "disable" ) ), wxT( "CPU Power State Configuration" ), wxYES_NO | wxNO_DEFAULT | wxICON_EXCLAMATION, this ) == wxYES )
        {
            if( !SetPowerState( e.IsChecked() ) )
            {
                wxMessageBox( wxString::Format( wxT( "Failed to %s the C1, C2 and C3 states for ALL processors." ), e.IsChecked() ? wxT( "enable" ) : wxT( "disable" ) ), wxT( "ERROR" ), wxOK | wxICON_ERROR, this );
            }
        }
        else
        {
            bool boValue = false;
            if( GetPowerState( boValue ) )
            {
                m_pMISettings_CPUIdleStatesEnabled->Check( boValue );
            }
            else
            {
                wxMessageBox( wxT( "Failed to query the C1, C2 and C3 states for ALL processors." ), wxT( "ERROR" ), wxOK | wxICON_ERROR, this );
            }
        }
    }
    else
    {
        ReportThatAdminRightsAreNeeded();
    }
}
#endif // #ifdef BUILD_WITH_PROCESSOR_POWER_STATE_CONFIGURATION_SUPPORT

//-----------------------------------------------------------------------------
void DeviceConfigureFrame::UpdateMenu( int deviceIndex )
//-----------------------------------------------------------------------------
{
    Device* pDev = 0;
    if( ( deviceIndex >= 0 ) && ( deviceIndex < static_cast<int>( m_devMgr.deviceCount() ) ) )
    {
        pDev = m_devMgr.getDevice( deviceIndex );
    }
    bool boFWUpdateSupported = false;
    bool boSetIDSupported = false;
    bool boDMABufferSizeSupported = false;
    bool boFWUpdateFromInternetSupported = false;
    bool boFWInMVUFile = false;
#ifdef BUILD_WITH_DIRECT_SHOW_SUPPORT
    bool boIsRegisteredForDirectShow = false;
#endif // #ifdef BUILD_WITH_DIRECT_SHOW_SUPPORT
    if( pDev )
    {
        ObjectDeleter<DeviceHandler> pHandler( m_deviceHandlerFactory.CreateObject( ConvertedString( pDev->family.read() ), pDev ) );
        if( pHandler.get() )
        {
            boFWUpdateSupported = pHandler->SupportsFirmwareUpdate();
            boSetIDSupported = pHandler->SupportsSetID();
            boDMABufferSizeSupported = pHandler->SupportsDMABufferSizeUpdate();
            boFWUpdateFromInternetSupported = pHandler->SupportsFirmwareUpdateFromInternet();
            boFWInMVUFile = pHandler->SupportsFirmwareUpdateFromMVU();
        }
#ifdef BUILD_WITH_DIRECT_SHOW_SUPPORT
        wxListItem info;
        info.m_itemId = deviceIndex;
        info.m_col = lcDSRegistered;
        info.m_mask = wxLIST_MASK_TEXT;
        if( m_pDevListCtrl->GetItem( info ) )
        {
            boIsRegisteredForDirectShow = ( info.m_text == wxT( "yes" ) );
        }
#endif // #ifdef BUILD_WITH_DIRECT_SHOW_SUPPORT
    }
    m_pMIActionSetID->Enable( boSetIDSupported );
    m_pMIActionUpdateFW->Enable( boFWInMVUFile );
    m_pMIActionUpdateFWDefault->Enable( boFWUpdateSupported );
    m_pMIActionFWUpdateFromInternet->Enable( boFWUpdateFromInternetSupported );
    m_pMIActionUpdateDMABufferSize->Enable( boDMABufferSizeSupported );
#ifdef BUILD_WITH_DIRECT_SHOW_SUPPORT
    m_pMIDirectShow_SetFriendlyName->Enable( ( pDev != 0 ) && m_boApplicationHasElevatedRights && boIsRegisteredForDirectShow );
#endif // #ifdef BUILD_WITH_DIRECT_SHOW_SUPPORT
}

//-----------------------------------------------------------------------------
void DeviceConfigureFrame::WriteErrorMessage( const wxString& msg )
//-----------------------------------------------------------------------------
{
    wxTextAttr errorStyle( wxColour( 255, 0, 0 ) );
    WriteLogMessage( msg, errorStyle );
}

//-----------------------------------------------------------------------------
void DeviceConfigureFrame::WriteLogMessage( const wxString& msg, const wxTextAttr& style /* = wxTextAttr(wxColour(0, 0, 0)) */ )
//-----------------------------------------------------------------------------
{
    if( m_pLogWindow )
    {
        long posBefore = m_pLogWindow->GetLastPosition();
        m_pLogWindow->WriteText( msg );
        long posAfter = m_pLogWindow->GetLastPosition();
        m_pLogWindow->SetStyle( posBefore, posAfter, style );
        m_pLogWindow->ScrollLines( m_pLogWindow->GetNumberOfLines() );
    }
}
