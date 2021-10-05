#include "CheckedListCtrl.h"

//=============================================================================
//=================== Implementation CheckedListCtrl ==========================
//=============================================================================
BEGIN_EVENT_TABLE( CheckedListCtrl, wxListCtrl )
    EVT_LEFT_DOWN( CheckedListCtrl::OnMouseEvent )
END_EVENT_TABLE()

//-----------------------------------------------------------------------------
void CheckedListCtrl::OnMouseEvent( wxMouseEvent& event )
//-----------------------------------------------------------------------------
{
    if( event.LeftDown() )
    {
        int flags;
        long item = HitTest( event.GetPosition(), flags );
        if( item > -1 && ( flags & wxLIST_HITTEST_ONITEMICON ) )
        {
            SetChecked( item, !IsChecked( item ) );
            return;
        }
    }
    event.Skip();
}

//-----------------------------------------------------------------------------
bool CheckedListCtrl::IsChecked( long item ) const
//-----------------------------------------------------------------------------
{
    wxListItem info;
    info.m_mask = wxLIST_MASK_IMAGE;
    info.m_itemId = item;

    if( GetItem( info ) )
    {
        return ( info.m_image == 1 );
    }
    return false;
}

//-----------------------------------------------------------------------------
void CheckedListCtrl::SetChecked( long item, bool checked )
//-----------------------------------------------------------------------------
{
    SetItemImage( item, ( checked ? 1 : 0 ), -1 );
}
