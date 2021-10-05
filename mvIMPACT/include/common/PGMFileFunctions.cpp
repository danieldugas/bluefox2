//--------------------------------------------------------------------------
#include <iostream>
#include <common/crt/mvstdio.h>
#include <string.h>
#include "PGMFileFunctions.h"
#include <sstream>

using namespace std;

//--------------------------------------------------------------------------
int writePGMFile( const unsigned char* pData, const unsigned long ulWidth, const unsigned long ulHeight,
                  const unsigned long ulPitch, unsigned int uiBypp, const char* pFileName )
//--------------------------------------------------------------------------
{
    // printf( "%s(%s: %d): pData: %p ulWidth: %ld ulHeight: %ld ulPitch: %ld uiBypp: %d pFileName: %s\n",
    //          __FUNCTION__, __FILE__, __LINE__, pData, ulWidth, ulHeight, ulPitch, uiBypp, pFileName );

    if( uiBypp > 1 )
    {
        cerr << "*** " << __FUNCTION__ << " - only 8-bit per pixel images supported! This image has " << uiBypp * 8 << "-bit per pixel!" << endl;
        return -1;
    }

    size_t retCount;
    int ret = -1, err = 0;
    unsigned int iImageFileSize = ulPitch * ulHeight;
    unsigned int iLineBytes = ulWidth * uiBypp;

    FILE* fp = mv_fopen_s( pFileName, "wb" );
    if( fp )
    {
        ostringstream imageHeader;
        imageHeader << "P5" << endl
                    << "# Created by MATRIX VISION GmbH" << endl
                    << ulWidth << endl
                    << ulHeight << endl
                    << "255" << endl;
        fwrite( imageHeader.str().c_str(), 1, imageHeader.str().length(), fp );
        if( ulPitch == iLineBytes )
        {
            if( ( retCount = fwrite( pData, 1, iImageFileSize, fp ) ) != iImageFileSize )
            {
                cerr << "*** " << __FUNCTION__ << "- only " << retCount << " bytes written to file " << pFileName << endl;
            }
            else
            {
                ret = 0;
            }
        }
        else
        {
            for( unsigned long y = 0; y < ulHeight; y++ )
            {
                if( ( retCount = fwrite( pData + y * ulPitch, 1, iLineBytes, fp ) ) != iLineBytes )
                {
                    cerr << "*** " << __FUNCTION__ << " - only " << retCount << " from " << iLineBytes << " bytes written to file " << pFileName << endl;
                    err = 1;
                    break;
                }
            }

            if( !err )
            {
                ret = 0;
            }
        }
        fclose( fp );
    }
    else
    {
        cerr << "*** " << __FUNCTION__ << " - can not open file " << pFileName << endl;
    }

    return ret;
}
