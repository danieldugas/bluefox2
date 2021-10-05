//-----------------------------------------------------------------------------
#include "../ProxyResolverContext.h"
#include <wx/utils.h>

using namespace std;

//-----------------------------------------------------------------------------
ProxyResolverContext::ProxyResolverContext( const wstring& /*userAgent*/, const wstring& /*url*/ ) : pImpl_( 0 )
//-----------------------------------------------------------------------------
{

}

//-----------------------------------------------------------------------------
ProxyResolverContext::~ProxyResolverContext()
//-----------------------------------------------------------------------------
{

}

//-----------------------------------------------------------------------------
wstring ProxyResolverContext::GetProxy( unsigned int /*index*/ ) const
//-----------------------------------------------------------------------------
{
    wxString envString;
    if( !wxGetEnv( "http_proxy", &envString ) )
    {
        return wstring();
    }
    return envString.BeforeLast( ':' ).ToStdWstring();
}

//-----------------------------------------------------------------------------
unsigned int ProxyResolverContext::GetProxyPort( unsigned int /*index*/ ) const
//-----------------------------------------------------------------------------
{
    wxString envString;
    if( !wxGetEnv( "http_proxy", &envString ) )
    {
        return 0;
    }
    return static_cast< unsigned int >( wxAtoi( envString.AfterLast( ':' ) ) );
}

//-----------------------------------------------------------------------------
/// \brief This function checks the token of the calling thread to see if the caller
/// belongs to the Administrators group.
/**
 * \return
   - true if the caller is an administrator on the local machine.
   - false otherwise
*/
bool IsCurrentUserLocalAdministrator( void )
//-----------------------------------------------------------------------------
{
    return true;
}
