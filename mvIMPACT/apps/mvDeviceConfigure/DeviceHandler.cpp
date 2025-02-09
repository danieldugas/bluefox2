//-----------------------------------------------------------------------------
#include <apps/Common/wxAbstraction.h>
#include "DeviceConfigureFrame.h"
#include "DeviceHandler.h"
#include <string>

#include <apps/Common/wxIncludePrologue.h>
#include "wx/numdlg.h"
#include <apps/Common/wxIncludeEpilogue.h>

using namespace std;

//-----------------------------------------------------------------------------
bool ShowMessageToUser( DeviceConfigureFrame* pParent, const wxString& caption, const wxString& msg, bool boSilentMode, long style, bool boError )
//-----------------------------------------------------------------------------
{
    bool boResult = false;
    if( boSilentMode )
    {
        if( pParent )
        {
            if( boError )
            {
                pParent->WriteErrorMessage( wxString::Format( wxT( "%s: %s\n" ), caption.c_str(), msg.c_str() ) );
            }
            else
            {
                pParent->WriteLogMessage( wxString::Format( wxT( "%s: %s\n" ), caption.c_str(), msg.c_str() ) );
            }
        }
        boResult = true;
    }
    else
    {
        wxMessageDialog dlg( pParent, msg, caption, style );
        switch( dlg.ShowModal() )
        {
        case wxID_OK:
        case wxID_YES:
        case wxID_APPLY:
        case wxID_YESTOALL:
            boResult = true;
            break;
        default:
            break;
        }
    }
    return boResult;
}

//=============================================================================
//======================== Implementation DeviceHandler =======================
//=============================================================================
//-----------------------------------------------------------------------------
bool DeviceHandler::GetIDFromUser( long& newID, const long minValue, const long maxValue ) const
//-----------------------------------------------------------------------------
{
    const string devSerial( pDev_->serial.read() );
    const string devProduct( pDev_->product.read() );
    if( pParent_ )
    {
        pParent_->WriteLogMessage( wxString::Format( wxT( "Trying to set new ID for %s(%s). Current ID: %d\n" ), ConvertedString( devSerial ).c_str(), ConvertedString( devProduct ).c_str(), pDev_->deviceID.read() ) );
    }
    newID = wxGetNumberFromUser( wxString::Format( wxT( "Enter the new ID for device %s(%s).\nMake sure that no other device of the same family already owns this new ID.\n" ), ConvertedString( devSerial ).c_str(), ConvertedString( devProduct ).c_str() ),
                                 wxT( "New ID:" ),
                                 wxString::Format( wxT( "Set New ID For %s(%s)" ), ConvertedString( devSerial ).c_str(), ConvertedString( pDev_->family.read() ).c_str() ),
                                 0,          // start
                                 minValue,   // min
                                 maxValue,   // max
                                 pParent_ );
    return true;
}

//-----------------------------------------------------------------------------
int DeviceHandler::UpdateFirmware( bool /*boSilentMode*/, bool /*boPersistentUserSets*/, bool /*boUseOnlineArchive*/ )
//-----------------------------------------------------------------------------
{
    if( pParent_ )
    {
        pParent_->WriteErrorMessage( wxString::Format( wxT( "Device %s doesn't implement firmware updates\n" ), ConvertedString( pDev_->serial.read() ).c_str() ) );
    }
    return urFeatureUnsupported;
}
