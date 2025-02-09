//-----------------------------------------------------------------------------
#ifndef expatHelperH
#define expatHelperH expatHelperH
//-----------------------------------------------------------------------------
#include <memory.h>
#include <stdio.h>
#include <Toolkits/expat/include/ExpatImpl.h>

//-----------------------------------------------------------------------------
inline bool ParseXMLFromFile( CExpatImpl* pParser, FILE* fp )
//-----------------------------------------------------------------------------
{
    if( !fp )
    {
        return false;
    }

    fseek( fp, 0, SEEK_END );
    const int fileSize = ftell( fp );
    fseek( fp, 0, SEEK_SET );

    if( fileSize <= 0 )
    {
        return false;
    }

    char* pszBuffer = reinterpret_cast<char*>( pParser->GetBuffer( fileSize  + 1 ) );
    if( !pszBuffer )
    {
        return false;
    }

    const size_t bytesRead = fread( pszBuffer, 1, static_cast<size_t>( fileSize ), fp );
    pszBuffer[bytesRead] = '\0'; // make this a well terminated string!
    return pParser->ParseBuffer( static_cast<int>( bytesRead ), true );
}

#endif // expatHelperH
