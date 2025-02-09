#include <algorithm>
#include <common/STLHelper.h>
#include "ImageCanvas.h"
#include "PlotCanvasImageAnalysis.h"
#include "PropViewFrame.h"
#include <limits>

#include <apps/Common/wxIncludePrologue.h>
#include <wx/arrstr.h>
#include <wx/clipbrd.h>
#include <wx/numdlg.h>
#include <apps/Common/wxIncludeEpilogue.h>

using namespace std;

//=============================================================================
//============== Implementation helper functions ==============================
//=============================================================================
//-----------------------------------------------------------------------------
inline double GetNextDoubleParam( const wxString& token )
//-----------------------------------------------------------------------------
{
    double val = 0;
    token.ToDouble( &val );
    return val;
}

//-----------------------------------------------------------------------------
inline long GetNextLongParam( const wxString& token )
//-----------------------------------------------------------------------------
{
    long val = 0;
    token.ToLong( &val );
    return val;
}

//-----------------------------------------------------------------------------
inline bool IsPointWithinRect( const wxPoint& p, const wxRect& r )
//-----------------------------------------------------------------------------
{
#if wxCHECK_VERSION(2, 8, 0)
    return r.Contains( p );
#else
    return r.Inside( p );
#endif // #if wxCHECK_VERSION(2, 8, 0)
}

//=============================================================================
//============== Implementation ImageCanvas ===================================
//=============================================================================
BEGIN_EVENT_TABLE( ImageCanvas, DrawingCanvas )
    EVT_PAINT( ImageCanvas::OnPaint )
    EVT_KEY_DOWN( ImageCanvas::OnKeyDown )
    EVT_LEFT_DCLICK( ImageCanvas::OnLeftDblClick )
    EVT_LEFT_DOWN( ImageCanvas::OnLeftDown )
    EVT_LEFT_UP( ImageCanvas::OnLeftUp )
    EVT_MOTION( ImageCanvas::OnMotion )
    EVT_MOUSEWHEEL( ImageCanvas::OnMouseWheel )
    EVT_RIGHT_DOWN( ImageCanvas::OnRightDown )
    EVT_RIGHT_UP( ImageCanvas::OnRightUp )
    EVT_MENU( miPopUpCopyCurrentImageToClipboard, ImageCanvas::OnPopUpCopyCurrentImageToClipboard )
    EVT_MENU( miPopUpFitToScreen, ImageCanvas::OnPopUpFitToScreen )
    EVT_MENU( miPopUpFullScreen, ImageCanvas::OnPopUpFullScreen )
    EVT_MENU( miPopUpOneToOneDisplay, ImageCanvas::OnPopUpOneToOneDisplay )
    EVT_MENU( miPopUpSaveCurrentImage, ImageCanvas::OnPopUpSaveCurrentImage )
    EVT_MENU( miPopUpScalerMode_Cubic, ImageCanvas::OnPopUp_ScalingMode_Changed )
    EVT_MENU( miPopUpScalerMode_Linear, ImageCanvas::OnPopUp_ScalingMode_Changed )
    EVT_MENU( miPopUpScalerMode_NearestNeighbour, ImageCanvas::OnPopUp_ScalingMode_Changed )
    EVT_MENU( miPopUpSelectRequestInfoOverlayColor, ImageCanvas::OnPopUpSelectRequestInfoOverlayColor )
    EVT_MENU( miPopUpSetShiftValue, ImageCanvas::OnPopUpSetShiftValue )
    EVT_MENU( miPopUpShowRequestInfoOverlay, ImageCanvas::OnPopUpShowRequestInfoOverlay )
    EVT_MENU( miPopUpShowPerformanceWarnings, ImageCanvas::OnPopUpShowPerformanceWarnings )
    EVT_MENU( miPopUpShowImageModificationsWarning, ImageCanvas::OnPopUpShowImageModificationsWarning )
END_EVENT_TABLE()

const double ImageCanvas::s_zoomFactor_Min = 0.125;

//-----------------------------------------------------------------------------
wxPoint ImageCanvas::AlignPointToAOIIncrementConstraints( const AOI* const pAOI, const wxPoint& unalignedPoint ) const
//-----------------------------------------------------------------------------
{
    wxPoint p( unalignedPoint );
    const int incX = pAOI->m_incX > pAOI->m_incW ? pAOI->m_incX : pAOI->m_incW;
    const int incY = pAOI->m_incY > pAOI->m_incH ? pAOI->m_incY : pAOI->m_incH;
    p.x = ( p.x / incX ) * incX;
    p.y = ( p.y / incY ) * incY;
    return p;
}

//-----------------------------------------------------------------------------
template<typename _Ty>
void ImageCanvas::AppendYUV411_UYYVYYDataPixelInfo( wxPoint pixel, wxString& pixelInfo, const ImageBuffer* pIB ) const
//-----------------------------------------------------------------------------
{
    const _Ty* p = reinterpret_cast<const _Ty*>( reinterpret_cast<const unsigned char*>( pIB->vpData ) + ( pixel.y * pIB->pChannels[0].iLinePitch ) + ( ( pixel.x / 4 ) * 6 ) );
    // 'p' points to the first 'U' of 4 pixels now
    int YPos = 1 + pixel.x % 4;
    // valid value are 0, 1, 3, 4. '2' contains the 'U' component
    if( ( pixel.x % 4 ) > 1 )
    {
        ++YPos; // jump over the 'U'
    }
    pixelInfo.append( wxString::Format( wxT( "%d(Y), %d(U), %d(V)" ), p[YPos], p[0], p[3] ) );
}

//-----------------------------------------------------------------------------
template<typename _Ty>
void ImageCanvas::AppendYUV422DataPixelInfo( wxPoint pixel, wxString& pixelInfo, const ImageBuffer* pIB ) const
//-----------------------------------------------------------------------------
{
    const _Ty* p = reinterpret_cast<const _Ty*>( reinterpret_cast<const unsigned char*>( pIB->vpData ) + ( pixel.y * pIB->pChannels[0].iLinePitch ) + ( pixel.x * pIB->iBytesPerPixel ) );
    if( ( pixel.x & 1 ) || ( pixel.x == pIB->iWidth - 1 ) )
    {
        pixelInfo.append( wxString::Format( wxT( "%d(Y), %d(U), %d(V)" ), p[0], p[-1], p[1] ) );
    }
    else
    {
        pixelInfo.append( wxString::Format( wxT( "%d(Y), %d(U), %d(V)" ), p[0], p[1], p[3] ) );
    }
}

//-----------------------------------------------------------------------------
template<typename _Ty>
void ImageCanvas::AppendYUV444DataPixelInfo( wxPoint pixel, wxString& pixelInfo, const ImageBuffer* pIB, const int order[3] ) const
//-----------------------------------------------------------------------------
{
    const _Ty* p = reinterpret_cast<const _Ty*>( reinterpret_cast<const unsigned char*>( pIB->vpData ) + ( pixel.y * pIB->pChannels[0].iLinePitch ) + ( pixel.x * pIB->iBytesPerPixel ) );
    pixelInfo.append( wxString::Format( wxT( "%d(Y), %d(U), %d(V)" ), p[order[0]], p[order[1]], p[order[2]] ) );
}

//-----------------------------------------------------------------------------
template<typename _Ty>
void ImageCanvas::AppendUYVDataPixelInfo( wxPoint pixel, wxString& pixelInfo, const ImageBuffer* pIB ) const
//-----------------------------------------------------------------------------
{
    const _Ty* p = reinterpret_cast<const _Ty*>( reinterpret_cast<const unsigned char*>( pIB->vpData ) + ( pixel.y * pIB->pChannels[0].iLinePitch ) + ( pixel.x * pIB->iBytesPerPixel ) );
    if( ( pixel.x & 1 ) || ( pixel.x == pIB->iWidth - 1 ) )
    {
        pixelInfo.append( wxString::Format( wxT( "%d(Y), %d(U), %d(V)" ), p[1], p[0], p[-2] ) );
    }
    else
    {
        pixelInfo.append( wxString::Format( wxT( "%d(Y), %d(U), %d(V)" ), p[1], p[0], p[2] ) );
    }
}

//-----------------------------------------------------------------------------
void ImageCanvas::BlitAOI( wxWindowDC& dc, double scaleFactor, int bmpXOff, int bmpYOff, int bmpW, int bmpH, const AOI* pAOI, bool boDrawOnTop ) const
//-----------------------------------------------------------------------------
{
    if( pAOI->m_boDrawOnTop == boDrawOnTop )
    {
        dc.SetPen( pAOI->m_pen );
        const int aoix = pAOI->m_rect.GetX();
        const int aoiy = pAOI->m_rect.GetY();
        const int aoiw = pAOI->m_rect.GetWidth();
        const int aoih = pAOI->m_rect.GetHeight();
        dc.DrawRectangle( static_cast<int>( ( aoix * scaleFactor ) + bmpXOff ),
                          static_cast<int>( ( aoiy * scaleFactor ) + bmpYOff ),
                          ( aoiw + aoix > bmpW ) ? static_cast<int>( ( bmpW - aoix ) * scaleFactor ) : static_cast<int>( aoiw * scaleFactor ),
                          ( aoih + aoiy > bmpH ) ? static_cast<int>( ( bmpH - aoiy ) * scaleFactor ) : static_cast<int>( aoih * scaleFactor ) );
        dc.SetTextForeground( pAOI->m_pen.GetColour() );
        const wxArrayString labelTokens = wxSplit( pAOI->m_description, wxT( '\n' ) );
        const wxArrayString::size_type labelTokenCount = labelTokens.size();
        wxCoord textWidth, textHeight;
        for( wxArrayString::size_type i = 0; i < labelTokenCount; i++ )
        {
            dc.GetTextExtent( labelTokens[i], &textWidth, &textHeight );
            dc.DrawText( labelTokens[i],
                         static_cast<wxCoord>( ( aoix * scaleFactor ) + bmpXOff + ( ( static_cast<int>( aoiw * scaleFactor ) - textWidth ) / 2 ) ),
                         static_cast<wxCoord>( ( ( aoiy * scaleFactor ) + bmpYOff + ( ( static_cast<int>( aoih * scaleFactor ) - textHeight ) / 2 ) ) ) - ( ( static_cast<wxCoord>( labelTokenCount ) * textHeight ) / 2 ) + ( static_cast<wxCoord>( i ) * textHeight ) );
        }
        dc.SetPen( wxNullPen );
    }
}

//-----------------------------------------------------------------------------
void ImageCanvas::BlitSizingPoints( wxWindowDC& dc, double scaleFactor, int bmpXOff, int bmpYOff ) const
//-----------------------------------------------------------------------------
{
    SizingPointContainer::const_iterator it = m_sizingPoints.begin();
    const SizingPointContainer::const_iterator itEND = m_sizingPoints.end();
    while( it != itEND )
    {
        dc.SetPen( it->second->m_pen );
        const int aoix = it->second->m_rect.GetX();
        const int aoiy = it->second->m_rect.GetY();
        dc.SetBrush( wxBrush( it->second->m_pen.GetColour() ) );
        wxRect sizingPoint( static_cast<int>( ( aoix * scaleFactor ) + bmpXOff ),
                            static_cast<int>( ( aoiy * scaleFactor ) + bmpYOff ),
                            it->second->m_rect.GetWidth(),
                            it->second->m_rect.GetHeight() );
        sizingPoint.Inflate( static_cast<wxCoord>( AOI_SIZING_ZONE_WIDTH ), static_cast<wxCoord>( AOI_SIZING_ZONE_WIDTH ) );
        dc.DrawRectangle( sizingPoint );
        dc.SetBrush( wxNullBrush );
        dc.SetPen( wxNullPen );
        ++it;
    }
}

//-----------------------------------------------------------------------------
void ImageCanvas::BlitAOIs( wxWindowDC& dc, double scaleFactor, int bmpXOff, int bmpYOff, int bmpW, int bmpH ) const
//-----------------------------------------------------------------------------
{
    dc.SetBrush( *wxTRANSPARENT_BRUSH );
    BlitAOIs( dc, scaleFactor, bmpXOff, bmpYOff, bmpW, bmpH, false );
    BlitAOIs( dc, scaleFactor, bmpXOff, bmpYOff, bmpW, bmpH, true );
    if( m_boMouseIsWithinAnAOIsSizingZone )
    {
        BlitSizingPoints( dc, scaleFactor, bmpXOff, bmpYOff );
    }
}

//-----------------------------------------------------------------------------
void ImageCanvas::BlitAOIs( wxWindowDC& dc, double scaleFactor, int bmpXOff, int bmpYOff, int bmpW, int bmpH, bool boDrawOnTop ) const
//-----------------------------------------------------------------------------
{
    AOIContainer::const_iterator it = m_aoiContainer.begin();
    const AOIContainer::const_iterator itEND = m_aoiContainer.end();
    while( it != itEND )
    {
        BlitAOI( dc, scaleFactor, bmpXOff, bmpYOff, bmpW, bmpH, *it, boDrawOnTop );
        ++it;
    }
}

//-----------------------------------------------------------------------------
/// Strings containing \a _PVDI_ (wxPropView Display Info) are treated differently.
///
/// These strings can be used to blit special information on top of the image.
/// Supported things are strings rectangles and ellipse overlays.
///
/// The string must have the following format:
///
/// token;token;token;token
///
/// one of these tokens must be \a _PVDI_ to inform the display that this string has to be treated
/// in a special way. Other valid tokens are:
///
/// - _RECT_=top,left,bottom,right,r,g,b
/// - _ELLI_=x,y,w,h,r,g,b
/// - _STR_=text,x,y,r,g,b
/// - _LN_=x1,y1,x2,y2,r,g,b (line)
///
/// _PVDI_;_STR_="text",x,y,r,g,b;_RECT_=top,left,bottom,right,r,g,b;_ELLI_=x,y,w,h,r,g,b
///
/// EXAMPLES:
///
/// _PVDI_;_STR_=dummy,100,100,0,255,0;_RECT_=50,50,200,200,255,0,0
///
/// This will draw 'dummy' at 100/100 in green on top of the image and a red rectangle from 50/50 to 200/200
///
/// _PVDI_;_STR_=dummy,100,100;_ELLI_=50,50,200,200,255,0,0
///
/// This will draw a green 'dummy' at 100/100(no colour info -> default colour) and a red circle (as w and h are equal)
/// at 50/50 with a radius of 200. Colour information can be omitted. Then 0/255/0 (green) will be used.
void ImageCanvas::BlitInfoStrings( wxWindowDC& dc, double scaleFactor, int bmpScaledViewXOff, int bmpScaledViewVOff, int bmpXOff, int bmpYOff, int bmpW, int bmpH )
//-----------------------------------------------------------------------------
{
    if( m_boShowInfoOverlay )
    {
        wxColour oldTextColour( dc.GetTextForeground() );
        const vector<wxString>::size_type vSize = m_infoStringsOverlay.size();
        for( vector<wxString>::size_type i = 0; i < vSize; i++ )
        {
            dc.SetTextForeground( m_InfoOverlayColor );
            dc.SetPen( *wxGREEN );
            static const wxString SPECIAL_DISPLAY_INFO_TOKEN( wxT( "_PVDI_" ) );
            int displayInfoPos = m_infoStringsOverlay[i].Find( SPECIAL_DISPLAY_INFO_TOKEN.c_str() );
            if( displayInfoPos >= 0 )
            {
                int pos = m_infoStringsOverlay[i].Find( wxT( ":" ) );
                if( pos >= 0 )
                {
                    wxString tokenList( m_infoStringsOverlay[i].Mid( pos + 1 ) );
                    tokenList.Trim().Trim( false );
                    wxArrayString tokens = wxSplit( tokenList, wxT( ';' ) );
                    const wxArrayString::size_type tokenCnt = tokens.size();
                    for( wxArrayString::size_type j = 0; j < tokenCnt; j++ )
                    {
                        wxArrayString params = wxSplit( tokens[j], wxT( '=' ) );
                        wxArrayString::size_type paramsCnt = params.size();
                        if( ( ( paramsCnt == 1 ) && ( tokens[j] != SPECIAL_DISPLAY_INFO_TOKEN ) ) /*|| ( ( paramsCnt < 1 ) || ( paramsCnt > 2 ) )*/ )
                        {
                            dc.DrawText( wxString::Format( wxT( "Invalid parameter string: %s" ), m_infoStringsOverlay[i].c_str() ), static_cast<wxCoord>( bmpXOff ), static_cast<wxCoord>( bmpYOff + 300 + ( i * 20 ) ) );
                            break;
                        }

                        if( params[0] == SPECIAL_DISPLAY_INFO_TOKEN )
                        {
                            continue;
                        }

                        dc.SetPen( *wxGREEN ); // the default pen
                        dc.SetTextForeground( *wxGREEN ); // the default text colour
                        if( params[0] == wxT( "_STR_" ) )
                        {
                            wxArrayString data;
                            wxString message( params[1].BeforeLast( wxT( '\"' ) ) );
                            if( message.empty() )
                            {
                                // old format: string message NOT encapsulated with "
                                data = wxSplit( params[1], wxT( ',' ) );
                            }
                            else
                            {
                                // new format: _STR_="<message>",x,y,r,g,b
                                wxString posAndColor( params[1].Mid( message.Len() + 2 ) );
                                data = wxSplit( posAndColor, wxT( ',' ) );
                                message.Remove( 0, 1 );
                                data.insert( data.begin(), message );
                            }

                            const wxArrayString::size_type dataCnt = data.size();
                            if( params[1].StartsWith( wxT( "," ) ) )
                            {
                                // this is an empty string we don't need to draw
                                continue;
                            }
                            if( dataCnt < 3 )
                            {
                                dc.SetTextForeground( *wxRED );
                                dc.DrawText( wxString::Format( wxT( "Invalid token: %s" ), tokens[j].c_str() ), static_cast<wxCoord>( bmpXOff ), static_cast<wxCoord>( bmpYOff + 320 + ( i * 20 ) ) );
                                continue;
                            }
                            if( dataCnt >= 6 )
                            {
                                dc.SetTextForeground( wxColour(
                                                          wxColourBase::ChannelType( GetNextLongParam( data[3] ) ),
                                                          wxColourBase::ChannelType( GetNextLongParam( data[4] ) ),
                                                          wxColourBase::ChannelType( GetNextLongParam( data[5] ) ) ) );
                            }
                            long x = GetNextLongParam( data[1] );
                            long y = GetNextLongParam( data[2] );
                            dc.DrawText( data[0], static_cast<wxCoord>( ( x * scaleFactor ) + bmpScaledViewXOff ), static_cast<wxCoord>( ( y * scaleFactor ) + bmpScaledViewVOff ) );
                        }
                        else
                        {
                            wxArrayString data = wxSplit( params[1], wxT( ',' ) );
                            const wxArrayString::size_type dataCnt = data.size();
                            if( params[0] == wxT( "_RECT_" ) )
                            {
                                if( dataCnt < 4 )
                                {
                                    dc.SetTextForeground( *wxRED );
                                    dc.DrawText( wxString::Format( wxT( "Invalid token: %s" ), tokens[j].c_str() ), static_cast<wxCoord>( bmpXOff ), static_cast<wxCoord>( bmpYOff + 320 + ( i * 20 ) ) );
                                    continue;
                                }
                                if( dataCnt >= 7 )
                                {
                                    dc.SetPen( wxColour(
                                                   wxColourBase::ChannelType( GetNextLongParam( data[4] ) ),
                                                   wxColourBase::ChannelType( GetNextLongParam( data[5] ) ),
                                                   wxColourBase::ChannelType( GetNextLongParam( data[6] ) ) ) );
                                }
                                long y = GetNextLongParam( data[0] );
                                long x = GetNextLongParam( data[1] );
                                long bottom = GetNextLongParam( data[2] );
                                long right = GetNextLongParam( data[3] );
                                dc.DrawRectangle( static_cast<int>( ( x * scaleFactor ) + bmpScaledViewXOff ),
                                                  static_cast<int>( ( y * scaleFactor ) + bmpScaledViewVOff ),
                                                  ( right > bmpW ) ? static_cast<int>( ( bmpW - x ) * scaleFactor ) : static_cast<int>( ( right - x ) * scaleFactor ),
                                                  ( bottom > bmpH ) ? static_cast<int>( ( bmpH - y ) * scaleFactor ) : static_cast<int>( ( bottom - y ) * scaleFactor ) );
                            }
                            else if( params[0] == wxT( "_ELLI_" ) )
                            {
                                if( dataCnt < 4 )
                                {
                                    dc.SetTextForeground( *wxRED );
                                    dc.DrawText( wxString::Format( wxT( "Invalid token: %s" ), tokens[j].c_str() ), static_cast<wxCoord>( bmpXOff ), static_cast<wxCoord>( bmpYOff + 320 + ( i * 20 ) ) );
                                    continue;
                                }
                                if( dataCnt >= 7 )
                                {
                                    dc.SetPen( wxColour(
                                                   wxColourBase::ChannelType( GetNextLongParam( data[4] ) ),
                                                   wxColourBase::ChannelType( GetNextLongParam( data[5] ) ),
                                                   wxColourBase::ChannelType( GetNextLongParam( data[6] ) ) ) );
                                }
                                long x = GetNextLongParam( data[0] );
                                long y = GetNextLongParam( data[1] );
                                long w = GetNextLongParam( data[2] );
                                long h = GetNextLongParam( data[3] );
                                dc.DrawEllipse( static_cast<int>( ( x * scaleFactor ) + bmpScaledViewXOff ),
                                                static_cast<int>( ( y * scaleFactor ) + bmpScaledViewVOff ),
                                                ( w + x > bmpW ) ? static_cast<int>( ( bmpW - x ) * scaleFactor ) : static_cast<int>( w * scaleFactor ),
                                                ( h + y > bmpH ) ? static_cast<int>( ( bmpH - y ) * scaleFactor ) : static_cast<int>( h * scaleFactor ) );
                            }
                            else if( params[0] == wxT( "_LN_" ) )
                            {
                                if( dataCnt < 4 )
                                {
                                    dc.SetTextForeground( *wxRED );
                                    dc.DrawText( wxString::Format( wxT( "Invalid token: %s" ), tokens[j].c_str() ), static_cast<wxCoord>( bmpXOff ), static_cast<wxCoord>( bmpYOff + 320 + ( i * 20 ) ) );
                                    continue;
                                }
                                if( dataCnt >= 7 )
                                {
                                    dc.SetPen( wxColour(
                                                   wxColourBase::ChannelType( GetNextLongParam( data[4] ) ),
                                                   wxColourBase::ChannelType( GetNextLongParam( data[5] ) ),
                                                   wxColourBase::ChannelType( GetNextLongParam( data[6] ) ) ) );
                                }
                                long x1 = GetNextLongParam( data[0] );
                                long y1 = GetNextLongParam( data[1] );
                                long x2 = GetNextLongParam( data[2] );
                                long y2 = GetNextLongParam( data[3] );
                                dc.DrawLine( static_cast<int>( ( x1 * scaleFactor ) + bmpScaledViewXOff ),
                                             static_cast<int>( ( y1 * scaleFactor ) + bmpScaledViewVOff ),
                                             static_cast<int>( ( x2 * scaleFactor ) + bmpScaledViewXOff ),
                                             static_cast<int>( ( y2 * scaleFactor ) + bmpScaledViewVOff ) );
                            }
                            else if( params[0] == wxT( "_PT_" ) )
                            {
                                if( dataCnt < 2 )
                                {
                                    dc.SetTextForeground( *wxRED );
                                    dc.DrawText( wxString::Format( wxT( "Invalid token: %s" ), tokens[j].c_str() ), static_cast<wxCoord>( bmpXOff ), static_cast<wxCoord>( bmpYOff + 320 + ( i * 20 ) ) );
                                    continue;
                                }
                                if( dataCnt >= 5 )
                                {
                                    dc.SetPen( wxColour(
                                                   wxColourBase::ChannelType( GetNextLongParam( data[2] ) ),
                                                   wxColourBase::ChannelType( GetNextLongParam( data[3] ) ),
                                                   wxColourBase::ChannelType( GetNextLongParam( data[4] ) ) ) );
                                }
                                long x = GetNextLongParam( data[0] );
                                long y = GetNextLongParam( data[1] );
                                dc.DrawPoint( static_cast<int>( ( x * scaleFactor ) + bmpScaledViewXOff ),
                                              static_cast<int>( ( y * scaleFactor ) + bmpScaledViewVOff ) );
                            }
                            else if( params[0] == wxT( "_DPT_" ) )
                            {
                                if( dataCnt < 2 )
                                {
                                    dc.SetTextForeground( *wxRED );
                                    dc.DrawText( wxString::Format( wxT( "Invalid token: %s" ), tokens[j].c_str() ), static_cast<wxCoord>( bmpXOff ), static_cast<wxCoord>( bmpYOff + 320 + ( i * 20 ) ) );
                                    continue;
                                }
                                if( dataCnt >= 5 )
                                {
                                    dc.SetPen( wxColour(
                                                   wxColourBase::ChannelType( GetNextLongParam( data[2] ) ),
                                                   wxColourBase::ChannelType( GetNextLongParam( data[3] ) ),
                                                   wxColourBase::ChannelType( GetNextLongParam( data[4] ) ) ) );
                                }
                                double x = GetNextDoubleParam( data[0] );
                                double y = GetNextDoubleParam( data[1] );
                                dc.DrawPoint( static_cast<int>( ( x * scaleFactor ) + bmpScaledViewXOff ),
                                              static_cast<int>( ( y * scaleFactor ) + bmpScaledViewVOff ) );
                            }
                            else if( params[0] == wxT( "_3DPT_" ) )
                            {
                                if( dataCnt < 2 )
                                {
                                    dc.SetTextForeground( *wxRED );
                                    dc.DrawText( wxString::Format( wxT( "Invalid token: %s" ), tokens[j].c_str() ), static_cast<wxCoord>( bmpXOff ), static_cast<wxCoord>( bmpYOff + 320 + ( i * 20 ) ) );
                                    continue;
                                }
                                if( dataCnt >= 6 )
                                {
                                    dc.SetPen( wxColour(
                                                   wxColourBase::ChannelType( GetNextLongParam( data[3] ) ),
                                                   wxColourBase::ChannelType( GetNextLongParam( data[4] ) ),
                                                   wxColourBase::ChannelType( GetNextLongParam( data[5] ) ) ) );
                                }
                                double x = GetNextDoubleParam( data[0] );
                                double y = GetNextDoubleParam( data[1] );
                                dc.DrawPoint( static_cast<int>( ( x * scaleFactor ) + bmpScaledViewXOff ),
                                              static_cast<int>( ( y * scaleFactor ) + bmpScaledViewVOff ) );
                            }
                            else
                            {
                                dc.SetTextForeground( *wxRED );
                                dc.DrawText( wxString::Format( wxT( "Invalid token: %s" ), tokens[j].c_str() ), static_cast<wxCoord>( bmpXOff ), static_cast<wxCoord>( bmpYOff + 320 + ( i * 20 ) ) );
                            }
                        }
                    }
                }
                else
                {
                    dc.DrawText( m_infoStringsOverlay[i], static_cast<wxCoord>( bmpXOff + TEXT_X_OFFSET ), static_cast<wxCoord>( bmpYOff + INFO_Y_OFFSET + ( i * 20 ) ) );
                }
            }
            else
            {
                dc.DrawText( m_infoStringsOverlay[i], static_cast<wxCoord>( bmpXOff + TEXT_X_OFFSET ), static_cast<wxCoord>( bmpYOff + INFO_Y_OFFSET + ( i * 20 ) ) );
            }
        }
        dc.SetPen( wxNullPen );
        dc.SetTextForeground( oldTextColour );
    }
}

//-----------------------------------------------------------------------------
void ImageCanvas::BlitPerformanceMessages( wxWindowDC& dc, int bmpXOff, int bmpYOff, TImageBufferPixelFormat pixelFormat )
//-----------------------------------------------------------------------------
{
    if( m_boShowPerformanceWarnings )
    {
        wxString performanceMsg;
        const bool boFormatWarning = ( pixelFormat != ibpfMono8 ) && ( pixelFormat != ibpfRGBx888Packed );
        if( boFormatWarning )
        {
            performanceMsg.Append( wxT( " pixel format conversion for display" ) );
        }

        if( m_boScaleToClientSize )
        {
            if( !performanceMsg.IsEmpty() )
            {
                performanceMsg.Append( wxT( " and" ) );
            }
            performanceMsg.Append( wxT( " scaling to fit to client size" ) );
        }

        if( !m_boScaleToClientSize && ( m_currentZoomFactor != 1.0 ) )
        {
            if( !performanceMsg.IsEmpty() )
            {
                performanceMsg.Append( wxT( " and" ) );
            }
            performanceMsg.Append( wxString::Format( wxT( " scaling (approx. zoom factor: %.3f)" ), m_currentZoomFactor ) );
        }

        if( !performanceMsg.IsEmpty() )
        {
            dc.SetTextForeground( *wxRED );
            dc.DrawText( wxString::Format( wxT( "Performance loss because of%s" ), performanceMsg.c_str() ), bmpXOff + TEXT_X_OFFSET, bmpYOff + PERFORMANCE_WARNINGS_Y_OFFSET );
        }

        wxString skippedMsg;
        if( m_skippedImages > 0 )
        {
            skippedMsg.Append( wxString::Format( wxT( "%8lu image%s skipped because internal processing was too slow" ), static_cast<long unsigned int>( m_skippedImages ), ( m_skippedImages == 1 ) ? wxT( "s" ) : wxT( "" ) ) );
        }
        if( m_skippedPaintEvents > 0 )
        {
            skippedMsg.Append( wxString::Format( wxT( "%s%8lu paint event%s skipped because the display engine was still busy" ),
                                                 skippedMsg.IsEmpty() ? wxT( "" ) : wxT( " and " ),
                                                 static_cast<long unsigned int>( m_skippedPaintEvents ),
                                                 ( m_skippedPaintEvents == 1 ) ? wxT( "s" ) : wxT( "" ) ) );
        }
        if( !skippedMsg.IsEmpty() )
        {
            dc.SetTextForeground( *wxRED );
            dc.DrawText( skippedMsg, bmpXOff + TEXT_X_OFFSET, bmpYOff + SKIPPED_IMAGE_MESSAGE_Y_OFFSET );
        }
    }

    const int appliedShift = GetAppliedShiftValue();
    const int shiftValue = GetShiftValue();
    const int bitsPerChannel = GetChannelBitDepth( pixelFormat );
    const int neededShiftForProperDisplay = bitsPerChannel - 8;
    if( m_boShowImageModificationWarning &&
        ( neededShiftForProperDisplay != 0 ) &&
        ( neededShiftForProperDisplay != appliedShift ) )
    {
        wxString maskedMSBs;
        for( int i = 0; i < appliedShift; i++ )
        {
            maskedMSBs.Append( wxT( "x" ) );
        }
        wxString maskedLSBs;
        for( int i = 0; i < bitsPerChannel - 8 - appliedShift; i++ )
        {
            maskedLSBs.Append( wxT( "x" ) );
        }
        dc.SetTextForeground( *wxRED );
        dc.DrawText( wxString::Format( wxT( "Displayed bits: %sDDDDDDDD%s (shift value(>>): Applied: %d, selected: %d)" ), maskedMSBs.c_str(), maskedLSBs.c_str(), neededShiftForProperDisplay - appliedShift, shiftValue ),
                     bmpXOff + TEXT_X_OFFSET, bmpYOff + IMAGE_MODIFICATIONS_Y_OFFSET );
    }

    if( m_bufferPartIndexDisplayed > 0 )
    {
        dc.SetTextForeground( *wxBLUE );
        dc.DrawText( wxString::Format( wxT( "Buffer part displayed: %u" ), m_bufferPartIndexDisplayed ),
                     bmpXOff + TEXT_X_OFFSET, bmpYOff + BUFFER_PART_SELECTION_Y_OFFSET );
    }
}

//-----------------------------------------------------------------------------
void ImageCanvas::ClipAOI( wxRect& rect, bool boForDragging ) const
//-----------------------------------------------------------------------------
{
    if( rect.GetX() < 0 )
    {
        rect.SetX( 0 );
    }
    if( rect.GetY() < 0 )
    {
        rect.SetY( 0 );
    }
    if( m_pIB && m_pIB->vpData )
    {
        if( boForDragging )
        {
            if( ( rect.GetX() + rect.GetWidth() ) > m_pIB->iWidth )
            {
                rect.SetX( m_pIB->iWidth - rect.GetWidth() );
            }
            if( ( rect.GetY() + rect.GetHeight() ) > m_pIB->iHeight )
            {
                rect.SetY( m_pIB->iHeight - rect.GetHeight() );
            }
        }
        if( ( rect.GetWidth() + rect.GetX() ) > m_pIB->iWidth )
        {
            rect.SetWidth( m_pIB->iWidth - rect.GetX() );
        }
        if( ( rect.GetHeight() + rect.GetY() ) > m_pIB->iHeight )
        {
            rect.SetHeight( m_pIB->iHeight - rect.GetY() );
        }
    }
    else
    {
        rect.SetWidth( 1 );
        rect.SetHeight( 1 );
    }
}

//-----------------------------------------------------------------------------
void ImageCanvas::CopyCurrentImageToClipboard( void )
//-----------------------------------------------------------------------------
{
    wxClipboardLocker clipboardLocker;
    if( !clipboardLocker )
    {
        return;
    }

    wxImage img;
    const TSaveResult result = GetCurrentImage( img );
    if( result == srOK )
    {
        wxBitmap bmp( img );
        wxTheClipboard->SetData( new wxBitmapDataObject( bmp.GetSubBitmap( wxRect( 0, 0, bmp.GetWidth(), bmp.GetHeight() ) ) ) );
    }
}

//-----------------------------------------------------------------------------
void ImageCanvas::DisableDoubleClickAndPrunePopupMenu( bool boDisable )
//-----------------------------------------------------------------------------
{
    wxCriticalSectionLocker locker( m_critSect );
    m_boDoubleClickDisabledAndPopupMenuPruned = boDisable;
}

//-----------------------------------------------------------------------------
void ImageCanvas::DragImageDisplay( void )
//-----------------------------------------------------------------------------
{
    wxPoint moved = m_lastMousePos - m_lastLeftMouseDownPos;
    int scrollUnitsX, scrollUnitsY;
    GetVirtualSize( &scrollUnitsX, &scrollUnitsY );
    int clientWidth, clientHeight;
    GetClientSize( &clientWidth, &clientHeight );
    Scroll( saveAssign( m_lastViewStart.x - moved.x, 0, scrollUnitsX - clientWidth ),
            saveAssign( m_lastViewStart.y - moved.y, 0, scrollUnitsY - clientHeight ) );
    GetViewStart( &m_lastViewStart.x, &m_lastViewStart.y );
}

//-----------------------------------------------------------------------------
int ImageCanvas::GetChannelBitDepth( TImageBufferPixelFormat format )
//-----------------------------------------------------------------------------
{
    switch( format )
    {
    case ibpfMono8:
    case ibpfBGR888Packed:
    case ibpfRGB888Packed:
    case ibpfRGBx888Packed:
    case ibpfRGB888Planar:
    case ibpfRGBx888Planar:
    case ibpfYUV411_UYYVYY_Packed:
    case ibpfYUV422Packed:
    case ibpfYUV422_UYVYPacked:
    case ibpfYUV444Packed:
    case ibpfYUV444_UYVPacked:
    case ibpfYUV422Planar:
    case ibpfYUV444Planar:
        return 8;
    case ibpfMono10:
    case ibpfBGR101010Packed_V2:
    case ibpfRGB101010Packed:
    case ibpfYUV422_10Packed:
    case ibpfYUV422_UYVY_10Packed:
    case ibpfYUV444_UYV_10Packed:
    case ibpfYUV444_10Packed:
        return 10;
    case ibpfMono12:
    case ibpfMono12Packed_V1:
    case ibpfMono12Packed_V2:
    case ibpfRGB121212Packed:
        return 12;
    case ibpfMono14:
    case ibpfRGB141414Packed:
        return 14;
    case ibpfMono16:
    case ibpfRGB161616Packed:
        return 16;
    case ibpfMono32:
        return 32;
    case ibpfAuto:
    case ibpfRaw:
        break;
        // do NOT add a default here! Whenever the compiler complains it is
        // missing not every format is handled here, which means that at least
        // one has been forgotten and that should be fixed!
    }
    assert( !"Unhandled pixel format detected!" );
    return 8;
}

//-----------------------------------------------------------------------------
wxString ImageCanvas::GetCurrentPixelDataAsString( void ) const
//-----------------------------------------------------------------------------
{
    wxCriticalSectionLocker locker( m_critSect );

    if( !m_pIB || !m_pIB->vpData || ( m_lastScaleFactor <= 0. ) )
    {
        return wxString( wxT( "-" ) );
    }

    wxPoint pixel( m_lastMousePos );
    if( ( m_lastMousePos.x < 0 ) || ( m_lastMousePos.x >= m_pIB->iWidth ) || ( m_pIB->iWidth < 1 ) )
    {
        pixel.x = -1;
    }

    if( ( m_lastMousePos.y < 0 ) || ( m_lastMousePos.y >= m_pIB->iHeight ) || ( m_pIB->iHeight < 1 ) )
    {
        pixel.y = -1;
    }

    wxString pixelInfo( wxString::Format( wxT( "(%d, %d): " ), pixel.x, pixel.y ) );
    if( ( pixel.x < 0 ) || ( pixel.y < 0 ) )
    {
        pixelInfo.append( wxT( "-" ) );
        return pixelInfo;
    }

    switch( m_pIB->pixelFormat )
    {
    case ibpfMono10:
    case ibpfMono12:
    case ibpfMono14:
    case ibpfMono16:
        {
            const unsigned short* p = reinterpret_cast<const unsigned short*>( reinterpret_cast<const unsigned char*>( m_pIB->vpData ) + ( pixel.y * m_pIB->pChannels[0].iLinePitch ) ) + pixel.x;
            pixelInfo.append( wxString::Format( wxT( "%d" ), *p ) );
        }
        break;
    case ibpfMono12Packed_V1:
        pixelInfo.append( wxString::Format( wxT( "%d" ), PlotCanvasImageAnalysis::GetMono12Packed_V1Pixel( static_cast<const unsigned char*>( m_pIB->vpData ), ( pixel.y * m_pIB->iWidth ) + pixel.x ) ) );
        break;
    case ibpfMono12Packed_V2:
        pixelInfo.append( wxString::Format( wxT( "%d" ), PlotCanvasImageAnalysis::GetMono12Packed_V2Pixel( static_cast<const unsigned char*>( m_pIB->vpData ), ( pixel.y * m_pIB->iWidth ) + pixel.x ) ) );
        break;
    case ibpfRGB101010Packed:
    case ibpfRGB121212Packed:
    case ibpfRGB141414Packed:
    case ibpfRGB161616Packed:
        {
            const unsigned short* p = reinterpret_cast<const unsigned short*>( reinterpret_cast<const unsigned char*>( m_pIB->vpData ) + ( pixel.y * m_pIB->pChannels[0].iLinePitch ) + ( pixel.x * m_pIB->iBytesPerPixel ) );
            pixelInfo.append( wxString::Format( wxT( "%d(R), %d(G), %d(B)" ), p[2], p[1], p[0] ) );
        }
        break;
    case ibpfBGR888Packed:
        {
            const unsigned char* p = reinterpret_cast<const unsigned char*>( m_pIB->vpData ) + ( pixel.y * m_pIB->pChannels[0].iLinePitch ) + ( pixel.x * m_pIB->iBytesPerPixel );
            pixelInfo.append( wxString::Format( wxT( "%d(R), %d(G), %d(B)" ), p[0], p[1], p[2] ) );
        }
        break;
    case ibpfBGR101010Packed_V2:
        {
            unsigned short red, green, blue;
            const unsigned int* p = reinterpret_cast<const unsigned int*>( reinterpret_cast<const unsigned char*>( m_pIB->vpData ) + ( pixel.y * m_pIB->pChannels[0].iLinePitch ) ) + pixel.x;
            PlotCanvasImageAnalysis::GetBGR101010Packed_V2Pixel( *p, red, green, blue );
            pixelInfo.append( wxString::Format( wxT( "%d(R), %d(G), %d(B)" ), red, green, blue ) );
        }
        break;
    case ibpfRGBx888Packed:
    case ibpfRGB888Packed:
        {
            const unsigned char* p = reinterpret_cast<const unsigned char*>( m_pIB->vpData ) + ( pixel.y * m_pIB->pChannels[0].iLinePitch ) + ( pixel.x * m_pIB->iBytesPerPixel );
            pixelInfo.append( wxString::Format( wxT( "%d(R), %d(G), %d(B)" ), p[2], p[1], p[0] ) );
        }
        break;
    case ibpfRGB888Planar:
    case ibpfRGBx888Planar:
        {
            const unsigned char* r = reinterpret_cast<unsigned char*>( m_pIB->vpData ) + m_pIB->pChannels[0].iChannelOffset + ( pixel.y * m_pIB->pChannels[0].iLinePitch ) + pixel.x;
            const unsigned char* g = reinterpret_cast<unsigned char*>( m_pIB->vpData ) + m_pIB->pChannels[1].iChannelOffset + ( pixel.y * m_pIB->pChannels[1].iLinePitch ) + pixel.x;
            const unsigned char* b = reinterpret_cast<unsigned char*>( m_pIB->vpData ) + m_pIB->pChannels[2].iChannelOffset + ( pixel.y * m_pIB->pChannels[2].iLinePitch ) + pixel.x;
            pixelInfo.append( wxString::Format( wxT( "%d(R), %d(G), %d(B)" ), *r, *g, *b ) );
        }
        break;
    case ibpfMono8:
        {
            const unsigned char* p = reinterpret_cast<const unsigned char*>( m_pIB->vpData ) + ( pixel.y * m_pIB->pChannels[0].iLinePitch ) + pixel.x;
            pixelInfo.append( wxString::Format( wxT( "%d" ), *p ) );
        }
        break;
    case ibpfYUV411_UYYVYY_Packed:
        AppendYUV411_UYYVYYDataPixelInfo<unsigned char>( pixel, pixelInfo, m_pIB );
        break;
    case ibpfYUV422Packed:
        AppendYUV422DataPixelInfo<unsigned char>( pixel, pixelInfo, m_pIB );
        break;
    case ibpfYUV422_10Packed:
        AppendYUV422DataPixelInfo<unsigned short>( pixel, pixelInfo, m_pIB );
        break;
    case ibpfYUV422_UYVYPacked:
        AppendUYVDataPixelInfo<unsigned char>( pixel, pixelInfo, m_pIB );
        break;
    case ibpfYUV422_UYVY_10Packed:
        AppendUYVDataPixelInfo<unsigned short>( pixel, pixelInfo, m_pIB );
        break;
    case ibpfYUV444_UYVPacked:
        {
            const int order[3] = { 1, 0, 2 };
            AppendYUV444DataPixelInfo<unsigned char>( pixel, pixelInfo, m_pIB, order );
        }
        break;
    case ibpfYUV444_UYV_10Packed:
        {
            const int order[3] = { 1, 0, 2 };
            AppendYUV444DataPixelInfo<unsigned short>( pixel, pixelInfo, m_pIB, order );
        }
        break;
    case ibpfYUV444Packed:
        {
            const int order[3] = { 0, 1, 2 };
            AppendYUV444DataPixelInfo<unsigned char>( pixel, pixelInfo, m_pIB, order );
        }
        break;
    case ibpfYUV444_10Packed:
        {
            const int order[3] = { 0, 1, 2 };
            AppendYUV444DataPixelInfo<unsigned short>( pixel, pixelInfo, m_pIB, order );
        }
        break;
    case ibpfYUV422Planar:
        {
            const unsigned char* y = reinterpret_cast<const unsigned char*>( m_pIB->vpData ) + m_pIB->pChannels[0].iChannelOffset + ( pixel.y * m_pIB->pChannels[0].iLinePitch ) + pixel.x;
            const unsigned char* u = reinterpret_cast<const unsigned char*>( m_pIB->vpData ) + m_pIB->pChannels[1].iChannelOffset + ( pixel.y * m_pIB->pChannels[1].iLinePitch ) + pixel.x / 2;
            const unsigned char* v = reinterpret_cast<const unsigned char*>( m_pIB->vpData ) + m_pIB->pChannels[2].iChannelOffset + ( pixel.y * m_pIB->pChannels[2].iLinePitch ) + pixel.x / 2;
            pixelInfo.append( wxString::Format( wxT( "%d(Y), %d(U), %d(V)" ), *y, *u, *v ) );
        }
        break;
    default:
        pixelInfo.append( wxT( "Unsupported format" ) );
        break;
    }

    return pixelInfo;
}

//-----------------------------------------------------------------------------
/// This code gets executed when the user has pressed the left mouse button
/// while the mouse was on top of one of the 8 rectangles around an AOI that
/// allows resizing.
void ImageCanvas::HandleAOIResize( void )
//-----------------------------------------------------------------------------
{
    if( m_aoiContainer.find( m_pAOICurrentlyBeingResized ) != m_aoiContainer.end() )
    {
        wxRect aoi( m_AOIAtLeftMouseDown.m_rect );
        const wxPoint moved( AlignMouseMovementToAOIIncrementConstraints( m_pAOICurrentlyBeingResized ) );
        aoi.Offset( moved );
        m_grabbedSizingPoint->second->m_rect = aoi;
        switch( m_grabbedSizingPoint->first )
        {
        case spTopLeft:
            {
                const wxPoint bottomRight( m_pAOICurrentlyBeingResized->m_rect.GetBottomRight() );
                wxPoint topLeft( AlignMousePositionToAOIIncrementConstraints( m_pAOICurrentlyBeingResized ) );
                topLeft.x = ( ( bottomRight.x - topLeft.x ) >= m_pAOICurrentlyBeingResized->m_minW ) ? topLeft.x : bottomRight.x - m_pAOICurrentlyBeingResized->m_minW;
                topLeft.y = ( ( bottomRight.y - topLeft.y ) >= m_pAOICurrentlyBeingResized->m_minH ) ? topLeft.y : bottomRight.y - m_pAOICurrentlyBeingResized->m_minH;
                m_pAOICurrentlyBeingResized->m_rect.SetTopLeft( topLeft );
                m_pAOICurrentlyBeingResized->m_rect.SetBottomRight( bottomRight );
            }
            break;
        case spTopMiddle:
            {
                const int bottom = m_pAOICurrentlyBeingResized->m_rect.GetBottom();
                int top = AlignMousePositionToAOIIncrementConstraints( m_pAOICurrentlyBeingResized ).y;
                top = ( ( bottom - top ) >= m_pAOICurrentlyBeingResized->m_minH ) ? top : bottom - m_pAOICurrentlyBeingResized->m_minH;
                m_pAOICurrentlyBeingResized->m_rect.SetTop( top );
                m_pAOICurrentlyBeingResized->m_rect.SetBottom( bottom );
            }
            break;
        case spTopRight:
            {
                const wxPoint bottomLeft( m_pAOICurrentlyBeingResized->m_rect.GetBottomLeft() );
                wxPoint topRight( AlignMousePositionToAOIIncrementConstraints( m_pAOICurrentlyBeingResized ) );
                topRight.y = ( ( bottomLeft.y - topRight.y ) >= m_pAOICurrentlyBeingResized->m_minH ) ? topRight.y : bottomLeft.y - m_pAOICurrentlyBeingResized->m_minH;
                m_pAOICurrentlyBeingResized->m_rect.SetTopRight( topRight );
                m_pAOICurrentlyBeingResized->m_rect.SetBottomLeft( bottomLeft );
            }
            break;
        case spMiddleLeft:
            {
                const int right = m_pAOICurrentlyBeingResized->m_rect.GetRight();
                int left = AlignMousePositionToAOIIncrementConstraints( m_pAOICurrentlyBeingResized ).x;
                left = ( ( right - left ) >= m_pAOICurrentlyBeingResized->m_minW ) ? left : right - m_pAOICurrentlyBeingResized->m_minW;
                m_pAOICurrentlyBeingResized->m_rect.SetLeft( left );
                m_pAOICurrentlyBeingResized->m_rect.SetRight( right );
            }
            break;
        case spMiddleRight:
            m_pAOICurrentlyBeingResized->m_rect.SetRight( AlignMousePositionToAOIIncrementConstraints( m_pAOICurrentlyBeingResized ).x );
            break;
        case spBottomLeft:
            {
                const int right = m_pAOICurrentlyBeingResized->m_rect.GetRight();
                wxPoint bottomLeft( AlignMousePositionToAOIIncrementConstraints( m_pAOICurrentlyBeingResized ) );
                bottomLeft.x = ( ( right - bottomLeft.x ) >= m_pAOICurrentlyBeingResized->m_minW ) ? bottomLeft.x : right - m_pAOICurrentlyBeingResized->m_minW;
                m_pAOICurrentlyBeingResized->m_rect.SetBottomLeft( bottomLeft );
                m_pAOICurrentlyBeingResized->m_rect.SetRight( right );
            }
            break;
        case spBottomMiddle:
            m_pAOICurrentlyBeingResized->m_rect.SetBottom( AlignMousePositionToAOIIncrementConstraints( m_pAOICurrentlyBeingResized ).y );
            break;
        case spBottomRight:
            m_pAOICurrentlyBeingResized->m_rect.SetBottomRight( AlignMousePositionToAOIIncrementConstraints( m_pAOICurrentlyBeingResized ) );
            break;
        }
        SendRefreshAOIControlsEvent( m_pAOICurrentlyBeingResized );
    }
}

//-----------------------------------------------------------------------------
void ImageCanvas::HandleMouseAndKeyboardEvents( bool boHandle )
//-----------------------------------------------------------------------------
{
    wxCriticalSectionLocker locker( m_critSect );
    m_boHandleMouseAndKeyboardEvents = boHandle;
}

//-----------------------------------------------------------------------------
void ImageCanvas::IncreaseSkippedCounter( size_t count )
//-----------------------------------------------------------------------------
{
    m_skippedImages += count;
}

//-----------------------------------------------------------------------------
void ImageCanvas::Init( const wxWindow* const pApp )
//-----------------------------------------------------------------------------
{
    m_InfoOverlayColor = wxColour( 128, 128, 0 );
    m_bufferPartIndexDisplayed = 0;
    m_boRefreshInProgress = false;
    m_pActiveAnalysisPlot = 0;
    m_boAOIWizardActive = false;
    m_pDraggedAOI = 0;
    m_pAOICurrentlyBeingResized = 0;
    m_boHandleMouseAndKeyboardEvents = true;
    m_boDoubleClickDisabledAndPopupMenuPruned = false;
    m_boShowInfoOverlay = false;
    m_boScaleToClientSize = false;
    m_boShowImageModificationWarning = true;
    m_boShowPerformanceWarnings = false;
    m_pApp = const_cast<wxWindow*>( pApp );
    m_pIB = 0;
    m_currentZoomFactor = 1.0;
    m_zoomFactor_Max = 1.0;
    m_skippedImages = 0;
    m_skippedPaintEvents = 0;
    m_pMonitorDisplay = 0;
    m_pVisiblePartOfImage = 0;
    m_scalingMode = smNearestNeighbour;
    m_userData = -1;
    SetScrollRate( 1, 1 );
    m_sizingPoints.insert( make_pair( spTopLeft, new AOI( wxRect( 0, 0, 1, 1 ), *wxBLACK_PEN, true, false, wxString( wxEmptyString ) ) ) );
    m_sizingPoints.insert( make_pair( spTopMiddle, new AOI( wxRect( 0, 0, 1, 1 ), *wxBLACK_PEN, true, false, wxString( wxEmptyString ) ) ) );
    m_sizingPoints.insert( make_pair( spTopRight, new AOI( wxRect( 0, 0, 1, 1 ), *wxBLACK_PEN, true, false, wxString( wxEmptyString ) ) ) );
    m_sizingPoints.insert( make_pair( spMiddleLeft, new AOI( wxRect( 0, 0, 1, 1 ), *wxBLACK_PEN, true, false, wxString( wxEmptyString ) ) ) );
    m_sizingPoints.insert( make_pair( spMiddleRight, new AOI( wxRect( 0, 0, 1, 1 ), *wxBLACK_PEN, true, false, wxString( wxEmptyString ) ) ) );
    m_sizingPoints.insert( make_pair( spBottomLeft, new AOI( wxRect( 0, 0, 1, 1 ), *wxBLACK_PEN, true, false, wxString( wxEmptyString ) ) ) );
    m_sizingPoints.insert( make_pair( spBottomMiddle, new AOI( wxRect( 0, 0, 1, 1 ), *wxBLACK_PEN, true, false, wxString( wxEmptyString ) ) ) );
    m_sizingPoints.insert( make_pair( spBottomRight, new AOI( wxRect( 0, 0, 1, 1 ), *wxBLACK_PEN, true, false, wxString( wxEmptyString ) ) ) );
    m_boMouseIsWithinAnAOIsSizingZone = false;
    m_grabbedSizingPoint = m_sizingPoints.end();
}

//-----------------------------------------------------------------------------
bool ImageCanvas::IsCursorOverHorizontalScrollBar( const int mousePositionY ) const
//-----------------------------------------------------------------------------
{
    if( HasScrollbar( wxHORIZONTAL ) )
    {
        const int scrollBarHeight = wxSystemSettings::GetMetric( wxSYS_HSCROLL_Y );
        int canvasHeight = 0;
        GetClientSize( 0, &canvasHeight );
        if( ( mousePositionY < ( canvasHeight + scrollBarHeight ) ) && ( mousePositionY > canvasHeight ) )
        {
            return true;
        }
    }
    return false;
}

//-----------------------------------------------------------------------------
bool ImageCanvas::IsCursorOverVerticalScrollBar( const int mousePositionX ) const
//-----------------------------------------------------------------------------
{
    if( HasScrollbar( wxVERTICAL ) )
    {
        const int scrollBarWidth = wxSystemSettings::GetMetric( wxSYS_VSCROLL_X );
        int canvasWidth = 0;
        GetClientSize( &canvasWidth, 0 );
        if( ( mousePositionX < ( canvasWidth + scrollBarWidth ) ) && ( mousePositionX > canvasWidth ) )
        {
            return true;
        }
    }
    return false;
}

//-----------------------------------------------------------------------------
bool ImageCanvas::IsPointWithinAOISizingZone( const wxPoint& p, const wxRect& r ) const
//-----------------------------------------------------------------------------
{
    const wxCoord sizingZoneWidth = GetSizingZoneWidth();
    return IsPointWithinRect( p, wxRect( r ).Inflate( sizingZoneWidth, sizingZoneWidth ) ) &&
           !IsPointWithinRect( p, wxRect( r ).Deflate( sizingZoneWidth, sizingZoneWidth ) );
}

//-----------------------------------------------------------------------------
void ImageCanvas::OnKeyDown( wxKeyEvent& e )
//-----------------------------------------------------------------------------
{
    if( m_boHandleMouseAndKeyboardEvents )
    {
        if( ( e.GetKeyCode() == wxT( '+' ) ) || ( e.GetKeyCode() == WXK_ADD ) || ( e.GetKeyCode() == WXK_NUMPAD_ADD ) )
        {
            UpdateZoomFactor( zimMultiply, 2.0 );
        }
        else if( ( e.GetKeyCode() == wxT( '-' ) ) || ( e.GetKeyCode() == WXK_SUBTRACT ) || ( e.GetKeyCode() == WXK_NUMPAD_SUBTRACT ) )
        {
            UpdateZoomFactor( zimDivide, 2.0 );
        }
        else if( ( e.GetKeyCode() == WXK_RIGHT ) || ( e.GetKeyCode() == WXK_NUMPAD_RIGHT ) )
        {
            DecreaseShiftValue();
        }
        else if( ( e.GetKeyCode() == WXK_LEFT ) || ( e.GetKeyCode() == WXK_NUMPAD_LEFT ) )
        {
            IncreaseShiftValue();
        }
        else if( ( e.GetKeyCode() == wxT( 'C' ) ) && e.ControlDown() )
        {
            CopyCurrentImageToClipboard();
        }
        else if( e.GetKeyCode() == WXK_SPACE )
        {
            SendSnapshotEvent();
        }
    }
    e.Skip();
}

//-----------------------------------------------------------------------------
void ImageCanvas::OnLeftDblClick( wxMouseEvent& e )
//-----------------------------------------------------------------------------
{
    if( m_boHandleMouseAndKeyboardEvents && !m_boDoubleClickDisabledAndPopupMenuPruned )
    {
        wxCommandEvent eToggleDisplayArea( toggleDisplayArea, m_pApp->GetId() );
        ::wxPostEvent( m_pApp->GetEventHandler(), eToggleDisplayArea );
    }
    else
    {
        e.Skip();
    }
}

//-----------------------------------------------------------------------------
void ImageCanvas::OnLeftDown( wxMouseEvent& e )
//-----------------------------------------------------------------------------
{
    if( m_boHandleMouseAndKeyboardEvents )
    {
        m_AOIAtLeftMouseDown = AOI();
        if( IsActive() )
        {
            m_lastLeftMouseDownPos = GetScaledMousePos( e.GetX(), e.GetY() );
            GetViewStart( &m_lastViewStart.x, &m_lastViewStart.y );
            wxCriticalSectionLocker locker( m_critSect );
            if( !m_aoiContainer.empty() )
            {
                if( m_boMouseIsWithinAnAOIsSizingZone )
                {
                    SizingPointContainer::const_iterator it = m_sizingPoints.begin();
                    const SizingPointContainer::const_iterator itEND = m_sizingPoints.end();
                    while( it != itEND )
                    {
                        const wxCoord sizingZoneWidth = GetSizingZoneWidth();
                        if( IsPointWithinRect( m_lastLeftMouseDownPos, wxRect( it->second->m_rect ).Inflate( sizingZoneWidth, sizingZoneWidth ) ) )
                        {
                            m_AOIAtLeftMouseDown = *it->second;
                            m_grabbedSizingPoint = it;
                            break;
                        }
                        ++it;
                    }
                }
                else
                {
                    AOIContainer::const_iterator it = m_aoiContainer.begin();
                    const AOIContainer::const_iterator itEND = m_aoiContainer.end();
                    while( it != itEND )
                    {
                        if( ( *it )->m_boCanDrag )
                        {
                            if( IsPointWithinRect( m_lastLeftMouseDownPos, ( *it )->m_rect ) )
                            {
                                m_AOIAtLeftMouseDown = **it;
                                m_pDraggedAOI = *it;
                                break;
                            }
                        }
                        ++it;
                    }
                }
            }
        }
    }
    e.Skip();
    wxCommandEvent imageCanvasSelectionEvent( imageCanvasSelected, m_pApp->GetId() );
    imageCanvasSelectionEvent.SetInt( m_userData );
    ::wxPostEvent( m_pApp->GetEventHandler(), imageCanvasSelectionEvent );
}

//-----------------------------------------------------------------------------
void ImageCanvas::OnLeftUp( wxMouseEvent& )
//-----------------------------------------------------------------------------
{
    m_pDraggedAOI = 0;
    m_pAOICurrentlyBeingResized = 0;
    m_grabbedSizingPoint = m_sizingPoints.end();
}

//-----------------------------------------------------------------------------
void ImageCanvas::OnMotion( wxMouseEvent& e )
//-----------------------------------------------------------------------------
{
    if( m_boHandleMouseAndKeyboardEvents && IsActive() && !IsFullScreen() && m_pIB && m_pIB->vpData )
    {
        m_lastMousePos = GetScaledMousePos( e.GetX(), e.GetY() );
        const bool boMouseWasWithinAnAOIsSizingZone = m_boMouseIsWithinAnAOIsSizingZone;
        m_boMouseIsWithinAnAOIsSizingZone = false;
        {
            wxCriticalSectionLocker locker( m_critSect );
            const bool boAOIFullModeActive = ( m_pActiveAnalysisPlot && m_pActiveAnalysisPlot->IsAOIFullModeActive() ) ? true : false;
            if( e.LeftIsDown() )
            {
                if( !boAOIFullModeActive && m_pAOICurrentlyBeingResized && ( m_grabbedSizingPoint != m_sizingPoints.end() ) )
                {
                    // The left mouse button was(and still is) pressed while hovering on a 'resize point' of an AOI
                    // and now moved to another location -> Resize the AOI.
                    HandleAOIResize();
                }
                else if( !boAOIFullModeActive && m_pDraggedAOI )
                {
                    // The left mouse button was(and still is) pressed while hovering inside an AOI -> Move the complete AOI to a new location.
                    AOIContainer::iterator it = m_aoiContainer.find( m_pDraggedAOI );
                    if( it != m_aoiContainer.end() )
                    {
                        wxRect aoi( m_AOIAtLeftMouseDown.m_rect );
                        aoi.Offset( AlignMouseMovementToAOIIncrementConstraints( m_pDraggedAOI ) );
                        ClipAOI( aoi, true );
                        ( *it )->m_rect = aoi;
                        SendRefreshAOIControlsEvent( *it );
                    }
                }
                else
                {
                    // The left mouse button was(and still is) pressed outside any AOI -> When the display is not in 'fit to screen' mode
                    // this acts like using both scrollbars simultaneously.
                    if( !IsScaled() )
                    {
                        DragImageDisplay();
                    }
                }
            }
            else if( e.RightIsDown() && ( ( m_pActiveAnalysisPlot && m_pActiveAnalysisPlot->IsActive() && !m_pActiveAnalysisPlot->IsAOIFullModeActive() &&
                                            ( m_aoiContainer.size() == 1 ) ) || m_boAOIWizardActive ) ) // as the right mouse key is used to completely redraw an existing AOI this can only work with a single AOI. Otherwise how to choose which one to delete?
            {
                // The right mouse button was(and still is) pressed outside any AOI -> Redraw a new AOI that spans from the point where the right
                // mouse button has been pressed to the current location
                wxPoint upperLeft;
                wxPoint lowerRight;
                upperLeft.x = ( m_lastRightMouseDownPos.x < m_lastMousePos.x ) ? m_lastRightMouseDownPos.x : m_lastMousePos.x;
                upperLeft.y = ( m_lastRightMouseDownPos.y < m_lastMousePos.y ) ? m_lastRightMouseDownPos.y : m_lastMousePos.y;
                lowerRight.x = ( m_lastRightMouseDownPos.x > m_lastMousePos.x ) ? m_lastRightMouseDownPos.x : m_lastMousePos.x;
                lowerRight.y = ( m_lastRightMouseDownPos.y > m_lastMousePos.y ) ? m_lastRightMouseDownPos.y : m_lastMousePos.y;
                if( upperLeft.x < 0 )
                {
                    upperLeft.x = 0;
                }
                if( upperLeft.y < 0 )
                {
                    upperLeft.y = 0;
                }
                wxRect rect( upperLeft, lowerRight );
                ClipAOI( rect, false );
                ( *m_aoiContainer.begin() )->m_rect = rect;
                SendRefreshAOIControlsEvent( *m_aoiContainer.begin() );
            }
            else
            {
                AOIContainer::const_iterator it = m_aoiContainer.begin();
                const AOIContainer::const_iterator itEND = m_aoiContainer.end();
                while( it != itEND )
                {
                    if( ( *it )->m_boCanDrag )
                    {
                        if( IsPointWithinAOISizingZone( m_lastMousePos, ( *it )->m_rect ) )
                        {
                            m_pAOICurrentlyBeingResized = *it;
                            UpdateAOISizingPoint( m_sizingPoints[spTopLeft], ( *it )->m_pen, ( *it )->m_rect.GetX(), ( *it )->m_rect.GetY() );
                            UpdateAOISizingPoint( m_sizingPoints[spTopMiddle], ( *it )->m_pen, ( *it )->m_rect.GetX() + ( *it )->m_rect.GetWidth() / 2, ( *it )->m_rect.GetY() );
                            UpdateAOISizingPoint( m_sizingPoints[spTopRight], ( *it )->m_pen, ( *it )->m_rect.GetX() + ( *it )->m_rect.GetWidth(), ( *it )->m_rect.GetY() );
                            UpdateAOISizingPoint( m_sizingPoints[spMiddleLeft], ( *it )->m_pen, ( *it )->m_rect.GetX(), ( *it )->m_rect.GetY() + ( *it )->m_rect.GetHeight() / 2 );
                            UpdateAOISizingPoint( m_sizingPoints[spMiddleRight], ( *it )->m_pen, ( *it )->m_rect.GetX() + ( *it )->m_rect.GetWidth(), ( *it )->m_rect.GetY() + ( *it )->m_rect.GetHeight() / 2 );
                            UpdateAOISizingPoint( m_sizingPoints[spBottomLeft], ( *it )->m_pen, ( *it )->m_rect.GetX(), ( *it )->m_rect.GetY() + ( *it )->m_rect.GetHeight() );
                            UpdateAOISizingPoint( m_sizingPoints[spBottomMiddle], ( *it )->m_pen, ( *it )->m_rect.GetX() + ( *it )->m_rect.GetWidth() / 2, ( *it )->m_rect.GetY() + ( *it )->m_rect.GetHeight() );
                            UpdateAOISizingPoint( m_sizingPoints[spBottomRight], ( *it )->m_pen, ( *it )->m_rect.GetX() + ( *it )->m_rect.GetWidth(), ( *it )->m_rect.GetY() + ( *it )->m_rect.GetHeight() );
                            m_boMouseIsWithinAnAOIsSizingZone = true;
                            break;
                        }
                    }
                    ++it;
                }
            }
        }
        wxCommandEvent eRefreshCurrentPixelData( refreshCurrentPixelData, m_pApp->GetId() );
        eRefreshCurrentPixelData.SetInt( m_userData );
        ::wxPostEvent( m_pApp->GetEventHandler(), eRefreshCurrentPixelData );
        if( m_boMouseIsWithinAnAOIsSizingZone != boMouseWasWithinAnAOIsSizingZone )
        {
            Refresh( false );
        }
    }
}

//-----------------------------------------------------------------------------
void ImageCanvas::OnMouseWheel( wxMouseEvent& e )
//-----------------------------------------------------------------------------
{
    if( m_boHandleMouseAndKeyboardEvents )
    {
        if( IsCursorOverHorizontalScrollBar( e.GetY() ) )
        {
            const int direction = ( e.GetWheelRotation() < 0 ) ? 1 : ( -1 );
            Scroll( GetViewStart().x + ( 1 * direction ), GetViewStart().y );
        }
        else if( IsCursorOverVerticalScrollBar( e.GetX() ) )
        {
            const int direction = ( e.GetWheelRotation() < 0 ) ? 1 : ( -1 );
            Scroll( GetViewStart().x, GetViewStart().y + ( 1 * direction ) );
        }
        else
        {
            if( e.GetWheelRotation() > 0 )
            {
                UpdateZoomFactor( zimMultiply, 2.0 );
            }
            else if( e.GetWheelRotation() < 0 )
            {
                UpdateZoomFactor( zimDivide, 2.0 );
            }
        }
    }
    else
    {
        e.Skip();
    }
}

//-----------------------------------------------------------------------------
void ImageCanvas::OnPopUp_ScalingMode_Changed( wxCommandEvent& e )
//-----------------------------------------------------------------------------
{
    switch( e.GetId() )
    {
    case miPopUpScalerMode_NearestNeighbour:
        SetScalingMode( smNearestNeighbour );
        break;
    case miPopUpScalerMode_Linear:
        SetScalingMode( smLinear );
        break;
    case miPopUpScalerMode_Cubic:
        SetScalingMode( smCubic );
        break;
    }
    Refresh( true );
}

//-----------------------------------------------------------------------------
void ImageCanvas::OnPopUpFitToScreen( wxCommandEvent& e )
//-----------------------------------------------------------------------------
{
    SetScaling( e.IsChecked() );
}

//-----------------------------------------------------------------------------
void ImageCanvas::OnPopUpOneToOneDisplay( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    UpdateZoomFactor( zimFixedValue, 1.0 );
}

//-----------------------------------------------------------------------------
void ImageCanvas::OnPopUpSaveCurrentImage( wxCommandEvent& /*e*/ )
//-----------------------------------------------------------------------------
{
    wxCommandEvent eSaveImage( saveImage, m_pApp->GetId() );
    ::wxPostEvent( m_pApp->GetEventHandler(), eSaveImage );
}

//-----------------------------------------------------------------------------
void ImageCanvas::OnPopUpSetShiftValue( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    int shift = static_cast<int>( ::wxGetNumberFromUser( wxT( "By default only the 8 MSBs of multi-byte pixel data will be displayed.\nThis dialog can be used to define which 8 bits of multi-byte image shall be displayed.\nThe value defined here will be subtracted from the value that would\nbe needed to display the 8 MSBs of a pixel, thus e.g. to see the 8 LSBs\nof a 12 bit mono format enter 4 here.\n\nThe shift values can be displayed by enabling the 'Performance Warning Overlay'\nand the shift value can also be adjusted by clicking on a display and then\npressing the left/right arrow keys on the keyboard" ), wxT( "Bit Shift Value:" ), wxT( "Set Custom Bit Shift Value" ), GetShiftValue(), 0, 8, this ) );
    if( shift >= 0 )
    {
        while( shift > GetShiftValue() )
        {
            IncreaseShiftValue();
        }
        while( shift < GetShiftValue() )
        {
            DecreaseShiftValue();
        }
    }
}

//-----------------------------------------------------------------------------
void ImageCanvas::OnPopUpShowImageModificationsWarning( wxCommandEvent& e )
//-----------------------------------------------------------------------------
{
    SetImageModificationWarningOutput( e.IsChecked() );
}

//-----------------------------------------------------------------------------
void ImageCanvas::OnPopUpShowPerformanceWarnings( wxCommandEvent& e )
//-----------------------------------------------------------------------------
{
    SetPerformanceWarningOutput( e.IsChecked() );
}

//-----------------------------------------------------------------------------
void ImageCanvas::OnPopUpShowRequestInfoOverlay( wxCommandEvent& e )
//-----------------------------------------------------------------------------
{
    SetInfoOverlayMode( e.IsChecked() );
}

//-----------------------------------------------------------------------------
void ImageCanvas::OnRightDown( wxMouseEvent& e )
//-----------------------------------------------------------------------------
{
    if( m_boHandleMouseAndKeyboardEvents )
    {
        m_lastRightMouseDownPos_Unscaled = wxPoint( e.GetX(), e.GetY() );
        if( IsActive() )
        {
            m_lastRightMouseDownPos = GetScaledMousePos( e.GetX(), e.GetY() );
        }
    }
    else
    {
        e.Skip();
    }
}

//-----------------------------------------------------------------------------
void ImageCanvas::OnRightUp( wxMouseEvent& e )
//-----------------------------------------------------------------------------
{
    if( m_boHandleMouseAndKeyboardEvents && !IsFullScreen() )
    {
        if( m_lastRightMouseDownPos_Unscaled == wxPoint( e.GetX(), e.GetY() ) )
        {
            wxMenu menu( wxT( "" ) );
            menu.Append( miPopUpFitToScreen, wxT( "Fit To Screen" ), wxT( "If active the captured image will be scaled to fit into the main display area(slower)" ), wxITEM_CHECK )->Check( IsScaled() );
            menu.Append( miPopUpOneToOneDisplay, wxT( "1:1 Display (Zoom 100%)" ), wxT( "Will restore the zoom factor back to 1" ), wxITEM_NORMAL );
            if( m_boDoubleClickDisabledAndPopupMenuPruned )
            {
                PopupMenu( &menu );
                return;
            }
            if( SupportsFullScreenMode() )
            {
                menu.Append( miPopUpFullScreen, wxT( "Full Screen (Press 'ESC' to exit full screen mode afterwards)" ), wxT( "" ), wxITEM_CHECK );
            }
            if( SupportsDifferentScalingModes() )
            {
                wxMenu* pMenuScalingModes = new wxMenu;
                wxMenuItem* pMI = pMenuScalingModes->Append( miPopUpScalerMode_NearestNeighbour, wxT( "Nearest Neighbour" ), wxT( "" ), wxITEM_RADIO );
                if( m_scalingMode == smNearestNeighbour )
                {
                    pMI->Check();
                }
                pMI = pMenuScalingModes->Append( miPopUpScalerMode_Linear, wxT( "Linear" ), wxT( "" ), wxITEM_RADIO );
                if( m_scalingMode == smLinear )
                {
                    pMI->Check();
                }
                pMI = pMenuScalingModes->Append( miPopUpScalerMode_Cubic, wxT( "Cubic" ), wxT( "" ), wxITEM_RADIO );
                if( m_scalingMode == smCubic )
                {
                    pMI->Check();
                }
                menu.Append( wxID_ANY, wxT( "Scaling Mode" ), pMenuScalingModes );
            }
            menu.Append( miPopUpSetShiftValue, wxT( "Set Bit Shift Value" ), wxT( "Defines a custom shift value for the display of multi-byte pixel data. This defines which 8 bits from a multi-byte pixel will be displayed" ) );
            menu.AppendSeparator();
            menu.Append( miPopUpShowRequestInfoOverlay, wxT( "Request Info Overlay" ), wxT( "If active the various information returned together with the image will be displayed as an overlay in the main display area(This will cost some additional CPU time)" ), wxITEM_CHECK )->Check( InfoOverlayActive() );
            menu.Append( miPopUpSelectRequestInfoOverlayColor, wxT( "Select Request Info Overlay Color" ), wxT( "Select Request Info Overlay Color" ) );
            menu.Append( miPopUpShowPerformanceWarnings, wxT( "Performance Warning Overlay" ), wxT( "If active various information will be drawn on top of the image in the main display area to inform e.g. about possible performance losses" ), wxITEM_CHECK )->Check( GetPerformanceWarningOutput() );
            menu.Append( miPopUpShowImageModificationsWarning, wxT( "Warn On Modifications Applied To The Image By The Display (By An Overlay)" ), wxT( "If active a message will be drawn on top of the image in the main display area to inform e.g. about data modifications applied to the image by the display module" ), wxITEM_CHECK )->Check( GetImageModificationWarningOutput() );
            menu.Append( miPopUpSaveCurrentImage, wxT( "Save Current Image" ), wxT( "Opens the save image dialog to store the currently shown image." ), wxITEM_NORMAL );
            menu.Append( miPopUpCopyCurrentImageToClipboard, wxT( "Copy Current Image To Clipboard (CTRL+C)" ), wxT( "Saves the currently shown image to the clipboard." ), wxITEM_NORMAL );
            PopupMenu( &menu );
        }
    }
    else
    {
        e.Skip();
    }
}

//-----------------------------------------------------------------------------
void ImageCanvas::RegisterAOI( AOI* pAOI )
//-----------------------------------------------------------------------------
{
    wxCriticalSectionLocker locker( m_critSect );
    m_aoiContainer.insert( pAOI );
}

//-----------------------------------------------------------------------------
void ImageCanvas::RegisterAOIs( const vector<AOI*>& AOIs )
//-----------------------------------------------------------------------------
{
    const vector<pair<wxRect, wxColour> >::size_type AOICount = AOIs.size();
    for( vector<pair<wxRect, wxColour> >::size_type i = 0; i < AOICount; i++ )
    {
        RegisterAOI( AOIs[i] );
    }
}

//-----------------------------------------------------------------------------
bool ImageCanvas::RegisterMonitorDisplay( ImageCanvas* pMonitorDisplay )
//-----------------------------------------------------------------------------
{
    wxCriticalSectionLocker locker( m_critSect );
    if( pMonitorDisplay && m_pMonitorDisplay )
    {
        return false;
    }

    if( pMonitorDisplay && !m_pMonitorDisplay )
    {
        const wxRect rect( 0, 0, numeric_limits<int>::max(), numeric_limits<int>::max() );
        m_pVisiblePartOfImage = new AOI( rect, *wxRED, GetName() );
        pMonitorDisplay->RegisterAOI( m_pVisiblePartOfImage );
        Refresh( false );
    }
    else if( !pMonitorDisplay && m_pMonitorDisplay )
    {
        m_pMonitorDisplay->UnregisterAOI( m_pVisiblePartOfImage );
        DeleteElement( m_pVisiblePartOfImage );
        m_pMonitorDisplay = 0;
    }
    else
    {
        return false;
    }
    m_pMonitorDisplay = pMonitorDisplay;
    return true;
}

//-----------------------------------------------------------------------------
void ImageCanvas::ResetRequestInProgressFlag( void )
//-----------------------------------------------------------------------------
{
    wxCriticalSectionLocker locker( m_critSect );
    m_boRefreshInProgress = false;
}

//-----------------------------------------------------------------------------
void ImageCanvas::ResetSkippedImagesCounter( void )
//-----------------------------------------------------------------------------
{
    wxCriticalSectionLocker locker( m_critSect );
    m_skippedImages = 0;
}

//-----------------------------------------------------------------------------
void ImageCanvas::SendRefreshAOIControlsEvent( AOI* pAOI ) const
//-----------------------------------------------------------------------------
{
    wxCommandEvent eRefreshAOIControls( refreshAOIControls, m_pApp->GetId() );
    eRefreshAOIControls.SetClientData( pAOI );
    ::wxPostEvent( m_pApp->GetEventHandler(), eRefreshAOIControls );
}

//-----------------------------------------------------------------------------
void ImageCanvas::SendSnapshotEvent( void ) const
//-----------------------------------------------------------------------------
{
    // Do NOT remove this function e.g. by copying these 2 lines to the only place
    // this function gets called! The sole purpose of this function is to hide a
    // known gcc bug that got triggered by doing exactly that!
    //
    // Read this before someone gets hurt:
    // - http://trac.wxwidgets.org/ticket/16359
    // - https://bugzilla.redhat.com/show_bug.cgi?id=1147265

    wxCommandEvent eSnapshotButtonHit( snapshotButtonHit, m_pApp->GetId() );
    ::wxPostEvent( m_pApp->GetEventHandler(), eSnapshotButtonHit );
}

//-----------------------------------------------------------------------------
void ImageCanvas::SetActiveAnalysisPlot( const PlotCanvasImageAnalysis* pPlot )
//-----------------------------------------------------------------------------
{
    {
        wxCriticalSectionLocker locker( m_critSect );
        m_pActiveAnalysisPlot = pPlot;
    }
    Refresh( false );
}

//-----------------------------------------------------------------------------
void ImageCanvas::SetAOIWizardActive( bool boActive )
//-----------------------------------------------------------------------------
{
    m_boAOIWizardActive = boActive;
}

//-----------------------------------------------------------------------------
void ImageCanvas::SetImageModificationWarningOutput( bool boOn )
//-----------------------------------------------------------------------------
{
    if( m_boShowImageModificationWarning != boOn )
    {
        m_boShowImageModificationWarning = boOn;
        Refresh( true );
    }
}

//-----------------------------------------------------------------------------
void ImageCanvas::SetInfoOverlay( const vector<wxString>& infoStrings )
//-----------------------------------------------------------------------------
{
    if( m_infoStringsOverlay != infoStrings )
    {
        m_infoStringsOverlay = infoStrings;
    }
}

//-----------------------------------------------------------------------------
void ImageCanvas::SetInfoOverlayMode( bool boOn )
//-----------------------------------------------------------------------------
{
    if( m_boShowInfoOverlay != boOn )
    {
        m_boShowInfoOverlay = boOn;
        Refresh( true );
    }
}

//-----------------------------------------------------------------------------
void ImageCanvas::SetMaxZoomFactor( const mvIMPACT::acquire::ImageBuffer* pIB, int oldMaxDim )
//-----------------------------------------------------------------------------
{
    if( !pIB )
    {
        m_zoomFactor_Max = 1.0;
        return;
    }

    int newMaxDim = max( pIB->iWidth, pIB->iHeight );
    if( oldMaxDim != newMaxDim )
    {
        m_zoomFactor_Max = 1.0;
        // Here the maximum zoom factor is being calculated by dividing the biggest image dimension by two until nothing remains.
        // However in the age of mega-pixel this method may create ridiculously large factors, which combined with the also ridiculously
        // large image dimensions leads to images that are so big that they cannot be handled by the GUI tools
        // Example: an image of 2048 * 2048 pixels gives a max zoom factor of 2048 with this method. The resulting image dimension in
        // max. magnification is 2048*2048 = 4 Million pixels. In only ONE dimension. The full resulting image is 4Million x 4Million pixels.)
        // Because of this we limit the dynamic zoom factor calculation to 512.
        while( ( ( newMaxDim = newMaxDim >> 1 ) != 0 ) &&
               ( m_currentZoomFactor < 512.0 ) &&
               ( m_zoomFactor_Max < 512.0 ) )
        {
            m_zoomFactor_Max *= 2;
        }
        if( m_currentZoomFactor > m_zoomFactor_Max )
        {
            m_currentZoomFactor = m_zoomFactor_Max;
        }
    }
}

//-----------------------------------------------------------------------------
void ImageCanvas::SetPerformanceWarningOutput( bool boOn )
//-----------------------------------------------------------------------------
{
    if( m_boShowPerformanceWarnings != boOn )
    {
        m_boShowPerformanceWarnings = boOn;
        Refresh( true );
    }
}

//-----------------------------------------------------------------------------
void ImageCanvas::SetScaling( bool boOn )
//-----------------------------------------------------------------------------
{
    if( m_boScaleToClientSize != boOn )
    {
        m_boScaleToClientSize = boOn;
        RefreshScrollbars();
        Refresh( true );
    }
}

//-----------------------------------------------------------------------------
void ImageCanvas::Shutdown( void )
//-----------------------------------------------------------------------------
{
    for_each( m_sizingPoints.begin(), m_sizingPoints.end(), ptr_fun( DeleteSecond<const TAOISizingPoint, AOI*> ) );
    m_sizingPoints.clear();
    UnregisterAllAOIs();
}

//-----------------------------------------------------------------------------
ImageCanvas::TSaveResult ImageCanvas::StoreImage( const wxImage& img, const wxString& filenameAndPath, const wxString& extension ) const
//-----------------------------------------------------------------------------
{
    if( extension == wxT( "bmp" ) )
    {
        if( img.SaveFile( filenameAndPath, wxBITMAP_TYPE_BMP ) )
        {
            return srOK;
        }
    }
    else if( extension == wxT( "png" ) )
    {
        if( img.SaveFile( filenameAndPath, wxBITMAP_TYPE_PNG ) )
        {
            return srOK;
        }
    }
    return srFailedToSave;
}

//-----------------------------------------------------------------------------
void ImageCanvas::UnregisterAllAOIs( void )
//-----------------------------------------------------------------------------
{
    wxCriticalSectionLocker locker( m_critSect );
    m_aoiContainer.clear();
}

//-----------------------------------------------------------------------------
bool ImageCanvas::UnregisterAOI( AOI* pAOI )
//-----------------------------------------------------------------------------
{
    wxCriticalSectionLocker locker( m_critSect );
    AOIContainer::iterator it = m_aoiContainer.find( pAOI );
    const bool boFound = it != m_aoiContainer.end();
    if( boFound )
    {
        m_aoiContainer.erase( it );
    }
    return boFound;
}

//-----------------------------------------------------------------------------
bool ImageCanvas::UpdateAOI( AOI* pAOI, int x, int y, int w, int h )
//-----------------------------------------------------------------------------
{
    wxCriticalSectionLocker locker( m_critSect );
    AOIContainer::iterator it = m_aoiContainer.find( pAOI );
    if( it == m_aoiContainer.end() )
    {
        return false;
    }

    if( x >= 0 )
    {
        pAOI->m_rect.SetX( x );
    }
    if( y >= 0 )
    {
        pAOI->m_rect.SetY( y );
    }
    if( w >= 0 )
    {
        pAOI->m_rect.SetWidth( w );
    }
    if( h >= 0 )
    {
        pAOI->m_rect.SetHeight( h );
    }
    Refresh( false );
    return true;
}

//-----------------------------------------------------------------------------
void ImageCanvas::UpdateAOISizingPoint( AOI* pAOI, const wxPen& pen, int centerX, int centerY )
//-----------------------------------------------------------------------------
{
    pAOI->m_rect = wxRect( centerX, centerY, 1, 1 );
    pAOI->m_pen = pen;
}

//-----------------------------------------------------------------------------
void ImageCanvas::UpdateZoomFactor( TZoomIncrementMode zim, double value )
//-----------------------------------------------------------------------------
{
    double zoomFactor = m_currentZoomFactor;
    switch( zim )
    {
    case zimMultiply:
        if( m_currentZoomFactor < m_zoomFactor_Max )
        {
            m_currentZoomFactor *= value;
        }
        break;
    case zimDivide:
        if( m_currentZoomFactor > s_zoomFactor_Min )
        {
            m_currentZoomFactor /= value;
        }
        break;
    case zimFixedValue:
        m_currentZoomFactor = 1.0;
        break;
    default:
        return;
    }

    RefreshScrollbars( true );
    if( !m_boScaleToClientSize )
    {
        Refresh( true );
    }

    if( m_boScaleToClientSize && ( zoomFactor != m_currentZoomFactor ) )
    {
        m_boScaleToClientSize = false;
        //wxMessageBox( wxString::Format( wxT( "Current zoom factor set to %.3f, but this is currently shadowed by the 'fit to screen' mode thus won't have visual effect." ), m_currentZoomFactor ), wxString( wxT( "Zoom factor changed" ) ), wxOK | wxICON_INFORMATION, this );
    }
}
