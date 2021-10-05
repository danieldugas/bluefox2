//-----------------------------------------------------------------------------
#ifndef DeviceHandlerBlueFOXH
#define DeviceHandlerBlueFOXH DeviceHandlerBlueFOXH
//-----------------------------------------------------------------------------
#include "DeviceHandler.h"

//-----------------------------------------------------------------------------
class DeviceHandlerBlueFOX : public DeviceHandler
//-----------------------------------------------------------------------------
{
public:
    DeviceHandlerBlueFOX( mvIMPACT::acquire::Device* pDev ) : DeviceHandler( pDev, true ) {}
    static DeviceHandler* Create( mvIMPACT::acquire::Device* pDev )
    {
        return new DeviceHandlerBlueFOX( pDev );
    }
    virtual bool SupportsFirmwareUpdate( void ) const
    {
        return true;
    }
    virtual bool SupportsFirmwareUpdateFromMVU( void ) const
    {
        return false;
    }
    virtual int GetLatestLocalFirmwareVersion( Version& latestFirmwareVersion ) const;
    virtual const wxString GetAdditionalUpdateInformation( void ) const
    {
        return wxString( wxT( "Firmware updates are part of the installed driver library. So updating the driver might also present a more recent firmware relase here!" ) );
    }
    virtual wxString GetStringRepresentationOfFirmwareVersion( const Version& version ) const
    {
        return wxString::Format( wxT( "%ld" ), version.major_ );
    }
    virtual int UpdateFirmware( bool boSilentMode, bool boPersistentUserSets, bool boUseOnlineArchive );
};

#endif // DeviceHandlerBlueFOXH
