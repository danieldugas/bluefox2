//-----------------------------------------------------------------------------
#include "Timeout.h"
#if defined(_WIN32) || defined(WIN32) || defined(__WIN32__)
#   include <windows.h>
#else
#   include <unistd.h>
#endif // #if defined(_WIN32) || defined(WIN32) || defined(__WIN32__)

//-----------------------------------------------------------------------------
CTimeout::CTimeout( unsigned long timeout_ms )
//-----------------------------------------------------------------------------
{
    Start( timeout_ms );
}

//-----------------------------------------------------------------------------
void CTimeout::Start( const unsigned long timeout_ms )
//-----------------------------------------------------------------------------
{
    start_ = clock();
    elapse_period_ = timeout_ms * CLOCKS_PER_SEC / 1000;
    if( elapse_period_ == 0 )
    {
        elapse_period_ = 1;
    }
    end_ = start_ + elapse_period_;
}

//-----------------------------------------------------------------------------
unsigned char CTimeout::Elapsed( void ) const
//-----------------------------------------------------------------------------
{
    return ( end_ < clock() );
}

//-----------------------------------------------------------------------------
void CTimeout::SleepMS( const unsigned long ms ) const
//-----------------------------------------------------------------------------
{
#if defined(_WIN32) || defined(WIN32) || defined(__WIN32__)
    Sleep( ms );
#else
    usleep( ms * 1000 );
#endif // #if defined(_WIN32) || defined(WIN32) || defined(__WIN32__)
}

//-----------------------------------------------------------------------------
unsigned long CTimeout::Remain( void ) const
//-----------------------------------------------------------------------------
{
    return Elapsed() ? 0 : ( ( clock() - end_ ) * 1000 ) / CLOCKS_PER_SEC;
}
