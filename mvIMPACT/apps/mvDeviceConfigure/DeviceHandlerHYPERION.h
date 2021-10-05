//-----------------------------------------------------------------------------
#ifndef mvHYPERIONHandlerH
#define mvHYPERIONHandlerH mvHYPERIONHandlerH
//-----------------------------------------------------------------------------
#include "DeviceHandler.h"

//-----------------------------------------------------------------------------
class DeviceHandlerHYPERION : public DeviceHandler
//-----------------------------------------------------------------------------
{
    wxString firmwareUpdateFileName_;
    wxString firmwareUpdateFolder_;
public:
    explicit DeviceHandlerHYPERION( mvIMPACT::acquire::Device* pDev ) : DeviceHandler( pDev ), firmwareUpdateFileName_(), firmwareUpdateFolder_() {}
    static DeviceHandler* Create( mvIMPACT::acquire::Device* pDev )
    {
        return new DeviceHandlerHYPERION( pDev );
    }
    virtual void SetCustomFirmwareFile( const wxString& customFirmwareFile );
    virtual void SetCustomFirmwarePath( const wxString& customFirmwarePath );
    virtual bool SupportsFirmwareUpdate( void ) const
    {
        return true;
    }
    virtual bool SupportsDMABufferSizeUpdate( int* pCurrentDMASize_kB = 0 );
    virtual bool SupportsFirmwareUpdateFromMVU( void ) const
    {
        return false;
    }
    virtual int UpdateFirmware( bool boSilentMode, bool boPersistentUserSets, bool boUseOnlineArchive );
    virtual int UpdatePermanentDMABufferSize( bool boSilentMode );
};

#endif // mvHYPERIONHandlerH
