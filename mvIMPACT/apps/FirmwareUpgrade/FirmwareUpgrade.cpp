//-----------------------------------------------------------------------------
#include <apps/Common/FirmwareUpdate_mvHYPERION/Epcs.h>
#include <apps/Common/exampleHelper.h>
#include <common/auto_array_ptr.h>
#include <common/crt/mvstdio.h>
#include <iostream>
#include <mvIMPACT_CPP/mvIMPACT_acquire_GenICam.h>
#include <mvIMPACT_CPP/mvIMPACT_acquire_GenICam_FileStream.h>
#if (defined( CPP_STANDARD_VERSION ) && ( CPP_STANDARD_VERSION >= 11 ))
#   include <chrono>
#   include <thread>
#endif // #if (defined( CPP_STANDARD_VERSION ) && ( CPP_STANDARD_VERSION >= 11 ))

using namespace mvIMPACT::acquire;
using namespace std;

//-----------------------------------------------------------------------------
class SystemSettingsGenTL : public SystemSettings
//-----------------------------------------------------------------------------
{
public:
    explicit SystemSettingsGenTL( Device* pDev ) : SystemSettings( pDev ),
        featurePollingEnable()
    {
        DeviceComponentLocator locator( pDev, dltSystemSettings );
        locator.bindComponent( featurePollingEnable, "FeaturePollingEnable" );
    }
    PropertyIBoolean featurePollingEnable;
};


//-----------------------------------------------------------------------------
struct UpdateInformation
//-----------------------------------------------------------------------------
{
    Device* pDev_;
    string firmwareFilePath_;
    explicit UpdateInformation( Device* pDev, const std::string& firmwareFilePath = "" ) : pDev_( pDev ), firmwareFilePath_( firmwareFilePath ) {}
};

//-----------------------------------------------------------------------------
enum TUpdateResult
//-----------------------------------------------------------------------------
{
    urOperationSuccessful = 0,
    urFeatureUnsupported = 1,
    urOperationCanceled = 2,
    urInvalidFileSelection = 3,
    urFileIOError = 4,
    urDeviceAccessError = 5,
    urInvalidParameter = 6,
    urFirmwareUpdateFailed = 7
};

//-----------------------------------------------------------------------------
void printHelp( void )
//-----------------------------------------------------------------------------
{
    cout << " Available commands / usage:" << endl
         << " ------------------------------------" << endl
         << " no parameters: Silent mode. Will update the firmware for every mvBlueFOX(AND ONLY THOSE!) connected to the system" << endl
         << "                without asking any further questions." << endl
         << endl
         << " -d<serial>;<path>: Will update the firmware for a device with the serial number specified by" << endl
         << "             <serial>. Pass '*' as a wildcard. Adding ';<path>' to the parameter defines an optional path to the file to upload to the device!" << endl
         << "             EXAMPLE: FirmwareUpgrade -dBF* // will update the firmware for every device with a serial number starting with 'BF'" << endl
         << "                      FirmwareUpgrade -dGX100000;bla.bin // will update the firmware for the device with a serial number 'GX100000' with the file bla.bin" << endl
         << "             Please make sure that this is the right file for this device! Suitable files are listed in the packageDescription.xml" << endl
         << "             UPLOADING AN INCORRECT FILE MIGHT RESULT IN THE DEVICE TO BECOME UNUSABLE!" << endl
         << endl
         << " -sel: Will prompt the user to select a device to update" << endl
         << endl
         << " -help: Will print this help." << endl;
}

//-----------------------------------------------------------------------------
int getFileSize( FILE* fp )
//-----------------------------------------------------------------------------
{
    const int cur = ftell( fp );
    fseek( fp, 0, SEEK_END );
    const int size = ftell( fp );
    fseek( fp, cur, SEEK_SET );
    return size;
}

//-----------------------------------------------------------------------------
TUpdateResult uploadFirmwareFile( Device* pDev, mvIMPACT::acquire::GenICam::ODevFileStream& uploadFile, const char* pBuf, const size_t bufSize, const size_t fileOperationBlockSize )
//-----------------------------------------------------------------------------
{
    uploadFile.open( pDev, "DeviceFirmware" );
    if( !uploadFile.fail() )
    {
        size_t bytesWritten = 0;
        size_t bytesToWriteTotal = bufSize;
        while( bytesWritten < bytesToWriteTotal )
        {
            const size_t bytesToWrite = ( ( bytesWritten + fileOperationBlockSize ) <= bytesToWriteTotal ) ? fileOperationBlockSize : bytesToWriteTotal - bytesWritten;
            uploadFile.write( pBuf + bytesWritten, bytesToWrite );
            bytesWritten += bytesToWrite;
            cout << "Applying Firmware Update(" << std::setw( 8 ) << std::setfill( '0' ) << bytesWritten << '/' << std::setw( 8 ) << std::setfill( '0' ) << bytesToWriteTotal << " bytes written)\r";
        }
        if( !uploadFile.good() )
        {
            cout << "Failed to upload firmware file to device " << pDev->serial.read() << "." << endl;
            uploadFile.close();
            return urFileIOError;
        }
    }
    else
    {
        cout << "Failed to open firmware file on device " << pDev->serial.read() << "." << endl;
        return urFileIOError;
    }
    return urOperationSuccessful;
}

//-----------------------------------------------------------------------------
bool waitForDeviceToReappear( Device* pDev )
//-----------------------------------------------------------------------------
{
    DeviceManager devMgr;
#if (defined( CPP_STANDARD_VERSION ) && ( CPP_STANDARD_VERSION >= 11 ))
    static const double maxSleepTime_s = 45.;
    auto start = chrono::high_resolution_clock::now();
    while( ( chrono::duration_cast<chrono::duration<double>>( chrono::high_resolution_clock::now() - start ).count() ) < maxSleepTime_s )
    {
        this_thread::sleep_for( chrono::milliseconds( 1000 ) );
        devMgr.updateDeviceList();
        if( pDev->state.read() == dsPresent )
        {
            return true;
        }
    }
    cout << "Device " << pDev->serial.read() << "(" << pDev->product.read() << ") did not re-appear within " << maxSleepTime_s << " seconds... Giving up now!" << endl;
#else
    // burn some time to give the device a chance to disappear
    // so that the Heartbeat Thread is able to recognize the missing device
    devMgr.updateDeviceList();
    // now the device has disappeared and we can wait for it to reappear
    static const unsigned int maxNumberOfUpdateDeviceListCalls = 45;
    unsigned int cnt = 1;
    while( ( pDev->state.read() != dsPresent ) && ( cnt <= maxNumberOfUpdateDeviceListCalls ) )
    {
        devMgr.updateDeviceList();
        ++cnt;
        if( pDev->state.read() == dsPresent )
        {
            return true;
        }
    }
    cout << "Device " << pDev->serial.read() << "(" << pDev->product.read() << ") did not re-appear within " << maxNumberOfUpdateDeviceListCalls << " calls to 'updateDeviceList'... Giving up now!" << endl;
#endif // #if (defined( CPP_STANDARD_VERSION ) && ( CPP_STANDARD_VERSION >= 11 ))
    return false;
}

//-----------------------------------------------------------------------------
bool rebootDevice( Device* pDev )
//-----------------------------------------------------------------------------
{
    mvIMPACT::acquire::GenICam::DeviceControl dc( pDev );
    if( dc.deviceReset.isValid() )
    {
        const int deviceResetResult = dc.deviceReset.call();
        if( static_cast<TDMR_ERROR>( deviceResetResult ) != DMR_NO_ERROR )
        {
            cout << "Update successful but resetting device failed. Please disconnect and reconnect device " << pDev->serial.read() << " now to activate the new firmware. Error reported from driver: " << deviceResetResult << "(" << ImpactAcquireException::getErrorCodeAsString( deviceResetResult ) << ")" << endl;
        }
    }
    else
    {
        cout << "Update successful. Please disconnect and reconnect device " << pDev->serial.read() << " now to activate the new firmware." << endl
             << "Press [ENTER] to continue!" << endl;
        cin.get();
    }

    pDev->close();
    return waitForDeviceToReappear( pDev );
}

//-----------------------------------------------------------------------------
TUpdateResult doFirmwareUpdate_BlueFOX3( Device* pDev, const std::string& serial, const char* pBuf, const size_t bufSize )
//-----------------------------------------------------------------------------
{
    pDev->interfaceLayout.write( dilGenICam );
    {
        // switch off automatic update of GenICam features triggered by recommended polling times in the GenICam XML file
        SystemSettingsGenTL ss( pDev );
        ss.featurePollingEnable.write( bFalse );
    }

    mvIMPACT::acquire::GenICam::FileAccessControl fac( pDev );

    cout << "Trying to erase flash on device " << serial << "! This will take a while!" << endl;
    fac.fileSelector.writeS( "DeviceFirmware" );
    fac.fileOperationSelector.writeS( "Delete" );
    int result = fac.fileOperationExecute.call();
    if( result == DMR_NO_ERROR )
    {
        cout << "Successfully deleted firmware on device!" << endl;
    }
    else
    {
        cout << "Failed to erase flash memory for firmware image on device(" << ImpactAcquireException::getErrorCodeAsString( result ) << "(" << result << "))." << endl;
        return urFileIOError;
    }

    static const size_t fileOperationBlockSize = 4096;
    {
        // download the firmware image
        mvIMPACT::acquire::GenICam::ODevFileStream uploadFile;
        const TUpdateResult uploadResult = uploadFirmwareFile( pDev, uploadFile, pBuf, bufSize, fileOperationBlockSize );
        if( uploadResult != urOperationSuccessful )
        {
            return uploadResult;
        }

        cout << "Successfully uploaded firmware image! Closing the file again. This again will take some time!" << endl;
        uploadFile.close();
        if( !uploadFile.good() )
        {
            cout << "Firmware verification for device %s failed. Please power-cycle the device now, update the device list and re-try to update the firmware." << endl;
            uploadFile.close();
            return urFileIOError;
        }
    }
    return rebootDevice( pDev ) ? urOperationSuccessful : urDeviceAccessError;
}

//-----------------------------------------------------------------------------
void updateFirmwareBlueFOX( Device* pDev )
//-----------------------------------------------------------------------------
{
    cout << "The firmware of device " << pDev->serial.read() << " is currently " << pDev->firmwareVersion.readS() << "." << endl;
    cout << "It will now be updated. During this time(approx. 30 sec.) the application will not react. Please be patient." << endl;
    int result = pDev->updateFirmware();
    if( result == DMR_FEATURE_NOT_AVAILABLE )
    {
        cout << "This device doesn't support firmware updates." << endl;
    }
    else if( result != DMR_NO_ERROR )
    {
        cout << "An error occurred: " << ImpactAcquireException::getErrorCodeAsString( result ) << ". (please refer to the manual for this error code)." << endl;
    }
    else
    {
        cout << "Firmware update done. Result: " << pDev->HWUpdateResult.readS() << endl;
        if( pDev->HWUpdateResult.read() == urUpdateFWOK )
        {
            cout << "Update successful." << endl;
            if( pDev->family.read() == "mvBlueFOX" )
            {
                cout << "Please disconnect and reconnect the device now to activate the new firmware." << endl;
            }
        }
    }
}

//-----------------------------------------------------------------------------
TUpdateResult checkForOutdatedBootProgrammerBlueFOX3( Device* pDev )
//-----------------------------------------------------------------------------
{
    DeviceComponentLocator locator( pDev->hDev() );
    Method updateBootProgrammer;
    locator.bindComponent( updateBootProgrammer, "UpdateBootProgrammer@i" );
    if( updateBootProgrammer.isValid() )
    {
        cout << "An outdated boot programmer has been detected running on device " << pDev->serial.read() << "(" << pDev->product.read() << "). In order to guarantee smooth device operation it is recommended to update this boot programmer now. Once this update did succeed the normal firmware update procedure will continue." << endl
             << endl
             << "Not updating the boot programmer will end the whole firmware update procedure." << endl
             << endl
             << "Press 'y' to proceed" << endl;

        char proceedCommand;
        std::cin >> proceedCommand;
        // remove the '\n' from the stream
        std::cin.get();
        /// \todo ACQ-3182 Testen!!!
        switch( proceedCommand )
        {
        case 'y':
        case 'Y':
            break;
        default:
            cout << "Boot programmer update canceled for device " << pDev->serial.read() << "(" << pDev->product.read() << ")!" << endl;
            return urOperationCanceled;
        }

        cout << "Will update the boot programmer now! This will take some time!" << endl;
        const int result = updateBootProgrammer.call();
        if( result == DMR_NO_ERROR )
        {
            waitForDeviceToReappear( pDev );
        }
        else
        {
            cout << "Boot programmer update for device " << pDev->serial.read() << "(" << pDev->product.read() << ") failed with the following error: " << result << "(" << ImpactAcquireException::getErrorCodeAsString( result ) << ")." << endl;
            return urDeviceAccessError;
        }
    }
    return urOperationSuccessful;
}

//-----------------------------------------------------------------------------
void updateFirmwareBlueFOX3( Device* pDev, const string& firmwarePath )
//-----------------------------------------------------------------------------
{
    std::string serial( pDev->serial.read() );

    if( checkForOutdatedBootProgrammerBlueFOX3( pDev ) != urOperationSuccessful )
    {
        return;
    }

    TUpdateResult result = urFileIOError;
    FILE* fp = mv_fopen_s( firmwarePath.c_str(), "rb" );
    if( fp == 0 )
    {
        cout << "*** Error: Failed to open file '" << firmwarePath << "'" << endl;
        return;
    }

    auto_array_ptr<char> pFileBuffer( getFileSize( fp ) );
    memset( pFileBuffer, 0, pFileBuffer.parCnt() );
    const bool boFailure = ( fread( pFileBuffer, pFileBuffer.parCnt(), 1, fp ) < 1 ) && ( feof( fp ) == 0 );
    fclose( fp );
    if( boFailure )
    {
        cout << "*** Error: Failed to read from file '" << firmwarePath << "'" << endl;
        return;
    }

    cout << "The firmware of device " << pDev->serial.read() << "(" << pDev->product.read() << ", current firmware version: " << pDev->firmwareVersion.readS() << ") is going to be update NOW!" << endl
         << "Selected firmware: '" << firmwarePath << "'" << endl
         << "Are you sure? (Press 'y' to confirm): ";
    char confirm = 'n';
    std::cin >> confirm;
    // remove the '\n' from the stream
    std::cin.get();
    if( ( confirm != 'y' ) && ( confirm != 'Y' ) )
    {
        cout << "Update canceled by user!" << endl;
        return;
    }

    cout << "The firmware of device " << pDev->serial.read() << " will now be updated. During this time(approx. 90 sec.) the application will not react. Please be patient." << endl;
    try
    {
        result = doFirmwareUpdate_BlueFOX3( pDev, serial, pFileBuffer.get(), pFileBuffer.parCnt() );
    }
    catch( const ImpactAcquireException& e )
    {
        cout << "Failed to update device " << serial << "(" << pDev->product.read() << ")(" << e.getErrorString() << ", " << e.getErrorCodeAsString() << ")." << endl;
        result = urDeviceAccessError;
    }

    if( result == urOperationSuccessful )
    {
        cout << "The Firmware of device " << serial << " has been updated. The new Firmware(" << pDev->firmwareVersion.readS() << ") is active now and the device can be used again by other applications." << endl;
    }
    else
    {
        cout << "Something went wrong while updating the firmware of device " << serial << "! Please check manually!" << endl;
    }

    if( pDev->isOpen() )
    {
        pDev->close();
    }
}

//-----------------------------------------------------------------------------
void updateFirmwareHYPERION( Device* pDev, const string& firmwarePath )
//-----------------------------------------------------------------------------
{
    if( !firmwarePath.empty() )
    {
        const string extension( ".rpd" );
        if( firmwarePath.find( extension ) == firmwarePath.length() - extension.length() )
        {
            FILE* fp = mv_fopen_s( firmwarePath.c_str(), "rb" );
            if( fp != 0 )
            {
                auto_array_ptr<unsigned char> pFileBuffer( getFileSize( fp ) );
                memset( pFileBuffer, 0, pFileBuffer.parCnt() );
                if( ( fread( pFileBuffer, pFileBuffer.parCnt(), 1, fp ) < 1 ) && ( feof( fp ) == 0 ) )
                {
                    cout << "*** Error: Failed to read from file '" << firmwarePath << "'" << endl;
                }
                else
                {
                    cout << "The firmware of device " << pDev->serial.read() << " will now be updated. During this time(approx. 90 sec.) the application will not react. Please be patient." << endl;
                    ostringstream logMsg;
                    FlashUpdate_mvHYPERION( pDev, pFileBuffer, static_cast<unsigned long>( pFileBuffer.parCnt() ), logMsg );
                    cout << logMsg.str();
                }
                fclose( fp );
            }
            else
            {
                cout << "*** Error: Failed to open file '" << firmwarePath << "'" << endl;
            }
        }
        else
        {
            cout << "*** Error: Invalid file type for updating mvHYPERION device " << pDev->serial.read() << " specified('" << firmwarePath << "', extension expected: '" << extension << "')" << endl;
        }
    }
    else
    {
        cout << "*** Error: Empty path for updating mvHYPERION device " << pDev->serial.read() << " specified" << endl;
    }
}

//-----------------------------------------------------------------------------
int main( int argc, char* argv[] )
//-----------------------------------------------------------------------------
{
    DeviceManager devMgr;
    const unsigned int devCnt = devMgr.deviceCount();
    if( devCnt == 0 )
    {
        cout << "*** Error: No MATRIX VISION device found! Unable to continue!" << endl;
        return 1;
    }

    cout << "Have found " << devCnt << " devices on this platform!" << endl
         << "Please note that this application will only work for 'mvBlueFOX', 'mvBlueFOX3' and 'mvHYPERION' devices" << endl
         << endl
         << "Detected devices:" << endl;
    for( unsigned int i = 0; i < devCnt; i++ )
    {
        cout << " [" << i << "] " << devMgr[i]->serial.read() << "(" << devMgr[i]->product.read() << ", Firmware version: " << devMgr[i]->firmwareVersion.readS() << ")" << endl;
    }
    cout << endl;

    if( argc == 1 )
    {
        int index = 0;
        Device* pDev = 0;
        while( ( pDev = devMgr.getDeviceByFamily( "mvBlueFOX", index ) ) != 0 )
        {
            updateFirmwareBlueFOX( pDev );
            ++index;
        }
    }
    else
    {
        vector<UpdateInformation> devicesToUpdate;
        for( int i = 1; i < argc; i++ )
        {
            const string arg( argv[i] );
            if( arg == "-sel" )
            {
                Device* pDev = getDeviceFromUserInput( devMgr );
                if( !pDev )
                {
                    cout << "Invalid device selection! Will ignore this parameter!" << endl;
                    continue;
                }
                devicesToUpdate.push_back( UpdateInformation( pDev ) );

            }
            else if( arg.find( "-d" ) == 0 )
            {
                const string token( arg.substr( 2 ) );
                const string::size_type separator = token.find_first_of( ";" );
                const string serial( token.substr( 0, separator ) );
                const string firmwarePath( ( separator != string::npos ) ? token.substr( separator + 1 ) : "" );
                int index = 0;
                Device* pDev = 0;
                while( ( pDev = devMgr.getDeviceBySerial( serial, index ) ) != 0 )
                {
                    devicesToUpdate.push_back( UpdateInformation( pDev, firmwarePath ) );
                    ++index;
                }
            }
            else if( arg == "-help" )
            {
                printHelp();
            }
            else
            {
                cout << "Invalid input parameter: '" << arg << "'" << endl
                     << endl;
                printHelp();
            }
        }

        const vector<Device*>::size_type devicesToUpdateCount = devicesToUpdate.size();
        for( vector<Device*>::size_type i = 0; i < devicesToUpdateCount; i++ )
        {
            Device* pDev = devicesToUpdate[i].pDev_;
            const string family( pDev->family.read() );
            if( family == "mvBlueFOX" )
            {
                updateFirmwareBlueFOX( pDev );
            }
            else if( family == "mvHYPERION" )
            {
                updateFirmwareHYPERION( pDev, devicesToUpdate[i].firmwareFilePath_ );
            }
            else if( family == "mvBlueFOX3" )
            {
                updateFirmwareBlueFOX3( pDev, devicesToUpdate[i].firmwareFilePath_ );
            }
            else
            {
                cout << "*** Error: This application is meant for 'mvBlueFOX', 'mvBlueFOX3' and 'mvHYPERION' devices only. This(" << pDev->serial.read() << ")is a '" << family << "' device." << endl;
            }
        }
    }

    return 0;
}
