//-----------------------------------------------------------------------------
#ifndef CheckedListCtrlH
#define CheckedListCtrlH CheckedListCtrlH
//-----------------------------------------------------------------------------
#include "icons.h"

#include <apps/Common/wxIncludePrologue.h>
#include <wx/listctrl.h>
#include <wx/imaglist.h>
#include <apps/Common/wxIncludeEpilogue.h>

//-----------------------------------------------------------------------------
class CheckedListCtrl : public wxListCtrl
//-----------------------------------------------------------------------------
{
    DECLARE_EVENT_TABLE()
    wxImageList m_imageList;

public:
    explicit CheckedListCtrl( wxWindow* pParent, wxWindowID id, const wxPoint& pt, const wxSize& sz, long style ) : wxListCtrl( pParent, id, pt, sz, style ), m_imageList( 16, 16, true )
    {
        SetImageList( &m_imageList, wxIMAGE_LIST_SMALL );
        const wxImage checkboxUnchecked( wxBitmap::NewFromPNGData( checkbox_unchecked_png, sizeof( checkbox_unchecked_png ) ).ConvertToImage() );
        m_imageList.Add( checkboxUnchecked.Scale( 16, 16 ) );
        const wxImage checkboxChecked( wxBitmap::NewFromPNGData( checkbox_checked_png, sizeof( checkbox_checked_png ) ).ConvertToImage() );
        m_imageList.Add( checkboxChecked.Scale( 16, 16 ) );
    }

    void OnMouseEvent( wxMouseEvent& event );
    bool IsChecked( long item ) const;
    void SetChecked( long item, bool checked );
};

#endif // CheckedListCtrlH
