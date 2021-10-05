//-----------------------------------------------------------------------------
#include <algorithm>
#include <apps/Common/wxAbstraction.h>
#include "DeviceConfigureFrame.h"
#include "DeviceHandlerBlueFOX.h"
#include <sstream>

#include <apps/Common/wxIncludePrologue.h>
#include "wx/msgdlg.h"
#include <apps/Common/wxIncludeEpilogue.h>

using namespace std;

//-----------------------------------------------------------------------------
int DeviceHandlerBlueFOX::GetLatestLocalFirmwareVersion( Version& latestFirmwareVersion ) const
//-----------------------------------------------------------------------------
{
    DeviceComponentLocator locator( pDev_->hDev() );
    PropertyI firmwareVersionToUpload;
    locator.bindComponent( firmwareVersionToUpload, "FirmwareVersionToUpload" );
    if( !firmwareVersionToUpload.isValid() )
    {
        return urFeatureUnsupported;
    }
    vector<int> availableFirmwareVersions;
    firmwareVersionToUpload.getTranslationDictValues( availableFirmwareVersions );
    sort( availableFirmwareVersions.begin(), availableFirmwareVersions.end() );
    latestFirmwareVersion = Version( availableFirmwareVersions.back(), 0, 0, 0 );
    return ( pDev_->firmwareVersion.read() < availableFirmwareVersions.back() ) ? urOperationSuccessful : urOperationCanceled;
}

//-----------------------------------------------------------------------------
int DeviceHandlerBlueFOX::UpdateFirmware( bool boSilentMode, bool /*boPersistentUserSets*/, bool /*boUseOnlineArchive*/ )
//-----------------------------------------------------------------------------
{
    const ConvertedString serial( pDev_->serial.read() );
    const ConvertedString product( pDev_->product.read() );
    if( MessageToUser( wxT( "Firmware Update" ), wxString::Format( wxT( "Are you sure you want to update the firmware of device %s?" ), serial.c_str() ), boSilentMode, wxNO_DEFAULT | wxYES_NO | wxICON_INFORMATION ) &&
        MessageToUser( wxT( "Information" ), wxT( "The firmware will now be updated. During this time(approx. 30 sec.) the application will not react. Please be patient." ), boSilentMode, wxOK | wxICON_INFORMATION ) )
    {
        if( pParent_ )
        {
            pParent_->WriteLogMessage( wxString::Format( wxT( "Updating firmware of device %s(%s). This might take some time. Please be patient.\n" ), serial.c_str(), product.c_str() ) );
        }
        const int result = pDev_->updateFirmware();
        if( pParent_ )
        {
            if( result == DMR_FEATURE_NOT_AVAILABLE )
            {
                pParent_->WriteLogMessage( wxString::Format( wxT( "Device %s(%s) doesn't support firmware updates.\n" ), serial.c_str(), product.c_str() ) );
                return urFeatureUnsupported;
            }
            else if( result != DMR_NO_ERROR )
            {
                pParent_->WriteLogMessage( wxString::Format( wxT( "An error occurred: %s(please refer to the manual for this error code).\n" ), ConvertedString( ImpactAcquireException::getErrorCodeAsString( result ) ).c_str() ) );
                return urFirmwareUpdateFailed;
            }
            else
            {
                pParent_->WriteLogMessage( wxString::Format( wxT( "%s.\n" ), ConvertedString( pDev_->HWUpdateResult.readS() ).c_str() ) );
            }
        }
        if( pDev_->HWUpdateResult.read() == urUpdateFWOK )
        {
            MessageToUser( wxT( "Update Result" ), wxString::Format( wxT( "Update successful. Please disconnect and reconnect device %s(%s) now to activate the new firmware." ), serial.c_str(), product.c_str() ), boSilentMode, wxOK | wxICON_INFORMATION );
        }
    }
    else
    {
        if( pParent_ )
        {
            pParent_->WriteLogMessage( wxString::Format( wxT( "Firmware update canceled for device %s(%s).\n" ), serial.c_str(), product.c_str() ) );
        }
        return urOperationCanceled;
    }
    return urOperationSuccessful;
}
