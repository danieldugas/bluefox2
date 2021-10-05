//-----------------------------------------------------------------------------
#ifndef ValuesFromUserDlgH
#define ValuesFromUserDlgH ValuesFromUserDlgH
//-----------------------------------------------------------------------------
#include <mvIMPACT_CPP/mvIMPACT_acquire.h>
#include <mvIMPACT_CPP/mvIMPACT_acquire_GenICam.h>
#include "DeviceConfigureFrame.h"

#include <apps/Common/wxIncludePrologue.h>
#include <wx/wx.h>
#include <apps/Common/wxIncludeEpilogue.h>

class DeviceConfigureFrame;
class PropData;
class CheckedListCtrl;
class wxWebView;

typedef std::map<wxString, wxString> StringToStringMap;

//-----------------------------------------------------------------------------
class OkAndCancelDlg : public wxDialog
//-----------------------------------------------------------------------------
{
    DECLARE_EVENT_TABLE()
protected:
    wxButton*                   pBtnApply_;
    wxButton*                   pBtnCancel_;
    wxButton*                   pBtnOk_;

    virtual void OnBtnApply( wxCommandEvent& ) {}
    virtual void OnBtnCancel( wxCommandEvent& )
    {
        EndModal( wxID_CANCEL );
    }
    virtual void OnBtnOk( wxCommandEvent& )
    {
        EndModal( wxID_OK );
    }
    void AddButtons( wxWindow* pWindow, wxSizer* pSizer, bool boCreateApplyButton = false );
    void FinalizeDlgCreation( wxWindow* pWindow, wxSizer* pSizer );
    //-----------------------------------------------------------------------------
    enum TWidgetIDs
    //-----------------------------------------------------------------------------
    {
        widBtnOk = 1,
        widBtnCancel = 2,
        widBtnApply = 3,
        widCheckedListCtrlFWPackage = 4,
        widDownloadSAelectedFWPackages = 5,
        widFirst = 100
    };
public:
    explicit OkAndCancelDlg( wxWindow* pParent, wxWindowID id, const wxString& title, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = wxDEFAULT_DIALOG_STYLE, const wxString& name = wxT( "OkAndCancelDlg" ) );
};

//-----------------------------------------------------------------------------
class UpdatesInformationDlg : public OkAndCancelDlg
//-----------------------------------------------------------------------------
{
    DECLARE_EVENT_TABLE()

    wxBoxSizer* pTopDownSizer_;
    wxCheckBox* pCBAutoCheckForOnlineUpdatesWeekly_;
    CheckedListCtrl* pFirmwarePackageList_;
    wxButton* pBtnDownloadSelected_;
    wxWebView* pWhatsNewWebView_;
    DeviceConfigureFrame* pParent_;

    const std::vector<DeviceConfigureFrame::UpdateableDevices>& updateableDevicesContainer_;

    typedef std::map<wxString, ProductUpdateURLs> DeviceFamilyUpdateURLMap;
    DeviceFamilyUpdateURLMap deviceFamilyUpdateURLs_;

    //-----------------------------------------------------------------------------
    enum TFirmwareListColumn
    //-----------------------------------------------------------------------------
    {
        flcMarkForDownload,
        flcDeviceType,
        flcDeviceSerial,
        flcLocalFirmwareVersion,
        flcOnlineFirmwareVersion,
        flcDateReleased,
        flcReleaseNotes
    };

    void OnBtnDownloadSelectedFWPackagesClicked( wxCommandEvent& )
    {
        DownloadSelectedFWPackages();
    }
    void BuildList( void );
    void DownloadSelectedFWPackages( void );
    void HandleListCtrlIndexChange( void );
    void OnSelectedIndexChanged( wxListEvent& )
    {
        HandleListCtrlIndexChange();
    }
    virtual void OnClose( wxCloseEvent& )
    {
        EndModal( wxID_CANCEL );
    }
    typedef DeviceConfigureFrame::UpdateableDevicesContainer UpdateableDevicesContainer;
public:
    explicit UpdatesInformationDlg( DeviceConfigureFrame* pParent, const wxString& title, const UpdateableDevicesContainer& updateableDevicesContainer, const bool boCurrentAutoUpdateCheckState );
    bool IsWeeklyAutoCheckForOnlineUpdatesActive( void ) const
    {
        return pCBAutoCheckForOnlineUpdatesWeekly_->GetValue();
    }
};

#endif // ValuesFromUserDlgH
