#if !defined(linux) && !defined(__linux) && !defined(__linux__)
#   error Sorry! Linux only code!
#endif // #if !defined(linux) && !defined(__linux) && !defined(__linux__)
#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <apps/Common/exampleHelper.h>
#include <mvIMPACT_CPP/mvIMPACT_acquire.h>
#include <mvIMPACT_CPP/mvIMPACT_acquire_GenICam.h>

#ifdef MALLOC_TRACE
#   include <mcheck.h>
#endif  // MALLOC_TRACE

#define PRESS_A_KEY_AND_RETURN          \
    cout << "Press a key..." << endl;   \
    getchar(); \
    return 0;

using namespace std;
using namespace mvIMPACT::acquire;

static bool s_boTerminated = false;

//-----------------------------------------------------------------------------
void liveLoop( Device* pDev, const string& settingName, bool boSingleShotMode )
//-----------------------------------------------------------------------------
{
    // establish access to the statistic properties
    Statistics statistics( pDev );
    // create an interface to the device found
    FunctionInterface fi( pDev );

    if( !settingName.empty() )
    {
        cout << "Trying to load setting " << settingName << "..." << endl;
        int result = fi.loadSetting( settingName );
        if( result != DMR_NO_ERROR )
        {
            cout << "loadSetting( \"" << settingName << "\" ); call failed: " << ImpactAcquireException::getErrorCodeAsString( result ) << endl;
        }
    }

    // If this is color sensor, we will NOT convert the Bayer data into a RGB image because this would
    // introduce additional CPU load and we might run on a tiny embedded system!
    ImageProcessing ip( pDev );
    if( ip.colorProcessing.isValid() )
    {
        ip.colorProcessing.write( cpmRaw );
    }

    SystemSettings ss( pDev );

    TDMR_ERROR result = DMR_NO_ERROR;
    int requestCount = 0;

    if( boSingleShotMode )
    {
        fi.imageRequestSingle();
        ++requestCount;
    }
    else
    {
        // Send all requests to the capture queue. There can be more than 1 queue for some devices, but for this sample
        // we will work with the default capture queue. If a device supports more than one capture or result
        // queue, this will be stated in the manual. If nothing is mentioned about it, the device supports one
        // queue only. This loop will send all requests currently available to the driver. To modify the number of requests
        // use the property mvIMPACT::acquire::SystemSettings::requestCount at runtime or the property
        // mvIMPACT::acquire::Device::defaultRequestCount BEFORE opening the device.
        while( ( result = static_cast<TDMR_ERROR>( fi.imageRequestSingle() ) ) == DMR_NO_ERROR )
        {
            ++requestCount;
        }
    }

    if( ( result != DMR_NO_ERROR ) && ( result != DEV_NO_FREE_REQUEST_AVAILABLE ) )
    {
        cout << "'FunctionInterface.imageRequestSingle' returned with an unexpected result: " << result
             << "(" << mvIMPACT::acquire::ImpactAcquireException::getErrorCodeAsString( result ) << ")" << endl;
    }

    cout << requestCount << " buffer" << ( ( requestCount > 1 ) ? "s" : "" ) << " requested";
    if( ss.requestCount.hasMaxValue() )
    {
        cout << ", maximum request count: " << ss.requestCount.getMaxValue();
    }
    cout << endl;
    cout << "Press <<ENTER>> to end the application!!" << endl;

    manuallyStartAcquisitionIfNeeded( pDev, fi );
    // run thread loop
    const unsigned int timeout_ms = 10000;   // USB 1.1 on an embedded system needs a large timeout for the first image
    unsigned int cnt = 0;
    while( !s_boTerminated )
    {
        // wait for results from the default capture queue
        const int requestNr = fi.imageRequestWaitFor( timeout_ms );
        if( fi.isRequestNrValid( requestNr ) )
        {
            Request* pRequest = fi.getRequest( requestNr );
            if( pRequest->isOK() )
            {
                ++cnt;
                // here we can display some statistical information every 100th image
                if( cnt % 10 == 0 )
                {
                    cout << cnt << ": Info from " << pDev->serial.read()
                         << ": " << statistics.framesPerSecond.name() << ": " << statistics.framesPerSecond.readS()
                         << ", " << statistics.errorCount.name() << ": " << statistics.errorCount.readS()
                         << ", " << statistics.captureTime_s.name() << ": " << statistics.captureTime_s.readS() << " Image count: " << cnt
                         << " (dimensions: " << pRequest->imageWidth.read() << "x" << pRequest->imageHeight.read() << ", format: " << pRequest->imagePixelFormat.readS();
                    cout << "), line pitch: " << pRequest->imageLinePitch.read() << endl;
                }
            }
            else
            {
                cout << "Error: " << pRequest->requestResult.readS() << endl;
            }

            // this image has been displayed thus the buffer is no longer needed...
            pRequest->unlock();
            if( !boSingleShotMode )
            {
                // send a new image request into the capture queue
                fi.imageRequestSingle();
            }
        }
        //else
        //{
        // Please note that slow systems or interface technologies in combination with high resolution sensors
        // might need more time to transmit an image than the timeout value which has been passed to imageRequestWaitFor().
        // If this is the case simply wait multiple times OR increase the timeout(not recommended as usually not necessary
        // and potentially makes the capture thread less responsive) and rebuild this application.
        // Once the device is configured for triggered image acquisition and the timeout elapsed before
        // the device has been triggered this might happen as well.
        // The return code would be -2119(DEV_WAIT_FOR_REQUEST_FAILED) in that case, the documentation will provide
        // additional information under TDMR_ERROR in the interface reference.
        // If waiting with an infinite timeout(-1) it will be necessary to call 'imageRequestReset' from another thread
        // to force 'imageRequestWaitFor' to return when no data is coming from the device/can be captured.
        // cout << "imageRequestWaitFor failed (" << requestNr << ", " << ImpactAcquireException::getErrorCodeAsString( requestNr ) << ")"
        //   << ", timeout value too small?" << endl;
        //}

        if( boSingleShotMode || ( waitForInput( 0, STDOUT_FILENO ) != 0 ) )
        {
            s_boTerminated = true;
        }
    }


    if( !boSingleShotMode )
    {
        manuallyStopAcquisitionIfNeeded( pDev, fi );
    }

    // clear all queues
    fi.imageRequestReset( 0, 0 );
}

//-----------------------------------------------------------------------------
int main( int argc, char* argv[] )
//-----------------------------------------------------------------------------
{
#ifdef MALLOC_TRACE
    mtrace();
#endif  // MALLOC_TRACE
    cout << " ++ starting application...." << endl;

    string settingName;
    int width = -1;
    int height = -1;
    string pixelFormat;
    string acquisitionMode;
    string deviceSerial;
    int defaultRequestCount = -1;
    for( int i = 1; i < argc; i++ )
    {
        const string arg( argv[i] );
        if( arg.find( "-a" ) == 0 )
        {
            acquisitionMode = arg.substr( 2 );
        }
        else if( arg.find( "-drc" ) == 0 )
        {
            defaultRequestCount = atoi( arg.substr( 4 ).c_str() );
        }
        else if( arg.find( "-h" ) == 0 )
        {
            height = atoi( arg.substr( 2 ).c_str() );
        }
        else if( arg.find( "-p" ) == 0 )
        {
            pixelFormat = arg.substr( 2 );
        }
        else if( arg.find( "-s" ) == 0 )
        {
            deviceSerial = arg.substr( 2 );
        }
        else if( arg.find( "-w" ) == 0 )
        {
            width = atoi( arg.substr( 2 ).c_str() );
        }
        else
        {
            // try to load this setting later on...
            settingName = string( argv[1] );
        }
    }

    if( argc <= 1 )
    {
        cout << "Available command line parameters:" << endl
             << endl
             << "-a<mode> to set the acquisition mode" << endl
             << "-h<height> to set the AOI width" << endl
             << "-p<pixelFormat> to set the pixel format" << endl
             << "-s<serialNumber> to pre-select a certain device. If this device can be found no further user interaction is needed" << endl
             << "-w<width> to set the AOI width" << endl
             << "-drc<bufferCount> to specify the default request count" << endl
             << "any other string will be interpreted as a name of a setting to load" << endl;
    }

    DeviceManager devMgr;
    Device* pDev = 0;
    bool bGoOn = true;
    while( bGoOn )
    {
        if( !deviceSerial.empty() )
        {
            pDev = devMgr.getDeviceBySerial( deviceSerial );
            if( pDev )
            {
                // if this device offers the 'GenICam' interface switch it on, as this will
                // allow are better control over GenICam compliant devices
                conditionalSetProperty( pDev->interfaceLayout, dilGenICam );
                // if this device offers a user defined acquisition start/stop behaviour
                // enable it as this allows finer control about the streaming behaviour
                conditionalSetProperty( pDev->acquisitionStartStopBehaviour, assbUser );
            }
        }
        if( !pDev )
        {
            // this will automatically set the interface layout etc. to the values from the branch above
            pDev = getDeviceFromUserInput( devMgr );
        }
        if( pDev )
        {
            deviceSerial = pDev->serial.read();
            cout << "Initialising device: " << pDev->serial.read() << ". This might take some time..." << endl
                 << "Using interface layout '" << pDev->interfaceLayout.readS() << "'." << endl;
            try
            {
                if( defaultRequestCount > 0 )
                {
                    cout << "Setting default request count to " << defaultRequestCount << endl;
                    pDev->defaultRequestCount.write( defaultRequestCount );
                }
                pDev->open();
                switch( pDev->interfaceLayout.read() )
                {
                case dilGenICam:
                    {
                        mvIMPACT::acquire::GenICam::ImageFormatControl ifc( pDev );
                        mvIMPACT::acquire::GenICam::AcquisitionControl ac( pDev );
                        if( width > 0 )
                        {
                            ifc.width.write( width );
                        }
                        if( height > 0 )
                        {
                            ifc.height.write( height );
                        }
                        if( !pixelFormat.empty() )
                        {
                            ifc.pixelFormat.writeS( pixelFormat );
                        }
                        if( !acquisitionMode.empty() )
                        {
                            ac.acquisitionMode.writeS( acquisitionMode );
                        }
                        acquisitionMode = ac.acquisitionMode.readS();
                        cout << "Device set up to " << ifc.pixelFormat.readS() << " " << ifc.width.read() << "x" << ifc.height.read() << endl;
                    }
                    break;
                case dilDeviceSpecific:
                    {
                        CameraSettingsBase cs( pDev );
                        if( width > 0 )
                        {
                            cs.aoiWidth.write( width );
                        }
                        if( height > 0 )
                        {
                            cs.aoiHeight.write( height );
                        }
                        cout << "Device set up to " << cs.aoiWidth.read() << "x" << cs.aoiHeight.read() << endl;
                    }
                    break;
                default:
                    break;
                }
                // start the execution of the 'live' loop.
                cout << "starting live loop" << endl;
                liveLoop( pDev, settingName, acquisitionMode == "SingleFrame" );
                cout << "finished live loop" << endl;
                pDev->close();
            }
            catch( const ImpactAcquireException& e )
            {
                // this e.g. might happen if the same device is already opened in another process...
                cout << "*** " << __FUNCTION__ << " - An error occurred while opening the device " << pDev->serial.read()
                     << "(error code: " << e.getErrorCode() << ", " << e.getErrorCodeAsString() << "). Press any key to end the application..." << endl;
            }
        }
        else
        {
            cout << "Unable to get device!";
            break;
        }

        if( waitForInput( 0, STDOUT_FILENO ) != 0 )
        {
            break;
        }
    }
    cout << " -- ending application...." << endl;
    return 0;
}
