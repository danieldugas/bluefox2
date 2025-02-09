//-----------------------------------------------------------------------------
#include <apps/Common/wxAbstraction.h>
#include "CaptureThread.h"
#include <common/STLHelper.h>
#include "DevCtrl.h"
#include "GlobalDataStorage.h"
#include <algorithm>
#include <functional>

#include <apps/Common/wxIncludePrologue.h>
#include <wx/arrstr.h>
#include <wx/wupdlock.h>
#include <apps/Common/wxIncludeEpilogue.h>

using namespace std;
using namespace mvIMPACT::acquire;

#define MEASSURE_DEVICE_OPEN_TIMES
#ifdef MEASSURE_DEVICE_OPEN_TIMES
#   define APPEND_TIMING_RESULT(STRING, MSG, TIMER, TOTAL_TIME) \
    { \
        const long e = TIMER.Time(); \
        TIMER.Start(); \
        TOTAL_TIME += e; \
        STRING.Append( wxString::Format( wxT( " (%ld ms, total: %ld ms): %s\n" ), e, TOTAL_TIME, MSG ) ); \
    }
#else
#   define APPEND_TIMING_RESULT(MSG, TIMER, TOTAL_TIME)
#endif // #ifdef MEASSURE_DEVICE_OPEN_TIMES

//=============================================================================
//================= Implementation DevicePropertyHandler ======================
//=============================================================================
//-----------------------------------------------------------------------------
DevicePropertyHandler::DevicePropertyHandler( wxPropertyGridPage* pPGDevice, bool boDisplayDebugInfo, bool boDisplayFullTree, bool boDisplayInvisibleComponents ) :
    m_actDevListChangedCounter( 0 ), m_actDrvListChangedCounter( 0 ), m_boDisplayDebugInfo( boDisplayDebugInfo ), m_boDisplayInvisibleComponents( boDisplayInvisibleComponents ),
    m_componentVisibility( cvBeginner ), m_fullTreeChangedCounter( 0 ), m_pFullTree( 0 ), m_pActiveDevData( 0 ), m_pActiveDevice( 0 ),
    m_pPGDevice( pPGDevice ), m_viewMode( vmStandard ), m_boUseHexIndices( false ), m_boUseDisplayNames( false ), m_boUseSelectorGrouping( false )
//-----------------------------------------------------------------------------
{
    if( boDisplayFullTree )
    {
        m_pFullTree = new PropTree( ROOT_LIST, "Full Tree", m_pPGDevice, static_cast<EDisplayFlags>( GetDisplayFlags() ) );
        m_pFullTree->Draw( false );
    }
}

//-----------------------------------------------------------------------------
DevicePropertyHandler::~DevicePropertyHandler()
//-----------------------------------------------------------------------------
{
    wxWindowUpdateLocker updateLock( m_pPGDevice->GetGrid() );
    DevToTreeMap::iterator itStart = m_devToTreeMap.begin();
    DevToTreeMap::iterator itEnd = m_devToTreeMap.end();
    while( itStart != itEnd )
    {
        CloseDriver( ConvertedString( itStart->first->serial.read() ) );
        if( itStart->second->pCaptureThread )
        {
            itStart->second->pCaptureThread->Delete();
            DeleteElement( itStart->second->pCaptureThread );
        }
        DeleteDriverTrees( itStart );
        delete itStart->second->pDriverTree;
        delete itStart->second->pDeviceTree;
        delete itStart->second->pFuncInterface;
        delete itStart->second->pStatistics;
        delete itStart->second->pInfo;
        delete itStart->second;
        ++itStart;
    }
    delete m_pFullTree;
}

//-----------------------------------------------------------------------------
void DevicePropertyHandler::AddFeatureToSetIfValid( set<HOBJ>& s, Component& c )
//-----------------------------------------------------------------------------
{
    if( c.isValid() )
    {
        s.insert( c.hObj() );
    }
}

//-----------------------------------------------------------------------------
void DevicePropertyHandler::CheckForWizards( mvIMPACT::acquire::Device* pDev, DeviceData* pDevData )
//-----------------------------------------------------------------------------
{
    if( pDev->interfaceLayout.isValid() && ( pDev->interfaceLayout.read() == dilGenICam ) )
    {
        {
            // check for SFNC compliant File Access Control
            mvIMPACT::acquire::GenICam::FileAccessControl fac( pDev );
            if( fac.fileAccessBuffer.isValid() && fac.fileAccessLength.isValid() && fac.fileAccessOffset.isValid() &&
                fac.fileOpenMode.isValid() && fac.fileOperationExecute.isValid() && fac.fileOperationResult.isValid() &&
                fac.fileOperationSelector.isValid() && fac.fileOperationStatus.isValid() && fac.fileSelector.isValid() )
            {
                set<HOBJ> s;
                s.insert( fac.fileAccessBuffer.parent().hObj() );
                s.insert( fac.fileAccessBuffer.hObj() );
                s.insert( fac.fileAccessLength.hObj() );
                s.insert( fac.fileAccessOffset.hObj() );
                s.insert( fac.fileOpenMode.hObj() );
                s.insert( fac.fileOperationExecute.hObj() );
                s.insert( fac.fileOperationResult.hObj() );
                s.insert( fac.fileOperationSelector.hObj() );
                s.insert( fac.fileOperationStatus.hObj() );
                s.insert( fac.fileSelector.hObj() );
                s.insert( fac.fileSize.hObj() );
                pDevData->supportedWizards.insert( make_pair( wFileAccessControl, s ) );
            }
        }

        {
            // check for SFNC compliant LUT Control
            mvIMPACT::acquire::GenICam::LUTControl lc( pDev );
            if( lc.LUTEnable.isValid() && lc.LUTIndex.isValid() &&
                lc.LUTValue.isValid() )
            {
                set<HOBJ> s;
                s.insert( lc.LUTEnable.parent().hObj() );
                s.insert( lc.LUTEnable.hObj() );
                s.insert( lc.LUTIndex.hObj() );
                AddFeatureToSetIfValid( s, lc.LUTSelector );
                s.insert( lc.LUTValue.hObj() );
                pDevData->supportedWizards.insert( make_pair( wLUTControl, s ) );
            }
        }

        {
            // check for MATRIX VISION specific multi-AOI features
            mvIMPACT::acquire::GenICam::ImageFormatControl ifc( pDev );
            if( ifc.mvMultiAreaMode.isValid() && ifc.mvAreaSelector.isValid() && ifc.mvAreaEnable.isValid() &&
                ifc.mvAreaOffsetX.isValid() && ifc.mvAreaOffsetY.isValid() && ifc.mvAreaWidth.isValid() && ifc.mvAreaHeight.isValid() )
            {
                set<HOBJ> s;
                s.insert( ifc.mvMultiAreaMode.hObj() );
                s.insert( ifc.mvAreaSelector.hObj() );
                s.insert( ifc.mvAreaEnable.hObj() );
                s.insert( ifc.mvAreaOffsetX.hObj() );
                s.insert( ifc.mvAreaOffsetY.hObj() );
                s.insert( ifc.mvAreaWidth.hObj() );
                s.insert( ifc.mvAreaHeight.hObj() );
                AddFeatureToSetIfValid( s, ifc.mvAreaResultingOffsetX );
                AddFeatureToSetIfValid( s, ifc.mvAreaResultingOffsetY );
                pDevData->supportedWizards.insert( make_pair( wMultiAOI, s ) );
            }
        }

        {
            // check for SFNC compliant LUT Control
            mvIMPACT::acquire::GenICam::SequencerControl sc( pDev );
            if( sc.sequencerSetSelector.isValid() && sc.sequencerSetNext.isValid() &&
                sc.sequencerMode.isValid() )
            {
                set<HOBJ> s;
                s.insert( sc.sequencerSetSelector.parent().hObj() );
                s.insert( sc.sequencerSetSelector.hObj() );
                s.insert( sc.sequencerSetNext.hObj() );
                s.insert( sc.sequencerMode.hObj() );
                AddFeatureToSetIfValid( s, sc.sequencerConfigurationMode );
                AddFeatureToSetIfValid( s, sc.sequencerFeatureEnable );
                AddFeatureToSetIfValid( s, sc.sequencerFeatureSelector );
                AddFeatureToSetIfValid( s, sc.sequencerPathSelector );
                AddFeatureToSetIfValid( s, sc.sequencerSetActive );
                AddFeatureToSetIfValid( s, sc.sequencerSetLoad );
                AddFeatureToSetIfValid( s, sc.sequencerSetSave );
                AddFeatureToSetIfValid( s, sc.sequencerSetStart );
                AddFeatureToSetIfValid( s, sc.sequencerTriggerActivation );
                AddFeatureToSetIfValid( s, sc.sequencerTriggerSource );
                pDevData->supportedWizards.insert( make_pair( wSequencerControl, s ) );
            }
        }

        // check for 'mvLensControl category
        try
        {
            // creating this object will raise an exception if the category is not supported!
            mvIMPACT::acquire::GenICam::mvLensControl lc( pDev );
            if( lc.mvDriveSelector.isValid() )
            {
                set<HOBJ> s;
                s.insert( lc.mvDriveSelector.parent().hObj() );
                s.insert( lc.mvDriveSelector.hObj() );
                AddFeatureToSetIfValid( s, lc.mvDriveBackward );
                AddFeatureToSetIfValid( s, lc.mvDriveDuration );
                AddFeatureToSetIfValid( s, lc.mvDriveForward );
                AddFeatureToSetIfValid( s, lc.mvDriveLevel );
                AddFeatureToSetIfValid( s, lc.mvIrisMode );
                AddFeatureToSetIfValid( s, lc.mvIrisSignalLevelMax );
                AddFeatureToSetIfValid( s, lc.mvIrisSignalLevelMin );
                AddFeatureToSetIfValid( s, lc.mvIrisType );
                pDevData->supportedWizards.insert( make_pair( wLensControl, s ) );
            }
        }
        catch( const ImpactAcquireException& )
        {
            // creating this object in case the category is not supported will raise an exception. This is intended!!!
        }

        {
            // check for MATRIX VISION multi-core acquisition optimizer features
            Info info( pDev );
            mvIMPACT::acquire::GenICam::DataStreamModule dm( pDev, 0, "Base" );
            if( info.systemLogicalProcessorCount.isValid() &&
                info.systemPhysicalProcessorCount.isValid() &&
                dm.mvMultiCoreAcquisitionEnable.isValid() &&
                dm.mvMultiCoreAcquisitionBaseCore.isValid() &&
                dm.mvMultiCoreAcquisitionCoreCount.isValid() &&
                dm.mvMultiCoreAcquisitionFirstCoreIndex.isValid() &&
                dm.mvMultiCoreAcquisitionCoreSwitchInterval.isValid() )
            {
                set<HOBJ> s;
                s.insert( info.systemLogicalProcessorCount.hObj() );
                s.insert( info.systemPhysicalProcessorCount.hObj() );
                s.insert( dm.mvMultiCoreAcquisitionEnable.hObj() );
                s.insert( dm.mvMultiCoreAcquisitionBaseCore.hObj() );
                s.insert( dm.mvMultiCoreAcquisitionCoreCount.hObj() );
                s.insert( dm.mvMultiCoreAcquisitionFirstCoreIndex.hObj() );
                s.insert( dm.mvMultiCoreAcquisitionCoreSwitchInterval.hObj() );
                pDevData->supportedWizards.insert( make_pair( wMultiCoreAcquisition, s ) );
            }
        }
    }

    // currently only MATRIX VISION BF, BF3, BC-X(D) and BN devices offer the quick setup wizard
    const string family( pDev->family.read() );
    const string product( pDev->product.read() );
    const string manufacturer( makeLowerCase( pDev->manufacturer.isValid() ? pDev->manufacturer.read() : "" ) );
    if( ( ( ( product.find( "mvBlueFOX3" ) != string::npos ) || ( family.find( "mvBlueFOX3" ) != string::npos ) || ( product.find( "mvBlueCOUGAR-X" ) != string::npos ) ) && ( manufacturer.find( "matrix vision" ) != string::npos ) ) ||
        IsBlueNAOS( pDev ) ||
        IsBlueCOUGAR_X_XAS( pDev ) ||
        IsBlueFOX3_XAS( pDev ) )
    {
        if( pDev->interfaceLayout.read() == dilGenICam )
        {
            pDevData->supportedWizards.insert( make_pair( wQuickSetup, set<HOBJ>() ) );
        }
    }
    else if( product.find( "mvBlueFOX" ) != string::npos )
    {
        pDevData->supportedWizards.insert( make_pair( wQuickSetup, set<HOBJ>() ) );
    }

    // check for mvIMPACT Acquire compliant color twist filter
    mvIMPACT::acquire::ImageProcessing ip( pDev );
    if( ip.colorTwistInputCorrectionMatrixEnable.isValid() &&
        ip.colorTwistInputCorrectionMatrixMode.isValid() &&
        ip.colorTwistInputCorrectionMatrixRow0.isValid() &&
        ip.colorTwistInputCorrectionMatrixRow1.isValid() &&
        ip.colorTwistInputCorrectionMatrixRow2.isValid() &&
        ip.colorTwistEnable.isValid() &&
        ip.colorTwistRow0.isValid() &&
        ip.colorTwistRow1.isValid() &&
        ip.colorTwistRow2.isValid() &&
        ip.colorTwistOutputCorrectionMatrixEnable.isValid() &&
        ip.colorTwistOutputCorrectionMatrixMode.isValid() &&
        ip.colorTwistOutputCorrectionMatrixRow0.isValid() &&
        ip.colorTwistOutputCorrectionMatrixRow1.isValid() &&
        ip.colorTwistOutputCorrectionMatrixRow2.isValid() &&
        ip.colorTwistResultingMatrixRow0.isValid() &&
        ip.colorTwistResultingMatrixRow1.isValid() &&
        ip.colorTwistResultingMatrixRow2.isValid() )
    {
        set<HOBJ> s;
        s.insert( ip.colorTwistEnable.parent().hObj() );
        s.insert( ip.colorTwistInputCorrectionMatrixEnable.hObj() );
        s.insert( ip.colorTwistInputCorrectionMatrixMode.hObj() );
        s.insert( ip.colorTwistInputCorrectionMatrixRow0.hObj() );
        s.insert( ip.colorTwistInputCorrectionMatrixRow1.hObj() );
        s.insert( ip.colorTwistInputCorrectionMatrixRow2.hObj() );
        s.insert( ip.colorTwistEnable.hObj() );
        s.insert( ip.colorTwistRow0.hObj() );
        s.insert( ip.colorTwistRow1.hObj() );
        s.insert( ip.colorTwistRow2.hObj() );
        s.insert( ip.colorTwistOutputCorrectionMatrixEnable.hObj() );
        s.insert( ip.colorTwistOutputCorrectionMatrixMode.hObj() );
        s.insert( ip.colorTwistOutputCorrectionMatrixRow0.hObj() );
        s.insert( ip.colorTwistOutputCorrectionMatrixRow1.hObj() );
        s.insert( ip.colorTwistOutputCorrectionMatrixRow2.hObj() );
        s.insert( ip.colorTwistResultingMatrixRow0.hObj() );
        s.insert( ip.colorTwistResultingMatrixRow1.hObj() );
        s.insert( ip.colorTwistResultingMatrixRow2.hObj() );
        if( pDev->interfaceLayout.isValid() && ( pDev->interfaceLayout.read() == dilGenICam ) )
        {
            // check for SFNC compliant Color Transformation Control features
            mvIMPACT::acquire::GenICam::ColorTransformationControl ctc( pDev );
            if( ctc.colorTransformationEnable.isValid() && ctc.colorTransformationValue.isValid() &&
                ctc.colorTransformationValueSelector.isValid() )
            {
                if( ctc.colorTransformationSelector.isValid() )
                {
                    s.insert( ctc.colorTransformationSelector.hObj() );
                }
                s.insert( ctc.colorTransformationEnable.parent().hObj() );
                s.insert( ctc.colorTransformationEnable.hObj() );
                s.insert( ctc.colorTransformationValue.hObj() );
                s.insert( ctc.colorTransformationValueSelector.hObj() );
            }
        }
        pDevData->supportedWizards.insert( make_pair( wColorCorrection, s ) );
    }

    // check for white balance
    {
        set<HOBJ> s;
        if( pDev->interfaceLayout.isValid() && ( pDev->interfaceLayout.read() == dilGenICam ) )
        {
            mvIMPACT::acquire::GenICam::AnalogControl ac( pDev );
            if( ac.balanceRatio.isValid() && ac.balanceRatioSelector.isValid() )
            {
                s.insert( ac.balanceRatioSelector.hObj() );
                s.insert( ac.balanceRatio.hObj() );
            }
        }
        if( ip.whiteBalance.isValid() && ip.whiteBalanceCalibration.isValid() )
        {
            s.insert( ip.whiteBalanceCalibration.hObj() );
            s.insert( ip.whiteBalance.hObj() );

            const unsigned int wbUserSettingsCount = ip.getWBUserSettingsCount();
            for( unsigned int i = 0; i < wbUserSettingsCount; i++ )
            {
                mvIMPACT::acquire::WhiteBalanceSettings& wbs( ip.getWBUserSetting( i ) );
                s.insert( wbs.hObj() );
            }
        }
        pDevData->supportedWizards.insert(  make_pair( wPseudoWizardWhiteBalance, s ) );
    }

    {
        set<HOBJ> s;
        if( ip.flatFieldFilterMode.isValid() && ip.flatFieldFilterCorrectionAoiMode.isValid() && ip.flatFieldFilterCalibrationAoiMode.isValid() )
        {
            s.insert( ip.flatFieldFilterMode.parent().hObj() );
            s.insert( ip.flatFieldFilterMode.hObj() );
            AddFeatureToSetIfValid( s, ip.flatFieldFilterCalibrationAoiOffsetX );
            AddFeatureToSetIfValid( s, ip.flatFieldFilterCalibrationAoiOffsetY );
            AddFeatureToSetIfValid( s, ip.flatFieldFilterCalibrationAoiWidth );
            AddFeatureToSetIfValid( s, ip.flatFieldFilterCalibrationAoiHeight );
            AddFeatureToSetIfValid( s, ip.flatFieldFilterCorrectionAoiOffsetX );
            AddFeatureToSetIfValid( s, ip.flatFieldFilterCorrectionAoiOffsetY );
            AddFeatureToSetIfValid( s, ip.flatFieldFilterCorrectionAoiWidth );
            AddFeatureToSetIfValid( s, ip.flatFieldFilterCorrectionAoiHeight );
        }

        const unsigned int wbUserSettingsCount = ip.getWBUserSettingsCount();
        for( unsigned int i = 0; i < wbUserSettingsCount; i++ )
        {
            mvIMPACT::acquire::WhiteBalanceSettings& wbs( ip.getWBUserSetting( i ) );
            s.insert( wbs.aoiHeight.parent().hObj() );
            AddFeatureToSetIfValid( s, wbs.WBAoiMode );
            AddFeatureToSetIfValid( s, wbs.aoiStartX );
            AddFeatureToSetIfValid( s, wbs.aoiStartY );
            AddFeatureToSetIfValid( s, wbs.aoiWidth );
            AddFeatureToSetIfValid( s, wbs.aoiHeight );
        }

        mvIMPACT::acquire::ImageDestination id( pDev );
        if( id.scalerMode.isValid() )
        {
            s.insert( id.scalerAoiHeight.parent().hObj() );
            AddFeatureToSetIfValid( s, id.scalerAoiEnable );
            AddFeatureToSetIfValid( s, id.scalerAoiStartX );
            AddFeatureToSetIfValid( s, id.scalerAoiStartY );
            AddFeatureToSetIfValid( s, id.scalerAoiWidth );
            AddFeatureToSetIfValid( s, id.scalerAoiHeight );
            AddFeatureToSetIfValid( s, id.imageWidth );
            AddFeatureToSetIfValid( s, id.imageHeight );
        }

        if( pDev->interfaceLayout.isValid() )
        {
            if( ( pDev->interfaceLayout.read() == dilDeviceSpecific ) && ( ( pDev->product.read().find( "mvBlueFOX" ) != string::npos ) || ( pDev->product.read().find( "VirtualDevice" ) != string::npos ) ) )
            {
                // check for AOI features
                mvIMPACT::acquire::CameraSettingsBase csb( pDev );
                s.insert( csb.aoiHeight.parent().hObj() );
                AddFeatureToSetIfValid( s, csb.aoiStartX );
                AddFeatureToSetIfValid( s, csb.aoiStartY );
                AddFeatureToSetIfValid( s, csb.aoiWidth );
                AddFeatureToSetIfValid( s, csb.aoiHeight );
            }
            else if( pDev->interfaceLayout.read() == dilGenICam )
            {
                // check for AOI features
                mvIMPACT::acquire::GenICam::ImageFormatControl ifc( pDev );
                mvIMPACT::acquire::GenICam::AcquisitionControl acq( pDev );
                mvIMPACT::acquire::GenICam::AnalogControl anc( pDev );

                if( ifc.offsetX.isValid() && ifc.offsetY.isValid() && ifc.width.isValid() && ifc.height.isValid() )
                {
                    s.insert( ifc.offsetX.hObj() );
                    s.insert( ifc.offsetY.hObj() );
                    s.insert( ifc.width.hObj() );
                    s.insert( ifc.height.hObj() );
                    AddFeatureToSetIfValid( s, acq.exposureAuto );
                    AddFeatureToSetIfValid( s, acq.mvExposureAutoAOIMode );
                    AddFeatureToSetIfValid( s, acq.mvExposureAutoOffsetX );
                    AddFeatureToSetIfValid( s, acq.mvExposureAutoOffsetY );
                    AddFeatureToSetIfValid( s, acq.mvExposureAutoWidth );
                    AddFeatureToSetIfValid( s, acq.mvExposureAutoHeight );
                    AddFeatureToSetIfValid( s, anc.gainAuto );
                    AddFeatureToSetIfValid( s, anc.mvGainAutoOffsetX );
                    AddFeatureToSetIfValid( s, anc.mvGainAutoOffsetY );
                    AddFeatureToSetIfValid( s, anc.mvGainAutoWidth );
                    AddFeatureToSetIfValid( s, anc.mvGainAutoHeight );
                    AddFeatureToSetIfValid( s, anc.balanceWhiteAuto );
                    AddFeatureToSetIfValid( s, anc.mvBalanceWhiteAutoAOIMode );
                    AddFeatureToSetIfValid( s, anc.mvBalanceWhiteAutoOffsetX );
                    AddFeatureToSetIfValid( s, anc.mvBalanceWhiteAutoOffsetY );
                    AddFeatureToSetIfValid( s, anc.mvBalanceWhiteAutoWidth );
                    AddFeatureToSetIfValid( s, anc.mvBalanceWhiteAutoHeight );
                }
            }
        }
        pDevData->supportedWizards.insert( make_pair( wAOI, s ) );
    }
}

//-----------------------------------------------------------------------------
void DevicePropertyHandler::CloseDriver( const wxString& name )
//-----------------------------------------------------------------------------
{
    string serialANSI( name.mb_str() );
    Device* pDev = m_devMgr.getDeviceBySerial( serialANSI );
    bool boMustRecreateFullTree = false;
    if( pDev && pDev->isOpen() )
    {
        DevToTreeMap::iterator it = m_devToTreeMap.find( pDev );
        if( it != m_devToTreeMap.end() )
        {
            wxCriticalSectionLocker locker( m_critSect );
            it->second->pCaptureThread->SetLiveMode( false );
            if( it->second->pCaptureThread )
            {
                it->second->pCaptureThread->Delete();
                DeleteElement( it->second->pCaptureThread );
            }
            it->second->boWasLive = false;
            {
                wxWindowUpdateLocker updateLock( m_pPGDevice->GetGrid() );
                if( m_pFullTree )
                {
                    DeleteElement( m_pFullTree );
                    boMustRecreateFullTree = true;
                }
                else
                {
                    DeleteDriverTrees( it );
                }
            }
            DeleteElement( it->second->pFuncInterface );
            DeleteElement( it->second->pStatistics );
            DeleteElement( it->second->pInfo );
            DeleteElement( it->second->pDeviceRegistersValidCallback );
            DeleteElement( it->second->pVideoStream );
            // We do not have an assignment operator, therefore in order to assign an INVALID handle to these properties(which we
            // want to do as we are CLOSING the driver instance) we need to bind them to a property we know for sure does NOT
            // exist. We use 'hDev' here for the locator as this is ALWAYS valid when we have a valid device pointer!
            ComponentLocator locator( pDev->hDev() );
            locator.bindComponent( it->second->deviceRegistersValid, "DUMMY_NON_FEATURE", 0, 1 );
            locator.bindComponent( it->second->acquisitionMode, "DUMMY_NON_FEATURE", 0, 1 );
            locator.bindComponent( it->second->acquisitionFrameCount, "DUMMY_NON_FEATURE", 0, 1 );
            locator.bindComponent( it->second->mvAcquisitionMemoryFrameCount, "DUMMY_NON_FEATURE", 0, 1 );
            it->second->supportedWizards.clear();
            it->second->sequencerSetToDisplayMap.clear();
            it->second->settingToDisplayDict.clear();
            it->second->featureVsTimeFeature = Component();
            it->second->featureVsTimePlotPath = wxEmptyString;
        }
        pDev->close();
    }

    if( boMustRecreateFullTree )
    {
        ReCreateFullTree();
    }
}

//-----------------------------------------------------------------------------
void DevicePropertyHandler::CreateDriverTrees( DevToTreeMap::iterator& it )
//-----------------------------------------------------------------------------
{
    if( !it->first->isOpen() )
    {
        return;
    }

    if( ( m_viewMode == vmStandard ) && it->second->pInfo->recommendedListsForUIs.isValid() )
    {
        m_vTreeProps.clear();
        const unsigned int cnt = it->second->pInfo->recommendedListsForUIs.valCount();
        for( unsigned int i = 0; i < cnt; i++ )
        {
            m_vTreeProps.push_back( TreeProp( it->second->pInfo->recommendedListsForUIs.read( i ), "" ) );
        }
    }
    else
    {
        PrepareTreeArray();
    }

    ComponentLocator cl( it->first->hDrv() );
    DeviceComponentLocator dcl( it->first->hDrv() );
    for( unsigned int i = 0; i < m_vTreeProps.size(); i++ )
    {
        try
        {
            if( m_vTreeProps[i].ListType == dltUndefined )
            {
                Component comp( cl.findComponent( m_vTreeProps[i].ListName.c_str(), smIgnoreMethods | smIgnoreProperties ) );
                string displayName( comp.displayName() );
                if( displayName.empty() )
                {
                    displayName = m_vTreeProps[i].DisplayName.empty() ? comp.name() : m_vTreeProps[i].DisplayName;
                }
                it->second->vDriverTrees.push_back( new PropTree( comp.hObj(), displayName.c_str(), m_pPGDevice, static_cast<EDisplayFlags>( GetDisplayFlags() ) ) );
            }
            else
            {
                it->second->vDriverTrees.push_back( new PropTree( dcl.bindSearchBaseList( it->first, m_vTreeProps[i].ListType ), m_vTreeProps[i].DisplayName.c_str(), m_pPGDevice, static_cast<EDisplayFlags>( GetDisplayFlags() ) ) );
            }
            it->second->vDriverTrees.back()->Draw( false );
        }
        catch( const ImpactAcquireException& )
        {
            // this list is not supported by this device
        }
    }
}

//-----------------------------------------------------------------------------
void DevicePropertyHandler::DeleteDriverTrees( DevToTreeMap::iterator& it )
//-----------------------------------------------------------------------------
{
    while( !it->second->vDriverTrees.empty() )
    {
        delete it->second->vDriverTrees.back();
        it->second->vDriverTrees.pop_back();
    }
}

//-----------------------------------------------------------------------------
bool DevicePropertyHandler::DoesActiveDeviceSupportWizard( TWizardIDs wID ) const
//-----------------------------------------------------------------------------
{
    wxCriticalSectionLocker locker( m_critSect );
    if( m_pActiveDevData == 0 )
    {
        return false;
    }
    return m_pActiveDevData->supportedWizards.find( wID ) != m_pActiveDevData->supportedWizards.end();
}

//-----------------------------------------------------------------------------
EDisplayFlags DevicePropertyHandler::GetDisplayFlags( void ) const
//-----------------------------------------------------------------------------
{
    EDisplayFlags flags = dfNone;
    if( m_boDisplayDebugInfo )
    {
        flags = EDisplayFlags( flags | dfDisplayDebugInfo );
    }
    if( m_boDisplayInvisibleComponents )
    {
        flags = EDisplayFlags( flags | dfDisplayInvisibleComponents );
    }
    if( m_boUseHexIndices )
    {
        flags = EDisplayFlags( flags | dfHexIndices );
    }
    if( m_boUseDisplayNames )
    {
        flags = EDisplayFlags( flags | dfDisplayNames );
    }
    if( m_boUseSelectorGrouping )
    {
        flags = EDisplayFlags( flags | dfSelectorGrouping );
    }
    if( m_viewMode == vmDeveloper )
    {
        flags = EDisplayFlags( flags | dfDontUseFriendlyNamesForMethods );
    }
    return flags;
}

//-----------------------------------------------------------------------------
void DevicePropertyHandler::GetInterfaceClasses( const DeviceData* p, FunctionInterface** ppFI /* = 0 */, const Statistics** ppS /* = 0 */, CaptureThread** ppCT /* = 0 */ ) const
//-----------------------------------------------------------------------------
{
    if( p )
    {
        if( ppFI )
        {
            *ppFI = p->pFuncInterface;
        }
        if( ppS )
        {
            *ppS = p->pStatistics;
        }
        if( ppCT )
        {
            *ppCT = p->pCaptureThread;
        }
    }
}

//-----------------------------------------------------------------------------
Device* DevicePropertyHandler::GetActiveDevice( FunctionInterface** ppFI /* = 0 */, const Statistics** ppS /* = 0 */, CaptureThread** ppCT /* = 0 */ ) const
//-----------------------------------------------------------------------------
{
    wxCriticalSectionLocker locker( m_critSect );
    GetInterfaceClasses( m_pActiveDevData, ppFI, ppS, ppCT );
    return m_pActiveDevice;
}

//-----------------------------------------------------------------------------
mvIMPACT::acquire::Property DevicePropertyHandler::GetActiveDeviceAcquisitionMode( void ) const
//-----------------------------------------------------------------------------
{
    wxCriticalSectionLocker locker( m_critSect );
    return m_pActiveDevData ? m_pActiveDevData->acquisitionMode : Property();
}

//-----------------------------------------------------------------------------
mvIMPACT::acquire::PropertyI64 DevicePropertyHandler::GetActiveDeviceAcquisitionFrameCount( void ) const
//-----------------------------------------------------------------------------
{
    wxCriticalSectionLocker locker( m_critSect );
    return m_pActiveDevData ? m_pActiveDevData->acquisitionFrameCount : PropertyI64();
}

//-----------------------------------------------------------------------------
mvIMPACT::acquire::PropertyI64 DevicePropertyHandler::GetActiveDeviceAcquisitionMemoryFrameCount( void ) const
//-----------------------------------------------------------------------------
{
    wxCriticalSectionLocker locker( m_critSect );
    return m_pActiveDevData ? m_pActiveDevData->mvAcquisitionMemoryFrameCount : PropertyI64();
}

//-----------------------------------------------------------------------------
mvIMPACT::acquire::PropertyIBoolean DevicePropertyHandler::GetActiveDeviceDeviceRegistersValid( void ) const
//-----------------------------------------------------------------------------
{
    wxCriticalSectionLocker locker( m_critSect );
    return m_pActiveDevData ? m_pActiveDevData->deviceRegistersValid : PropertyIBoolean();
}

//-----------------------------------------------------------------------------
void DevicePropertyHandler::GetActiveDeviceFeatureVsTimePlotInfo( Component& feature, wxString& featureFullPath ) const
//-----------------------------------------------------------------------------
{
    wxCriticalSectionLocker locker( m_critSect );
    if( m_pActiveDevData )
    {
        feature = m_pActiveDevData->featureVsTimeFeature;
        featureFullPath = m_pActiveDevData->featureVsTimePlotPath;
    }
}

//-----------------------------------------------------------------------------
long DevicePropertyHandler::GetActiveDeviceImagesPerDisplayCount( void ) const
//-----------------------------------------------------------------------------
{
    wxCriticalSectionLocker locker( m_critSect );
    return m_pActiveDevData ? m_pActiveDevData->imagesPerDisplayCount : 1;
}

//-----------------------------------------------------------------------------
void DevicePropertyHandler::GetActiveDevicePropTreeExpansionData( wxPropertyGrid* pPropGrid, std::set<wxString>& s, wxString& selectedGridProperty ) const
//-----------------------------------------------------------------------------
{
    if( pPropGrid )
    {
        wxPGProperty* pProp = pPropGrid->GetSelectedProperty();
        if( pProp )
        {
            selectedGridProperty = pProp->GetName();
        }
        wxPropertyGridConstIterator it = pPropGrid->GetIterator( wxPG_ITERATE_ALL );
        while( !it.AtEnd() )
        {
            if( ( *it )->IsExpanded() )
            {
                s.insert( ( *it )->GetName() );
            }
            ++it;
        }
    }
}

//-----------------------------------------------------------------------------
const SequencerSetToDisplayMap DevicePropertyHandler::GetActiveDeviceSequencerSetToDisplayMap( void ) const
//-----------------------------------------------------------------------------
{
    wxCriticalSectionLocker locker( m_critSect );
    return m_pActiveDevData ? m_pActiveDevData->sequencerSetToDisplayMap : SequencerSetToDisplayMap();
}

//-----------------------------------------------------------------------------
const SettingToDisplayDict DevicePropertyHandler::GetActiveDeviceSettingToDisplayDict( void ) const
//-----------------------------------------------------------------------------
{
    wxCriticalSectionLocker locker( m_critSect );
    return m_pActiveDevData ? m_pActiveDevData->settingToDisplayDict : SettingToDisplayDict();
}

//-----------------------------------------------------------------------------
const WizardFeatureMap DevicePropertyHandler::GetActiveDeviceSupportedWizards( void ) const
//-----------------------------------------------------------------------------
{
    wxCriticalSectionLocker locker( m_critSect );
    return m_pActiveDevData ? m_pActiveDevData->supportedWizards : WizardFeatureMap();
}

//-----------------------------------------------------------------------------
mvIMPACT::acquire::labs::VideoStream* DevicePropertyHandler::GetActiveDeviceVideoStream( void ) const
//-----------------------------------------------------------------------------
{
    wxCriticalSectionLocker locker( m_critSect );
    return m_pActiveDevData ? m_pActiveDevData->pVideoStream : 0;
}

//-----------------------------------------------------------------------------
bool DevicePropertyHandler::GetDeviceData( mvIMPACT::acquire::Device* pDev, mvIMPACT::acquire::FunctionInterface** ppFI /* = 0 */, const mvIMPACT::acquire::Statistics** ppS /* = 0 */, CaptureThread** ppCT /* = 0  */ ) const
//-----------------------------------------------------------------------------
{
    wxCriticalSectionLocker locker( m_critSect );
    DevToTreeMap::const_iterator it = m_devToTreeMap.find( pDev );
    if( it != m_devToTreeMap.end() )
    {
        GetInterfaceClasses( it->second, ppFI, ppS, ppCT );
        return true;
    }
    return false;
}

//-----------------------------------------------------------------------------
wxString DevicePropertyHandler::GetProductFromManufacturerInfo( mvIMPACT::acquire::Device* pDev )
//-----------------------------------------------------------------------------
{
    wxString product;
    if( pDev->manufacturerSpecificInformation.isValid() )
    {
        const wxArrayString tokens = wxSplit( ConvertedString( pDev->manufacturerSpecificInformation.read() ), wxT( ';' ) );
        if( !tokens.IsEmpty() )
        {
            // all parameters here use the <param>=<value> syntax (e.g. FW=3.5.67.3) except the product type
            // which is always the LAST token. Tokens are separated by a single ';'. Thus if a device specifies
            // a device type here this might look like this:
            // FW=2.17.360.0;DS1-024ZG-11111-00
            // So if the last token does NOT contain a '=' we assume this to be the product type!
            const wxArrayString keyValPair = wxSplit( tokens[tokens.size() - 1], wxT( '=' ) );
            if( keyValPair.size() == 1 )
            {
                product = tokens[tokens.size() - 1];
            }
        }
    }
    return product;
}

//-----------------------------------------------------------------------------
bool DevicePropertyHandler::IsBlueCOUGAR_X( Device* pDev )
//-----------------------------------------------------------------------------
{
    if( ( ConvertedString( pDev->product.read() ).Find( wxT( "mvBlueCOUGAR-X" ) ) == wxNOT_FOUND ) )
    {
        return false;
    }
    ComponentLocator locator( pDev->hDev() );
    PropertyI64 prop;
    locator.bindComponent( prop, "DeviceMACAddress" );
    if( !prop.isValid() )
    {
        return false;
    }

    const int64_type MAC( prop.read() );
    return ( ( ( MAC >= 0x0000000c8d508001LL ) && ( MAC <= 0x0000000c8d59a7bfLL ) ) ||
             ( ( MAC >= 0x0000000c8d600001LL ) && ( MAC <= 0x0000000c8d617fffLL ) ) ||
             ( ( MAC >= 0x0000000c8d630001LL ) && ( MAC <= 0x0000000c8d637fffLL ) ) ||
             ( ( MAC >= 0x0000000c8d700001LL ) && ( MAC <= 0x0000000c8d707fffLL ) ) ||
             ( ( MAC >= 0x0000000c8d708001LL ) && ( MAC <= 0x0000000c8d70bfffLL ) ) ||
             ( ( MAC >= 0x0000000c8d70c001LL ) && ( MAC <= 0x0000000c8d70cfffLL ) ) ||
             ( ( MAC >= 0x0000000c8d710001LL ) && ( MAC <= 0x0000000c8d717fffLL ) ) );
}

//-----------------------------------------------------------------------------
bool DevicePropertyHandler::IsBlueCOUGAR_XT( Device* pDev )
//-----------------------------------------------------------------------------
{
    if( ( ConvertedString( pDev->product.read() ).Find( wxT( "mvBlueCOUGAR-X" ) ) == wxNOT_FOUND ) )
    {
        return false;
    }
    ComponentLocator locator( pDev->hDev() );
    PropertyI64 prop;
    locator.bindComponent( prop, "DeviceMACAddress" );
    if( !prop.isValid() )
    {
        return false;
    }

    const int64_type MAC( prop.read() );
    return ( ( MAC >= 0x0000000c8d638001LL ) && ( MAC <= 0x0000000c8d6fffffLL ) );
}

//-----------------------------------------------------------------------------
bool DevicePropertyHandler::IsBlueNAOS( Device* pDev )
//-----------------------------------------------------------------------------
{
    return pDev->family.read().find( "mvBlueNAOS" ) != string::npos;
}

//-----------------------------------------------------------------------------
wxString DevicePropertyHandler::OpenDriver( const wxString& name, wxWindow* pParentWindow, unsigned int pendingImageQueueDepth )
//-----------------------------------------------------------------------------
{
#ifdef MEASSURE_DEVICE_OPEN_TIMES
    wxStopWatch stopWatch;
    long totalTime_ms = 0;
#endif //
    wxString timingData;
    string serialANSI( name.mb_str() );
    Device* pDev = m_devMgr.getDeviceBySerial( serialANSI );
    if( pDev && !pDev->isOpen() )
    {
        UpdateTreeMap();
        pDev->open();
        APPEND_TIMING_RESULT( timingData, wxT( "Device open" ), stopWatch, totalTime_ms )
        DevToTreeMap::iterator it = m_devToTreeMap.find( pDev );
        if( it != m_devToTreeMap.end() )
        {
            {
                wxCriticalSectionLocker locker( m_critSect );
                if( !it->second->pFuncInterface )
                {
                    it->second->pFuncInterface = new FunctionInterface( pDev );
                }
                if( !it->second->pCaptureThread )
                {
                    it->second->pCaptureThread = new CaptureThread( it->second->pFuncInterface, pDev, pParentWindow, pendingImageQueueDepth );
                    it->second->pCaptureThread->Create();
                    it->second->pCaptureThread->SetPriority( 80 );
                    it->second->pCaptureThread->SetImagesPerDisplayCount( it->second->imagesPerDisplayCount );
                    it->second->pCaptureThread->Run();
                    it->second->pCaptureThread->SetActive();
                }
                if( !it->second->pStatistics )
                {
                    it->second->pStatistics = new Statistics( pDev );
                }
                if( !it->second->pInfo )
                {
                    it->second->pInfo = new Info( pDev );
                }
                ComponentLocator locator( pDev->hDrv() );
                if( !it->second->acquisitionMode.isValid() )
                {
                    locator.bindComponent( it->second->acquisitionMode, "AcquisitionMode" );
                }
                if( !it->second->acquisitionFrameCount.isValid() )
                {
                    locator.bindComponent( it->second->acquisitionFrameCount, "AcquisitionFrameCount" );
                }
                if( !it->second->mvAcquisitionMemoryFrameCount.isValid() )
                {
                    locator.bindComponent( it->second->mvAcquisitionMemoryFrameCount, "mvAcquisitionMemoryFrameCount" );
                }
                if( !it->second->deviceRegistersValid.isValid() )
                {
                    locator.bindComponent( it->second->deviceRegistersValid, "DeviceRegistersValid" );
                    it->second->pCaptureThread->AttachDeviceRegistersValidProperty( &it->second->deviceRegistersValid );
                    if( it->second->deviceRegistersValid.isValid() )
                    {
                        it->second->pDeviceRegistersValidCallback = new DeviceRegistersValidCallback( pParentWindow );
                        it->second->pDeviceRegistersValidCallback->registerComponent( it->second->deviceRegistersValid );
#ifndef __clang_analyzer__ // The static analyzer assumes this event memory to be lost :-(
                        wxQueueEvent( pParentWindow->GetEventHandler(), new wxCommandEvent( deviceRegistersValidChanged, pParentWindow->GetId() ) );
#endif // #ifndef __clang_analyzer__
                    }
                }
                CheckForWizards( pDev, it->second );
                if( !m_pFullTree &&
                    ( m_vTreeProps.empty() || ( it->second->vDriverTrees.size() != m_vTreeProps.size() ) ) )
                {
                    CreateDriverTrees( it );
                }
                APPEND_TIMING_RESULT( timingData, wxT( "Feature tree creation(GUI overhead)" ), stopWatch, totalTime_ms )
                it->second->boWasLive = false;
            }

            if( m_pFullTree )
            {
                m_pFullTree->Draw( false );
                APPEND_TIMING_RESULT( timingData, wxT( "Full tree creation(GUI overhead)" ), stopWatch, totalTime_ms )
            }
        }
    }
    return timingData;
}

//-----------------------------------------------------------------------------
void DevicePropertyHandler::PrepareTreeArray( void )
//-----------------------------------------------------------------------------
{
    m_vTreeProps.clear();
    switch( m_viewMode )
    {
    case vmDeveloper:
        m_vTreeProps.push_back( TreeProp( dltCameraDescriptions, "dmltCameraDescriptions" ) );
        m_vTreeProps.push_back( TreeProp( dltDeviceSpecificData, "dmltDeviceSpecificData" ) );
        m_vTreeProps.push_back( TreeProp( dltEventSubSystemSettings, "dmltEventSubSystemSettings" ) );
        m_vTreeProps.push_back( TreeProp( dltEventSubSystemResults, "dmltEventSubSystemResults" ) );
        m_vTreeProps.push_back( TreeProp( dltRequestCtrl, "dmltRequestCtrl" ) );
        m_vTreeProps.push_back( TreeProp( dltInfo, "dmltInfo" ) );
        m_vTreeProps.push_back( TreeProp( dltRequest, "dmltRequest" ) );
        m_vTreeProps.push_back( TreeProp( dltIOSubSystem, "dmltIOSubSystem" ) );
        m_vTreeProps.push_back( TreeProp( dltSetting, "dmltSetting" ) );
        m_vTreeProps.push_back( TreeProp( dltStatistics, "dmltStatistics" ) );
        m_vTreeProps.push_back( TreeProp( dltSystemSettings, "dmltSystemSettings" ) );
        m_vTreeProps.push_back( TreeProp( dltImageMemoryManager, "dmltImageMemoryManager" ) );
        break;
    case vmStandard:
    default:
        m_vTreeProps.push_back( TreeProp( dltRequestCtrl, "Request Controls" ) );
        m_vTreeProps.push_back( TreeProp( dltSetting, "Image Settings" ) );
        PrepareTreeArrayCommon();
        break;
    }
}

//-----------------------------------------------------------------------------
void DevicePropertyHandler::PrepareTreeArrayCommon( void )
//-----------------------------------------------------------------------------
{
    m_vTreeProps.push_back( TreeProp( dltCameraDescriptions, "Camera Descriptions" ) );
    m_vTreeProps.push_back( TreeProp( "EventSubSystem", "Events" ) );
    m_vTreeProps.push_back( TreeProp( dltIOSubSystem, "Digital I/O" ) );
    m_vTreeProps.push_back( TreeProp( dltImageMemoryManager, "Image Memory Manager" ) );
    m_vTreeProps.push_back( TreeProp( dltInfo, "Info" ) );
    m_vTreeProps.push_back( TreeProp( dltRequest, "Requests" ) );
    m_vTreeProps.push_back( TreeProp( dltStatistics, "Statistics" ) );
    m_vTreeProps.push_back( TreeProp( dltSystemSettings, "System Settings" ) );
    m_vTreeProps.push_back( TreeProp( dltDeviceSpecificData, "Device Specific Data" ) );
}

//-----------------------------------------------------------------------------
void DevicePropertyHandler::SetActiveDevice( const wxString& devName )
//-----------------------------------------------------------------------------
{
    wxCriticalSectionLocker locker( m_critSect );
    string serialANSI( devName.mb_str() );
    Device* pDev = m_devMgr.getDeviceBySerial( serialANSI );

    if( pDev )
    {
        UpdateTreeMap();
        DevToTreeMap::iterator itNewDev = m_devToTreeMap.find( pDev );

        if( itNewDev != m_devToTreeMap.end() )
        {
            if( m_pActiveDevice != pDev )
            {
                if( m_pActiveDevice )
                {
                    DevToTreeMap::iterator itCurDev = m_devToTreeMap.find( m_pActiveDevice );
                    if( itCurDev != m_devToTreeMap.end() )
                    {
                        if( !m_pFullTree )
                        {
                            GetActiveDevicePropTreeExpansionData( m_pPGDevice->GetGrid(), m_pActiveDevData->expandedGridProperties, m_pActiveDevData->selectedGridProperty );
                            wxWindowUpdateLocker updateLock( m_pPGDevice->GetGrid() );
                            delete itCurDev->second->pDriverTree;
                            itCurDev->second->pDriverTree = 0;
                            delete itCurDev->second->pDeviceTree;
                            itCurDev->second->pDeviceTree = 0;
                            DeleteDriverTrees( itCurDev );
                        }
                        CaptureThread* pThread = itCurDev->second->pCaptureThread;
                        if( pThread )
                        {
                            itCurDev->second->boWasLive = pThread->GetLiveMode();
                            pThread->SetLiveMode( false );
                        }
                        else
                        {
                            itCurDev->second->boWasLive = false;
                        }
                    }
                }

                m_pActiveDevice = pDev;
                m_pActiveDevData = itNewDev->second;

                if( m_pFullTree )
                {
                    m_fullTreeChangedCounter = m_pFullTree->Draw( false );
                }
                else
                {
                    if( !itNewDev->second->pDriverTree )
                    {
                        itNewDev->second->pDriverTree = new PropTree( pDev->deviceDriverFeatureList(), "Driver", m_pPGDevice, static_cast<EDisplayFlags>( GetDisplayFlags() ) );
                    }
                    m_actDrvListChangedCounter = itNewDev->second->pDriverTree->Draw( false );

                    if( !itNewDev->second->pDeviceTree )
                    {
                        itNewDev->second->pDeviceTree = new PropTree( pDev->hDev(), "Device", m_pPGDevice, static_cast<EDisplayFlags>( GetDisplayFlags() ) );
                    }
                    m_actDevListChangedCounter = itNewDev->second->pDeviceTree->Draw( false );
                    if( pDev->isOpen() )
                    {
                        CreateDriverTrees( itNewDev );
                    }
                }

                const PropTreeVector::size_type vSize = itNewDev->second->vDriverTrees.size();
                for( PropTreeVector::size_type i = 0; i < vSize; i++ )
                {
                    if( itNewDev->second->vDriverTrees[i] )
                    {
                        m_vTreeProps[i].ChangedCounter = itNewDev->second->vDriverTrees[i]->Draw( false );
                    }
                }

                SetActiveDevicePropTreeExpansionData( m_pPGDevice->GetGrid(), m_pActiveDevData->expandedGridProperties, m_pActiveDevData->selectedGridProperty );

                CaptureThread* pThread = itNewDev->second->pCaptureThread;
                if( pThread )
                {
                    pThread->SetActive();
                    if( itNewDev->second->boWasLive )
                    {
                        itNewDev->second->boWasLive = pThread->GetLiveMode();
                        pThread->SetLiveMode( true );
                    }
                }
            }
        }
    }
}

//-----------------------------------------------------------------------------
void DevicePropertyHandler::SetActiveDeviceFeatureVsTimePlotInfo( Component feature, const wxString& featureFullPath )
//-----------------------------------------------------------------------------
{
    wxCriticalSectionLocker locker( m_critSect );
    if( m_pActiveDevData )
    {
        m_pActiveDevData->featureVsTimeFeature = feature;
        m_pActiveDevData->featureVsTimePlotPath = featureFullPath;
    }
}

//-----------------------------------------------------------------------------
void DevicePropertyHandler::SetActiveDeviceImagesPerDisplayCount( long imagesPerDisplayCount )
//-----------------------------------------------------------------------------
{
    wxCriticalSectionLocker locker( m_critSect );
    if( m_pActiveDevData )
    {
        m_pActiveDevData->imagesPerDisplayCount = imagesPerDisplayCount;
        if( m_pActiveDevData->pCaptureThread )
        {
            m_pActiveDevData->pCaptureThread->SetImagesPerDisplayCount( imagesPerDisplayCount );
        }
    }
}

//-----------------------------------------------------------------------------
void DevicePropertyHandler::SetActiveDevicePropTreeExpansionData( wxPropertyGrid* pPropGrid, std::set<wxString>& s, wxString& selectedGridProperty )
//-----------------------------------------------------------------------------
{
    wxWindowUpdateLocker updateLocker( pPropGrid );
    if( pPropGrid )
    {
        const std::set<wxString>::const_iterator itEND = s.end();
        std::set<wxString>::const_iterator it = s.begin();
        while( it != itEND )
        {
            wxPGProperty* pProp = pPropGrid->GetProperty( *it );
            if( pProp )
            {
                pPropGrid->Expand( pProp );
            }
            ++it;
        }
        if( !selectedGridProperty.IsEmpty() )
        {
            pPropGrid->SelectProperty( selectedGridProperty, true );
            selectedGridProperty.Clear();
        }
    }
    s.clear();
}

//-----------------------------------------------------------------------------
void DevicePropertyHandler::SetActiveDeviceSequencerSetToDisplayMap( const SequencerSetToDisplayMap& m )
//-----------------------------------------------------------------------------
{
    wxCriticalSectionLocker locker( m_critSect );
    if( m_pActiveDevData )
    {
        m_pActiveDevData->sequencerSetToDisplayMap = m;
    }
}

//-----------------------------------------------------------------------------
void DevicePropertyHandler::SetActiveDeviceSettingToDisplayDict( const SettingToDisplayDict& m )
//-----------------------------------------------------------------------------
{
    wxCriticalSectionLocker locker( m_critSect );
    if( m_pActiveDevData )
    {
        m_pActiveDevData->settingToDisplayDict = m;
    }
}

//-----------------------------------------------------------------------------
void DevicePropertyHandler::SetActiveDeviceVideoStream( mvIMPACT::acquire::labs::VideoStream* pVideoStream )
//-----------------------------------------------------------------------------
{
    wxCriticalSectionLocker locker( m_critSect );
    if( m_pActiveDevData )
    {
        DeleteElement( m_pActiveDevData->pVideoStream );
        m_pActiveDevData->pVideoStream = pVideoStream;
    }
}

//-----------------------------------------------------------------------------
void DevicePropertyHandler::SetViewMode( TViewMode mode, bool boUseHexIndices, bool boUseDisplayNames, bool boUseSelectorGrouping )
//-----------------------------------------------------------------------------
{
    if( ( mode != m_viewMode ) || ( boUseHexIndices != m_boUseHexIndices ) ||
        ( boUseDisplayNames != m_boUseDisplayNames ) || ( boUseSelectorGrouping != m_boUseSelectorGrouping ) )
    {
        m_viewMode = mode;
        m_boUseHexIndices = boUseHexIndices;
        m_boUseDisplayNames = boUseDisplayNames;
        m_boUseSelectorGrouping = boUseSelectorGrouping;
        if( m_pPGDevice->GetGrid() )
        {
            GetActiveDevicePropTreeExpansionData( m_pPGDevice->GetGrid(), m_pActiveDevData->expandedGridProperties, m_pActiveDevData->selectedGridProperty );
        }
        if( m_pFullTree )
        {
            ReCreateFullTree();
        }
        else
        {
            ReCreateDriverTrees();
        }
        if( m_pActiveDevData )
        {
            SetActiveDevicePropTreeExpansionData( m_pPGDevice->GetGrid(), m_pActiveDevData->expandedGridProperties, m_pActiveDevData->selectedGridProperty );
        }
    }
}

//-----------------------------------------------------------------------------
void DevicePropertyHandler::ReCreateDriverTrees( void )
//-----------------------------------------------------------------------------
{
    DevToTreeMap::iterator itCurDev = m_devToTreeMap.find( m_pActiveDevice );
    if( itCurDev != m_devToTreeMap.end() )
    {
        DeleteDriverTrees( itCurDev );
        CreateDriverTrees( itCurDev );
    }
}

//-----------------------------------------------------------------------------
void DevicePropertyHandler::ReCreateFullTree( void )
//-----------------------------------------------------------------------------
{
    delete m_pFullTree;
    // there has been one before, so there should be one now! WORK AROUND!!!
    m_pFullTree = new PropTree( ROOT_LIST, "Full Tree", m_pPGDevice, static_cast<EDisplayFlags>( GetDisplayFlags() ) );
    m_pFullTree->Draw( false );
}

//-----------------------------------------------------------------------------
void DevicePropertyHandler::UpdateTree( PropTree* pTree, unsigned int& actChangedCounter, const bool boForceRedraw, const bool boForceCleanup )
//-----------------------------------------------------------------------------
{
    if( boForceCleanup )
    {
        pTree->CleanupTree();
    }
    if( boForceRedraw || ( actChangedCounter != pTree->GetChangedCounter() ) )
    {
        actChangedCounter = pTree->Draw( boForceRedraw );
    }
}

//-----------------------------------------------------------------------------
void DevicePropertyHandler::UpdateTreeMap( void )
//-----------------------------------------------------------------------------
{
    if( m_devMgr.deviceCount() != m_devToTreeMap.size() )
    {
        for( unsigned int i = 0; i < m_devMgr.deviceCount(); i++ )
        {
            DevToTreeMap::iterator it = m_devToTreeMap.find( m_devMgr[i] );
            if( it == m_devToTreeMap.end() )
            {
                PropTree* pDriverTree( m_pFullTree ? 0 : new PropTree( m_devMgr[i]->deviceDriverFeatureList(), "Driver", m_pPGDevice, static_cast<EDisplayFlags>( GetDisplayFlags() ) ) );
                PropTree* pDeviceTree( m_pFullTree ? 0 : new PropTree( m_devMgr[i]->hDev(), "Device", m_pPGDevice, static_cast<EDisplayFlags>( GetDisplayFlags() ) ) );
                m_devToTreeMap.insert( make_pair( m_devMgr[i], new DeviceData( pDriverTree, pDeviceTree ) ) );
            }
        }
    }
}

//-----------------------------------------------------------------------------
void DevicePropertyHandler::ValidateTrees( bool boForceCleanup /* = false */ )
//-----------------------------------------------------------------------------
{
    bool boForceRedraw = false;
    if( m_componentVisibility != GlobalDataStorage::Instance()->GetComponentVisibility() )
    {
        boForceRedraw = true;
        m_componentVisibility = GlobalDataStorage::Instance()->GetComponentVisibility();
    }

    if( m_pFullTree )
    {
        UpdateTree( m_pFullTree, m_fullTreeChangedCounter, boForceRedraw, boForceCleanup );
    }
    else if( m_pActiveDevData )
    {
        UpdateTree( m_pActiveDevData->pDriverTree, m_actDrvListChangedCounter, boForceRedraw, boForceCleanup );
        UpdateTree( m_pActiveDevData->pDeviceTree, m_actDevListChangedCounter, boForceRedraw, boForceCleanup );
        const PropTreeVector::size_type vSize = m_pActiveDevData->vDriverTrees.size();
        for( PropTreeVector::size_type i = 0; i < vSize; i++ )
        {
            if( m_pActiveDevData->vDriverTrees[i] )
            {
                UpdateTree( m_pActiveDevData->vDriverTrees[i], m_vTreeProps[i].ChangedCounter, boForceRedraw, boForceCleanup );
            }
        }
    }
}
