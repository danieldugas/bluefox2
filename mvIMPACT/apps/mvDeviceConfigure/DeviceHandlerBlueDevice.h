//-----------------------------------------------------------------------------
#ifndef DeviceHandlerBlueDeviceH
#define DeviceHandlerBlueDeviceH DeviceHandlerBlueDeviceH
//-----------------------------------------------------------------------------
#include <common/auto_array_ptr.h>
#include "DeviceHandler.h"
#include <mvIMPACT_CPP/mvIMPACT_acquire_GenICam_FileStream.h>
#include "PackageDescriptionParser.h"

class wxProgressDialog;

//-----------------------------------------------------------------------------
class DeviceHandlerBlueDeviceBase : public DeviceHandler
//-----------------------------------------------------------------------------
{
    wxString temporaryFolder_;
protected:
    wxString GenICamFile_;
    wxString                            GetFullyQualifiedPathForUserSetBackup( void ) const
    {
        return temporaryFolder_ + wxString( pDev_->serial.readS().c_str(), wxConvUTF8 ) + wxT( ".xml" );
    }
    void                                SelectCustomGenICamFile( const wxString& descriptionFile = wxEmptyString );
    wxString                            SetUserSetDefault( const wxString& userSetDefaultValue );
    void                                UserSetBackup( bool boSilentMode );
    void                                UserSetRestore( bool boSilentMode, const wxString& previousUserSetDefaultValueToRestore );
    void                                WaitForDeviceToReappear( const wxString& serial );
public:
    explicit                            DeviceHandlerBlueDeviceBase( mvIMPACT::acquire::Device* pDev, bool boSetIDSupported = false );
    virtual                             ~DeviceHandlerBlueDeviceBase();
    virtual int                         GetLatestLocalFirmwareVersion( Version& latestFirmwareVersion ) const = 0;
    virtual int                         UpdateFirmware( bool boSilentMode, bool boPersistentUserSets, bool boUseOnlineArchive ) = 0;
};

//-----------------------------------------------------------------------------
class DeviceHandlerBlueDevice : public DeviceHandlerBlueDeviceBase
//-----------------------------------------------------------------------------
{
private:
    virtual int                         CheckForIncompatibleFirmwareVersions( bool /*boSilentMode*/, const wxString& /*serial*/, const FileEntryContainer& /*fileEntries*/, const wxString& /*selection*/, const Version& /*currentFirmwareVersion*/ ) = 0;
    void                                DetermineValidFirmwareChoices( const wxString& productFromManufacturerInfo, const wxString& product, const FileEntryContainer& fileEntries, const FileEntryContainer::size_type cnt, const Version& deviceVersion, wxArrayString& choices, wxArrayString& unusableFiles );
    virtual TUpdateResult               DoFirmwareUpdate( bool /*boSilentMode*/, const wxString& /*serial*/, const char* /*pBuf*/, const size_t /*bufSize*/, const Version& /*currentFirmwareVersion*/ ) = 0;
    bool                                ExtractFileVersion( const wxString& fileName, Version& fileVersion ) const;
    static bool                         GetFileFromArchive( const wxString& firmwareFileAndPath, const char* pArchive, size_t archiveSize, const wxString& filename, auto_array_ptr<char>& data, DeviceConfigureFrame* pParent );
    static wxString                     GetProductFromManufacturerInfo( Device* pDev );
    std::vector<std::map<wxString, SuitableProductKey>::const_iterator> GetSuitableFirmwareIterators( const FileEntry& entry, const wxString& product, const wxString& productFromManufacturerInfo ) const;
    bool                                IsFirmwareUpdateMeaningless( bool boSilentMode, const Version& deviceFWVersion, const Version& selectedFWVersion, const wxString& defaultFWArchive, const Version& defaultFolderFWVersion ) const;
    virtual bool                        IsMatrixVisionDeviceRunningWithNormalFirmware( void ) const
    {
        return true;
    }
    int                                 UpdateCOUGAR_SDevice( bool boSilentMode );
    int                                 UpdateDevice( bool boSilentMode, bool boPersistentUserSets, bool boUseOnlineArchive );
    int                                 UploadFile( const wxString& fullPath, const wxString& descriptionFile );
protected:
    enum TProductGroup
    {
        pgUnknown,
        pgBlueCOUGAR_S,
        pgBlueCOUGAR_X,
        pgBlueCOUGAR_XT,
        pgBlueCOUGAR_Y,
        pgBlueFOX3
    };
    TProductGroup product_;
    wxString firmwareUpdateFileName_;
    wxString firmwareUpdateFileFilter_;
    wxString firmwareUpdateFolder_;
    wxString firmwareUpdateDefaultFolder_;
    wxString packageDescription_;
    static bool                         IsSpecificType( const wxString& manufacturerSpecificInformation, const wxString& typeSuffix )
    {
        const wxArrayString tokens = wxSplit( manufacturerSpecificInformation, wxT( '-' ) );
        if( tokens.Count() == 4 )
        {
            if( ( tokens[0].length() == 3 ) &&
                ( tokens[1].length() == 6 ) &&
                ( tokens[2].length() == 6 ) &&
                ( tokens[3].length() == 4 ) &&
                ( ( tokens[3].find( typeSuffix ) == 0 ) ) )
            {
                return true;
            }
        }
        return false;
    }
    static bool                         IsBlueCOUGAR_X_BAS( Device* pDev )
    {
        const wxString token = GetProductFromManufacturerInfo( pDev );
        const std::string tokenANSI( token.mb_str() );
        if( ( IsSpecificType( token, "BAS" ) ) && token.StartsWith( wxT( "GX" ) ) && ( token.Length() > 2 ) && isdigit( tokenANSI[2] ) )
        {
            return true;
        }
        return false;
    }
    static bool                         IsBlueCOUGAR_X_XAS( Device* pDev )
    {
        const wxString token = GetProductFromManufacturerInfo( pDev );
        if( token.StartsWith( wxT( "X" ) ) && token.Contains( wxT( "9XAS1" ) ) ) // initial nomenclature
        {
            return true;
        }
        const std::string tokenANSI( token.mb_str() );
        if( IsSpecificType( token, "XAS" ) && token.StartsWith( wxT( "GX" ) ) && ( token.Length() > 2 ) && isdigit( tokenANSI[2] ) )
        {
            return true;
        }
        return false;
    }
    static bool                         IsBlueCOUGAR_XT_MACAddress( const int64_type MAC )
    {
        return ( ( MAC >= 0x0000000c8d638001LL ) && ( MAC <= 0x0000000c8d6fffffLL ) );
    }
    bool                                IsBlueCOUGAR_Y( void ) const;
    static bool                         IsBlueFOX3_XAS( Device* pDev )
    {
        const wxString token = GetProductFromManufacturerInfo( pDev );
        if( token.StartsWith( wxT( "2" ) ) && token.Contains( wxT( "9XAS1" ) ) ) // initial nomenclature
        {
            return true;
        }
        if( IsSpecificType( token, "XAS" ) && token.StartsWith( wxT( "SF" ) ) )
        {
            return true;
        }
        return false;
    }
    bool                                IsBlueSIRIUS( void ) const
    {
        return pDev_->product.read().find( "mvBlueSIRIUS" ) != std::string::npos;
    }
    void                                RebootDevice( bool boSilentMode, const wxString& serial );
    TUpdateResult                       UploadFirmwareFile( mvIMPACT::acquire::GenICam::ODevFileStream& uploadFile, bool boSilentMode, const wxString& serial, const char* pBuf, const size_t bufSize, const wxFileOffset fileOperationBlockSize );
public:
    explicit                            DeviceHandlerBlueDevice( mvIMPACT::acquire::Device* pDev );
    virtual                            ~DeviceHandlerBlueDevice() {}
    virtual int GetFirmwareUpdateFolder( wxString& firmwareUpdateFolder ) const
    {
        if( !firmwareUpdateFolder_.empty() )
        {
            firmwareUpdateFolder = firmwareUpdateFolder_;
            return urOperationSuccessful;
        }
        return urFileIOError;
    }
    static TUpdateResult                ParseUpdatePackage( PackageDescriptionFileParser& fileParser, const wxString& firmwareFileAndPath, DeviceConfigureFrame* pParent, auto_array_ptr<char>& pBuffer );
    virtual int                         GetLatestLocalFirmwareVersion( Version& latestFirmwareVersion ) const;
    virtual int                         GetLatestLocalFirmwareVersionToMap( Version& latestFirmwareVersion, ProductToFileEntryMap& /*notUsed*/ ) const;
    virtual int                         GetLatestFirmwareFromList( Version& latestFirmwareVersion, const FileEntryContainer& fileEntries, size_t& fileEntryPosition ) const;
    virtual const wxString              GetAdditionalUpdateInformation( void ) const
    {
        return wxString( wxT( "Firmware updates can be downloaded from www.matrix-vision.com. When copied into $(MVIMPACT_ACQUIRE_DATA_DIR)/FirmwareUpdates/<productFamily> this tool will automatically check if the firmware archive contains an update for this device." ) );
    }
    static bool                         IsBlueCOUGAR_X( Device* pDev );
    static bool                         IsBlueCOUGAR_XT( Device* pDev );
    static bool                         IsBlueFOX3( Device* pDev )
    {
        return( ( ConvertedString( pDev->family.read() ).Find( wxT( "mvBlueFOX3" ) ) == wxNOT_FOUND ) &&
                !IsBlueFOX3_XAS( pDev ) ) ? false : true;
    }
    static TUpdateResult                ParsePackageDescriptionFromBuffer( PackageDescriptionFileParser& parser, const char* pPackageDescription, const size_t bufferSize, DeviceConfigureFrame* pParent );
    virtual void                        SetCustomFirmwareFile( const wxString& customFirmwareFile );
    virtual void                        SetCustomFirmwarePath( const wxString& customFirmwarePath );
    virtual void                        SetCustomGenICamFile( const wxString& customGenICamFile )
    {
        GenICamFile_ = customGenICamFile;
    }
    void                        SetPackageDescription( const wxString& packageDescription )
    {
        packageDescription_ = packageDescription;
    }
    virtual bool                        SupportsFirmwareUpdate( void ) const
    {
        return !firmwareUpdateFileName_.IsEmpty();
    }
    virtual bool                        SupportsFirmwareUpdateFromInternet( void ) const
    {
        return false;
    }
    virtual int                         UpdateFirmware( bool boSilentMode, bool boPersistentUserSets, bool boUseOnlineArchive );
};

int         CompareMVUFileVersion( const wxString& first, const wxString& second );
wxString    GetPlatformSpecificDownloadPath( const wxString& pathSuffix = wxEmptyString );

//-----------------------------------------------------------------------------
class DeviceHandlerBlueNAOS : public DeviceHandlerBlueDeviceBase
//-----------------------------------------------------------------------------
{
    bool boSupportsFirmwareUpdate_;
    mvIMPACT::acquire::Method* pUpdateFirmware_;
public:
    explicit                            DeviceHandlerBlueNAOS( mvIMPACT::acquire::Device* pDev );
    virtual                             ~DeviceHandlerBlueNAOS();
    static DeviceHandler*               Create( mvIMPACT::acquire::Device* pDev )
    {
        return new DeviceHandlerBlueNAOS( pDev );
    }
    virtual bool                        SupportsFirmwareUpdate( void ) const
    {
        return boSupportsFirmwareUpdate_;
    }
    virtual bool SupportsFirmwareUpdateFromMVU( void ) const
    {
        return false;
    }
    bool                                GetIDFromUser( long& /*newID*/, const long /*minValue*/, const long /*maxValue*/ ) const;
    virtual int                         GetLatestLocalFirmwareVersion( Version& latestFirmwareVersion ) const;
    virtual int                         UpdateFirmware( bool boSilentMode, bool boPersistentUserSets, bool boUseOnlineArchive );
};

//-----------------------------------------------------------------------------
class DeviceHandlerBlueFOX3 : public DeviceHandlerBlueDevice
//-----------------------------------------------------------------------------
{
    virtual int                         CheckForIncompatibleFirmwareVersions( bool /*boSilentMode*/, const wxString& /*serial*/, const FileEntryContainer& /*fileEntries*/, const wxString& /*selection*/, const Version& /*currentFirmwareVersion*/ )
    {
        return urOperationSuccessful;
    }
    virtual TUpdateResult               DoFirmwareUpdate( bool boSilentMode, const wxString& serial, const char* pBuf, const size_t bufSize, const Version& /*currentFirmwareVersion*/ );
    virtual bool                        IsMatrixVisionDeviceRunningWithNormalFirmware( void ) const;
public:
    explicit                            DeviceHandlerBlueFOX3( mvIMPACT::acquire::Device* pDev );
    static DeviceHandler* Create( mvIMPACT::acquire::Device* pDev )
    {
        return new DeviceHandlerBlueFOX3( pDev );
    }
    bool                                GetIDFromUser( long& /*newID*/, const long /*minValue*/, const long /*maxValue*/ ) const;
    virtual bool                        SupportsFirmwareUpdateFromInternet( void ) const
    {
        return true;
    }
    virtual bool SupportsFirmwareUpdateFromMVU( void ) const
    {
        return true;
    }
};

//-----------------------------------------------------------------------------
class DeviceHandlerBlueCOUGAR : public DeviceHandlerBlueDevice
//-----------------------------------------------------------------------------
{
    virtual int                         CheckForIncompatibleFirmwareVersions( bool boSilentMode, const wxString& serial, const FileEntryContainer& fileEntries, const wxString& selection, const Version& currentFirmwareVersion );
    virtual TUpdateResult               DoFirmwareUpdate( bool boSilentMode, const wxString& serial, const char* pBuf, const size_t bufSize, const Version& currentFirmwareVersion );
    virtual bool                        IsMatrixVisionDeviceRunningWithNormalFirmware( void ) const;
public:
    explicit                            DeviceHandlerBlueCOUGAR( mvIMPACT::acquire::Device* pDev );
    static DeviceHandler* Create( mvIMPACT::acquire::Device* pDev )
    {
        return new DeviceHandlerBlueCOUGAR( pDev );
    }
    virtual bool                        SupportsFirmwareUpdateFromInternet( void ) const
    {
        return true;
    }
    virtual bool SupportsFirmwareUpdateFromMVU( void ) const
    {
        return true;
    }
    bool                                GetIDFromUser( long& /*newID*/, const long /*minValue*/, const long /*maxValue*/ ) const;
};

//-----------------------------------------------------------------------------
class BVSCADeviceHandlerCreator
//-----------------------------------------------------------------------------
{
public:
    static DeviceHandler* Create( mvIMPACT::acquire::Device* pDev )
    {
        if( DeviceHandlerBlueDevice::IsBlueCOUGAR_X( pDev ) )
        {
            return new DeviceHandlerBlueCOUGAR( pDev );
        }
        else if( DeviceHandlerBlueDevice::IsBlueFOX3( pDev ) )
        {
            return new DeviceHandlerBlueFOX3( pDev );
        }
        return 0;
    }
};

#endif // DeviceHandlerBlueDeviceH
