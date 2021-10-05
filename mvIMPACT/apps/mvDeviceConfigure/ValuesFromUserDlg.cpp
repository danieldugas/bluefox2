//-----------------------------------------------------------------------------
#include <apps/Common/CommonGUIFunctions.h>
#include <apps/Common/wxAbstraction.h>
#include <common/STLHelper.h>
#include "ValuesFromUserDlg.h"
#include "CheckedListCtrl.h"

#include <apps/Common/wxIncludePrologue.h>
#include <wx/webview.h>
#include <wx/progdlg.h>
#include <apps/Common/wxIncludeEpilogue.h>

using namespace std;
using namespace mvIMPACT::acquire::GenICam;

//=============================================================================
//============== Implementation OkAndCancelDlg ================================
//=============================================================================
BEGIN_EVENT_TABLE( OkAndCancelDlg, wxDialog )
    EVT_BUTTON( widBtnOk, OkAndCancelDlg::OnBtnOk )
    EVT_BUTTON( widBtnCancel, OkAndCancelDlg::OnBtnCancel )
    EVT_BUTTON( widBtnApply, OkAndCancelDlg::OnBtnApply )
END_EVENT_TABLE()

//-----------------------------------------------------------------------------
OkAndCancelDlg::OkAndCancelDlg( wxWindow* pParent, wxWindowID id, const wxString& title, const wxPoint& pos /*= wxDefaultPosition*/,
                                const wxSize& size /*= wxDefaultSize*/, long style /*= wxDEFAULT_DIALOG_STYLE*/, const wxString& name /* = "OkAndCancelDlg" */ )
    : wxDialog( pParent, id, title, pos, size, style, name ), pBtnApply_( 0 ), pBtnCancel_( 0 ), pBtnOk_( 0 )
//-----------------------------------------------------------------------------
{

}

//-----------------------------------------------------------------------------
void OkAndCancelDlg::FinalizeDlgCreation( wxWindow* pWindow, wxSizer* pSizer )
//-----------------------------------------------------------------------------
{
    pWindow->SetSizer( pSizer );
    pSizer->SetSizeHints( this );
    SetClientSize( pSizer->GetMinSize() );
    SetSizeHints( GetSize() );
}

//=============================================================================
//=================== Implementation UpdatesInformationDlg ====================
//=============================================================================
BEGIN_EVENT_TABLE( UpdatesInformationDlg, OkAndCancelDlg )
    EVT_CLOSE( UpdatesInformationDlg::OnClose )
    EVT_LIST_ITEM_SELECTED( widCheckedListCtrlFWPackage, UpdatesInformationDlg::OnSelectedIndexChanged )
    EVT_BUTTON( widDownloadSAelectedFWPackages, UpdatesInformationDlg::OnBtnDownloadSelectedFWPackagesClicked )
END_EVENT_TABLE()

//-----------------------------------------------------------------------------
UpdatesInformationDlg::UpdatesInformationDlg( DeviceConfigureFrame* pParent, const wxString& title, const UpdateableDevicesContainer& updateableDevicesContainer, const bool boCurrentAutoUpdateCheckState ) :
    OkAndCancelDlg( pParent, wxID_ANY, title, wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMAXIMIZE_BOX | wxMINIMIZE_BOX ), pParent_( pParent ), updateableDevicesContainer_( updateableDevicesContainer )
//-----------------------------------------------------------------------------
{
    /*
    |-------------------------------------|
    | pTopDownSizer_                      |
    |                spacer               |
    | |---------------------------------| |
    | |          Release Info           | |
    | |---------------------------------| |
    |                spacer               |
    | |---------------------------------| |
    | |            WhatsNew             | |
    | |---------------------------------| |
    |-------------------------------------|
    */

    pTopDownSizer_ = new wxBoxSizer( wxVERTICAL );
    pTopDownSizer_->AddSpacer( 20 );
    wxPanel* pPanel = new wxPanel( this );

    wxFont tmpFont = pPanel->GetFont();
    tmpFont.SetPointSize( pPanel->GetFont().GetPointSize() + 1 );

    wxStaticText* pVersionText = new wxStaticText( pPanel, wxID_ANY, ( !updateableDevicesContainer_.empty() ) ? wxT( "A newer firmware package has been released:" ) : wxT( "The newest Firmware package version has already been installed on your system:" ) );
    pVersionText->SetFont( tmpFont );
    pTopDownSizer_->Add( pVersionText, wxSizerFlags().Align( wxALIGN_CENTRE_HORIZONTAL ) );
    pTopDownSizer_->AddSpacer( 10 );

    pFirmwarePackageList_ = new CheckedListCtrl( pPanel, widCheckedListCtrlFWPackage, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_HRULES | wxLC_VRULES );
    BuildList();
    pTopDownSizer_->Add( pFirmwarePackageList_, wxSizerFlags().Align( wxALIGN_CENTRE_HORIZONTAL ).Expand() );
    pTopDownSizer_->AddSpacer( 10 );
    pTopDownSizer_->AddSpacer( 25 );
    wxStaticText* pWhatsNewText = new wxStaticText( pPanel, wxID_ANY, wxT( "What's new:" ) );
    pTopDownSizer_->Add( pWhatsNewText, wxSizerFlags().Align( wxALIGN_CENTRE_HORIZONTAL ) );

    pWhatsNewWebView_ = wxWebView::New( pPanel, wxID_ANY, wxT( "" ), wxDefaultPosition, wxSize( pFirmwarePackageList_->GetSize().x, pFirmwarePackageList_->GetSize().y * 2 ), wxWebViewBackendDefault, 0, wxT( "What's new:" ) );
    pTopDownSizer_->Add( pWhatsNewWebView_, wxSizerFlags( 1 ).Expand() );

    if( pFirmwarePackageList_->GetItemCount() > 0 )
    {
        pFirmwarePackageList_->SetItemState( 0, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED );
    }
    // lower line of buttons
    wxBoxSizer* pButtonSizer = new wxBoxSizer( wxHORIZONTAL );
    pCBAutoCheckForOnlineUpdatesWeekly_ = new wxCheckBox( pPanel, wxID_ANY, wxT( "&Auto-Check For Updates Every Week..." ) );
    pCBAutoCheckForOnlineUpdatesWeekly_->SetValue( boCurrentAutoUpdateCheckState );
    pButtonSizer->Add( pCBAutoCheckForOnlineUpdatesWeekly_, wxSizerFlags().Border( wxALL, 7 ) );
    pButtonSizer->AddStretchSpacer( 100 );

    if( pFirmwarePackageList_->GetItemCount() > 0 )
    {
        pBtnDownloadSelected_ = new wxButton( pPanel, widDownloadSAelectedFWPackages, wxT( "&Download Selected" ) );
        pButtonSizer->Add( pBtnDownloadSelected_, wxSizerFlags().Border( wxALL, 7 ) );
    }

    pBtnCancel_ = new wxButton( pPanel, widBtnCancel, wxT( "&Ok" ) );
    pButtonSizer->Add( pBtnCancel_, wxSizerFlags().Border( wxALL, 7 ) );
    pTopDownSizer_->Add( pButtonSizer, wxSizerFlags().Expand() );

    FinalizeDlgCreation( pPanel, pTopDownSizer_ );
    SetSize( 800, -1 );
    Center();
}

//-----------------------------------------------------------------------------
void UpdatesInformationDlg::BuildList( void )
//-----------------------------------------------------------------------------
{
    pFirmwarePackageList_->InsertColumn( flcMarkForDownload, wxT( "Download" ) );
    pFirmwarePackageList_->InsertColumn( flcDeviceType, wxT( "Type" ) );
    pFirmwarePackageList_->InsertColumn( flcDeviceSerial, wxT( "Serial" ) );
    pFirmwarePackageList_->InsertColumn( flcOnlineFirmwareVersion, wxT( "Latest Version" ) );
    pFirmwarePackageList_->InsertColumn( flcLocalFirmwareVersion, wxT( "Local Version" ) );
    pFirmwarePackageList_->InsertColumn( flcDateReleased, wxT( "Date Released" ) );

    UpdateableDevicesContainer::const_iterator it = updateableDevicesContainer_.begin();
    const UpdateableDevicesContainer::const_iterator itEND = updateableDevicesContainer_.end();

    while( it != itEND )
    {
        const long indexToUse = pFirmwarePackageList_->GetItemCount();
        const long index = pFirmwarePackageList_->InsertItem( indexToUse, 1 );
        pFirmwarePackageList_->SetItem( index, flcDeviceType, it->product_, -1 );
        pFirmwarePackageList_->SetItem( index, flcDeviceSerial, it->serial_, -1 );
        pFirmwarePackageList_->SetItem( index, flcOnlineFirmwareVersion, VersionToString( it->fileEntry_.version_ ), -1 );
        pFirmwarePackageList_->SetItem( index, flcLocalFirmwareVersion, VersionToString( it->deviceFWVersion_ ), -1 );
        pFirmwarePackageList_->SetItem( index, flcDateReleased, it->fileEntry_.onlineUpdateInfos_.timestamp_, -1 );
        deviceFamilyUpdateURLs_[it->serial_] = it->fileEntry_.onlineUpdateInfos_;
        it++;
    }

    pFirmwarePackageList_->SetColumnWidth( flcMarkForDownload, wxLIST_AUTOSIZE_USEHEADER );
    pFirmwarePackageList_->SetColumnWidth( flcDeviceType, wxLIST_AUTOSIZE );
    pFirmwarePackageList_->SetColumnWidth( flcDeviceSerial, wxLIST_AUTOSIZE );
    pFirmwarePackageList_->SetColumnWidth( flcOnlineFirmwareVersion, wxLIST_AUTOSIZE_USEHEADER );
    pFirmwarePackageList_->SetColumnWidth( flcLocalFirmwareVersion, wxLIST_AUTOSIZE_USEHEADER );
    pFirmwarePackageList_->SetColumnWidth( flcDateReleased, wxLIST_AUTOSIZE_USEHEADER );
}

//-----------------------------------------------------------------------------
void UpdatesInformationDlg::DownloadSelectedFWPackages( void )
//-----------------------------------------------------------------------------
{
    vector<wxString> packagesToDownload;
    long itemIndex = -1;
    while( ( itemIndex = pFirmwarePackageList_->GetNextItem( itemIndex, wxLIST_NEXT_ALL, wxLIST_STATE_DONTCARE ) ) != wxNOT_FOUND )
    {
        if( pFirmwarePackageList_->IsChecked( itemIndex ) == true )
        {
            packagesToDownload.push_back( pFirmwarePackageList_->GetItemText( itemIndex, flcDeviceSerial ) );
        }
    }
    const vector<wxString>::size_type pakcagesToDownloadSize = packagesToDownload.size();
    for( vector<wxString>::size_type i = 0; i < pakcagesToDownloadSize; i++ )
    {
        DeviceFamilyUpdateURLMap::iterator it = deviceFamilyUpdateURLs_.find( packagesToDownload[i] );
        const DeviceFamilyUpdateURLMap::const_iterator itEND = deviceFamilyUpdateURLs_.end();
        if( it != itEND )
        {
            pParent_->DownloadFWUpdateFile( it->second.updateArchiveUrl_ );
        }
    }
}

//-----------------------------------------------------------------------------
void UpdatesInformationDlg::HandleListCtrlIndexChange( void )
//-----------------------------------------------------------------------------
{
    long itemIndex = -1;
    while( ( itemIndex = pFirmwarePackageList_->GetNextItem( itemIndex, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED ) ) != wxNOT_FOUND )
    {
        const wxString selectedDevice = pFirmwarePackageList_->GetItemText( itemIndex, flcDeviceSerial );
        pWhatsNewWebView_->LoadURL( deviceFamilyUpdateURLs_[selectedDevice].releaseNotesUrl_ );
    }
    pWhatsNewWebView_->Refresh();
}
