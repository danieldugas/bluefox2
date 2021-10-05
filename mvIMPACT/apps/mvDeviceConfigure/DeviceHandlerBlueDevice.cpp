//-----------------------------------------------------------------------------
#include <apps/Common/CommonGUIFunctions.h>
#include <apps/Common/wxAbstraction.h>
#include <common/STLHelper.h>
#include "DeviceConfigureFrame.h"
#include "DeviceHandlerBlueDevice.h"
#include <sstream>

#include <apps/Common/wxIncludePrologue.h>
#include <wx/artprov.h>
#include <wx/choicdlg.h>
#include <wx/dir.h>
#include <wx/ffile.h>
#include <wx/filedlg.h>
#include <wx/mstream.h>
#include <wx/progdlg.h>
#include <wx/thread.h>
#include <wx/tokenzr.h>
#include <wx/utils.h>
#include <wx/zipstrm.h>
#include <apps/Common/wxIncludeEpilogue.h>

#if defined __WIN32__
#  include <apps/Common/ProxyResolverContext.h>
#endif // defined __WIN32__

//=============================================================================
//=================== internal helper functions, classes and structs ==========
//=============================================================================
// ----------------------------------------------------------------------------
class FirmwareUpdateConfirmationDialog : public wxDialog
//-----------------------------------------------------------------------------
{
public:
    FirmwareUpdateConfirmationDialog( wxWindow* parent, wxWindowID id,
                                      const wxString& title,
                                      const wxPoint& pos = wxDefaultPosition,
                                      const wxSize& size = wxDefaultSize,
                                      long style = wxDEFAULT_DIALOG_STYLE,
                                      const wxString& message = wxT( "" ),
                                      bool keepUserSets = true ) : wxDialog( parent, id, title, pos, size, style, wxT( "FirmwareUpdateConfirmationDialog" ) ), pCBKeepUserSetSettings_( 0 )
    {
        const wxIcon infoIcon = wxArtProvider::GetMessageBoxIcon( wxICON_INFORMATION );
        SetIcon( infoIcon );
        SetBackgroundColour( *wxWHITE );
        wxPanel* pPanel = new wxPanel( this, wxID_ANY );

        pCBKeepUserSetSettings_ = new wxCheckBox( pPanel, wxID_ANY, wxT( "Keep UserSet Settings(Takes longer but preserves UserSet settings stored on the device)" ) );
        wxButton* pBtnOk = new wxButton( pPanel, wxID_OK, wxT( "OK" ) );
        wxButton* pBtnCancel = new wxButton( pPanel, wxID_CANCEL, wxT( "Cancel" ) );

        wxBoxSizer* pTextSizer = new wxBoxSizer( wxHORIZONTAL );
        pTextSizer->AddSpacer( 10 );
        pTextSizer->Add( new wxStaticBitmap( pPanel, wxID_ANY, infoIcon ) );
        pTextSizer->AddSpacer( 10 );
        pTextSizer->Add( new wxStaticText( pPanel, wxID_ANY, message ) );
        pTextSizer->AddSpacer( 10 );

        wxBoxSizer* pCheckBoxSizer = new wxBoxSizer( wxHORIZONTAL );
        pCheckBoxSizer->AddSpacer( 10 );
        pCheckBoxSizer->Add( pCBKeepUserSetSettings_ );
        pCheckBoxSizer->AddSpacer( 10 );

        wxBoxSizer* pButtonSizer = new wxBoxSizer( wxHORIZONTAL );
        pButtonSizer->Add( pBtnOk, wxSizerFlags().Border( wxALL, 7 ) );
        pButtonSizer->Add( pBtnCancel, wxSizerFlags().Border( wxALL, 7 ) );

        wxBoxSizer* pTopDownSizer = new wxBoxSizer( wxVERTICAL );
        pTopDownSizer->AddSpacer( 10 );
        pTopDownSizer->Add( pTextSizer );
        pTopDownSizer->AddSpacer( 20 );
        pTopDownSizer->Add( pCheckBoxSizer );
        pTopDownSizer->AddSpacer( 5 );
        pTopDownSizer->Add( pButtonSizer, wxSizerFlags().Right() );

        pPanel->SetSizer( pTopDownSizer );
        pTopDownSizer->SetSizeHints( this );

        pCBKeepUserSetSettings_->SetValue( keepUserSets ? true : false );
        Center();
    }
    bool shallKeepUserSets( void ) const
    {
        return pCBKeepUserSetSettings_->IsChecked();
    }
private:
    wxCheckBox* pCBKeepUserSetSettings_;
};

//-----------------------------------------------------------------------------
struct UpdateFeatures
//-----------------------------------------------------------------------------
{
    Device* pDev_;
    PropertyS sourceFileName_;
    PropertyS destinationFileName_;
    PropertyI transferMode_;
    PropertyS transferBuffer_;
    Method upload_;
    Method download_;
    Method install_;
    Method updateFirmware_;
    PropertyS lastResult_;
    void bindFeatures( void )
    {
        DeviceComponentLocator locator( pDev_, dltSystemSettings, "FileExchange" );
        locator.bindComponent( sourceFileName_, "SourceFileName" );
        locator.bindComponent( destinationFileName_, "DestinationFileName" );
        locator.bindComponent( transferMode_, "TransferMode" );
        locator.bindComponent( transferBuffer_, "TransferBuffer" );
        locator.bindComponent( upload_, "DoFileUpload@i" );
        locator.bindComponent( download_, "DoFileDownload@i" );
        locator.bindComponent( install_, "DoInstallFile@i" );
        locator.bindComponent( updateFirmware_, "DoFirmwareUpdate@i" );
        locator.bindComponent( lastResult_, "LastResult" );
    }
    explicit UpdateFeatures( Device* pDev ) : pDev_( pDev )
    {
        bindFeatures();
    }
};

//------------------------------------------------------------------------------
class EraseFlashThread : public wxThread
//------------------------------------------------------------------------------
{
    mvIMPACT::acquire::GenICam::FileAccessControl    fac_;
    int&                                             fileOperationExecuteResult_;
protected:
    void* Entry()
    {
        // erase the flash before downloading the new firmware
        fac_.fileSelector.writeS( "DeviceFirmware" );
        fac_.fileOperationSelector.writeS( "Delete" );
        fileOperationExecuteResult_ = fac_.fileOperationExecute.call();
        return 0;
    }
public:
    explicit EraseFlashThread( mvIMPACT::acquire::GenICam::FileAccessControl& fac, int& fileOperationExecuteResult_ ) : wxThread( wxTHREAD_JOINABLE ), fac_( fac ), fileOperationExecuteResult_( fileOperationExecuteResult_ ) {}
};

//------------------------------------------------------------------------------
class SaveSettingThread : public wxThread
//------------------------------------------------------------------------------
{
    mvIMPACT::acquire::FunctionInterface fi_;
    wxString path_;
    TDMR_ERROR& result_;
protected:
    void* Entry()
    {
        result_ = static_cast<TDMR_ERROR>( fi_.saveSetting( std::string( path_.mb_str() ), static_cast<TStorageFlag>( sfFile | sfProcessGenICamSequencerData | sfProcessGenICamUserSetData ), sUser ) );
        if( result_ != DMR_NO_ERROR )
        {
            // There is an error in the GenApi implementation < 3.2.0 when trying to save GenICam sequencer data for devices with no Sequencer.
            // Therefore we try again without the flag sfProcessGenICamSequencerData
            result_ = static_cast<TDMR_ERROR>( fi_.saveSetting( std::string( path_.mb_str() ), static_cast<TStorageFlag>( sfFile | sfProcessGenICamUserSetData ), sUser ) );
        }
        return 0;
    }
public:
    explicit SaveSettingThread( mvIMPACT::acquire::FunctionInterface& fi, TDMR_ERROR& result, const wxString& path ) : wxThread( wxTHREAD_JOINABLE ), fi_( fi ), path_( path ), result_( result ) {}
};

//------------------------------------------------------------------------------
class LoadSettingThread : public wxThread
//------------------------------------------------------------------------------
{
    mvIMPACT::acquire::FunctionInterface fi_;
    wxString path_;
    TDMR_ERROR& result_;
protected:
    void* Entry()
    {
        result_ = static_cast<TDMR_ERROR>( fi_.loadSetting( std::string( path_.mb_str() ), sfFile, sUser ) );
        return 0;
    }
public:
    explicit LoadSettingThread( mvIMPACT::acquire::FunctionInterface& fi, TDMR_ERROR& result, const wxString& path ) : wxThread( wxTHREAD_JOINABLE ), fi_( fi ), path_( path ), result_( result ) {}
};

//------------------------------------------------------------------------------
class RebootDeviceThread : public wxThread
//------------------------------------------------------------------------------
{
    Device* pDev_;
    int maxSleepTime_s_;
    int napTime_s_;
protected:
    void* Entry()
    {
        int timeSlept_s = 0;
        DeviceManager devMgr;

        // Wait up to 'maxSleepTime_s_' minutes for the device to reboot. If it doesn't re-appear
        // until then something went wrong but we don't want to stay here
        // forever. An error will be generated further up the stack then.
        while( timeSlept_s < maxSleepTime_s_ )
        {
            wxSleep( napTime_s_ );
            timeSlept_s += napTime_s_;
            devMgr.updateDeviceList(); // this is needed in order to invalidate cached data that might no longer be valid(e.g. the URLs for the description files)
            if( pDev_->state.read() == dsPresent )
            {
                break;
            }
        }
        return 0;
    }
public:
    explicit RebootDeviceThread( Device* pDev, const int maxSleepTime_s, const int napTime_s ) : wxThread( wxTHREAD_JOINABLE ), pDev_( pDev ), maxSleepTime_s_( maxSleepTime_s ), napTime_s_( napTime_s ) {}
};

//------------------------------------------------------------------------------
class StoreToFlashThread : public wxThread
//------------------------------------------------------------------------------
{
    mvIMPACT::acquire::GenICam::FileAccessControl& fac_;
    int result_;
protected:
    void* Entry()
    {
        fac_.fileOperationSelector.writeS( "MvFlashWrite" );
        result_ = fac_.fileOperationExecute.call();
        return 0;
    }
public:
    explicit StoreToFlashThread( mvIMPACT::acquire::GenICam::FileAccessControl& fac ) : wxThread( wxTHREAD_JOINABLE ), fac_( fac ), result_( DMR_NO_ERROR ) {}
    int GetResult( void ) const
    {
        return result_;
    }
};

//------------------------------------------------------------------------------
class UpdateBootProgrammerThread : public wxThread
//------------------------------------------------------------------------------
{
    mvIMPACT::acquire::Method updateBootProgrammer_;
    int result_;
protected:
    void* Entry()
    {
        result_ = updateBootProgrammer_.call();
        return 0;
    }
public:
    explicit UpdateBootProgrammerThread( mvIMPACT::acquire::Method updateBootProgrammer ) : wxThread( wxTHREAD_JOINABLE ), updateBootProgrammer_( updateBootProgrammer ), result_( DMR_NO_ERROR ) {}
    int GetResult( void ) const
    {
        return result_;
    }
};

//-----------------------------------------------------------------------------
class SystemSettingsGenTL : public SystemSettings
//-----------------------------------------------------------------------------
{
public:
    explicit SystemSettingsGenTL( Device* pDev ) : SystemSettings( pDev ),
        featurePollingEnable()
    {
        DeviceComponentLocator locator( pDev, dltSystemSettings );
        locator.bindComponent( featurePollingEnable, "FeaturePollingEnable" );
    }
    PropertyIBoolean featurePollingEnable;
};

//------------------------------------------------------------------------------
class WaitForCloseThread : public wxThread
//------------------------------------------------------------------------------
{
    mvIMPACT::acquire::GenICam::ODevFileStream* pUploadFile_;
protected:
    void* Entry()
    {
        pUploadFile_->close();
        return 0;
    }
public:
    explicit WaitForCloseThread( mvIMPACT::acquire::GenICam::ODevFileStream* pUploadFile ) : wxThread( wxTHREAD_JOINABLE ), pUploadFile_( pUploadFile ) {}
};

//-----------------------------------------------------------------------------
template<class _Elem, class _Traits, class _Ax, class _AVx>
typename std::vector<std::basic_string<_Elem, _Traits, _AVx> >::size_type split(
    /// The string to split
    const std::basic_string<_Elem, _Traits, _Ax>& str,
    /// A string defining a separator
    const std::basic_string<_Elem, _Traits, _Ax>& separator,
    /// A vector for the resulting strings
    std::vector<std::basic_string<_Elem, _Traits, _Ax>, _AVx >& v )
//-----------------------------------------------------------------------------
{
    v.clear();
    typename std::basic_string<_Elem, _Traits, _Ax>::size_type start = 0, end = 0;
    std::basic_string<_Elem, _Traits, _Ax> param, key, value;
    while( ( start = str.find_first_not_of( separator, start ) ) != std::basic_string<_Elem, _Traits, _Ax>::npos )
    {
        end = str.find_first_of( separator, start );
        v.push_back( ( end == std::basic_string<_Elem, _Traits, _Ax>::npos ) ? str.substr( start ) : str.substr( start, end - start ) );
        start = end;
    }
    return v.size();
}

//-----------------------------------------------------------------------------
static wxString ExtractVersionNumber( const wxString& s )
//-----------------------------------------------------------------------------
{
    // %s(%s, file version: %d.%d)
    static const wxString TOKEN( wxT( "version: " ) );
    int pos = s.Find( TOKEN.c_str() );
    if( pos == wxNOT_FOUND )
    {
        return wxString();
    }
    wxString version( s.Mid( pos + TOKEN.Length() ) );
    int posVersionEND = version.Find( wxT( ')' ) );
    return ( ( posVersionEND == wxNOT_FOUND ) ? version : version.Mid( 0, posVersionEND ) );
}

//-----------------------------------------------------------------------------
/// Highest file number first!
static int CompareFileVersion( const Version& first, const Version& second )
//-----------------------------------------------------------------------------
{
    if( first < second )
    {
        return 1;
    }
    else if( first > second )
    {
        return -1;
    }
    return 0;
}

//-----------------------------------------------------------------------------
/// Highest file number first!
static int CompareFileVersion( const wxString& first, const wxString& second )
//-----------------------------------------------------------------------------
{
    const Version firstVersion( VersionFromString( ExtractVersionNumber( first ) ) );
    const Version secondVersion( VersionFromString( ExtractVersionNumber( second ) ) );
    return CompareFileVersion( firstVersion, secondVersion );
}

//-----------------------------------------------------------------------------
static wxString ExtractVersionNumberFromMVUArchiveName( const wxString& s )
//-----------------------------------------------------------------------------
{
    // <product>_Update-<version>.mvu
    static const wxString s_magicToken( wxT( "_Update" ) );
    const int pos = s.Find( s_magicToken );
    if( pos == wxNOT_FOUND )
    {
        return s.BeforeLast( wxT( '.' ) ).AfterLast( wxT( '-' ) );
    }
    return s.Mid( pos + s_magicToken.Length() ).BeforeLast( wxT( '.' ) ).AfterLast( wxT( '-' ) );
}

//-----------------------------------------------------------------------------
/// Highest file number first!
/**
 * - old versioning scheme: 'major.minor.subMinor.release' (e.g. 'mvBlueCOUGAR-X_Update-2.26.1138.0.mvu')
 * - new versioning scheme: 'yymmdd' (e.g. 'mvBlueCOUGAR-X_Update-190613.mvu')
 *
 * So
 * - a name with at least one dot is always older than one without
 * - when both names use the old scheme use the overloaded operators for the 'Version' structure
 * - when both names use the new scheme use a simple string compare
 */
int CompareMVUFileVersion( const wxString& first, const wxString& second )
//-----------------------------------------------------------------------------
{
    const wxString firstVersionString( ExtractVersionNumberFromMVUArchiveName( first ) );
    const wxString secondVersionString( ExtractVersionNumberFromMVUArchiveName( second ) );
    if( firstVersionString.Contains( wxT( "." ) ) && secondVersionString.Contains( wxT( "." ) ) )
    {
        // both archives use the old scheme
        const Version firstVersion( VersionFromString( firstVersionString ) );
        const Version secondVersion( VersionFromString( secondVersionString ) );
        return CompareFileVersion( firstVersion, secondVersion );
    }
    else if( !firstVersionString.Contains( wxT( "." ) ) && !secondVersionString.Contains( wxT( "." ) ) )
    {
        // both archives use the new scheme
        return secondVersionString.CmpNoCase( firstVersionString );
    }

    // if we get here one string uses the old, the other uses the new scheme. The new scheme always wins then!
    if( firstVersionString.Contains( wxT( "." ) ) )
    {
        return 1;
    }
    else if( secondVersionString.Contains( wxT( "." ) ) )
    {
        return -1;
    }
    return 0;
}

//-----------------------------------------------------------------------------
static bool GetNextNumber( wxString& str, long& number )
//-----------------------------------------------------------------------------
{
    wxString numberString = str.BeforeFirst( wxT( '_' ) );
    str = str.AfterFirst( wxT( '_' ) );
    return numberString.ToLong( &number );
}

//-----------------------------------------------------------------------------
wxString GetPlatformSpecificDownloadPath( const wxString& pathSuffix /* = wxEmptyString */ )
//-----------------------------------------------------------------------------
{
    static const wxString s_MVIMPACT_ACQUIRE_DATA_DIR( wxT( "MVIMPACT_ACQUIRE_DATA_DIR" ) );
    wxString path;
#if defined(linux) || defined(__linux) || defined(__linux__)
    if( ::wxGetEnv( s_MVIMPACT_ACQUIRE_DATA_DIR, &path ) )
    {
        AppendPathSeparatorIfNeeded( path );
        return path + pathSuffix;
    }
    return wxT( "/tmp/mv/" ) + pathSuffix;
#else
    if( ::wxGetEnv( s_MVIMPACT_ACQUIRE_DATA_DIR, &path ) )
    {
        AppendPathSeparatorIfNeeded( path );
        return path + pathSuffix;
    }
    else
    {
        static const wxString DOWNLOADS_DIR( wxT( "USERPROFILE" ) );
        ::wxGetEnv( DOWNLOADS_DIR, &path );
        AppendPathSeparatorIfNeeded( path );
        return path + wxT( "Downloads/" ) + pathSuffix;
    }
#endif // #if defined(linux) || defined(__linux) || defined(__linux__)
}

//=============================================================================
//============= implementation DeviceHandlerBlueDeviceBase ====================
//=============================================================================
//-----------------------------------------------------------------------------
DeviceHandlerBlueDeviceBase::DeviceHandlerBlueDeviceBase( mvIMPACT::acquire::Device* pDev, bool boSetIDSupported ) :
    DeviceHandler( pDev, boSetIDSupported ), temporaryFolder_(), GenICamFile_()
//-----------------------------------------------------------------------------
{
#if defined(linux) || defined(__linux) || defined(__linux__)
    temporaryFolder_ = wxT( "/tmp" );
#elif defined __WIN32__
    wxGetEnv( "TEMP", &temporaryFolder_ );
#else
#   error Nothing known about the target platform. This code might need adjustment!
#endif // #if defined(linux) || defined(__linux) || defined(__linux__)
    temporaryFolder_ += wxT( "/" );
}

//-----------------------------------------------------------------------------
DeviceHandlerBlueDeviceBase::~DeviceHandlerBlueDeviceBase()
//-----------------------------------------------------------------------------
{
}

//-----------------------------------------------------------------------------
void DeviceHandlerBlueDeviceBase::UserSetBackup( bool boSilentMode )
//-----------------------------------------------------------------------------
{
    if( pDev_->isOpen() )
    {
        pDev_->close();
    }
    pDev_->interfaceLayout.write( dilGenICam );
    SelectCustomGenICamFile( GenICamFile_ );
    pDev_->open();
    FunctionInterface fi( pDev_ );
    const wxString path = GetFullyQualifiedPathForUserSetBackup();
    TDMR_ERROR result = DMR_NO_ERROR;
    wxStopWatch stopWatch;
    static const int MAX_TIME_MILLISECONDS = 40000;
    static const int THREAD_INTERVAL_MILLISECONDS = 100;
    wxProgressDialog* pDlg = IsProgressDisplayActive() ? new wxProgressDialog( wxT( "Backing Up Device Data" ),
                             wxT( "Please wait...                                " ),
                             MAX_TIME_MILLISECONDS, // range
                             pParent_,
                             wxPD_AUTO_HIDE | wxPD_APP_MODAL | wxPD_ELAPSED_TIME ) : 0;
    SaveSettingThread thread( fi, result, path );
    RunThreadAndShowProgress( &thread, pDlg, THREAD_INTERVAL_MILLISECONDS, MAX_TIME_MILLISECONDS );
    DeleteElement( pDlg );
    if( result != DMR_NO_ERROR )
    {
        MessageToUser( wxT( "Warning" ), wxString::Format( wxT( "Failed to store '%s'. Error code: %d(%s). NOTE: This might only indicate that this device does not have a set stored at this position!\n" ), path.c_str(), result, ConvertedString( ImpactAcquireException::getErrorCodeAsString( result ) ).c_str() ), boSilentMode, wxOK | wxICON_INFORMATION );
    }
    pDev_->close();
}

//-----------------------------------------------------------------------------
void DeviceHandlerBlueDeviceBase::UserSetRestore( bool boSilentMode, const wxString& previousUserSetDefaultValueToRestore )
//-----------------------------------------------------------------------------
{
    if( pDev_->isOpen() )
    {
        pDev_->close();
    }
    pDev_->interfaceLayout.write( dilGenICam );
    SelectCustomGenICamFile();
    pDev_->open();
    FunctionInterface fi( pDev_ );
    const wxString path = GetFullyQualifiedPathForUserSetBackup();
    TDMR_ERROR result = DMR_NO_ERROR;
    wxStopWatch stopWatch;
    static const int MAX_TIME_MILLISECONDS = 40000;
    static const int THREAD_INTERVAL_MILLISECONDS = 100;
    wxProgressDialog* pDlg = IsProgressDisplayActive() ? new wxProgressDialog( wxT( "Restoring Device Data" ),
                             wxT( "Please wait...                                " ),
                             MAX_TIME_MILLISECONDS, // range
                             pParent_,
                             wxPD_AUTO_HIDE | wxPD_APP_MODAL | wxPD_ELAPSED_TIME ) : 0;
    LoadSettingThread thread( fi, result, path );
    RunThreadAndShowProgress( &thread, pDlg, THREAD_INTERVAL_MILLISECONDS, MAX_TIME_MILLISECONDS );
    DeleteElement( pDlg );
    if( result != DMR_NO_ERROR )
    {
        MessageToUser( wxT( "Warning" ), wxString::Format( wxT( "Failed to restore setting from setting file '%s'. Error code: %d(%s). NOTE: This might only indicate that this device does not have a set stored at this position!\n" ), path.c_str(), result, ConvertedString( ImpactAcquireException::getErrorCodeAsString( result ) ).c_str() ), boSilentMode, wxOK | wxICON_INFORMATION );
    }
    {
        // when trying to remove a non-existent file the result can be an annoying message box.
        // Therefore we switch off logging during the storage operation.
        wxLogNull logSuspendScope;
        wxRemoveFile( path );
    }
    try
    {
        const std::string previousUserSetDefaultValueToRestoreANSI( previousUserSetDefaultValueToRestore.mb_str() );
        mvIMPACT::acquire::GenICam::UserSetControl usc( pDev_ );
        usc.userSetSelector.writeS( previousUserSetDefaultValueToRestoreANSI );
        result = static_cast<TDMR_ERROR>( usc.userSetLoad.call() );
        if( result == DMR_NO_ERROR )
        {
            // The setting can still be loaded. Restore the old default value in non-volatile memory
            usc.userSetDefault.writeS( previousUserSetDefaultValueToRestoreANSI );
        }
        else if( pParent_ )
        {
            pParent_->WriteLogMessage( wxString::Format( wxT( "Failed to load user set '%s', will NOT restore the previous value of 'UserSetDefault'! Error: %s(%d(%s)).\n" ), previousUserSetDefaultValueToRestore.c_str(), ConvertedString( ExceptionFactory::getLastErrorString() ).c_str(), result, ConvertedString( ImpactAcquireException::getErrorCodeAsString( result ) ).c_str() ) );
        }
    }
    catch( const ImpactAcquireException& e )
    {
        if( pParent_ )
        {
            pParent_->WriteLogMessage( wxString::Format( wxT( "Failed to set 'UserSetSelector' to the UserSet specified in 'UserSetDefault'! Error: %s(%d(%s)).\n" ), ConvertedString( e.getErrorString() ).c_str(), e.getErrorCode(), ConvertedString( e.getErrorCodeAsString() ).c_str() ) );
        }
    }
    pDev_->close();
}

//-----------------------------------------------------------------------------
wxString DeviceHandlerBlueDeviceBase::SetUserSetDefault( const wxString& userSetDefaultValue )
//-----------------------------------------------------------------------------
{
    if( pDev_->isOpen() )
    {
        pDev_->close();
    }
    pDev_->interfaceLayout.write( dilGenICam );
    SelectCustomGenICamFile( GenICamFile_ );
    pDev_->open();

    wxString previousValue( wxT( "Default" ) );
    try
    {
        const std::string userSetDefaultValueANSI( userSetDefaultValue.mb_str() );
        mvIMPACT::acquire::GenICam::UserSetControl usc( pDev_ );
        if( usc.userSetDefault.isValid() )
        {
            previousValue = ConvertedString( usc.userSetDefault.readS() );
            usc.userSetDefault.writeS( userSetDefaultValueANSI );
        }
    }
    catch( const ImpactAcquireException& e )
    {
        if( pParent_ )
        {
            pParent_->WriteLogMessage( wxString::Format( wxT( "Failed to set 'UserSetDefault' to '%s'! Error: %d(%s).\n" ), userSetDefaultValue.c_str(), e.getErrorCode(), ConvertedString( e.getErrorCodeAsString() ).c_str() ) );
        }
    }
    pDev_->close();
    return previousValue;
}

//-----------------------------------------------------------------------------
void DeviceHandlerBlueDeviceBase::WaitForDeviceToReappear( const wxString& serial )
//-----------------------------------------------------------------------------
{
    wxStopWatch stopWatch;
    static const int MAX_TIME_MILLISECONDS = 180000;
    static const int THREAD_INTERVAL_MILLISECONDS = 3000;
    wxProgressDialog* pDlg = IsProgressDisplayActive() ? new wxProgressDialog( wxT( "Rebooting device" ),
                             wxT( "Please wait...                                " ),
                             MAX_TIME_MILLISECONDS, // range
                             pParent_,     // parent
                             wxPD_AUTO_HIDE | wxPD_APP_MODAL | wxPD_ELAPSED_TIME ) : 0;
    RebootDeviceThread rebootThread( pDev_, MAX_TIME_MILLISECONDS / 1000, THREAD_INTERVAL_MILLISECONDS / 1000 );
    RunThreadAndShowProgress( &rebootThread, pDlg, 100, MAX_TIME_MILLISECONDS );
    DeleteElement( pDlg );
    if( ( pDev_->state.read() == dsPresent ) && pParent_ )
    {
        pParent_->WriteLogMessage( wxString::Format( wxT( "Rebooting device '%s' took %ld ms.\n" ), serial.c_str(), stopWatch.Time() ) );
    }
}

//-----------------------------------------------------------------------------
void DeviceHandlerBlueDeviceBase::SelectCustomGenICamFile( const wxString& descriptionFile/* = wxEmptyString */ )
//-----------------------------------------------------------------------------
{
    ComponentLocator locatorDevice( pDev_->hDev() );
    PropertyI descriptionToUse( locatorDevice.findComponent( "DescriptionToUse" ) );
    PropertyS customDescriptionFilename( locatorDevice.findComponent( "CustomDescriptionFileName" ) );
    if( descriptionFile.IsEmpty() == true )
    {
        descriptionToUse.writeS( "XMLLocation0" );
    }
    else
    {
        descriptionToUse.writeS( "CustomFile" );
        customDescriptionFilename.write( std::string( wxConvCurrent->cWX2MB( descriptionFile.c_str() ) ) );
    }
}

//=============================================================================
//=================== implementation DeviceHandlerBlueDevice ==================
//=============================================================================
//-----------------------------------------------------------------------------
DeviceHandlerBlueDevice::DeviceHandlerBlueDevice( mvIMPACT::acquire::Device* pDev ) : DeviceHandlerBlueDeviceBase( pDev, true ), product_( pgUnknown ),
    firmwareUpdateFileName_(), firmwareUpdateFileFilter_(), firmwareUpdateFolder_()
//-----------------------------------------------------------------------------
{
    ConvertedString product( pDev_->product.read() );
    if( product.Find( wxT( "mvBlueCOUGAR-S" ) ) != wxNOT_FOUND )
    {
        product_ = pgBlueCOUGAR_S;
    }
    else if( IsBlueCOUGAR_XT( pDev_ ) )
    {
        product_ = pgBlueCOUGAR_XT;
    }
    else if( IsBlueCOUGAR_X( pDev_ ) )
    {
        product_ = pgBlueCOUGAR_X;
    }
    else if( IsBlueCOUGAR_Y() )
    {
        product_ = pgBlueCOUGAR_Y;
    }
    else if( IsBlueFOX3( pDev_ ) )
    {
        product_ = pgBlueFOX3;
    }

    static const wxString s_MVIMPACT_ACQUIRE_DATA_DIR( wxT( "MVIMPACT_ACQUIRE_DATA_DIR" ) );
    static const wxString s_MVIMPACT_ACQUIRE_SOURCE_DIR( wxT( "MVIMPACT_ACQUIRE_SOURCE_DIR" ) );

    if( !::wxGetEnv( s_MVIMPACT_ACQUIRE_DATA_DIR, &firmwareUpdateFolder_ ) )
    {
        if( pParent_ )
        {
            pParent_->WriteErrorMessage( wxT( "Can't install new firmware automatically as a crucial environment variable(MVIMPACT_ACQUIRE_DATA_DIR) is missing...\nPlease select the folder containing the files manually.\n" ) );
        }
        if( ::wxGetEnv( s_MVIMPACT_ACQUIRE_SOURCE_DIR, NULL ) )
        {
            firmwareUpdateFolder_ = GetPlatformSpecificDownloadPath();
        }
    }
    AppendPathSeparatorIfNeeded( firmwareUpdateFolder_ );
}

//-----------------------------------------------------------------------------
void DeviceHandlerBlueDevice::DetermineValidFirmwareChoices( const wxString& productFromManufacturerInfo, const wxString& product, const FileEntryContainer& fileEntries, const FileEntryContainer::size_type cnt, const Version& deviceVersion, wxArrayString& choices, wxArrayString& unusableFiles )
//-----------------------------------------------------------------------------
{
    for( FileEntryContainer::size_type i = 0; i < cnt; i++ )
    {
        std::vector<std::map<wxString, SuitableProductKey>::const_iterator> vSuitableFirmwareIterators = GetSuitableFirmwareIterators( fileEntries[i], product, productFromManufacturerInfo );
        const std::vector<std::map<wxString, SuitableProductKey>::const_iterator>::size_type suitableFirmwareIteratorCnt = vSuitableFirmwareIterators.size();
        bool boSuitableFirmwareFound = false;
        for( std::vector<std::map<wxString, SuitableProductKey>::const_iterator>::size_type j = 0; j < suitableFirmwareIteratorCnt; j++ )
        {
            if( ( vSuitableFirmwareIterators[j] != fileEntries[i].suitableProductKeys_.end() ) && ( fileEntries[i].type_ == wxString( wxT( "Firmware" ) ) ) &&
                IsVersionWithinRange( deviceVersion, vSuitableFirmwareIterators[j]->second.revisionMin_, vSuitableFirmwareIterators[j]->second.revisionMax_ ) )
            {
                choices.Add( wxString::Format( wxT( "%s(%s, %s file version: %s)" ), vSuitableFirmwareIterators[j]->second.name_.c_str(), fileEntries[i].name_.c_str(), fileEntries[i].description_.c_str(), GetStringRepresentationOfFirmwareVersion( fileEntries[i].version_ ).c_str() ) );
                boSuitableFirmwareFound = true;
                break;
            }
        }
        if( !boSuitableFirmwareFound )
        {
            wxString suitableProducts;
            std::map<wxString, SuitableProductKey>::const_iterator it = fileEntries[i].suitableProductKeys_.begin();
            while( it != fileEntries[i].suitableProductKeys_.end() )
            {
                suitableProducts.Append( wxString::Format( wxT( "  %s(%s, rev. %ld.%ld.%ld - %ld.%ld.%ld)\n" ), it->first.c_str(), it->second.name_.c_str(),
                                         it->second.revisionMin_.major_, it->second.revisionMin_.minor_, it->second.revisionMin_.release_,
                                         it->second.revisionMax_.major_, it->second.revisionMax_.minor_, it->second.revisionMax_.release_ ) );
                ++it;
            }
            unusableFiles.Add( wxString::Format( wxT( "%s(%s, file version: %s), suitable products:\n %s" ), fileEntries[i].name_.c_str(), fileEntries[i].description_.c_str(), GetStringRepresentationOfFirmwareVersion( fileEntries[i].version_ ).c_str(), suitableProducts.c_str() ) );
        }
    }
    choices.Sort( CompareFileVersion );
}

//-----------------------------------------------------------------------------
bool DeviceHandlerBlueDevice::ExtractFileVersion( const wxString& fileName, Version& fileVersion ) const
//-----------------------------------------------------------------------------
{
    static const wxString TOKEN( wxT( "_GigE_" ) );
    int pos = fileName.Find( TOKEN.c_str() );
    if( pos == wxNOT_FOUND )
    {
        return false;
    }
    wxString versionString( fileName.Mid( pos + TOKEN.Length() ) );
    return ( GetNextNumber( versionString, fileVersion.major_ ) &&
             GetNextNumber( versionString, fileVersion.minor_ ) &&
             GetNextNumber( versionString, fileVersion.subMinor_ ) );
}

//-----------------------------------------------------------------------------
bool DeviceHandlerBlueDevice::GetFileFromArchive( const wxString& firmwareFileAndPath, const char* pArchive, size_t archiveSize, const wxString& filename, auto_array_ptr<char>& data, DeviceConfigureFrame* pParent )
//-----------------------------------------------------------------------------
{
    wxMemoryInputStream zipData( pArchive, archiveSize );
    wxZipInputStream zipStream( zipData );
    wxZipEntry* pZipEntry = 0;
    while( ( pZipEntry = zipStream.GetNextEntry() ) != 0 )
    {
        /// GetNextEntry returns the ownership to the object!
        ObjectDeleter<wxZipEntry> ptrGuard( pZipEntry );
        if( !zipStream.OpenEntry( *pZipEntry ) )
        {
            if( pParent )
            {
                pParent->WriteErrorMessage( wxString::Format( wxT( "Failed to open zip entry of archive %s\n" ), firmwareFileAndPath.c_str() ) );
            }
            continue;
        }

        if( pZipEntry->IsDir() )
        {
            continue;
        }

        int pos = pZipEntry->GetInternalName().Find( filename.c_str() );
        if( pos == wxNOT_FOUND )
        {
            continue;
        }

        if( pZipEntry->GetInternalName().Mid( pos ) == filename )
        {
            data.realloc( static_cast<size_t>( pZipEntry->GetSize() ) );
            zipStream.Read( data.get(), data.parCnt() );
            return true;
        }
    }
    return false;
}

//-----------------------------------------------------------------------------
int DeviceHandlerBlueDevice::GetLatestFirmwareFromList( Version& latestFirmwareVersion, const FileEntryContainer& fileEntries, size_t& fileEntryPosition ) const
//-----------------------------------------------------------------------------
{
    const FileEntryContainer::size_type cnt = fileEntries.size();
    const wxString product( ConvertedString( pDev_->product.read() ) );
    const wxString productFromManufacturerInfo( GetProductFromManufacturerInfo( pDev_ ) );
    const wxString deviceVersionAsString( pDev_->deviceVersion.isValid() ? ConvertedString( pDev_->deviceVersion.read() ) : wxString( wxT( "1.0" ) ) );
    const Version deviceVersion( VersionFromString( deviceVersionAsString ) );
    wxArrayString choices;
    for( FileEntryContainer::size_type i = 0; i < cnt; i++ )
    {
        std::vector<std::map<wxString, SuitableProductKey>::const_iterator> vSuitableFirmwareIterators = GetSuitableFirmwareIterators( fileEntries[i], product, productFromManufacturerInfo );
        const std::vector<std::map<wxString, SuitableProductKey>::const_iterator>::size_type suitableFirmwareIteratorCnt = vSuitableFirmwareIterators.size();
        for( std::vector<std::map<wxString, SuitableProductKey>::const_iterator>::size_type j = 0; j < suitableFirmwareIteratorCnt; j++ )
        {
            if( ( vSuitableFirmwareIterators[j] != fileEntries[i].suitableProductKeys_.end() ) && ( fileEntries[i].type_ == wxString( wxT( "Firmware" ) ) ) &&
                IsVersionWithinRange( deviceVersion, vSuitableFirmwareIterators[j]->second.revisionMin_, vSuitableFirmwareIterators[j]->second.revisionMax_ ) )
            {
                choices.Add( wxString::Format( wxT( "%s(%s, file version: %s)" ), fileEntries[i].name_.c_str(), fileEntries[i].description_.c_str(), GetStringRepresentationOfFirmwareVersion( fileEntries[i].version_ ).c_str() ) );
                break;
            }
        }
    }
    choices.Sort( CompareFileVersion );

    wxString selection;
    if( choices.IsEmpty() )
    {
        return urInvalidFileSelection;
    }

    selection = choices[0].BeforeFirst( wxT( '(' ) );

    const Version currentFirmwareVersion = VersionFromString( ConvertedString( pDev_->firmwareVersion.readS() ) );
    for( FileEntryContainer::size_type i = 0; i < cnt; i++ )
    {
        if( fileEntries[i].name_ == selection )
        {
            fileEntryPosition = i;
            if( currentFirmwareVersion >= fileEntries[i].version_ )
            {
                latestFirmwareVersion = fileEntries[i].version_;
                return urOperationCanceled;
            }
            else
            {
                latestFirmwareVersion = fileEntries[i].version_;
                break;
            }
        }
    }
    return urOperationSuccessful;
}

//-----------------------------------------------------------------------------
int DeviceHandlerBlueDevice::GetLatestLocalFirmwareVersion( Version& latestFirmwareVersion ) const
//-----------------------------------------------------------------------------
{
    ProductToFileEntryMap throwAwayMap;
    return GetLatestLocalFirmwareVersionToMap( latestFirmwareVersion, throwAwayMap );
}

//-----------------------------------------------------------------------------
int DeviceHandlerBlueDevice::GetLatestLocalFirmwareVersionToMap( Version& latestFirmwareVersion, ProductToFileEntryMap& m_productToFirmwareFilesLocalMap ) const
//-----------------------------------------------------------------------------
{
    if( product_ == pgBlueCOUGAR_S )
    {
        return urFeatureUnsupported;
    }
    if( !::wxDirExists( firmwareUpdateFolder_ ) )
    {
        return urFileIOError;
    }

    wxString firmwareFileAndPath;
    wxString firmwareFileName;
    wxArrayString firmwareFiles;
    wxStringTokenizer firmwareUpdateFileFiltersTokens( firmwareUpdateFileFilter_, "|" );
    const size_t firmwareUpdateFileFiltersTokensCount = firmwareUpdateFileFiltersTokens.CountTokens();
    for( size_t i = 0; i < firmwareUpdateFileFiltersTokensCount; i++ )
    {
        wxDir::GetAllFiles( firmwareUpdateFolder_, &firmwareFiles, firmwareUpdateFileFiltersTokens.GetNextToken(), wxDIR_FILES );
    }
    if( !firmwareFiles.IsEmpty() )
    {
        firmwareFiles.Sort( CompareMVUFileVersion );
        firmwareFileAndPath = firmwareFiles[0];
    }
    if( !::wxFileExists( firmwareFileAndPath ) )
    {
        return urFileIOError;
    }
    if( firmwareFileAndPath.Find( wxT( "/" ) ) != wxNOT_FOUND )
    {
        firmwareFileName = firmwareFileAndPath.AfterLast( wxT( '/' ) );
    }
    else
    {
        firmwareFileName = firmwareFileAndPath.AfterLast( wxT( '\\' ) );
    }
    PackageDescriptionFileParser parser;

    if( !packageDescription_.IsEmpty() )
    {
        const std::string downloadedFileContentANSI( packageDescription_.mb_str() );
        DeviceHandlerBlueDevice::ParsePackageDescriptionFromBuffer( parser, downloadedFileContentANSI.c_str(), downloadedFileContentANSI.length(), pParent_ );
    }
    else
    {
        auto_array_ptr<char> pBuffer( 0 );
        const TUpdateResult result = ParseUpdatePackage( parser, firmwareFileAndPath, 0, pBuffer );
        if( result != urOperationSuccessful )
        {
            return result;
        }
    }

    const FileEntryContainer& fileEntries = parser.GetResults();
    size_t fileEntryPosition = 0;
    int result = GetLatestFirmwareFromList( latestFirmwareVersion, fileEntries, fileEntryPosition );
    if( result != urInvalidFileSelection )
    {
        m_productToFirmwareFilesLocalMap.insert( std::pair<std::string, FileEntry>( pDev_->product.read(), fileEntries[fileEntryPosition] ) );
        m_productToFirmwareFilesLocalMap.find( pDev_->product.read() )->second.localMVUFileName_ = firmwareFileName;
        m_productToFirmwareFilesLocalMap.find( pDev_->product.read() )->second.onlineUpdateInfos_.productFamily_ = pDev_->family.read();
    }
    return result;
}

//-----------------------------------------------------------------------------
wxString DeviceHandlerBlueDevice::GetProductFromManufacturerInfo( Device* pDev )
//-----------------------------------------------------------------------------
{
    wxString product;
    if( pDev->manufacturerSpecificInformation.isValid() )
    {
        std::vector<std::string> v;
        std::vector<std::string>::size_type cnt = split( pDev->manufacturerSpecificInformation.readS(), std::string( ";" ), v );
        if( cnt > 0 )
        {
            // all parameters here use the <param>=<value> syntax (e.g. FW=3.5.67.3) except the product type
            // which is always the LAST token. Tokens are separated by a single ';'. Thus if a device specifies
            // a device type here this might look like this:
            // FW=2.17.360.0;DS1-024ZG-11111-00
            // So if the last token does NOT contain a '=' we assume this to be the product type!
            std::vector<std::string> keyValPair;
            if( split( v[cnt - 1], std::string( "=" ), keyValPair ) == 1 )
            {
                product = ConvertedString( v[cnt - 1] );
            }
        }
    }
    return product;
}

//-----------------------------------------------------------------------------
std::vector<std::map<wxString, SuitableProductKey>::const_iterator> DeviceHandlerBlueDevice::GetSuitableFirmwareIterators( const FileEntry& entry, const wxString& product, const wxString& productFromManufacturerInfo ) const
//-----------------------------------------------------------------------------
{
    std::vector<std::map<wxString, SuitableProductKey>::const_iterator> results;
    const std::map<wxString, SuitableProductKey>::const_iterator itEND = entry.suitableProductKeys_.end();
    std::map<wxString, SuitableProductKey>::const_iterator it = itEND;
    if( !productFromManufacturerInfo.IsEmpty() )
    {
        it = entry.suitableProductKeys_.begin();
        while( it != itEND )
        {
            if( productFromManufacturerInfo.Matches( it->first ) )
            {
                results.push_back( it );
            }
            ++it;
        }
    }
    if( it == itEND )
    {
        it = entry.suitableProductKeys_.begin();
        while( it != itEND )
        {
            if( product.Matches( it->first ) )
            {
                results.push_back( it );
            }
            ++it;
        }
    }
    return results;
}

//-----------------------------------------------------------------------------
bool DeviceHandlerBlueDevice::IsBlueCOUGAR_X( Device* pDev )
//-----------------------------------------------------------------------------
{
    if( ( ConvertedString( pDev->product.read() ).Find( wxT( "mvBlueCOUGAR-X" ) ) != wxNOT_FOUND ) ||
        IsBlueCOUGAR_X_XAS( pDev ) || IsBlueCOUGAR_X_BAS( pDev ) )
    {
        return true;
    }

    ComponentLocator locator( pDev->hDev() );
    PropertyI64 prop;
    locator.bindComponent( prop, "DeviceMACAddress" );
    if( !prop.isValid() )
    {
        return false;
    }

    const int64_type MAC( prop.read() );
    return ( ( ( MAC >= 0x0000000c8d508001LL ) && ( MAC <= 0x0000000c8d59a7bfLL ) ) ||
             ( ( MAC >= 0x0000000c8d600001LL ) && ( MAC <= 0x0000000c8d617fffLL ) ) ||
             ( ( MAC >= 0x0000000c8d630001LL ) && ( MAC <= 0x0000000c8d637fffLL ) ) ||
             ( ( MAC >= 0x0000000c8d700001LL ) && ( MAC <= 0x0000000c8d707fffLL ) ) ||
             ( ( MAC >= 0x0000000c8d708001LL ) && ( MAC <= 0x0000000c8d70bfffLL ) ) ||
             ( ( MAC >= 0x0000000c8d70c001LL ) && ( MAC <= 0x0000000c8d70cfffLL ) ) ||
             ( ( MAC >= 0x0000000c8d710001LL ) && ( MAC <= 0x0000000c8d717fffLL ) ) );
}

//-----------------------------------------------------------------------------
bool DeviceHandlerBlueDevice::IsBlueCOUGAR_XT( Device* pDev )
//-----------------------------------------------------------------------------
{
    if( ( ConvertedString( pDev->product.read() ).Find( wxT( "BVS CA-GT1" ) ) != wxNOT_FOUND ) )
    {
        return true;
    }
    ComponentLocator locator( pDev->hDev() );
    PropertyI64 prop;
    locator.bindComponent( prop, "DeviceMACAddress" );
    if( !prop.isValid() )
    {
        return false;
    }
    return IsBlueCOUGAR_XT_MACAddress( prop.read() );
}

//-----------------------------------------------------------------------------
bool DeviceHandlerBlueDevice::IsBlueCOUGAR_Y( void ) const
//-----------------------------------------------------------------------------
{
    if( ConvertedString( pDev_->product.read() ).Find( wxT( "mvBlueCOUGAR-Y" ) ) != wxNOT_FOUND )
    {
        return true;
    }

    ComponentLocator locator( pDev_->hDev() );
    PropertyI64 prop;
    locator.bindComponent( prop, "DeviceMACAddress" );
    if( !prop.isValid() )
    {
        return false;
    }

    const int64_type MAC( prop.read() );
    return( ( MAC >= 0x0000000c8d620001LL ) && ( MAC <= 0x0000000c8d627fffLL ) );
}

//-----------------------------------------------------------------------------
bool DeviceHandlerBlueDevice::IsFirmwareUpdateMeaningless( bool boSilentMode, const Version& deviceFWVersion, const Version& selectedFWVersion, const wxString& defaultFWArchive, const Version& defaultFolderFWVersion ) const
//-----------------------------------------------------------------------------
{
    wxString issues;
    if( deviceFWVersion >= selectedFWVersion )
    {
        issues.append( wxString::Format( wxT( "You are about to %sgrade from firmware version %ld.%ld.%ld to %ld.%ld.%ld so the new version will be %s the one currently running on the device!\n\n" ),
                                         ( selectedFWVersion == deviceFWVersion ) ? wxT( "up" ) : wxT( "down" ),
                                         deviceFWVersion.major_, deviceFWVersion.minor_, deviceFWVersion.subMinor_,
                                         selectedFWVersion.major_, selectedFWVersion.minor_, selectedFWVersion.subMinor_,
                                         ( selectedFWVersion == deviceFWVersion ) ? wxT( "equal to" ) : wxT( "older than" ) ) );
    }
    if( defaultFolderFWVersion > selectedFWVersion )
    {
        issues.append( wxString::Format( wxT( "The firmware version in the MATRIX VISION default firmware folder (%s) is newer (%ld.%ld.%ld) than the one currently selected (%ld.%ld.%ld)!\n\n" ),
                                         defaultFWArchive.c_str(), defaultFolderFWVersion.major_, defaultFolderFWVersion.minor_, defaultFolderFWVersion.subMinor_, selectedFWVersion.major_, selectedFWVersion.minor_, selectedFWVersion.subMinor_ ) );
    }
    if( !issues.IsEmpty() )
    {
        issues.append( wxT( "Should the operation continue?\n" ) );
        if( !MessageToUser( wxT( "Firmware Update Issues Detected!" ), issues, boSilentMode, wxCANCEL | wxOK | wxICON_EXCLAMATION ) )
        {
            if( pParent_ )
            {
                pParent_->WriteLogMessage( wxString::Format( wxT( "Firmware update canceled for device %s(%s).\n" ), ConvertedString( pDev_->serial.read() ).c_str(), ConvertedString( pDev_->product.read() ).c_str() ) );
            }
            return true;
        }
    }
    return false;
}

//-----------------------------------------------------------------------------
DeviceHandler::TUpdateResult DeviceHandlerBlueDevice::ParseUpdatePackage( PackageDescriptionFileParser& parser, const wxString& firmwareFileAndPath, DeviceConfigureFrame* pParent, auto_array_ptr<char>& pBuffer )
//-----------------------------------------------------------------------------
{
    // first check if the file actually exists as the 'wxFFile' constructor will produce a modal dialog box if not...
    if( !::wxFileExists( firmwareFileAndPath ) )
    {
        if( pParent )
        {
            pParent->WriteErrorMessage( wxString::Format( wxT( "Could not open update archive %s.\n" ), firmwareFileAndPath.c_str() ) );
        }
        return urFileIOError;
    }
    wxFFile archive( firmwareFileAndPath.c_str(), wxT( "rb" ) );
    if( !archive.IsOpened() )
    {
        if( pParent )
        {
            pParent->WriteErrorMessage( wxString::Format( wxT( "Could not open update archive %s.\n" ), firmwareFileAndPath.c_str() ) );
        }
        return urFileIOError;
    }

    pBuffer.realloc( static_cast<size_t>( archive.Length() ) );
    size_t bytesRead = archive.Read( pBuffer.get(), pBuffer.parCnt() );
    if( bytesRead != static_cast<size_t>( archive.Length() ) )
    {
        if( pParent )
        {
            wxString lengthS;
            lengthS << archive.Length();
            pParent->WriteErrorMessage( wxString::Format( wxT( "Could not read full content of archive '%s'(wanted: %s, bytes, got %d bytes).\n" ), firmwareFileAndPath.c_str(), lengthS.c_str(), bytesRead ) );
        }
        return urFileIOError;
    }

    wxMemoryInputStream zipData( pBuffer.get(), pBuffer.parCnt() );
    wxZipInputStream zipStream( zipData );
    int fileCount = zipStream.GetTotalEntries();
    if( fileCount < 2 )
    {
        if( pParent )
        {
            pParent->WriteErrorMessage( wxString::Format( wxT( "The archive %s claims to contain %d file(s) while at least 2 are needed\n" ), firmwareFileAndPath.c_str(), fileCount ) );
        }
        return urInvalidFileSelection;
    }

    auto_array_ptr<char> pPackageDescription;
    if( !GetFileFromArchive( firmwareFileAndPath, pBuffer.get(), pBuffer.parCnt(), wxString( wxT( "packageDescription.xml" ) ), pPackageDescription, pParent ) )
    {
        if( pParent )
        {
            pParent->WriteErrorMessage( wxString::Format( wxT( "Could not extract package description from archive %s.\n" ), firmwareFileAndPath.c_str() ) );
        }
        return urFileIOError;
    }
    return ParsePackageDescriptionFromBuffer( parser, pPackageDescription.get(), pPackageDescription.parCnt(), pParent );
}

//-----------------------------------------------------------------------------
DeviceHandler::TUpdateResult DeviceHandlerBlueDevice::ParsePackageDescriptionFromBuffer( PackageDescriptionFileParser& parser, const char* pPackageDescription, const size_t bufferSize, DeviceConfigureFrame* pParent )
//-----------------------------------------------------------------------------
{
    parser.Create();
    const bool fSuccess = parser.Parse( pPackageDescription, static_cast<int>( bufferSize ), true );
    if( !fSuccess || ( parser.GetErrorCode() != XML_ERROR_NONE ) )
    {
        if( pParent )
        {
            pParent->WriteErrorMessage( wxString::Format( wxT( "Package description file parser error(fSuccess: %d, XML error: %d(%s)).\n" ), fSuccess, parser.GetErrorCode(), parser.GetErrorString( parser.GetErrorCode() ) ) );
        }
        return urFileIOError;
    }

    if( !parser.GetLastError().empty() )
    {
        if( pParent )
        {
            pParent->WriteErrorMessage( wxString::Format( wxT( "Package description file parser error(last error: %s).\n" ), parser.GetLastError().c_str() ) );
        }
        return urFileIOError;
    }

    return urOperationSuccessful;
}

//-----------------------------------------------------------------------------
void DeviceHandlerBlueDevice::RebootDevice( bool boSilentMode, const wxString& serial )
//-----------------------------------------------------------------------------
{
    mvIMPACT::acquire::GenICam::DeviceControl dc( pDev_ );
    if( dc.deviceReset.isValid() )
    {
        const int deviceResetResult = dc.deviceReset.call();
        if( static_cast<TDMR_ERROR>( deviceResetResult ) != DMR_NO_ERROR )
        {
            MessageToUser( wxT( "Update Result" ), wxString::Format( wxT( "Update successful but resetting device failed. Please disconnect and reconnect device %s now to activate the new firmware. Error reported from driver: %d(%s)." ), ConvertedString( serial ).c_str(), deviceResetResult, ConvertedString( ImpactAcquireException::getErrorCodeAsString( deviceResetResult ) ).c_str() ), boSilentMode, wxOK | wxICON_INFORMATION );
        }
    }
    else
    {
        MessageToUser( wxT( "Update Result" ), wxString::Format( wxT( "Update successful. Please disconnect and reconnect device %s now to activate the new firmware." ), ConvertedString( serial ).c_str() ), boSilentMode, wxOK | wxICON_INFORMATION );
    }

    pDev_->close();
    WaitForDeviceToReappear( serial );
}

//-----------------------------------------------------------------------------
void DeviceHandlerBlueDevice::SetCustomFirmwareFile( const wxString& customFirmwareFile )
//-----------------------------------------------------------------------------
{
    if( !customFirmwareFile.IsEmpty() )
    {
        firmwareUpdateFileName_ = customFirmwareFile;
    }
}

//-----------------------------------------------------------------------------
void DeviceHandlerBlueDevice::SetCustomFirmwarePath( const wxString& customFirmwarePath )
//-----------------------------------------------------------------------------
{
    if( !customFirmwarePath.IsEmpty() )
    {
        firmwareUpdateFolder_ = customFirmwarePath;
    }
}

//-----------------------------------------------------------------------------
int DeviceHandlerBlueDevice::UpdateCOUGAR_SDevice( bool boSilentMode )
//-----------------------------------------------------------------------------
{
    ConvertedString serial( pDev_->serial.read() );
    wxString firmwareFileAndPath;
    wxString descriptionFileAndPath;
    bool boGotDescriptionFile = false;
    if( boSilentMode )
    {
        firmwareFileAndPath = firmwareUpdateFolder_ + wxString( wxT( "/" ) ) + firmwareUpdateFileName_;
        wxArrayString descriptionFiles;
        wxDir::GetAllFiles( firmwareUpdateFolder_, &descriptionFiles, wxT( "MATRIXVISION_mvBlueCOUGAR-S_GigE_*.zip" ) );
        const size_t cnt = descriptionFiles.GetCount();
        Version highestVersion;
        for( size_t i = 0; i < cnt; i++ )
        {
            Version currentVersion;
            wxFileName fileName( descriptionFiles[i] );
            if( ExtractFileVersion( fileName.GetName(), currentVersion ) )
            {
                if( highestVersion < currentVersion )
                {
                    descriptionFileAndPath = descriptionFiles[i];
                    boGotDescriptionFile = true;
                    highestVersion = currentVersion;
                }
            }
            else if( pParent_ )
            {
                pParent_->WriteLogMessage( wxString::Format( wxT( "Failed to parse filenames major version number(%s).\n" ), descriptionFiles[i].c_str() ) );
            }
        }
    }

    if( !boGotDescriptionFile )
    {
        wxFileDialog fileDlg( pParent_, wxT( "Please select the firmware file" ), firmwareUpdateFolder_, firmwareUpdateFileName_, wxString::Format( wxT( "%s firmware file (%s)|%s" ), ConvertedString( pDev_->product.read() ).c_str(), firmwareUpdateFileFilter_.c_str(), firmwareUpdateFileFilter_.c_str() ), wxFD_OPEN | wxFD_FILE_MUST_EXIST );
        if( fileDlg.ShowModal() != wxID_OK )
        {
            if( pParent_ )
            {
                pParent_->WriteLogMessage( wxString::Format( wxT( "Firmware update canceled for device %s(%s).\n" ), ConvertedString( serial ).c_str(), ConvertedString( pDev_->product.read() ).c_str() ) );
            }
            return urOperationCanceled;
        }

        if( fileDlg.GetFilename().Find( firmwareUpdateFileName_.c_str() ) != 0 )
        {
            if( pParent_ )
            {
                pParent_->WriteLogMessage( wxString::Format( wxT( "The file %s is not meant for updating device %s(%s).\n" ), fileDlg.GetFilename().c_str(), serial.c_str(), ConvertedString( pDev_->product.read() ).c_str() ), wxTextAttr( wxColour( 255, 0, 0 ) ) );
            }
            return urInvalidFileSelection;
        }

        firmwareFileAndPath = fileDlg.GetPath();

        wxFileDialog fileDlgDescriptionFile( pParent_, wxT( "Please select the GenICam description file that came with the firmware" ), firmwareUpdateFolder_, wxT( "" ), wxString::Format( wxT( "%s GenICam description file (MATRIXVISION_mvBlueCOUGAR-S_GigE*.zip)|MATRIXVISION_mvBlueCOUGAR-S_GigE*.zip" ), ConvertedString( pDev_->product.read() ).c_str() ), wxFD_OPEN | wxFD_FILE_MUST_EXIST );
        if( fileDlgDescriptionFile.ShowModal() != wxID_OK )
        {
            if( pParent_ )
            {
                pParent_->WriteLogMessage( wxString::Format( wxT( "Firmware update canceled for device %s(%s).\n" ), ConvertedString( serial ).c_str(), ConvertedString( pDev_->product.read() ).c_str() ) );
            }
            return urOperationCanceled;
        }

        if( fileDlgDescriptionFile.GetFilename().Find( wxT( "MATRIXVISION_mvBlueCOUGAR-S_GigE" ) ) != 0 )
        {
            if( pParent_ )
            {
                pParent_->WriteLogMessage( wxString::Format( wxT( "The file %s is not meant for updating device %s(%s).\n" ), fileDlgDescriptionFile.GetFilename().c_str(), serial.c_str(), ConvertedString( pDev_->product.read() ).c_str() ), wxTextAttr( wxColour( 255, 0, 0 ) ) );
            }
            return urInvalidFileSelection;
        }

        descriptionFileAndPath = fileDlgDescriptionFile.GetPath();
    }

    if( !MessageToUser( wxT( "Firmware Update" ), wxString::Format( wxT( "Are you sure you want to update the firmware and description file of device %s?\nThis might take several minutes during which the application will not react.\n\nPlease be patient and do NOT disconnect the device from the power supply during this time." ), serial.c_str() ), boSilentMode, wxNO_DEFAULT | wxYES_NO | wxICON_INFORMATION ) )
    {
        if( pParent_ )
        {
            pParent_->WriteLogMessage( wxString::Format( wxT( "Firmware update canceled for device %s(%s).\n" ), ConvertedString( serial ).c_str(), ConvertedString( pDev_->product.read() ).c_str() ) );
        }
        return urOperationCanceled;
    }

    int result = UploadFile( descriptionFileAndPath, descriptionFileAndPath );
    if( result != urOperationSuccessful )
    {
        return result;
    }
    result = UploadFile( firmwareFileAndPath, descriptionFileAndPath );
    if( result == urOperationSuccessful )
    {
        MessageToUser( wxT( "Information" ), wxT( "The Firmware has been updated. The new Firmware is active now and the device can be used again by other applications." ), boSilentMode, wxOK | wxICON_INFORMATION );
    }
    return result;
}

//-----------------------------------------------------------------------------
int DeviceHandlerBlueDevice::UpdateDevice( bool boSilentMode, bool boPersistentUserSets, bool boUseOnlineArchive )
//-----------------------------------------------------------------------------
{
    ConvertedString serial( pDev_->serial.read() );
    wxString firmwareFileAndPath;
    wxString descriptionFileAndPath;

    DeviceComponentLocator locator( pDev_->hDev() );
    Method updateBootProgrammer;
    locator.bindComponent( updateBootProgrammer, "UpdateBootProgrammer@i" );
    if( updateBootProgrammer.isValid() )
    {
        if( !boSilentMode )
        {
            wxMessageDialog outdatedBootProgrammerDlg( NULL,
                    wxString::Format( wxT( "An outdated boot programmer has been detected running on device %s(%s). In order to guarantee smooth device operation it is recommended to update this boot programmer now. Once this update did succeed the normal firmware update procedure will continue.\n\nNot updating the boot programmer will end the whole firmware update procedure.\n\nProceed?" ), serial.c_str(), ConvertedString( pDev_->product.read() ).c_str() ),
                    wxT( "Outdated Boot Programmer Detected" ),
                    wxYES_NO | wxICON_INFORMATION );
            switch( outdatedBootProgrammerDlg.ShowModal() )
            {
            case wxID_YES:
                break;
            default:
                if( pParent_ )
                {
                    pParent_->WriteLogMessage( wxString::Format( wxT( "Boot programmer update canceled for device %s(%s).\n" ), serial.c_str(), ConvertedString( pDev_->product.read() ).c_str() ) );
                }
                return urOperationCanceled;
            }
        }

        static const int MAX_UPDATE_TIME_MILLISECONDS = 40000;
        static const int UPDATE_THREAD_PROGRESS_INTERVAL_MILLISECONDS = 100;
        wxProgressDialog* pDlg = IsProgressDisplayActive() ? new wxProgressDialog( wxT( "Updating Boot Programmer In Flash" ),
                                 wxT( "Please wait...                                " ),
                                 MAX_UPDATE_TIME_MILLISECONDS, // range
                                 pParent_,     // parent
                                 wxPD_AUTO_HIDE | wxPD_APP_MODAL | wxPD_ELAPSED_TIME ) : 0;
        UpdateBootProgrammerThread updateThread( updateBootProgrammer );
        RunThreadAndShowProgress( &updateThread, pDlg, UPDATE_THREAD_PROGRESS_INTERVAL_MILLISECONDS, MAX_UPDATE_TIME_MILLISECONDS );
        DeleteElement( pDlg );
        if( updateThread.GetResult() == DMR_NO_ERROR )
        {
            WaitForDeviceToReappear( serial );
        }
        else
        {
            pParent_->WriteLogMessage( wxString::Format( wxT( "Boot programmer update for device %s(%s) failed with the following error: %d(%s).\n" ), serial.c_str(), ConvertedString( pDev_->product.read() ).c_str(), updateThread.GetResult(), ConvertedString( ImpactAcquireException::getErrorCodeAsString( updateThread.GetResult() ) ).c_str() ) );
            return urDeviceAccessError;
        }
    }

    if( boSilentMode || boUseOnlineArchive )
    {
        firmwareFileAndPath = firmwareUpdateFolder_ + wxString( wxT( "/" ) ) + firmwareUpdateFileName_;
    }
    else
    {
        wxFileDialog fileDlg( pParent_, wxT( "Please select the update archive containing the firmware" ), firmwareUpdateFolder_, firmwareUpdateFileName_, wxString::Format( wxT( "%s update archive (%s)|%s" ), ConvertedString( pDev_->product.read() ).c_str(), firmwareUpdateFileFilter_.c_str(), firmwareUpdateFileFilter_.c_str() ), wxFD_OPEN | wxFD_FILE_MUST_EXIST );
        if( fileDlg.ShowModal() != wxID_OK )
        {
            if( pParent_ )
            {
                pParent_->WriteLogMessage( wxString::Format( wxT( "Firmware update canceled for device %s(%s).\n" ), serial.c_str(), ConvertedString( pDev_->product.read() ).c_str() ) );
            }
            return urOperationCanceled;
        }
        firmwareFileAndPath = fileDlg.GetPath();
    }

    PackageDescriptionFileParser parser;
    auto_array_ptr<char> pBuffer( 0 );
    TUpdateResult result = ParseUpdatePackage( parser, firmwareFileAndPath, pParent_, pBuffer );
    if( result != urOperationSuccessful )
    {
        return result;
    }

    const FileEntryContainer& fileEntries = parser.GetResults();
    const FileEntryContainer::size_type cnt = fileEntries.size();
    const wxString product( ConvertedString( pDev_->product.read() ) );
    const wxString productFromManufacturerInfo( GetProductFromManufacturerInfo( pDev_ ) );
    const wxString deviceVersionAsString( pDev_->deviceVersion.isValid() ? ConvertedString( pDev_->deviceVersion.read() ) : wxString( wxT( "1.0" ) ) );
    const Version deviceVersion( VersionFromString( deviceVersionAsString ) );
    wxArrayString choices;
    wxArrayString unusableFiles;
    wxString selection;

    // First check for productFromManufacturerInfo matches
    DetermineValidFirmwareChoices( productFromManufacturerInfo, std::string( "" ), fileEntries, cnt, deviceVersion, choices, unusableFiles );
    choices.Sort( true );
    if( ( choices.size() > 1 ) &&
        !boSilentMode )
    {
        wxSingleChoiceDialog dlg( pParent_, wxT( "This archive contains more than one firmware file matching the productFromManufacturerInfo .\nPlease select a file you want to program the device with." ), wxT( "Please select a file to upload and install on the device" ), choices );
        if( dlg.ShowModal() == wxID_OK )
        {
            selection = dlg.GetStringSelection().AfterFirst( wxT( '(' ) ).BeforeFirst( wxT( ',' ) );
        }
        else
        {
            if( pParent_ )
            {
                pParent_->WriteLogMessage( wxString::Format( wxT( "Firmware update canceled for device %s(%s).\n" ), serial.c_str(), ConvertedString( pDev_->product.read() ).c_str() ) );
            }
            return urOperationCanceled;
        }
    }
    else if( ( choices.size() == 1 ) ||
             ( ( choices.size() > 1 ) && boSilentMode ) )
    {
        selection = choices[0].AfterFirst( wxT( '(' ) ).BeforeFirst( wxT( ',' ) );
    }
    else
    {
        // Then fall back to product matches
        DetermineValidFirmwareChoices( std::string( "" ), product, fileEntries, cnt, deviceVersion, choices, unusableFiles );
        choices.Sort( true );
        if( ( choices.size() > 1 ) &&
            !boSilentMode )
        {
            selection = choices[0].AfterFirst( wxT( '(' ) ).BeforeFirst( wxT( ',' ) );
        }
        else if( ( choices.size() == 1 ) ||
                 ( ( choices.size() > 1 ) && boSilentMode ) )
        {
            selection = choices[0].AfterFirst( wxT( '(' ) ).BeforeFirst( wxT( ',' ) );
        }
        else
        {
            if( pParent_ )
            {
                pParent_->WriteErrorMessage( wxString::Format( wxT( "The archive %s does not contain a suitable firmware file for device %s(%s, Rev. %s). Files detected:\n" ), firmwareFileAndPath.c_str(), serial.c_str(), product.c_str(), deviceVersionAsString.c_str() ) );
                const wxArrayString::size_type unusableFilesCount = unusableFiles.size();
                for( wxArrayString::size_type i = 0; i < unusableFilesCount; i++ )
                {
                    pParent_->WriteErrorMessage( unusableFiles[i] );
                }
            }
            return urInvalidFileSelection;
        }
    }

    const Version currentFirmwareVersion = VersionFromString( ConvertedString( pDev_->firmwareVersion.readS() ) );
    const int incompatibleFirmwareCheckResult = CheckForIncompatibleFirmwareVersions( boSilentMode, serial, fileEntries, selection, currentFirmwareVersion );
    if( incompatibleFirmwareCheckResult != urOperationSuccessful )
    {
        return incompatibleFirmwareCheckResult;
    }

    if( boSilentMode )
    {
        for( FileEntryContainer::size_type i = 0; i < cnt; i++ )
        {
            if( fileEntries[i].name_ == selection )
            {
                if( !pParent_->IsFirmwareUpdateForced() && currentFirmwareVersion == fileEntries[i].version_ )
                {
                    if( pParent_ )
                    {
                        pParent_->WriteLogMessage( wxString::Format( wxT( "Firmware update canceled for device %s(%s) as the current firmware version(%s) is equal to the one provided in the archive(%s).\n" ),
                                                   ConvertedString( serial ).c_str(),
                                                   ConvertedString( pDev_->product.read() ).c_str(),
                                                   GetStringRepresentationOfFirmwareVersion( currentFirmwareVersion ).c_str(),
                                                   GetStringRepresentationOfFirmwareVersion( fileEntries[i].version_ ).c_str() ) );
                    }
                    return urOperationSuccessful;
                }
                else
                {
                    // different version -> proceed
                    break;
                }
            }
        }
    }
    auto_array_ptr<char> pUploadFileBuffer;
    if( !GetFileFromArchive( firmwareFileAndPath, pBuffer.get(), pBuffer.parCnt(), selection, pUploadFileBuffer, pParent_ ) )
    {
        if( pParent_ )
        {
            pParent_->WriteErrorMessage( wxString::Format( wxT( "Could not extract file %s from archive %s.\n" ), selection.c_str(), firmwareFileAndPath.c_str() ) );
        }
        return urFileIOError;
    }

    Version latestVersion;
    GetLatestLocalFirmwareVersion( latestVersion );

    const Version fwVersionDevice ( VersionFromString( ConvertedString( pDev_->firmwareVersion.readS() ) ) );
    Version fwVersionPackage;

    for( size_t i = 0; i < fileEntries.size(); i++ )
    {
        std::vector<std::map<wxString, SuitableProductKey>::const_iterator> vSuitableFirmwareIterators = GetSuitableFirmwareIterators( fileEntries[i], product, productFromManufacturerInfo );
        const std::vector<std::map<wxString, SuitableProductKey>::const_iterator>::size_type suitableFirmwareIteratorCnt = vSuitableFirmwareIterators.size();
        for( std::vector<std::map<wxString, SuitableProductKey>::const_iterator>::size_type j = 0; j < suitableFirmwareIteratorCnt; j++ )
        {
            if( ( vSuitableFirmwareIterators[j] != fileEntries[i].suitableProductKeys_.end() ) && ( fileEntries[i].type_ == wxString( wxT( "Firmware" ) ) ) &&
                IsVersionWithinRange( deviceVersion, vSuitableFirmwareIterators[j]->second.revisionMin_, vSuitableFirmwareIterators[j]->second.revisionMax_ ) )
            {
                fwVersionPackage.major_ = fileEntries[i].version_.major_;
                fwVersionPackage.minor_ = fileEntries[i].version_.minor_;
                fwVersionPackage.subMinor_ = fileEntries[i].version_.subMinor_;
                fwVersionPackage.release_ = fileEntries[i].version_.release_;
                break;
            }
        }
    }

    if( !boSilentMode && ( IsFirmwareUpdateMeaningless( boSilentMode, fwVersionDevice, fwVersionPackage, firmwareUpdateDefaultFolder_, latestVersion ) ) )
    {
        return urOperationCanceled;
    }

    if( !pParent_->IsFirmwareUpdateForced() && boSilentMode && ( VersionFromString( ConvertedString( pDev_->firmwareVersion.readS() ) ) == fileEntries[0].version_ ) )
    {
        // In silent mode the firmware update will be done even if the "new" version is older than the "old" one. This is done
        // in order to allow a defined downgrade from a command line or script operation.
        // In case the versions are EXACTLY the same, no update action will take place, as this would be a definite waste of time.
        return urOperationCanceled;
    }

    if( !boSilentMode )
    {
        FirmwareUpdateConfirmationDialog dlg( pParent_,
                                              wxID_ANY,
                                              wxT( "Firmware Update" ),
                                              wxDefaultPosition,
                                              wxDefaultSize,
                                              wxNO_DEFAULT | wxYES_NO,
                                              wxString::Format( wxT( "Are you sure you want to update the firmware of device %s?\nThis might take several minutes during which the application will not react.\n\nPlease be patient and do NOT disconnect the device from the\npower supply during this time." ), serial.c_str() ),
                                              boPersistentUserSets );
        switch( dlg.ShowModal() )
        {
        case wxID_OK:
            break;
        default:
            if( pParent_ )
            {
                pParent_->WriteLogMessage( wxString::Format( wxT( "Firmware update canceled for device %s(%s).\n" ), ConvertedString( serial ).c_str(), ConvertedString( pDev_->product.read() ).c_str() ) );
            }
            return urOperationCanceled;
        }
        boPersistentUserSets = dlg.shallKeepUserSets();
    }

    // avoid keeping UserSets when the camera runs as Bootloader device
    static const Version VERSION_1_0_0_0( 1, 0, 0, 0 );
    if( ( boPersistentUserSets == true ) &&
        ( pDev_->family.readS() == "mvBlueFOX3" ) &&
        ( ( currentFirmwareVersion == VERSION_1_0_0_0 ) || ( currentFirmwareVersion.major_ == 0 ) ) ) // old bootloader code did use version 1.0.0.0, newer versions reflect the source they have been build from by reverting the firmware version of that time thus it is 0.minor.subminor.major
    {
        if( pParent_ )
        {
            pParent_->WriteLogMessage( wxString::Format( wxT( "No attempt to keep UserSets will be done for device %s(%s), because it is currently a bootloader device and cannot have valid UserSet settings.\n" ), ConvertedString( serial ).c_str(), ConvertedString( pDev_->product.read() ).c_str() ) );
        }
        boPersistentUserSets = false;
    }

    try
    {
        // In order to make sure an inconsistent setting will not block the correct startup of the device after the update switch the setting
        // to use at startup back to 'Default'. If the user selects to keep the settings on the device restore the value later after checking
        // that it can actually be used.
        const wxString previousUserSetDefault = SetUserSetDefault( wxT( "Default" ) );
        if( boPersistentUserSets )
        {
            UserSetBackup( boSilentMode );
        }

        SelectCustomGenICamFile( GenICamFile_ );
        result = DoFirmwareUpdate( boSilentMode, serial, pUploadFileBuffer.get(), pUploadFileBuffer.parCnt(), currentFirmwareVersion );

        if( boPersistentUserSets )
        {
            // restore the previous value for 'UserSetDefault'. If the user did chose not to keep the settings we also do not need
            // to restore the value for 'UserSetDefault'. It wouldn't do anything useful on the device anyway.
            UserSetRestore( boSilentMode, previousUserSetDefault );
        }
    }
    catch( const ImpactAcquireException& e )
    {
        if( pParent_ )
        {
            pParent_->WriteLogMessage( wxString::Format( wxT( "Failed to update device %s(%s)(%s, %s).\n" ),
                                       ConvertedString( serial ).c_str(),
                                       ConvertedString( pDev_->product.read() ).c_str(),
                                       ConvertedString( e.getErrorString() ).c_str(),
                                       ConvertedString( e.getErrorCodeAsString() ).c_str() ) );
        }
        result = urDeviceAccessError;
    }

    if( result == urOperationSuccessful )
    {
        MessageToUser( wxT( "Information" ), wxT( "The Firmware has been updated. The new Firmware is active now and the device can be used again by other applications." ), boSilentMode, wxOK | wxICON_INFORMATION );
    }

    if( pDev_->isOpen() )
    {
        pDev_->close();
    }

    return result;
}

//-----------------------------------------------------------------------------
int DeviceHandlerBlueDevice::UpdateFirmware( bool boSilentMode, bool boPersistentUserSets, bool boUseOnlineArchive )
//-----------------------------------------------------------------------------
{
    ConvertedString serial( pDev_->serial.read() );
    if( !MessageToUser( wxT( "Firmware Update" ), wxString::Format( wxT( "Please close all other applications using device %s before proceeding." ), serial.c_str() ), boSilentMode, wxCANCEL | wxOK | wxICON_INFORMATION ) )
    {
        if( pParent_ )
        {
            pParent_->WriteLogMessage( wxString::Format( wxT( "Firmware update canceled for device %s(%s).\n" ), serial.c_str(), ConvertedString( pDev_->product.read() ).c_str() ) );
        }
        return urOperationCanceled;
    }

    switch( product_ )
    {
    case pgBlueCOUGAR_S:
        return UpdateCOUGAR_SDevice( boSilentMode );
    default:
        return UpdateDevice( boSilentMode, boPersistentUserSets, boUseOnlineArchive );
    }

    /*  if( pParent_ )
      {
          pParent_->WriteErrorMessage( wxString::Format( wxT( "Could not detect type of device %s(%s).\n" ), serial.c_str(), ConvertedString( pDev_->product.read() ).c_str() ) );
      }*/

    return urFeatureUnsupported;
}

//-----------------------------------------------------------------------------
int DeviceHandlerBlueDevice::UploadFile( const wxString& fullPath, const wxString& descriptionFile )
//-----------------------------------------------------------------------------
{
    int result = urOperationSuccessful;
    const wxString serial( ConvertedString( pDev_->serial.read() ) );
    try
    {
        SelectCustomGenICamFile( descriptionFile );
        pDev_->open();
    }
    catch( const ImpactAcquireException& e )
    {
        if( pParent_ )
        {
            pParent_->WriteLogMessage( wxString::Format( wxT( "Failed to open device %s(%s)(%s, %s).\n" ),
                                       serial.c_str(),
                                       ConvertedString( pDev_->product.read() ).c_str(),
                                       ConvertedString( e.getErrorString() ).c_str(),
                                       ConvertedString( e.getErrorCodeAsString() ).c_str() ) );
        }
        return urDeviceAccessError;
    }
    try
    {
        UpdateFeatures uf( pDev_ );
        uf.transferMode_.writeS( "Binary" );
        if( pParent_ )
        {
            pParent_->WriteLogMessage( wxString::Format( wxT( "Installing %s. This will take some minutes. During this time the application will not react.\n" ), fullPath.c_str() ) );
        }

        // upload the file
        uf.sourceFileName_.write( std::string( wxConvCurrent->cWX2MB( fullPath.c_str() ) ) );
        uf.upload_.call( std::string( wxConvCurrent->cWX2MB( wxT( "" ) ) ) );
        if( pParent_ )
        {
            pParent_->WriteLogMessage( wxString::Format( wxT( "Result of the installation of %s: %s\n" ), fullPath.c_str(), ConvertedString( uf.lastResult_.read() ).c_str() ) );
        }
    }
    catch( const ImpactAcquireException& e )
    {
        if( pParent_ )
        {
            pParent_->WriteLogMessage( wxString::Format( wxT( "Failed to update device %s(%s)(%s, %s).\n" ),
                                       serial.c_str(),
                                       ConvertedString( pDev_->product.read() ).c_str(),
                                       ConvertedString( e.getErrorString() ).c_str(),
                                       ConvertedString( e.getErrorCodeAsString() ).c_str() ) );
        }
        result = urDeviceAccessError;
    }
    pDev_->close();
    return result;
}

//-----------------------------------------------------------------------------
DeviceHandler::TUpdateResult DeviceHandlerBlueDevice::UploadFirmwareFile( mvIMPACT::acquire::GenICam::ODevFileStream& uploadFile, bool boSilentMode, const wxString& serial, const char* pBuf, const size_t bufSize, const wxFileOffset fileOperationBlockSize )
//-----------------------------------------------------------------------------
{
    uploadFile.open( pDev_, "DeviceFirmware" );
    if( !uploadFile.fail() )
    {
        wxFileOffset bytesWritten = 0;
        wxFileOffset bytesToWriteTotal = bufSize;
        std::ostringstream oss;
        oss << std::setw( 8 ) << std::setfill( '0' ) << bytesWritten << '/'
            << std::setw( 8 ) << std::setfill( '0' ) << bytesToWriteTotal;
        wxProgressDialog* pDlg = IsProgressDisplayActive() ? new wxProgressDialog( wxT( "Applying Firmware Update" ),
                                 wxString::Format( wxT( "Applying Firmware Update(%s bytes written)" ), ConvertedString( oss.str() ).c_str() ),
                                 static_cast<int>( bytesToWriteTotal / fileOperationBlockSize ), // range
                                 pParent_,          // parent
                                 wxPD_AUTO_HIDE | wxPD_APP_MODAL | wxPD_ELAPSED_TIME | wxPD_ESTIMATED_TIME | wxPD_REMAINING_TIME ) : 0;
        while( bytesWritten < bytesToWriteTotal )
        {
            const wxFileOffset bytesToWrite = ( ( bytesWritten + fileOperationBlockSize ) <= bytesToWriteTotal ) ? fileOperationBlockSize : bytesToWriteTotal - bytesWritten;
            uploadFile.write( pBuf + bytesWritten, bytesToWrite );
            bytesWritten += bytesToWrite;
            std::ostringstream progress;
            progress << std::setw( 8 ) << std::setfill( '0' ) << bytesWritten << '/'
                     << std::setw( 8 ) << std::setfill( '0' ) << bytesToWriteTotal;
            if( pDlg )
            {
                pDlg->Update( static_cast<int>( bytesWritten / fileOperationBlockSize ), wxString::Format( wxT( "Applying Firmware Update(%s bytes written)" ), ConvertedString( progress.str() ).c_str() ) );
            }
        }
        DeleteElement( pDlg );
        if( !uploadFile.good() )
        {
            MessageToUser( wxT( "Warning" ), wxString::Format( wxT( "Failed to upload firmware file to device %s." ), ConvertedString( serial ).c_str() ), boSilentMode, wxOK | wxICON_INFORMATION );
            uploadFile.close();
            return urFileIOError;
        }
    }
    else
    {
        MessageToUser( wxT( "Warning" ), wxString::Format( wxT( "Failed to open firmware file on device %s." ), ConvertedString( serial ).c_str() ), boSilentMode, wxOK | wxICON_INFORMATION );
        return urFileIOError;
    }
    return urOperationSuccessful;
}

//=============================================================================
//=================== implementation DeviceHandlerBlueNAOS ====================
//=============================================================================
//-----------------------------------------------------------------------------

DeviceHandlerBlueNAOS::DeviceHandlerBlueNAOS( mvIMPACT::acquire::Device* pDev ) : DeviceHandlerBlueDeviceBase( pDev, true ),
    boSupportsFirmwareUpdate_( false ), pUpdateFirmware_( 0 )
//-----------------------------------------------------------------------------
{
    // find the GenTL interface this device claims to be connected to
    PropertyI64 interfaceID;
    DeviceComponentLocator locator( pDev->hDev() );
    locator.bindComponent( interfaceID, "InterfaceID" );
    const std::string serial( pDev->serial.read() );

    if( interfaceID.isValid() )
    {
        const std::string interfaceDeviceHasBeenFoundOn( interfaceID.readS() );
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
                        !im.deviceSelector.isValid() ||
                        !im.deviceSerialNumber.isValid() )
                    {
                        // most likely this is a 3rd party producer. As we cannot obtain the
                        // information we are after just skip this interface!
                        continue;
                    }

                    if( interfaceDeviceHasBeenFoundOn != im.interfaceID.readS() )
                    {
                        continue;
                    }

                    const std::string interfaceType( im.interfaceType.readS() );
                    if( interfaceType != "PCI" )
                    {
                        continue;
                    }

                    // this might be an interface we are looking for (so far there is no robust way to detect a PCIe mvBlueNAOS interface)
                    const int64_type deviceCount( im.deviceSelector.getMaxValue() + 1 );
                    for( int64_type deviceIndex = 0; deviceIndex < deviceCount; deviceIndex++ )
                    {
                        im.deviceSelector.write( deviceIndex );
                        if( im.deviceSerialNumber.read() != serial )
                        {
                            continue;
                        }

                        boSupportsFirmwareUpdate_ = im.mvDeviceUpdateFirmware.isValid();
                        if( boSupportsFirmwareUpdate_ )
                        {
                            pUpdateFirmware_ = new Method( im.mvDeviceUpdateFirmware.hObj() );
                        }
                        break;
                    }
                    if( boSupportsFirmwareUpdate_ )
                    {
                        break;
                    }
                }
            }
            catch( const ImpactAcquireException& e )
            {
                if( pParent_ )
                {
                    const ConvertedString serial( pDev_->serial.read() );
                    const ConvertedString product( pDev_->product.read() );
                    pParent_->WriteErrorMessage( wxString::Format( wxT( "Failed to obtain firmware update availability of system module %u for device %s(%s). %s(numerical error representation: %d (%s)). The associated GenTL producer might not support a GenICam file for the System-module and therefore the required properties are not available.\n" ), serial.c_str(), product.c_str(), static_cast<unsigned int>( systemModuleIndex ), ConvertedString( e.getErrorString() ).c_str(), e.getErrorCode(), ConvertedString( e.getErrorCodeAsString() ).c_str() ) );
                }
            }
        }
    }
}

//-----------------------------------------------------------------------------
DeviceHandlerBlueNAOS::~DeviceHandlerBlueNAOS()
//-----------------------------------------------------------------------------
{
    DeleteElement( pUpdateFirmware_ );
}

//-----------------------------------------------------------------------------
bool DeviceHandlerBlueNAOS::GetIDFromUser( long& /*newID*/, const long /*minValue*/, const long /*maxValue*/ ) const
//-----------------------------------------------------------------------------
{
    ConvertedString serial( pDev_->serial.read() );
    wxString tool = wxT( "wxPropView" );
    wxString standard = wxT( "GenTL" );;

    if( MessageToUser( wxT( "Set Device ID" ), wxString::Format( wxT( "Device %s does not support setting an integer device ID.\n\nAs this device is %s compliant it might however support setting a user defined string by modifying the property 'DeviceUserID' that can be used instead.\n\nThis string can be assigned using the tool '%s'. Do you want to launch this application now?" ), serial.c_str(), standard.c_str(), tool.c_str() ), false, wxNO_DEFAULT | wxYES_NO | wxICON_INFORMATION ) )
    {
        ::wxExecute( tool );
    }
    return false;
}

//-----------------------------------------------------------------------------
int DeviceHandlerBlueNAOS::GetLatestLocalFirmwareVersion( Version& latestFirmwareVersion ) const
//-----------------------------------------------------------------------------
{
    int result = urFeatureUnsupported;
    const Version currentFirmwareVersion = VersionFromString( ConvertedString( pDev_->firmwareVersion.readS() ) );
    PropertyI64 firmwareVersionLatest;
    DeviceComponentLocator locator( pDev_->hDev() );

    if( locator.bindComponent( firmwareVersionLatest, "FirmwareVersionLatest" ) )
    {
        if( firmwareVersionLatest.isValid() )
        {
            latestFirmwareVersion = VersionFromString( ConvertedString( firmwareVersionLatest.readS() ) );
            if( currentFirmwareVersion == latestFirmwareVersion )
            {
                // already the latest version
                result = urOperationCanceled;
            }
            else
            {
                // latest version != current version
                result = urOperationSuccessful;
            }
        }
    }
    return result;
}

//-----------------------------------------------------------------------------
int DeviceHandlerBlueNAOS::UpdateFirmware( bool boSilentMode, bool /*boPersistentUserSets*/, bool /*boUseOnlineArchive*/ )
//-----------------------------------------------------------------------------
{
    int result = urFirmwareUpdateFailed;
    const ConvertedString serial( pDev_->serial.read() );
    const ConvertedString product( pDev_->product.read() );

#if defined __WIN32__
    if( !IsCurrentUserLocalAdministrator() )
    {
        if( pParent_ )
        {
            pParent_->ReportThatAdminRightsAreNeeded();
            result = urDeviceAccessError;
            pParent_->WriteLogMessage( wxT( "Operation canceled by the user.\n" ) );
        }
    }
    else
#endif
    {
        if( MessageToUser( wxT( "Firmware Update" ), wxString::Format( wxT( "Are you sure you want to update the firmware of device %s?" ), serial.c_str() ), boSilentMode, wxNO_DEFAULT | wxYES_NO | wxICON_INFORMATION ) &&
            MessageToUser( wxT( "Information" ), wxT( "The firmware will now be updated. During this time (approx. 30 sec.) the application will not react. Please be patient. You may have to reboot your system and the camera in order for the new firmware to be used." ), boSilentMode, wxOK | wxICON_INFORMATION ) )
        {
            if( !pUpdateFirmware_ )
            {
                pParent_->WriteLogMessage( wxString::Format( wxT( "Device %s(%s) doesn't support firmware updates.\n" ), serial.c_str(), product.c_str() ) );
                return urFeatureUnsupported;
            }

            if( pParent_ )
            {
                pParent_->WriteLogMessage( wxString::Format( wxT( "Updating firmware of device %s(%s). This might take some time. Please be patient.\n" ), serial.c_str(), product.c_str() ) );
            }
            try
            {
                result = pUpdateFirmware_->call();
                if( pParent_ )
                {
                    if( result != DMR_NO_ERROR )
                    {
                        pParent_->WriteLogMessage( wxString::Format( wxT( "An error occurred: %s(%s)(please refer to the manual for this error code).\n" ), ConvertedString( ImpactAcquireException::getErrorCodeAsString( result ) ).c_str(), ConvertedString( ExceptionFactory::getLastErrorString() ).c_str() ) );
                        return urFirmwareUpdateFailed;
                    }
                    else
                    {
                        pParent_->WriteLogMessage( wxString::Format( wxT( "Successfully updated firmware of device %s(%s).\n" ), serial.c_str(), product.c_str() ) );
                        result = urOperationSuccessful;
                    }
                }
            }
            catch( const ImpactAcquireException& e )
            {
                if( pParent_ )
                {
                    pParent_->WriteErrorMessage( wxString::Format( wxT( "Failed to update firmware of device %s(%s). %s(%s)(numerical error representation: %d (%s)).\n" ), serial.c_str(), product.c_str(), ConvertedString( e.getErrorString() ).c_str(), ConvertedString( ExceptionFactory::getLastErrorString() ).c_str(), e.getErrorCode(), ConvertedString( e.getErrorCodeAsString() ).c_str() ) );
                }
                return urFirmwareUpdateFailed;
            }
        }
    }

    if( result == urOperationSuccessful )
    {
        WaitForDeviceToReappear( serial );
        // now check if the new firmware is actually active
        try
        {
            if( pDev_->isOpen() )
            {
                pDev_->close();
            }

            DeviceManager devMgr;
            devMgr.updateDeviceList(); // this is needed in order to invalidate cached data that might no longer be valid(e.g. the URLs for the description files)
        }
        catch( const ImpactAcquireException& e )
        {
            if( pParent_ )
            {
                pParent_->WriteLogMessage( wxString::Format( wxT( "Failed to close and re-open device %s(%s)(%s, %s).\n" ),
                                           ConvertedString( pDev_->serial.read() ).c_str(),
                                           ConvertedString( pDev_->product.read() ).c_str(),
                                           ConvertedString( e.getErrorString() ).c_str(),
                                           ConvertedString( e.getErrorCodeAsString() ).c_str() ) );
            }
        }
        MessageToUser( wxT( "Information" ), wxT( "The Firmware has been updated. The new Firmware is active now and the device can be used again by other applications." ), boSilentMode, wxOK | wxICON_INFORMATION );
    }

    if( pDev_->isOpen() )
    {
        pDev_->close();
    }

    return result;
}

//=============================================================================
//=================== implementation DeviceHandlerBlueFOX3 ====================
//=============================================================================
//-----------------------------------------------------------------------------
DeviceHandlerBlueFOX3::DeviceHandlerBlueFOX3( mvIMPACT::acquire::Device* pDev ) : DeviceHandlerBlueDevice( pDev )
//-----------------------------------------------------------------------------
{
    switch( product_ )
    {
    case pgBlueFOX3:
        if( IsBlueFOX3_XAS( pDev_ ) )
        {
            firmwareUpdateFileName_ = wxString( wxT( "BVS_CA_Update.mvu" ) );
            firmwareUpdateFileFilter_ = wxString( wxT( "BVS_CA_Update*.mvu" ) );
        }
        else
        {
            firmwareUpdateFileName_ = wxString( wxT( "mvBlueFOX3_Update.mvu" ) );
            firmwareUpdateFileFilter_ = wxString( wxT( "mvBlueFOX3_Update*.mvu" ) );
        }
        break;
    default:
        break;
    }
    firmwareUpdateFolder_.append( wxT( "FirmwareUpdates/mvBlueFOX" ) );
    firmwareUpdateDefaultFolder_ = firmwareUpdateFolder_;
    std::replace( firmwareUpdateDefaultFolder_.begin(), firmwareUpdateDefaultFolder_.end(), '/', static_cast< char >( wxFileName::GetPathSeparator() ) );
}

//-----------------------------------------------------------------------------
DeviceHandler::TUpdateResult DeviceHandlerBlueFOX3::DoFirmwareUpdate( bool boSilentMode, const wxString& serial, const char* pBuf, const size_t bufSize, const Version& /*currentFirmwareVersion*/ )
//-----------------------------------------------------------------------------
{
    pDev_->interfaceLayout.write( dilGenICam );
    {
        // switch off automatic update of GenICam features triggered by recommended polling times in the GenICam XML file
        SystemSettingsGenTL ss( pDev_ );
        ss.featurePollingEnable.write( bFalse );
    }

    mvIMPACT::acquire::GenICam::FileAccessControl fac( pDev_ );
    int fileOperationExecuteResult = DMR_NO_ERROR;

    static const int MAX_UPDATE_TIME_MILLISECONDS = 40000;
    static const int UPDATE_THREAD_PROGRESS_INTERVAL_MILLISECONDS = 100;

    wxProgressDialog* pDlg = IsProgressDisplayActive() ? new wxProgressDialog( wxT( "Flash Erase" ),
                             wxT( "Erasing flash memory..." ),
                             MAX_UPDATE_TIME_MILLISECONDS, // range
                             pParent_,     // parent
                             wxPD_AUTO_HIDE | wxPD_APP_MODAL | wxPD_ELAPSED_TIME ) : 0;

    EraseFlashThread eraseThread( fac, fileOperationExecuteResult );
    RunThreadAndShowProgress( &eraseThread, pDlg, UPDATE_THREAD_PROGRESS_INTERVAL_MILLISECONDS, MAX_UPDATE_TIME_MILLISECONDS );
    DeleteElement( pDlg );
    if( static_cast< TDMR_ERROR >( fileOperationExecuteResult ) != DMR_NO_ERROR )
    {
        MessageToUser( wxT( "Warning" ), wxString::Format( wxT( "Failed to erase flash memory for firmware image on device %s. Error reported from driver: %d(%s)." ), ConvertedString( serial ).c_str(), fileOperationExecuteResult, ConvertedString( ImpactAcquireException::getErrorCodeAsString( fileOperationExecuteResult ) ).c_str() ), boSilentMode, wxOK | wxICON_INFORMATION );
        return urFileIOError;
    }

    static const wxFileOffset fileOperationBlockSize = 4096;
    {
        // download the firmware image
        mvIMPACT::acquire::GenICam::ODevFileStream uploadFile;
        const TUpdateResult uploadResult = UploadFirmwareFile( uploadFile, boSilentMode, serial, pBuf, bufSize, fileOperationBlockSize );
        if( uploadResult != urOperationSuccessful )
        {
            return uploadResult;
        }

        // Early releases of the mvBlueFOX3 firmware could not be verified on the device itself, thus earlier versions of this application
        // did download and compare the firmware image here inside the application. Nowadays however a checksum has been embedded into the
        // firmware image thus simply closing the file and checking the result of this operation is enough here as the device will then perform
        // the verification of the uploaded image and will report an error as a result of the 'close' operation if the verification did fail.
        wxProgressDialog* pDlg = IsProgressDisplayActive() ? new wxProgressDialog( wxT( "Verifying Firmware Update" ),
                                 wxT( "Please wait...                                " ),
                                 MAX_UPDATE_TIME_MILLISECONDS, // range
                                 pParent_,     // parent
                                 wxPD_AUTO_HIDE | wxPD_APP_MODAL | wxPD_ELAPSED_TIME ) : 0;
        WaitForCloseThread closeThread( &uploadFile );
        RunThreadAndShowProgress( &closeThread, pDlg, UPDATE_THREAD_PROGRESS_INTERVAL_MILLISECONDS, MAX_UPDATE_TIME_MILLISECONDS );
        DeleteElement( pDlg );
        if( !uploadFile.good() )
        {
            MessageToUser( wxT( "Warning" ), wxString::Format( wxT( "Firmware verification for device %s failed. Please power-cycle the device now, update the device list and re-try to update the firmware." ), ConvertedString( serial ).c_str() ), boSilentMode, wxOK | wxICON_INFORMATION );
            uploadFile.close();
            return urFileIOError;
        }
    }
    RebootDevice( boSilentMode, serial );
    return urOperationSuccessful;
}

//-----------------------------------------------------------------------------
bool DeviceHandlerBlueFOX3::GetIDFromUser( long& /*newID*/, const long /*minValue*/, const long /*maxValue*/ ) const
//-----------------------------------------------------------------------------
{
    ConvertedString serial( pDev_->serial.read() );
    wxString tool = wxT( "wxPropView" );
    wxString standard = wxT( "USB3 Vision" );

    if( MessageToUser( wxT( "Set Device ID" ), wxString::Format( wxT( "Device %s does not support setting an integer device ID.\n\nAs this device is %s compliant it might however support setting a user defined string by modifying the property 'DeviceUserID' that can be used instead.\n\nThis string can be assigned using the tool '%s'. Do you want to launch this application now?" ), serial.c_str(), standard.c_str(), tool.c_str() ), false, wxNO_DEFAULT | wxYES_NO | wxICON_INFORMATION ) )
    {
        ::wxExecute( tool );
    }
    return false;
}

//-----------------------------------------------------------------------------
bool DeviceHandlerBlueFOX3::IsMatrixVisionDeviceRunningWithNormalFirmware( void ) const
//-----------------------------------------------------------------------------
{
    bool boResult = true;
    try
    {
        if( VersionFromString( wxString( pDev_->firmwareVersion.readS() ) ).major_ == 0 )
        {
            boResult = false;
        }
    }
    catch( const ImpactAcquireException& ) {}
    return boResult;
}

//=============================================================================
//=================== implementation DeviceHandlerBlueCOUGAR ====================
//=============================================================================
//-----------------------------------------------------------------------------
DeviceHandlerBlueCOUGAR::DeviceHandlerBlueCOUGAR( mvIMPACT::acquire::Device* pDev ) : DeviceHandlerBlueDevice( pDev )
//-----------------------------------------------------------------------------
{
    switch( product_ )
    {
    case pgBlueCOUGAR_S:
        firmwareUpdateFileName_ = pDev_->product.read();
        if( pDev->firmwareVersion.read() >= 0x10100 )
        {
            // this is HW revision 1, which means it needs a different firmware
            firmwareUpdateFileName_.Append( wxT( "_R001" ) );
        }
        firmwareUpdateFileFilter_ = firmwareUpdateFileName_;
        firmwareUpdateFileFilter_.Append( wxT( "_Update*.fpg" ) );
        firmwareUpdateFileName_.Append( wxT( "_Update.fpg" ) );
        break;
    case pgBlueCOUGAR_X:
        if( IsBlueCOUGAR_X_XAS( pDev_ ) )
        {
            firmwareUpdateFileName_ = wxString( wxT( "BVS_CA_Update.mvu" ) );
            firmwareUpdateFileFilter_ = wxString( wxT( "BVS_CA_Update*.mvu" ) );
        }
        else if( IsBlueSIRIUS() )
        {
            firmwareUpdateFileName_ = wxString( wxT( "mvBlueSIRIUS_Update.mvu" ) );
            firmwareUpdateFileFilter_ = wxString( wxT( "mvBlueSIRIUS_Update*.mvu" ) );
        }
        else if( IsBlueCOUGAR_XT( pDev_ ) )
        {
            firmwareUpdateFileName_ = wxString( wxT( "mvBlueCOUGAR-XT_Update.mvu" ) );
            firmwareUpdateFileFilter_ = wxString( wxT( "mvBlueCOUGAR-XT_Update*.mvu" ) );
        }
        else if( IsBlueCOUGAR_X_BAS( pDev_ ) )
        {
            firmwareUpdateFileName_ = wxString( wxT( "BVS_CA_Update.mvu" ) );
            firmwareUpdateFileFilter_ = wxString( wxT( "BVS_CA_Update*.mvu|BVS_CA_GEV_Update*.mvu|mvBlueCOUGAR-X_Update*.mvu" ) );
        }
        else
        {
            firmwareUpdateFileName_ = wxString( wxT( "mvBlueCOUGAR-X_Update.mvu" ) );
            firmwareUpdateFileFilter_ = wxString( wxT( "mvBlueCOUGAR-X_Update*.mvu" ) );
        }
        break;
    case pgBlueCOUGAR_XT:
        firmwareUpdateFileName_ = wxString( wxT( "mvBlueCOUGAR-XT_Update.mvu" ) );
        firmwareUpdateFileFilter_ = wxString( wxT( "mvBlueCOUGAR-XT_Update*.mvu" ) );
        break;
    case pgBlueCOUGAR_Y:
        firmwareUpdateFileName_ = wxString( wxT( "mvBlueCOUGAR-Y_Update.mvu" ) );
        firmwareUpdateFileFilter_ = wxString( wxT( "mvBlueCOUGAR-Y_Update*.mvu" ) );
        break;
    default:
        break;
    }

    if( product_ == pgBlueCOUGAR_X )
    {
        firmwareUpdateFolder_.append( wxT( "FirmwareUpdates/mvBlueCOUGAR" ) );
    }
    else if( product_ == pgBlueCOUGAR_XT )
    {
        firmwareUpdateFolder_.append( wxT( "FirmwareUpdates/mvBlueCOUGAR-XT" ) );
    }
    else if( pParent_ )
    {
        pParent_->WriteErrorMessage( wxT( "No online firmware update possible. Product is not known." ) );
    }
    firmwareUpdateDefaultFolder_ = firmwareUpdateFolder_;
    std::replace( firmwareUpdateDefaultFolder_.begin(), firmwareUpdateDefaultFolder_.end(), '/', static_cast< char >( wxFileName::GetPathSeparator() ) );
}

//-----------------------------------------------------------------------------
int DeviceHandlerBlueCOUGAR::CheckForIncompatibleFirmwareVersions( bool boSilentMode, const wxString& serial, const FileEntryContainer& fileEntries, const wxString& selection, const Version& currentFirmwareVersion )
//-----------------------------------------------------------------------------
{
    if( !IsMatrixVisionDeviceRunningWithNormalFirmware() )
    {
        // Don't warn if the device is running with a fall-back firmware as this is most likely something that did happen
        // because of a failed firmware update in some previous operation.
        return urOperationSuccessful;
    }

    // check for updates between incompatible firmware versions.
    const FileEntryContainer::size_type cnt = fileEntries.size();
    static const Version s_firstVersionWithNewNames( 1, 1, 86, 0 );
    bool boCompatibleInterfaces = true;
    FileEntryContainer::size_type fileIndex = 0;
    for( fileIndex = 0; fileIndex < cnt; fileIndex++ )
    {
        if( fileEntries[fileIndex].name_ == selection )
        {
            if( ( currentFirmwareVersion < s_firstVersionWithNewNames ) &&
                ( fileEntries[fileIndex].version_ >= s_firstVersionWithNewNames ) )
            {
                boCompatibleInterfaces = false;
                break;
            }
            else if( ( currentFirmwareVersion >= s_firstVersionWithNewNames ) &&
                     ( fileEntries[fileIndex].version_ < s_firstVersionWithNewNames ) )
            {
                boCompatibleInterfaces = false;
                break;
            }
        }
    }

    if( !boCompatibleInterfaces )
    {
        if( !MessageToUser( wxT( "Firmware Update" ), wxString::Format( wxT( "There has been an incompatible interface change between the firmware version currently running on the device(%s) and the version you are about to install(%s).\nFor further information please refer to the mvBlueCOUGAR-X manual section 'C++ developer -> Porting existing code -> Notes about mvIMPACT Acquire version 1.11.32 or higher and/or using mvBlueCOUGAR-X with firmware 1.2.0 or higher'.\n\nAre you sure you want to continue updating the firmware of device %s?" ),
                            GetStringRepresentationOfFirmwareVersion( currentFirmwareVersion ).c_str(),
                            GetStringRepresentationOfFirmwareVersion( fileEntries[fileIndex].version_ ).c_str(),
                            serial.c_str() ), boSilentMode, wxNO_DEFAULT | wxYES_NO | wxICON_INFORMATION ) )
        {
            if( pParent_ )
            {
                pParent_->WriteLogMessage( wxString::Format( wxT( "Firmware update canceled for device %s(%s).\n" ), ConvertedString( serial ).c_str(), ConvertedString( pDev_->product.read() ).c_str() ) );
            }
            return urOperationCanceled;
        }
    }

    return urOperationSuccessful;
}

//-----------------------------------------------------------------------------
DeviceHandler::TUpdateResult DeviceHandlerBlueCOUGAR::DoFirmwareUpdate( bool boSilentMode, const wxString& serial, const char* pBuf, const size_t bufSize, const Version& currentFirmwareVersion )
//-----------------------------------------------------------------------------
{
    static const Version s_firstVersionWithPendingACK( 1, 1, 3, 0 );

    pDev_->interfaceLayout.write( dilGenICam );
    {
        // switch off automatic update of GenICam features triggered by recommended polling times in the GenICam XML file
        SystemSettingsGenTL ss( pDev_ );
        ss.featurePollingEnable.write( bFalse );
        if( currentFirmwareVersion < s_firstVersionWithPendingACK )
        {
            // we need to increase the method polling retry count here, as this device does not support
            // the pending ACK feature. Instead executing the 'fileOperationExecute' call will simply
            // take much longer
            ss.methodPollingInterval_ms.write( 1000 );
        }
    }

    const bool boDeviceMustBeUnplugged = !IsMatrixVisionDeviceRunningWithNormalFirmware() && !IsBlueCOUGAR_XT( pDev_ );
    mvIMPACT::acquire::GenICam::FileAccessControl fac( pDev_ );
    TUpdateResult result = urOperationSuccessful;

    static const wxFileOffset fileOperationBlockSize = 4096;
    {
        // download the firmware image
        mvIMPACT::acquire::GenICam::ODevFileStream uploadFile;
        const TUpdateResult uploadResult = UploadFirmwareFile( uploadFile, boSilentMode, serial, pBuf, bufSize, fileOperationBlockSize );
        if( uploadResult != urOperationSuccessful )
        {
            return uploadResult;
        }
    }
    {
        static const int MAX_UPDATE_TIME_MILLISECONDS = 40000;
        static const int UPDATE_THREAD_PROGRESS_INTERVAL_MILLISECONDS = 100;
        wxProgressDialog* pDlg = IsProgressDisplayActive() ? new wxProgressDialog( wxT( "Storing Firmware To Flash" ),
                                 wxT( "Please wait...                                " ),
                                 MAX_UPDATE_TIME_MILLISECONDS, // range
                                 pParent_,     // parent
                                 wxPD_AUTO_HIDE | wxPD_APP_MODAL | wxPD_ELAPSED_TIME ) : 0;

        if( currentFirmwareVersion < s_firstVersionWithPendingACK )
        {
            // we need to increase the method polling retry count here, as this device does not support
            // the pending ACK feature. Instead executing the 'fileOperationExecute' call will simply
            // take much longer
            SystemSettingsGenTL ss( pDev_ );
            ss.methodPollingMaxRetryCount.write( MAX_UPDATE_TIME_MILLISECONDS / ss.methodPollingInterval_ms.read() );
        }
        StoreToFlashThread storeThread( fac );
        RunThreadAndShowProgress( &storeThread, pDlg, UPDATE_THREAD_PROGRESS_INTERVAL_MILLISECONDS, MAX_UPDATE_TIME_MILLISECONDS );
        DeleteElement( pDlg );
        if( static_cast< TDMR_ERROR >( storeThread.GetResult() ) != DMR_NO_ERROR )
        {
            MessageToUser( wxT( "Warning" ), wxString::Format( wxT( "Failed to write firmware to non-volatile memory of device %s failed. Error reported from driver: %d(%s). Please power-cycle the device now, update the device list and re-try to update the firmware." ), ConvertedString( serial ).c_str(), storeThread.GetResult(), ConvertedString( ImpactAcquireException::getErrorCodeAsString( storeThread.GetResult() ) ).c_str() ), boSilentMode, wxOK | wxICON_INFORMATION );
            return urFileIOError;
        }
        // it would be correct to enable this next section but there was a bug in the firmware until version 1.6.129 that would return
        // an error here, thus lots of devices out there would suddenly report a problem during the update so we leave this code disabled!
        //if( fac.fileOperationStatus.readS() != "Success" )
        //{
        //    MessageToUser( wxT( "Warning" ), wxString::Format( wxT( "Failed to write firmware to non-volatile memory of device %s failed. Error reported from device (fileOperationStatus): '%s'. Please power-cycle the device now, update the device list and re-try to update the firmware." ), ConvertedString( serial ).c_str(), ConvertedString( fac.fileOperationStatus.readS() ).c_str() ), boSilentMode, wxOK | wxICON_INFORMATION );
        //    return urFileIOError;
        //}
    }
    RebootDevice( boSilentMode, serial );

    if( result == urOperationSuccessful )
    {
        // now check if the new firmware is actually active
        try
        {
            if( pDev_->isOpen() )
            {
                pDev_->close();
            }
            DeviceManager devMgr;
            devMgr.updateDeviceList(); // this is needed in order to invalidate cached data that might no longer be valid(e.g. the URLs for the description files)
            SelectCustomGenICamFile();
            pDev_->open();
        }
        catch( const ImpactAcquireException& e )
        {
            if( pParent_ )
            {
                pParent_->WriteLogMessage( wxString::Format( wxT( "Failed to close and re-open device %s(%s)(%s, %s).\n" ),
                                           ConvertedString( pDev_->serial.read() ).c_str(),
                                           ConvertedString( pDev_->product.read() ).c_str(),
                                           ConvertedString( e.getErrorString() ).c_str(),
                                           ConvertedString( e.getErrorCodeAsString() ).c_str() ) );
            }
        }

        GenICam::DeviceControl dc( pDev_ );
        if( dc.mvDeviceFirmwareSource.isValid() )
        {
            if( boDeviceMustBeUnplugged )
            {
                MessageToUser( wxT( "Warning" ), wxT( "The device was running with a fall-back firmware version before the update. You need to PHYSICALLY UNPLUG the device from the power supply NOW and then after reconnecting and rebooting check if the new firmware is active manually. If not please contact MATRIX VISION." ), boSilentMode, wxOK | wxICON_INFORMATION );
            }
            else if( ConvertedString( dc.mvDeviceFirmwareSource.readS() ) != wxString( wxT( "ProgramSection" ) ) )
            {
                MessageToUser( wxT( "Warning" ), wxT( "The firmware update did not succeed. The device is running with a fall-back firmware version now as the update section is damaged. Please repeat the firmware update and at the end power-cycle the device when this message shows up again. If afterwards the property 'DeviceControl/mvDeviceFirmwareSource' does not show 'ProgramSection' please contact MATRIX VISION." ), boSilentMode, wxOK | wxICON_INFORMATION );
                result = urFirmwareUpdateFailed;
            }
        }
        else
        {
            MessageToUser( wxT( "Warning" ), wxT( "The firmware of this device could not be verified. Please check if the firmware version displayed matches the expected version after power-cycling the device." ), boSilentMode, wxOK | wxICON_INFORMATION );
        }
    }
    pDev_->close();
    return result;
}

//-----------------------------------------------------------------------------
bool DeviceHandlerBlueCOUGAR::GetIDFromUser( long& /*newID*/, const long /*minValue*/, const long /*maxValue*/ ) const
//-----------------------------------------------------------------------------
{
    ConvertedString serial( pDev_->serial.read() );
    wxString tool = wxT( "mvIPConfigure" );
    wxString standard = wxT( "GigE Vision" );

    if( MessageToUser( wxT( "Set Device ID" ), wxString::Format( wxT( "Device %s does not support setting an integer device ID.\n\nAs this device is %s compliant it might however support setting a user defined string by modifying the property 'DeviceUserID' that can be used instead.\n\nThis string can be assigned using the tool '%s'. Do you want to launch this application now?" ), serial.c_str(), standard.c_str(), tool.c_str() ), false, wxNO_DEFAULT | wxYES_NO | wxICON_INFORMATION ) )
    {
        ::wxExecute( tool );
    }
    return false;
}

//-----------------------------------------------------------------------------
bool DeviceHandlerBlueCOUGAR::IsMatrixVisionDeviceRunningWithNormalFirmware( void ) const
//-----------------------------------------------------------------------------
{
    const bool wasOpen = pDev_->isOpen();
    bool boResult = true;
    try
    {
        if( !wasOpen )
        {
            pDev_->interfaceLayout.write( dilGenICam );
        }
        GenICam::DeviceControl dc( pDev_ );
        if( dc.mvDeviceFirmwareSource.isValid() )
        {
            if( ConvertedString( dc.mvDeviceFirmwareSource.readS() ) != wxString( wxT( "ProgramSection" ) ) )
            {
                boResult = false;
            }
        }
        if( !wasOpen )
        {
            pDev_->close();
        }
    }
    catch( const ImpactAcquireException& ) {}
    return boResult;
}
