//-----------------------------------------------------------------------------
#ifndef DeviceHandlerH
#define DeviceHandlerH DeviceHandlerH
//-----------------------------------------------------------------------------
#include <mvIMPACT_CPP/mvIMPACT_acquire.h>
#include "PackageDescriptionParser.h"

class DeviceConfigureFrame;

bool ShowMessageToUser( DeviceConfigureFrame* pParent, const wxString& caption, const wxString& msg, bool boSilentMode, long style, bool boError );

//-----------------------------------------------------------------------------
class DeviceHandler
//-----------------------------------------------------------------------------
{
    bool boSetIDSupported_;
    bool boDisplayProgress_;
protected:
    mvIMPACT::acquire::Device* pDev_;
    DeviceConfigureFrame* pParent_;
    wxString customFirmwarePath_;
    bool MessageToUser( const wxString& caption, const wxString& msg, bool boSilentMode, long style ) const
    {
        return ShowMessageToUser( pParent_, caption, msg, boSilentMode, style, false );
    }
public:
    explicit DeviceHandler( mvIMPACT::acquire::Device* pDev, bool boSetIDSupported = false ) : boSetIDSupported_( boSetIDSupported ), boDisplayProgress_( true ), pDev_( pDev ), pParent_( 0 ) {}
    virtual ~DeviceHandler() {}
    void AttachParent( DeviceConfigureFrame* pParent )
    {
        pParent_ = pParent;
    }
    void ConfigureProgressDisplay( const bool boDisplayProgress )
    {
        boDisplayProgress_ = boDisplayProgress;
    }
    bool IsProgressDisplayActive( void ) const
    {
        return boDisplayProgress_;
    }
    virtual bool SupportsFirmwareUpdate( void ) const = 0;
    virtual bool SupportsFirmwareUpdateFromInternet( void ) const
    {
        return false;
    }
    virtual bool SupportsFirmwareUpdateFromMVU( void ) const = 0;
    virtual int GetFirmwareUpdateFolder( wxString& /*firmwareUpdateFolder*/ ) const
    {
        return urFeatureUnsupported;
    }
    virtual int GetLatestFirmwareFromList( Version& /*latestFirmwareVersion*/, const  FileEntryContainer& /*fileEntries*/, size_t& /*fileEntryPosition*/ ) const
    {
        return urFeatureUnsupported;
    }
    virtual int GetLatestLocalFirmwareVersion( Version& /*latestFirmwareVersion*/ ) const
    {
        return urFeatureUnsupported;
    }
    virtual int GetLatestLocalFirmwareVersionToMap( Version& /*latestFirmwareVersion*/, ProductToFileEntryMap& /*Map*/ ) const
    {
        return urFeatureUnsupported;
    }
    virtual const wxString GetAdditionalUpdateInformation( void ) const
    {
        return wxString( wxEmptyString );
    }
    virtual wxString GetStringRepresentationOfFirmwareVersion( const Version& version ) const
    {
        return wxString::Format( wxT( "%ld.%ld.%ld.%ld" ), version.major_, version.minor_, version.subMinor_, version.release_ );
    }
    bool SupportsSetID( void ) const
    {
        return boSetIDSupported_;
    }
    virtual bool GetIDFromUser( long& newID, const long minValue, const long maxValue ) const;
    virtual bool SupportsDMABufferSizeUpdate( int* /* pCurrentDMASize_kB */ = 0 )
    {
        return false;
    }
    virtual int UpdateFirmware( bool boSilentMode, bool boPersistentUserSets, bool boUseOnlineArchive );
    virtual int UpdatePermanentDMABufferSize( bool /*boSilentMode*/ )
    {
        return urFeatureUnsupported;
    }
    virtual void SetCustomFirmwareFile( const wxString& /* customFirmwareFile */ ) {}
    virtual void SetCustomFirmwarePath( const wxString& /* customFirmwarePath */ ) {}
    virtual void SetCustomGenICamFile( const wxString& /* customGenICamFile */ ) {}
    //-----------------------------------------------------------------------------
    enum TUpdateResult
    //-----------------------------------------------------------------------------
    {
        urOperationSuccessful = 0,
        urFeatureUnsupported = 1,
        urOperationCanceled = 2,
        urInvalidFileSelection = 3,
        urFileIOError = 4,
        urDeviceAccessError = 5,
        urInvalidParameter = 6,
        urFirmwareUpdateFailed = 7
    };
};

//-----------------------------------------------------------------------------
class DeviceHandler3rdParty : public DeviceHandler
//-----------------------------------------------------------------------------
{
public:
    explicit DeviceHandler3rdParty( mvIMPACT::acquire::Device* pDev ) : DeviceHandler( pDev ) {}
    static DeviceHandler* Create( mvIMPACT::acquire::Device* pDev )
    {
        return new DeviceHandler3rdParty( pDev );
    }
    virtual bool SupportsFirmwareUpdate( void ) const
    {
        return false;
    }
    virtual bool SupportsFirmwareUpdateFromMVU( void ) const
    {
        return false;
    }
};

#endif // DeviceHandlerH
