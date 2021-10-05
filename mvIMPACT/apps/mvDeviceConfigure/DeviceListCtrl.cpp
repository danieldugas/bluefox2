//-----------------------------------------------------------------------------
#include <common/STLHelper.h>
#include "DeviceListCtrl.h"
#include "DeviceConfigureFrame.h"
#include "LogOutputHandlerDlg.h"
#include <string>
#include <apps/Common/wxAbstraction.h>

BEGIN_EVENT_TABLE( DeviceListCtrl, wxListCtrl )
    EVT_LIST_COL_CLICK( LIST_CTRL, DeviceListCtrl::OnColClick )
    EVT_LIST_ITEM_ACTIVATED( LIST_CTRL, DeviceListCtrl::OnActivated )
    EVT_LIST_ITEM_SELECTED( LIST_CTRL, DeviceListCtrl::OnSelected )
    EVT_LIST_ITEM_RIGHT_CLICK( LIST_CTRL, DeviceListCtrl::OnItemRightClick )
    EVT_LIST_ITEM_DESELECTED( LIST_CTRL, DeviceListCtrl::OnDeselected )
    EVT_MENU( LIST_SET_ID, DeviceListCtrl::OnSetID )
    EVT_MENU( LIST_UPDATE_FW_LATEST_LOCAL, DeviceListCtrl::OnUpdateFirmwareLocalLatest )
    EVT_MENU( LIST_UPDATE_FW, DeviceListCtrl::OnUpdateFirmware )
    EVT_MENU( LIST_UPDATE_FW_FROM_INTERNET, DeviceListCtrl::OnUpdateFirmwareFromInternet )
    EVT_MENU( LIST_UPDATE_DMA_BUFFER_SIZE, DeviceListCtrl::OnUpdateDMABufferSize )
#ifdef BUILD_WITH_DIRECT_SHOW_SUPPORT
    EVT_MENU( LIST_SET_DS_FRIENDLY_NAME, DeviceListCtrl::OnSetDSFriendlyName )
#endif // #ifdef BUILD_WITH_DIRECT_SHOW_SUPPORT
END_EVENT_TABLE()

using namespace mvIMPACT::acquire;
using namespace std;

//-----------------------------------------------------------------------------
#if wxCHECK_VERSION(2, 9, 1)
int wxCALLBACK ListCompareFunction( wxIntPtr item1, wxIntPtr item2, wxIntPtr column )
#else
int wxCALLBACK ListCompareFunction( long item1, long item2, long column )
#endif // #if wxCHECK_VERSION(2, 9, 1)
//-----------------------------------------------------------------------------
{
    DeviceManager devMgr;
    switch( column )
    {
    case lcFamily:
        return devMgr[static_cast<unsigned int>( item1 )]->family.read().compare( devMgr[static_cast<unsigned int>( item2 )]->family.read() );
    case lcProduct:
        return devMgr[static_cast<unsigned int>( item1 )]->product.read().compare( devMgr[static_cast<unsigned int>( item2 )]->product.read() );
    case lcSerial:
        return devMgr[static_cast<unsigned int>( item1 )]->serial.read().compare( devMgr[static_cast<unsigned int>( item2 )]->serial.read() );
    case lcState:
        return devMgr[static_cast<unsigned int>( item1 )]->state.readS().compare( devMgr[static_cast<unsigned int>( item2 )]->state.readS() );
    case lcFWVersion:
        return devMgr[static_cast<unsigned int>( item1 )]->firmwareVersion.read() - devMgr[static_cast<unsigned int>( item2 )]->firmwareVersion.read();
    case lcDeviceID:
        return devMgr[static_cast<unsigned int>( item1 )]->deviceID.read() - devMgr[static_cast<unsigned int>( item2 )]->deviceID.read();
    default:
        return 0;
    }
}

//-----------------------------------------------------------------------------
DeviceListCtrl::DeviceListCtrl( wxWindow* parent, const wxWindowID id, const wxPoint& pos, const wxSize& size, long style, DeviceConfigureFrame* pParentFrame )
    : wxListCtrl( parent, id, pos, size, style ), m_pParentFrame( pParentFrame ), m_selectedItemID( -1 )  {}
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
void DeviceListCtrl::OnActivated( wxListEvent& )
//-----------------------------------------------------------------------------
{
    if( m_pParentFrame )
    {
        m_pParentFrame->ActivateDeviceIn_wxPropView( m_selectedItemID );
    }
}

//-----------------------------------------------------------------------------
void DeviceListCtrl::OnColClick( wxListEvent& e )
//-----------------------------------------------------------------------------
{
    SortItems( ListCompareFunction, e.GetColumn() );
}

//-----------------------------------------------------------------------------
void DeviceListCtrl::OnSelected( wxListEvent& e )
//-----------------------------------------------------------------------------
{
    m_selectedItemID = static_cast<int>( GetItemData( e.m_itemIndex ) );
    if( m_pParentFrame )
    {
        m_pParentFrame->UpdateMenu( m_selectedItemID );
    }
}

//-----------------------------------------------------------------------------
void DeviceListCtrl::OnDeselected( wxListEvent& )
//-----------------------------------------------------------------------------
{
    m_selectedItemID = -1;
    if( m_pParentFrame )
    {
        m_pParentFrame->UpdateMenu( m_selectedItemID );
    }
}

//-----------------------------------------------------------------------------
void DeviceListCtrl::OnItemRightClick( wxListEvent& e )
//-----------------------------------------------------------------------------
{
    wxMenu menu( wxT( "" ) );
    wxMenu* menuFWUpdate = new wxMenu( wxT( "" ) );
    wxMenuItem* pMISetID = menu.Append( LIST_SET_ID, wxT( "&Set ID" ) );
    menu.AppendSubMenu( menuFWUpdate, wxT( "Update Firmware" ) );
    wxMenuItem* pMIFWUpdateLatestLocal = menuFWUpdate->Append( LIST_UPDATE_FW_LATEST_LOCAL, wxT( "From Latest Local &Firmware Package" ) );
    wxMenuItem* pMIFWUpdate = menuFWUpdate->Append( LIST_UPDATE_FW, wxT( "From Specific Local &Firmware Package" ) );
    wxMenuItem* pMIFWUpdateFromInternet = menuFWUpdate->Append( LIST_UPDATE_FW_FROM_INTERNET, wxT( "From Online Firmware Package" ) );
    wxMenuItem* pMIDMABufferSizeUpdate = menu.Append( LIST_UPDATE_DMA_BUFFER_SIZE, wxT( "Update &Permanent DMA Buffer Size" ) );
#ifdef BUILD_WITH_DIRECT_SHOW_SUPPORT
    wxMenuItem* pMISetFriendlyName = menu.Append( LIST_SET_DS_FRIENDLY_NAME, wxT( "Set DirectShow Friendly Name" ) );
#endif // #ifdef BUILD_WITH_DIRECT_SHOW_SUPPORT
    bool boSetIDSupported = false;
    bool boFWUPdateSupport = false;
    bool boUpdateDMABufferSize = false;
    bool boFWUPdateOnline = false;
    bool boFWInMVUFile = false;
#ifdef BUILD_WITH_DIRECT_SHOW_SUPPORT
    bool boIsRegisteredForDirectShow = false;
#endif // #ifdef BUILD_WITH_DIRECT_SHOW_SUPPORT
    if( m_pParentFrame &&
        ( m_selectedItemID >= 0 ) &&
        ( static_cast<int>( m_pParentFrame->GetDeviceManager().deviceCount() ) > m_selectedItemID ) )
    {
        Device* pDev = m_pParentFrame->GetDeviceManager().getDevice( static_cast<unsigned int>( GetItemData( e.m_itemIndex ) ) );
        ObjectDeleter<DeviceHandler> pHandler( m_pParentFrame->GetHandlerFactory().CreateObject( ConvertedString( pDev->family.read() ), pDev ) );
        if( pHandler.get() )
        {
            boSetIDSupported = pHandler->SupportsSetID();
            boFWUPdateSupport = pHandler->SupportsFirmwareUpdate();
            boUpdateDMABufferSize = pHandler->SupportsDMABufferSizeUpdate();
            boFWUPdateOnline = pHandler->SupportsFirmwareUpdateFromInternet();
            boFWInMVUFile = pHandler->SupportsFirmwareUpdateFromMVU();
        }
#ifdef BUILD_WITH_DIRECT_SHOW_SUPPORT
        wxListItem info;
        info.m_itemId = e.m_itemIndex;
        info.m_col = lcDSRegistered;
        info.m_mask = wxLIST_MASK_TEXT;
        if( GetItem( info ) )
        {
            boIsRegisteredForDirectShow = ( info.m_text == wxT( "yes" ) );
        }
#endif // #ifdef BUILD_WITH_DIRECT_SHOW_SUPPORT
    }
    pMISetID->Enable( boSetIDSupported );
    pMIFWUpdateLatestLocal->Enable( boFWUPdateSupport );
    pMIFWUpdate->Enable( boFWInMVUFile );
    pMIFWUpdateFromInternet->Enable( boFWUPdateOnline );
    pMIDMABufferSizeUpdate->Enable( boUpdateDMABufferSize );
#ifdef BUILD_WITH_DIRECT_SHOW_SUPPORT
    pMISetFriendlyName->Enable( m_pParentFrame->ApplicationHasElevatedRights() && boIsRegisteredForDirectShow );
#endif // #ifdef BUILD_WITH_DIRECT_SHOW_SUPPORT
    PopupMenu( &menu, e.GetPoint() );
}

//-----------------------------------------------------------------------------
void DeviceListCtrl::OnSetID( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    if( m_pParentFrame )
    {
        m_pParentFrame->SetID( m_selectedItemID );
    }
}

//-----------------------------------------------------------------------------
void DeviceListCtrl::OnUpdateFirmware( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    if( m_pParentFrame )
    {
        m_pParentFrame->UpdateFirmware( m_selectedItemID );
    }
}

//-----------------------------------------------------------------------------
void DeviceListCtrl::OnUpdateFirmwareLocalLatest( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    if( m_pParentFrame )
    {
        m_pParentFrame->UpdateFirmwareLocalLatest( m_selectedItemID );
    }
}

//-----------------------------------------------------------------------------
void DeviceListCtrl::OnUpdateFirmwareFromInternet( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    if( m_pParentFrame )
    {
        m_pParentFrame->UpdateFirmwareFromInternet( m_selectedItemID );
    }
}

//-----------------------------------------------------------------------------
void DeviceListCtrl::OnUpdateDMABufferSize( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    if( m_pParentFrame )
    {
        m_pParentFrame->UpdateDMABufferSize( m_selectedItemID );
    }
}

#ifdef BUILD_WITH_DIRECT_SHOW_SUPPORT
//-----------------------------------------------------------------------------
void DeviceListCtrl::OnSetDSFriendlyName( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    if( m_pParentFrame )
    {
        m_pParentFrame->SetDSFriendlyName( m_selectedItemID );
    }
}
#endif // #ifdef BUILD_WITH_DIRECT_SHOW_SUPPORT

BEGIN_EVENT_TABLE( LogOutputListCtrl, wxListCtrl )
    EVT_LIST_COL_CLICK( LIST_CTRL, LogOutputListCtrl::OnColClick )
    EVT_LIST_COL_RIGHT_CLICK( LIST_CTRL, LogOutputListCtrl::OnColumnRightClick )
    EVT_LIST_ITEM_SELECTED( LIST_CTRL, LogOutputListCtrl::OnSelected )
    EVT_LIST_ITEM_RIGHT_CLICK( LIST_CTRL, LogOutputListCtrl::OnItemRightClick )
    EVT_LIST_ITEM_DESELECTED( LIST_CTRL, LogOutputListCtrl::OnDeselected )
    EVT_MENU( LIST_CONFIGURE, LogOutputListCtrl::OnModifyItem )
    EVT_MENU( LIST_ADD, LogOutputListCtrl::OnAddItem )
    EVT_MENU( LIST_DELETE, LogOutputListCtrl::OnDeleteItem )
    EVT_MENU( LIST_DELETE_ALL, LogOutputListCtrl::OnDeleteAll )
END_EVENT_TABLE()

//-----------------------------------------------------------------------------
LogOutputListCtrl::LogOutputListCtrl( wxWindow* parent, const wxWindowID id, const wxPoint& pos, const wxSize& size, long style, LogOutputHandlerDlg* pParent )
    : wxListCtrl( parent, id, pos, size, style ), m_pParent( pParent ), m_selectedItemID( -1 )  {}
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
void LogOutputListCtrl::OnColClick( wxListEvent& )
//-----------------------------------------------------------------------------
{

}

//-----------------------------------------------------------------------------
void LogOutputListCtrl::OnColumnRightClick( wxListEvent& e )
//-----------------------------------------------------------------------------
{
    ShowPopup( e );
}

//-----------------------------------------------------------------------------
void LogOutputListCtrl::OnSelected( wxListEvent& e )
//-----------------------------------------------------------------------------
{
    m_selectedItemID = e.m_itemIndex;
}

//-----------------------------------------------------------------------------
void LogOutputListCtrl::OnDeselected( wxListEvent& )
//-----------------------------------------------------------------------------
{
    m_selectedItemID = -1;
}

//-----------------------------------------------------------------------------
void LogOutputListCtrl::OnItemRightClick( wxListEvent& e )
//-----------------------------------------------------------------------------
{
    ShowPopup( e );
}

//-----------------------------------------------------------------------------
void LogOutputListCtrl::OnModifyItem( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    m_pParent->ConfigureLogger( m_selectedItemID );
}

//-----------------------------------------------------------------------------
void LogOutputListCtrl::OnAddItem( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    m_pParent->InsertItem( m_selectedItemID );
}

//-----------------------------------------------------------------------------
void LogOutputListCtrl::OnDeleteItem( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    m_pParent->DeleteItem( m_selectedItemID );
}

//-----------------------------------------------------------------------------
void LogOutputListCtrl::OnDeleteAll( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    m_selectedItemID = -1;
    m_pParent->DeleteList();
    DeleteAllItems();
}

//-----------------------------------------------------------------------------
void LogOutputListCtrl::ShowPopup( wxListEvent& e )
//-----------------------------------------------------------------------------
{
    wxMenu menu( wxT( "" ) );
    if( m_selectedItemID != -1 )
    {
        menu.Append( LIST_CONFIGURE, wxT( "&Configure Item" ) );
    }
    menu.Append( LIST_ADD, wxT( "&Add Item" ) );
    if( m_selectedItemID != -1 )
    {
        menu.Append( LIST_DELETE, wxT( "&Delete Item" ) );
    }
    menu.Append( LIST_DELETE_ALL, wxT( "C&lear List" ) );
    PopupMenu( &menu, e.GetPoint() );
}
