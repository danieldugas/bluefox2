#ifdef _MSC_VER         // is Microsoft compiler?
#   if _MSC_VER < 1300  // is 'old' VC 6 compiler?
#       pragma warning( disable : 4786 ) // 'identifier was truncated to '255' characters in the debug information'
#   endif // #if _MSC_VER < 1300
#endif // #ifdef _MSC_VER
#include <iostream>
#include <apps/Common/exampleHelper.h>
#include <mvIMPACT_CPP/mvIMPACT_acquire_GenICam.h>
#include <cassert>
#include <iomanip>
#include <limits>
#include <common/crt/mvstdio.h>
#if defined(linux) || defined(__linux) || defined(__linux__)
#   include <sys/time.h>
#   include <unistd.h>
#else
#   include <windows.h>
#   undef min // Otherwise we can't work with the 'numeric_limits' template here as Windows defines a macro 'min'
#   undef max // Otherwise we can't work with the 'numeric_limits' template here as Windows defines a macro 'max'
#   include <process.h>
#   include <mvDisplay/Include/mvIMPACT_acquire_display.h>
using namespace mvIMPACT::acquire::display;
#endif // #if defined(linux) || defined(__linux) || defined(__linux__)

using namespace mvIMPACT::acquire;
using namespace std;

//=============================================================================
//================= static variables ==========================================
//=============================================================================
static bool s_boTerminated = false;

//=============================================================================
//================= function declarations =====================================
//=============================================================================
static void checkedMethodCall( Device* pDev, Method& method );
static double getMinimalExposureTime( void );
static int64_type getOverallSequenceLength( void );

//=============================================================================
//================= time measurement specific stuff ===========================
//=============================================================================
#if defined(linux) || defined(__linux) || defined(__linux__)
//-----------------------------------------------------------------------------
/// \brief Provides platform independent clock and time measurement functions
class CTime
//-----------------------------------------------------------------------------
{
    struct timespec m_tsStart;
    struct timespec m_tsEnd;
    clockid_t m_ClockId;
    // Returns a time difference in milli-seconds
    long diffTime( void )
    {
        clock_gettime( m_ClockId, &m_tsEnd );
        static struct timespec tsDiff;
        tsDiff.tv_sec  = m_tsEnd.tv_sec  - m_tsStart.tv_sec ;
        tsDiff.tv_nsec = m_tsEnd.tv_nsec - m_tsStart.tv_nsec ;

        if( tsDiff.tv_nsec < 0 ) // handle 1 sec borrow
        {
            tsDiff.tv_nsec += 1000000000 ; // usec / sec
            tsDiff.tv_sec -= 1;
        }
        return tsDiff.tv_sec * 1000 + tsDiff.tv_nsec / 1000000;
    }
public:
    explicit CTime() : m_tsStart(), m_tsEnd(), m_ClockId( CLOCK_REALTIME )
    {
#ifdef USE_MONOTONIC_CLOCK
        struct timespec ts;
        m_ClockId = ( clock_gettime( CLOCK_MONOTONIC, &ts ) == 0 ) ? CLOCK_MONOTONIC : CLOCK_REALTIME;
#endif // #ifdef USE_MONOTONIC_CLOCK
        start();
    }
    void start( void )
    {
        clock_gettime( m_ClockId, &m_tsStart );
    }
    double elapsed( void )
    {
        return static_cast<double>( diffTime() / 1000. ); // in sec
    }
    double restart( void )
    {
        double result = elapsed();
        m_tsStart = m_tsEnd;
        return result;
    }
};
#else
//-----------------------------------------------------------------------------
class CTime
//-----------------------------------------------------------------------------
{
    LARGE_INTEGER frequency_;
    LARGE_INTEGER start_;
    LARGE_INTEGER end_;
public:
    explicit CTime()
    {
        QueryPerformanceFrequency( &frequency_ );
        QueryPerformanceCounter( &start_ );
    }
    void start( void )
    {
        QueryPerformanceCounter( &( start_ ) );
    }
    double elapsed( void )
    {
        QueryPerformanceCounter( &end_ );
        return static_cast<double>( end_.QuadPart - start_.QuadPart ) / frequency_.QuadPart;
    }
    double restart( void )
    {
        QueryPerformanceCounter( &end_ );
        double result = static_cast<double>( end_.QuadPart - start_.QuadPart ) / frequency_.QuadPart;
        start_ = end_;
        return result;
    }
};
#endif // #if defined(linux) || defined(__linux) || defined(__linux__)

//=============================================================================
//================= sequencer specific stuff ==================================
//=============================================================================
//-----------------------------------------------------------------------------
struct SequencerSetParameter
//-----------------------------------------------------------------------------
{
    const int64_type setNr_;
    const int64_type sequencerSetNext_;
    const double exposureTime_us_;
    const int64_type frameCount_;
    const int64_type horizontalBinningOrDecimation_;
    const int64_type verticalBinningOrDecimation_;
    double expectedFrameRate_;
    explicit SequencerSetParameter( const int64_type setNr, const int64_type sequencerSetNext, const double exposureTime_us, const int64_type frameCount, const int64_type horizontalBinningOrDecimation, const int64_type verticalBinningOrDecimation ) :
        setNr_( setNr ), sequencerSetNext_( sequencerSetNext ), exposureTime_us_( exposureTime_us ), frameCount_( frameCount ), horizontalBinningOrDecimation_( horizontalBinningOrDecimation ), verticalBinningOrDecimation_( verticalBinningOrDecimation ), expectedFrameRate_( 0.0 )
    {
    }
};

//-----------------------------------------------------------------------------
struct ThreadParameter
//-----------------------------------------------------------------------------
{
    Device* pDev;
    FunctionInterface fi;
    Statistics statistics;
    GenICam::AcquisitionControl ac;
    GenICam::ImageFormatControl ifc;
    GenICam::ChunkDataControl cdc;
    GenICam::CounterAndTimerControl ctc;
    GenICam::SequencerControl sc;
#if defined(linux) || defined(__linux) || defined(__linux__)
    explicit ThreadParameter( Device* p ) : pDev( p ), fi( pDev ), statistics( pDev ), ac( pDev ), ifc( pDev ), cdc( pDev ), ctc( pDev ), sc( pDev )
    {
    }
#else
    ImageDisplayWindow  displayWindow;
    explicit ThreadParameter( Device* p, const string& windowTitle ) : pDev( p ), fi( pDev ), statistics( pDev ), ac( pDev ), ifc( pDev ), cdc( pDev ), ctc( pDev ), sc( pDev ), displayWindow( windowTitle )
    {
    }
#endif // #if defined(linux) || defined(__linux) || defined(__linux__)
};

//-----------------------------------------------------------------------------
static SequencerSetParameter s_SequencerData[] =
//-----------------------------------------------------------------------------
{
#define USE_EXTENDED_SEQUENCER
#ifdef USE_EXTENDED_SEQUENCER
    SequencerSetParameter( 0, 1,  1000.,  5, 2, 2 ), // Set 0: Capture 5 frames Exposure = 1000 us HBinning = 2 VBinning = 2, then jump to set 1
    SequencerSetParameter( 1, 2,  2000., 16, 2, 2 ), // Set 1: Capture 16 frames Exposure = 2000 us HBinning = 2 VBinning = 2, then jump to set 2
    SequencerSetParameter( 2, 3,  2000.,  8, 1, 1 ), // Set 2: Capture 8 frames Exposure = 2000 us HBinning = 1 VBinning = 1, then jump to set 3
    SequencerSetParameter( 3, 4, 10000., 16, 1, 1 ), // Set 3: Capture 16 frames Exposure = 10000 us HBinning = 1 VBinning = 1, then jump to set 4
    SequencerSetParameter( 4, 5,  5000.,  5, 1, 1 ), // Set 4: Capture 5 frames Exposure = 5000 us HBinning = 1 VBinning = 1, then jump to set 5
    SequencerSetParameter( 5, 6,  2000.,  5, 2, 2 ), // Set 5: Capture 5 frames Exposure = 2000 us HBinning = 2 VBinning = 2, then jump to set 6
    SequencerSetParameter( 6, 7,  1000., 16, 2, 2 ), // Set 6: Capture 16 frames Exposure = 1000 us HBinning = 2 VBinning = 2, then jump to set 7
    SequencerSetParameter( 7, 8,  5000.,  8, 1, 1 ), // Set 7: Capture 8 frames Exposure = 5000 us HBinning = 1 VBinning = 1, then jump to set 8
    SequencerSetParameter( 8, 9, 10000., 16, 1, 1 ), // Set 8: Capture 16 frames Exposure = 10000 us HBinning = 1 VBinning = 1, then jump to set 9
    SequencerSetParameter( 9, 0, 15000.,  5, 1, 1 )  // Set 9: Capture 5 frames Exposure = 15000 us HBinning = 1 VBinning = 1, then jump back to set 0*/
#else
    SequencerSetParameter( 0, 1,  1000.,  5, 1, 1 ), // Set 0: Capture  5 frames with an exposure time of 1000  us, then jump to set 1
    SequencerSetParameter( 1, 2, 15000., 40, 1, 1 ), // Set 1: Capture 40 frames with an exposure time of 15000 us, then jump to set 2
    SequencerSetParameter( 2, 3,  2000., 20, 1, 1 ), // Set 2: Capture 20 frames with an exposure time of 2000  us, then jump to set 3
    SequencerSetParameter( 3, 4, 10000., 40, 1, 1 ), // Set 3: Capture 40 frames with an exposure time of 10000 us, then jump to set 4
    SequencerSetParameter( 4, 0,  5000.,  5, 1, 1 )  // Set 4: Capture  5 frames with an exposure time of 5000  us, then jump back to set 0
#endif
};

//-----------------------------------------------------------------------------
// Configures all the stuff that needs to be done only once. All the stuff related
// to setting up the actual sequencer could be called multiple times whenever an
// application gets re-configured. This is not the case here, but the code has been
// split in order to logically group what belongs together.
//
// Whenever 'conditionalSetEnumPropertyByString' or 'conditionalSetProperty' is
// not used here the stuff MUST succeed as otherwise when the device doesn't allow
// this feature the whole example does not work!
void configureDevice( Device* pDev )
//-----------------------------------------------------------------------------
{
    try
    {
        // Restore the factory default first in order to make sure nothing is incorrectly configured
        GenICam::UserSetControl usc( pDev );
        conditionalSetEnumPropertyByString( usc.userSetSelector, "Default" );
        const TDMR_ERROR result = static_cast<TDMR_ERROR>( usc.userSetLoad.call() );
        if( result != DMR_NO_ERROR )
        {
            cout << "An error occurred while restoring the factory default for device " << pDev->serial.read()
                 << "(error code: " << ImpactAcquireException::getErrorCodeAsString( result ) << ")." << endl;
        }

        // Auto exposure or an open shutter will not be helpful for this example thus switch it off if possible.
        GenICam::AcquisitionControl acqc( pDev );
        conditionalSetEnumPropertyByString( acqc.exposureMode, "Timed" );
        conditionalSetEnumPropertyByString( acqc.exposureAuto, "Off" );

        // Auto gain will not be helpful for this example either thus switch it off if possible.
        GenICam::AnalogControl ac( pDev );
        if( ac.gainSelector.isValid() )
        {
            // There might be more than a single 'Gain' as a 'GainSelector' is present. Iterate over all
            // 'Gain's that can be configured and switch off every 'Auto' feature detected.
            vector<string> validGainSelectorValues;
            ac.gainSelector.getTranslationDictStrings( validGainSelectorValues );
            const vector<string>::size_type cnt = validGainSelectorValues.size();
            for( vector<string>::size_type i = 0; i < cnt; i++ )
            {
                conditionalSetEnumPropertyByString( ac.gainSelector, validGainSelectorValues[i] );
                conditionalSetEnumPropertyByString( ac.gainAuto, "Off" );
            }
        }
        else
        {
            // There is just a single 'Gain' turn off the 'Auto' feature if supported.
            conditionalSetEnumPropertyByString( ac.gainAuto, "Off" );
        }

        // Chunk mode is needed in order to get back all the information needed to properly check
        // if an image has been taken using the desired parameters.
        GenICam::ChunkDataControl cdc( pDev );
        cdc.chunkModeActive.write( bTrue );

        // The sequencer program will jump from one set to the next after 'CounterDuration'
        // frames have been captured with the current set, thus we need to configure the counter
        // to count 'frames' and to reset itself once 'CounterDuration' has been reached.
        GenICam::CounterAndTimerControl ctc( pDev );
        ctc.counterSelector.writeS( "Counter1" );
        ctc.counterEventSource.writeS( "ExposureEnd" );
        ctc.counterTriggerSource.writeS( "Counter1End" );

        // In order to have at least some kind of external trigger we use a timer running with the
        // highest frequency defined by all sequencer set, thus the reciprocal value of the
        // smallest exposure time defined in the set array.
        ctc.timerSelector.writeS( "Timer1" );
        ctc.timerDuration.write( getMinimalExposureTime() );
        ctc.timerTriggerSource.writeS( "Timer1End" );
        acqc.triggerSelector.writeS( "FrameStart" );
        acqc.triggerMode.writeS( "On" );
        acqc.triggerSource.writeS( "Timer1End" );

        // This is needed to correctly calculate the expected capture time
        conditionalSetEnumPropertyByString( acqc.mvAcquisitionFrameRateLimitMode, "mvDeviceLinkThroughput" );
        conditionalSetEnumPropertyByString( acqc.mvAcquisitionFrameRateEnable, "Off" );

        // As we want to keep ALL images belonging to the full sequence in RAM we need as many requests as
        // there are frames defined by the sequence.
        SystemSettings ss( pDev );
        ss.requestCount.write( static_cast<int>( getOverallSequenceLength() ) );

        // We want to act fast, thus if e.g. Bayer-images arrive in the system do NOT convert them on the fly as depending
        // on the device speed the host system might be too slow deal with the amount of data
        ImageProcessing ip( pDev );
        ip.colorProcessing.write( cpmRaw );
        if( ip.tapSortEnable.isValid() )
        {
            ip.tapSortEnable.write( bFalse );
        }
    }
    catch( const ImpactAcquireException& e )
    {
        // This e.g. might happen if the same device is already opened in another process...
        cout << "An error occurred while configuring the device " << pDev->serial.read()
             << "(error code: " << e.getErrorCodeAsString() << ")." << endl
             << "Press [ENTER] to end the application..." << endl;
        cin.get();
        exit( 1 );
    }
}

//-----------------------------------------------------------------------------
// Configures a single 'SequencerSet' so that 'X' frames are captured using a
// certain exposure time and afterwards another sets will be used.
void configureSequencerSet( ThreadParameter* pThreadParameter, const SequencerSetParameter& ssp )
//-----------------------------------------------------------------------------
{
    pThreadParameter->sc.sequencerSetSelector.write( ssp.setNr_ );
    pThreadParameter->ac.exposureTime.write( ssp.exposureTime_us_ );
    if( pThreadParameter->ifc.binningHorizontal.isValid() )
    {
        pThreadParameter->ifc.binningHorizontal.write( ssp.horizontalBinningOrDecimation_ );
    }
    else if( pThreadParameter->ifc.decimationHorizontal.isValid() )
    {
        pThreadParameter->ifc.decimationHorizontal.write( ssp.horizontalBinningOrDecimation_ );
    }
    if( pThreadParameter->ifc.binningVertical.isValid() )
    {
        pThreadParameter->ifc.binningVertical.write( ssp.verticalBinningOrDecimation_ );
    }
    else if( pThreadParameter->ifc.decimationVertical.isValid() )
    {
        pThreadParameter->ifc.decimationVertical.write( ssp.verticalBinningOrDecimation_ );
    }
    pThreadParameter->ifc.height.write( pThreadParameter->ifc.heightMax.read() );
    pThreadParameter->ctc.counterDuration.write( ssp.frameCount_ );
    pThreadParameter->sc.sequencerPathSelector.write( 0LL );
    pThreadParameter->sc.sequencerTriggerSource.writeS( "Counter1End" );
    pThreadParameter->sc.sequencerSetNext.write( ssp.sequencerSetNext_ );
    checkedMethodCall( pThreadParameter->pDev, pThreadParameter->sc.sequencerSetSave );
}

//-----------------------------------------------------------------------------
// This function will configure the sequencer on the device to take a sequence of
// 'X' images where the sequence is split into parts of different length and each
// part of the sequence can use a different exposure time. Thus e.g.
// -  5 frames with 1000us
// - 40 frames with 15000us
// - 20 frames with 2000us
// - 10 frames with 10000us
// -  5 frames with 5000us
// can be captured. To change the sequence edit the 's_SequencerData' data array
// and recompile the application.
void configureSequencer( ThreadParameter* pThreadParameter )
//-----------------------------------------------------------------------------
{
    try
    {
        pThreadParameter->sc.sequencerMode.writeS( "Off" );
        pThreadParameter->sc.sequencerConfigurationMode.writeS( "On" );
        pThreadParameter->sc.sequencerFeatureSelector.writeS( "ExposureTime" );
        pThreadParameter->sc.sequencerFeatureEnable.write( bTrue );
        pThreadParameter->sc.sequencerFeatureSelector.writeS( "CounterDuration" );
        pThreadParameter->sc.sequencerFeatureEnable.write( bTrue );
        const size_t cnt = sizeof( s_SequencerData ) / sizeof( s_SequencerData[0] );
        for( size_t i = 0; i < cnt; i++ )
        {
            configureSequencerSet( pThreadParameter, s_SequencerData[i] );
            s_SequencerData[i].expectedFrameRate_ = pThreadParameter->ac.mvResultingFrameRate.read();
        }
        pThreadParameter->sc.sequencerSetStart.write( 0 );
        pThreadParameter->sc.sequencerConfigurationMode.writeS( "Off" );
        pThreadParameter->sc.sequencerMode.writeS( "On" );
    }
    catch( const ImpactAcquireException& e )
    {
        cout << "An error occurred while setting up the sequencer for device " << pThreadParameter->pDev->serial.read()
             << "(error code: " << e.getErrorCodeAsString() << ")." << endl;
        s_boTerminated = true;
    }
}

//-----------------------------------------------------------------------------
// Returns the expected sequencer set for frame 'frameNr' based on the data
// defined by the entries of 's_SequencerData'.
size_t getExpectedSequencerSet( int64_type frameNr )
//-----------------------------------------------------------------------------
{
    const size_t cnt = sizeof( s_SequencerData ) / sizeof( s_SequencerData[0] );
    int64_type framesUpToHere = 0LL;
    for( size_t i = 0; i < cnt; i++ )
    {
        framesUpToHere += s_SequencerData[i].frameCount_;
        if( frameNr < framesUpToHere )
        {
            return i;
        }
    }
    return 0xFFFFFFFF;
}

//-----------------------------------------------------------------------------
// Returns the minimal exposure time defined by the entries of 's_SequencerData'.
double getMinimalExposureTime( void )
//-----------------------------------------------------------------------------
{
    const size_t cnt = sizeof( s_SequencerData ) / sizeof( s_SequencerData[0] );
    double minExposureTime_us = numeric_limits<double>::max();
    for( size_t i = 0; i < cnt; i++ )
    {
        if( minExposureTime_us > s_SequencerData[i].exposureTime_us_ )
        {
            minExposureTime_us = s_SequencerData[i].exposureTime_us_;
        }
    }
    // make sure we found at least one entry, that makes sense!
    assert( minExposureTime_us != numeric_limits<double>::max() );
    return minExposureTime_us;
}

//-----------------------------------------------------------------------------
// Calculates the overall number of frames that will form a complete sequence as
// defined by the entries of 's_SequencerData'.
int64_type getOverallSequenceLength( void )
//-----------------------------------------------------------------------------
{
    const size_t cnt = sizeof( s_SequencerData ) / sizeof( s_SequencerData[0] );
    int64_type overallFrameCount = 0LL;
    for( size_t i = 0; i < cnt; i++ )
    {
        overallFrameCount += s_SequencerData[i].frameCount_;
    }
    return overallFrameCount;
}

//-----------------------------------------------------------------------------
// Returns the pure acquisition time in seconds. This is the sum of the defined exposure
// times or sensor read-out times (depending on which value is larger) of all frames
// that belong to the full sequence. This might be useful to
// e.g. calculate the difference between the overall capture time and the ideal
// capture time (when the sensor read-out is always faster than the exposure etc.).
double getPureAcquisitionTimeOfCapturedFrames( const int64_type framesCaptured )
//-----------------------------------------------------------------------------
{
    const size_t cnt = sizeof( s_SequencerData ) / sizeof( s_SequencerData[0] );
    int64_type framesProcessed = 0;
    double pureAcquisitionTime_us = 0.0;
    for( size_t i = 0; i < cnt; i++ )
    {
        const int64_type framesToConsider = ( ( framesProcessed + s_SequencerData[i].frameCount_ ) > framesCaptured ) ? framesCaptured - framesProcessed : s_SequencerData[i].frameCount_;
        if( ( s_SequencerData[i].expectedFrameRate_ > 0.0 ) &&
            ( ( 1.0 / s_SequencerData[i].expectedFrameRate_ * 1000000. ) > s_SequencerData[i].exposureTime_us_ ) )
        {
            pureAcquisitionTime_us += ( 1.0 / s_SequencerData[i].expectedFrameRate_ * 1000000. ) * framesToConsider;
        }
        else
        {
            pureAcquisitionTime_us += s_SequencerData[i].exposureTime_us_ * framesToConsider;
        }
        framesProcessed += framesToConsider;
        if( framesProcessed >= framesCaptured )
        {
            break;
        }
    }
    return pureAcquisitionTime_us / 1000000.;
}

//-----------------------------------------------------------------------------
// Stores a frame in RAW format using a file name that contains all information
// needed to reconstruct the image later. This uses the same format that is understood
// by wxPropView, thus such a file can be displayed in wxPropView by simply
// dragging the file into the display area of the tool or by loading the image
// via the corresponding menu items.
void storeRawFrame( Request* pRequest )
//-----------------------------------------------------------------------------
{
    const void* pData = pRequest->imageData.read();
    if( pData )
    {
        ostringstream fileName;
        fileName << "image" << setw( 6 ) << setfill( '0' ) << pRequest->infoFrameID.read() << "_"
                 << "Set=" << pRequest->chunkSequencerSetActive.read() << "_"
                 << "Exposure=" << static_cast<unsigned int>( pRequest->chunkExposureTime.read() ) << "." // the 'cast' is just to get rid of the '.' in the 'double' value as this otherwise breaks the 'RAW' file import of wxPropView...
                 << pRequest->imageWidth.read() << "x" << pRequest->imageHeight.read()
                 << "." << pRequest->imagePixelFormat.readS();
        if( pRequest->imageBayerMosaicParity.read() != bmpUndefined )
        {
            fileName << "(BayerPattern=" << pRequest->imageBayerMosaicParity.readS() << ")";
        }
        fileName << ".raw";
        FILE* pFile = mv_fopen_s( fileName.str().c_str(), "wb" );
        if( pFile )
        {
            if( fwrite( pData, pRequest->imageSize.read(), 1, pFile ) != 1 )
            {
                cout << "Failed to write file '" << fileName.str() << "'." << endl;
            }
            else
            {
                cout << "Successfully written file '" << fileName.str() << "'." << endl;
            }
            fclose( pFile );
        }
    }
}

//=============================================================================
//================= helper functions ==========================================
//=============================================================================
//-----------------------------------------------------------------------------
// Calls the function bound to an mvIMPACT::acquire::Method object and displays
// an error message if the function call did fail.
void checkedMethodCall( Device* pDev, Method& method )
//-----------------------------------------------------------------------------
{
    const TDMR_ERROR result = static_cast<TDMR_ERROR>( method.call() );
    if( result != DMR_NO_ERROR )
    {
        cout << "An error was returned while calling function '" << method.displayName() << "' on device " << pDev->serial.read()
             << "(" << pDev->product.read() << "): " << ImpactAcquireException::getErrorCodeAsString( result ) << endl;
    }
}

//=============================================================================
//================= main implementation =======================================
//=============================================================================
//-----------------------------------------------------------------------------
unsigned int DMR_CALL liveThread( void* pData )
//-----------------------------------------------------------------------------
{
    ThreadParameter* pThreadParameter = reinterpret_cast<ThreadParameter*>( pData );
    CTime timer;

    // store width and height for checking correct image size
    const int64_type orgWidth = pThreadParameter->ifc.width.read();
    const int64_type orgHeight = pThreadParameter->ifc.height.read();
    cout << "OrgWidth = " << orgWidth << " OrgHeight = " << orgHeight << endl;

    // Now configure SFNC(Standard Feature Naming Convention) compliant features(see http://www.emva.org to find out more
    // about the standard and to download the latest SFNC document version)
    //
    // IMPORTANT:
    //
    // The SFNC unfortunately does NOT define numerical values for enumerations, thus a device independent piece of software
    // should use the enum-strings defined in the SFNC to ensure interoperability between devices. This is slightly slower
    // but should not cause problems in real world applications. When the device type AND GenICam XML file version is
    // guaranteed to be constant for a certain version of software, the driver internal code generator can be used to create
    // a interface header, that has numerical constants for enumerations as well. See device driver documentation under
    // 'Use Cases -> GenICam to mvIMPACT Acquire code generator' for details.
    configureDevice( pThreadParameter->pDev );
    cout << "Setting up the device took " << timer.restart() << " seconds." << endl;
    configureSequencer( pThreadParameter );
    cout << "Setting up the sequencer took " << timer.restart() << " seconds." << endl;

    // Send all requests to the capture queue. There can be more than 1 queue for some devices, but for this sample
    // we will work with the default capture queue. If a device supports more than one capture or result
    // queue, this will be stated in the manual. If nothing is mentioned about it, the device supports one
    // queue only. This loop will send all requests currently available to the driver. To modify the number of requests
    // use the property mvIMPACT::acquire::SystemSettings::requestCount at runtime or the property
    // mvIMPACT::acquire::Device::defaultRequestCount BEFORE opening the device.
    TDMR_ERROR result = DMR_NO_ERROR;
    while( ( result = static_cast<TDMR_ERROR>( pThreadParameter->fi.imageRequestSingle() ) ) == DMR_NO_ERROR );
    if( result != DEV_NO_FREE_REQUEST_AVAILABLE )
    {
        cout << "'FunctionInterface.imageRequestSingle' returned with an unexpected result: " << result
             << "(" << mvIMPACT::acquire::ImpactAcquireException::getErrorCodeAsString( result ) << ")" << endl;
    }
    cout << "Queuing capture buffers took " << timer.restart() << " seconds." << endl;

    manuallyStartAcquisitionIfNeeded( pThreadParameter->pDev, pThreadParameter->fi );
    cout << "Starting the acquisition took " << timer.restart() << " seconds." << endl;

    const int framesToCapture = pThreadParameter->fi.requestCount();
    int64_type framesCaptured = 0;
    bool isFirstValidImage = true;
    vector<string> information;
    const unsigned int timeout_ms = 2500;
    while( !s_boTerminated && ( framesCaptured < framesToCapture ) )
    {
        ostringstream oss;
        // wait for results from the default capture queue
        int requestNr = pThreadParameter->fi.imageRequestWaitFor( timeout_ms );
        if( pThreadParameter->fi.isRequestNrValid( requestNr ) )
        {
            const Request* pRequest = pThreadParameter->fi.getRequest( requestNr );
            if( pRequest->isOK() )
            {
                // Within this scope we have a valid buffer of data that can be an image or any other chunk of data.
                if( isFirstValidImage == true )
                {
                    oss << "The first frame arrived after " << timer.elapsed() << " seconds using the following format: "
                        << pRequest->imageWidth.read() << "x" << pRequest->imageHeight.read() << ", " << pRequest->imagePixelFormat.readS()
                        << endl;
                    isFirstValidImage = false;
                }
                oss << "Image captured: "
                    << "TimeStamp: " << setw( 16 ) << pRequest->infoTimeStamp_us.read() << ", "
                    << "ChunkExposureTime: " << setw( 10 ) << static_cast<int>( pRequest->chunkExposureTime.read() ) << ", "
                    << "ChunkSequencerSetActive: " << pRequest->chunkSequencerSetActive.read() << ", "
                    << "ChunkWidth: " << pRequest->chunkWidth.read() << ", "
                    << "ChunkHeight: " << pRequest->chunkHeight.read() << ", "
                    << "FrameID: " << setw( 16 ) << pRequest->infoFrameID.read() << ".";
                const size_t expectedSet = getExpectedSequencerSet( framesCaptured );
                if( expectedSet < ( sizeof( s_SequencerData ) / sizeof( s_SequencerData[0] ) ) )
                {
                    if( expectedSet != static_cast<size_t>( pRequest->chunkSequencerSetActive.read() ) )
                    {
                        oss << " ERROR! Expected set " << expectedSet << ", reported set " << pRequest->chunkSequencerSetActive.read();
                    }
                    // check exposure time
                    const double reportedExposureTime = pRequest->chunkExposureTime.read();
                    if( ( s_SequencerData[expectedSet].exposureTime_us_ * 0.95 > reportedExposureTime ) ||
                        ( s_SequencerData[expectedSet].exposureTime_us_ * 1.05 < reportedExposureTime ) )
                    {
                        oss << " ERROR! Expected exposure time " << s_SequencerData[expectedSet].exposureTime_us_ << ", reported exposure time " << static_cast<int>( reportedExposureTime );
                    }
                    // check image width
                    const int64_type reportedWidth = pRequest->chunkWidth.read();
                    const int64_type expectedWidth = orgWidth / s_SequencerData[expectedSet].horizontalBinningOrDecimation_;
                    if( ( expectedWidth != reportedWidth ) &&
                        // some cameras have a binned image with a width that is always a multiple of 32 bytes
                        ( ( ( expectedWidth / 32 ) * 32 ) != reportedWidth ) )
                    {
                        oss << " ERROR! Expected width " << expectedWidth << ", reported width " << static_cast<int>( reportedWidth );
                    }
                    // check image height
                    const int64_type reportedHeight = pRequest->chunkHeight.read();
                    if( ( s_SequencerData[expectedSet].verticalBinningOrDecimation_ * reportedHeight != orgHeight ) )
                    {
                        oss << " ERROR! Expected height " << orgHeight / s_SequencerData[expectedSet].verticalBinningOrDecimation_ << ", reported height " << static_cast<int>( reportedHeight );
                    }
                }
                else
                {
                    oss << "Internal error! Failed to locate matching sequencer set!";
                }
                oss << endl;
#if !defined(linux) && !defined(__linux) && !defined(__linux__)
                pThreadParameter->displayWindow.GetImageDisplay().SetImage( pRequest );
                pThreadParameter->displayWindow.GetImageDisplay().Update();
#endif  // #if !defined(linux) && !defined(__linux) && !defined(__linux__)
            }
            else
            {
                oss << "Error: " << pRequest->requestResult.readS() << endl;
            }
            ++framesCaptured; // in case of an error this is not correct, but if we don't count here the full sequence check will not work!
            // Do not unlock any request as we want to store the data later on!
            // Also do not request new requests here as the full sequence has been allocated and queued already in 'setupAndQueueCaptureBuffers'!
        }
        else
        {
            // If the error code is -2119(DEV_WAIT_FOR_REQUEST_FAILED), the documentation will provide
            // additional information under TDMR_ERROR in the interface reference
            oss << "'imageRequestWaitFor' failed (" << requestNr << ", " << ImpactAcquireException::getErrorCodeAsString( requestNr ) << ")." << endl;
            s_boTerminated = true;
        }
        information.push_back( oss.str() );
#if defined(linux) || defined(__linux) || defined(__linux__)
        s_boTerminated = waitForInput( 0, STDOUT_FILENO ) == 0 ? false : true; // break by STDIN
#endif // #if defined(linux) || defined(__linux) || defined(__linux__)
    }

    const double captureTime = timer.elapsed();
    // Writing to stdout is very slow, thus we buffer the information first and output it after measuring the capture time
    const vector<string>::size_type informationCount = information.size();
    for( vector<string>::size_type i = 0; i < informationCount; i++ )
    {
        cout << information[i];
    }
    cout << "Capturing the sequence took " << captureTime << " seconds while the pure acquisition time of all frames would have been " << getPureAcquisitionTimeOfCapturedFrames( framesCaptured ) << " seconds." << endl;
    timer.restart();
    manuallyStopAcquisitionIfNeeded( pThreadParameter->pDev, pThreadParameter->fi );
    cout << "Stopping the acquisition took " << timer.restart() << " seconds." << endl;

#if !defined(linux) && !defined(__linux) && !defined(__linux__)
    // stop the displayWindow from showing data we are about to free
    pThreadParameter->displayWindow.GetImageDisplay().RemoveImage();
#endif // #if !defined(linux) && !defined(__linux) && !defined(__linux__)

    cout << endl << "If the " << framesCaptured << " frames shall be stored to disc press 'y' [ENTER] now: ";
    string store;
    cin >> store;
    if( store == "y" )
    {
        cout << "Storing...." << endl;
        for( unsigned int i = 0; i < framesCaptured; i++ )
        {
            storeRawFrame( pThreadParameter->fi.getRequest( i ) );
        }
        cout << endl << "All files have been stored in RAW format. They can e.g. be watched by dragging them onto the display area of wxPropView!" << endl;
    }

    // In this sample all the next lines are redundant as the device driver will be
    // closed now, but in a real world application a thread like this might be started
    // several times an then it becomes crucial to clean up correctly.

    for( unsigned int i = 0; i < framesCaptured; i++ )
    {
        result = static_cast<TDMR_ERROR>( pThreadParameter->fi.imageRequestUnlock( i ) );
        if( result != DMR_NO_ERROR )
        {
            cout << "Failed to unlock request number " << i << "(" << ImpactAcquireException::getErrorCodeAsString( result ) << ")" << endl;
        }
    }

    // clear all queues. In this example this should no do anything as we captured precisely the number of images we requested, thus
    // whenever another request is returned here, this is a severe malfunction!
    pThreadParameter->fi.imageRequestReset( 0, 0 );
    return 0;
}

//-----------------------------------------------------------------------------
// This function will allow to select devices that support the GenICam interface
// layout(these are devices, that claim to be compliant with the GenICam standard)
// and that are bound to drivers that support the user controlled start and stop
// of the internal acquisition engine. Other devices will not be listed for
// selection as the code of the example relies on these features in the code.
bool isDeviceSupportedBySample( const Device* const pDev )
//-----------------------------------------------------------------------------
{
    if( !pDev->interfaceLayout.isValid() &&
        !pDev->acquisitionStartStopBehaviour.isValid() )
    {
        return false;
    }

    vector<TDeviceInterfaceLayout> availableInterfaceLayouts;
    pDev->interfaceLayout.getTranslationDictValues( availableInterfaceLayouts );
    return find( availableInterfaceLayouts.begin(), availableInterfaceLayouts.end(), dilGenICam ) != availableInterfaceLayouts.end();
}

//-----------------------------------------------------------------------------
int main( void )
//-----------------------------------------------------------------------------
{
    DeviceManager devMgr;
    Device* pDev = getDeviceFromUserInput( devMgr, isDeviceSupportedBySample );
    if( !pDev )
    {
        cout << "Could not obtain a valid pointer to a device. Unable to continue!";
        cout << "Press [ENTER] to end the application" << endl;
        cin.get();
        return 1;
    }

    try
    {
        cout << "Initialising the device. This might take some time..." << endl << endl;
        pDev->interfaceLayout.write( dilGenICam ); // This is also done 'silently' by the 'getDeviceFromUserInput' function but your application needs to do this as well so state this here clearly!
        pDev->open();
    }
    catch( const ImpactAcquireException& e )
    {
        // this e.g. might happen if the same device is already opened in another process...
        cout << "An error occurred while opening the device " << pDev->serial.read()
             << "(error code: " << e.getErrorCodeAsString() << ")." << endl
             << "Press [ENTER] to end the application..." << endl;
        cin.get();
        return 1;
    }

    // start the execution of the 'live' thread.
#if defined(linux) || defined(__linux) || defined(__linux__)
    ThreadParameter threadParam( pDev );
    liveThread( &threadParam );
#else
    unsigned int dwThreadID;
    string windowTitle( "mvIMPACT_acquire sequencer sample, Device " + pDev->serial.read() );
    // initialise displayWindow
    // IMPORTANT: It's NOT safe to create multiple displayWindow's in multiple threads!!!
    ThreadParameter threadParam( pDev, windowTitle );
    HANDLE hThread = ( HANDLE )_beginthreadex( 0, 0, liveThread, ( LPVOID )( &threadParam ), 0, &dwThreadID );
    WaitForSingleObject( hThread, INFINITE );
    CloseHandle( hThread );
#endif  // #if defined(linux) || defined(__linux) || defined(__linux__)
    return 0;
}
