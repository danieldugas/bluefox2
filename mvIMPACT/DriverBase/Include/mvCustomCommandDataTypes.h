//-----------------------------------------------------------------------------
// (C) Copyright 2005 - 2021 by MATRIX VISION GmbH
//
// This software is provided by MATRIX VISION GmbH "as is"
// and any express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular purpose
// are disclaimed.
//
// In no event shall MATRIX VISION GmbH be liable for any direct,
// indirect, incidental, special, exemplary, or consequential damages
// (including, but not limited to, procurement of substitute goods or services;
// loss of use, data, or profits; or business interruption) however caused and
// on any theory of liability, whether in contract, strict liability, or tort
// (including negligence or otherwise) arising in any way out of the use of
// this software, even if advised of the possibility of such damage.

//-----------------------------------------------------------------------------
#ifndef MV_CUSTOM_COMMAND_DATA_TYPES_H
#ifndef DOXYGEN_SHOULD_SKIP_THIS
#   define MV_CUSTOM_COMMAND_DATA_TYPES_H MV_CUSTOM_COMMAND_DATA_TYPES_H
#endif // #ifndef DOXYGEN_SHOULD_SKIP_THIS
//-----------------------------------------------------------------------------

#ifdef __GNUC__
#   define ATTR_PACK __attribute__((packed)) __attribute__ ((aligned (1)))
#elif defined _MSC_VER || defined __BORLANDC__
#   define ATTR_PACK
#   pragma pack(push, 1) // 1 byte structure alignment
#elif !defined(SWIG)
#   error add your compiler specific structure alignment function here
#endif

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#if defined(MVIMPACT_ACQUIRE_CUSTOM_COMMANDS_H) || defined(DOXYGEN_CPP_DOCUMENTATION)
namespace mvIMPACT
{
namespace acquire
{
namespace GenICam
{
#endif // #if defined(MVIMPACT_ACQUIRE_CUSTOM_COMMANDS_H) || defined(DOXYGEN_CPP_DOCUMENTATION)

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#   if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
typedef __int16 int16_type;
typedef unsigned __int16 uint16_type;
typedef __int32 int32_type;
typedef unsigned __int32 uint32_type;
#       ifdef __BORLANDC__ // is Borland compiler?
#           pragma option push -b // force enums to the size of integer
#       endif // __BORLANDC__
#   elif defined(linux) || defined(__linux) || defined(__linux__) || defined(__GNUC__)
#       ifndef WRAP_ANY
#           include <stdint.h>
#       endif // #ifndef WRAP_ANY
typedef int16_t int16_type;
typedef uint16_t uint16_type;
typedef int32_t int32_type;
typedef uint32_t uint32_type;
#   else
#       error "unsupported target environment"
#   endif // #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
#endif // #ifndef DOXYGEN_SHOULD_SKIP_THIS

//-----------------------------------------------------------------------------
/// \brief Defines valid request transmission modes.
/**
 * Using the custom command interpreter images can requested again from a device
 * supporting it in different resolutions. This enumeration can be used to define
 * what/how to transmit the image.
 *
 * \since 2.18.0
 * \ingroup GenICamInterfaceDevice
 */
enum TRequestTransmissionMode
//-----------------------------------------------------------------------------
{
    /// \brief Request transmission of an image in full resolution.
    rtmFullResolution
};

//-----------------------------------------------------------------------------
/// \brief Defines valid parameters that can be modified within a sequencer set at runtime.
/**
 * Using the custom command interpreter features controlled by a running sequencer can
 * be modified while the sequencer is running. This enumeration can be used to define
 * which parameter shall/can be modified.
 *
 * \since 2.18.0
 * \ingroup GenICamInterfaceDevice
 */
enum TSequencerSetParameter
//-----------------------------------------------------------------------------
{
    /// \brief Requests the <tt>CounterDuration[CounterSelector=Counter1]</tt> property to be modified in a user selected sequencer set.
    sspCounterDuration_Counter1,
    /// \brief Requests the <tt>CounterDuration[CounterSelector=Counter2]</tt> property to be modified in a user selected sequencer set.
    sspCounterDuration_Counter2,
    /// \brief Requests the <tt>CounterDuration[CounterSelector=Counter3]</tt> property to be modified in a user selected sequencer set.
    sspCounterDuration_Counter3,
    /// \brief Requests the <tt>CounterDuration[CounterSelector=Counter4]</tt> property to be modified in a user selected sequencer set.
    sspCounterDuration_Counter4,
    /// \brief Requests the <tt>CounterDuration[CounterSelector=Counter5]</tt> property to be modified in a user selected sequencer set.
    sspCounterDuration_Counter5,
    /// \brief Requests the <tt>CounterDuration[CounterSelector=Counter6]</tt> property to be modified in a user selected sequencer set.
    sspCounterDuration_Counter6,
    /// \brief Requests the <tt>CounterDuration[CounterSelector=Counter7]</tt> property to be modified in a user selected sequencer set.
    sspCounterDuration_Counter7,
    /// \brief Requests the <tt>CounterDuration[CounterSelector=Counter8]</tt> property to be modified in a user selected sequencer set.
    sspCounterDuration_Counter8,
    /// \brief Requests the \c ExposureTime property to be modified in a user selected sequencer set.
    sspExposureTime,
    /// \brief Requests the <tt>Gain[GainSelector=AnalogAll]</tt> property to be modified in a user selected sequencer set.
    sspGain_AnalogAll,
    /// \brief Requests the <tt>Gain[GainSelector=AnalogRed]</tt> property to be modified in a user selected sequencer set.
    sspGain_AnalogRed,
    /// \brief Requests the <tt>Gain[GainSelector=AnalogGreen]</tt> property to be modified in a user selected sequencer set.
    sspGain_AnalogGreen,
    /// \brief Requests the <tt>Gain[GainSelector=AnalogBlue]</tt> property to be modified in a user selected sequencer set.
    sspGain_AnalogBlue,
    /// \brief Requests the <tt>Gain[GainSelector=AnalogTap1]</tt> property to be modified in a user selected sequencer set.
    sspGain_AnalogTap1,
    /// \brief Requests the <tt>Gain[GainSelector=AnalogTap2]</tt> property to be modified in a user selected sequencer set.
    sspGain_AnalogTap2,
    /// \brief Requests the <tt>Gain[GainSelector=AnalogTap3]</tt> property to be modified in a user selected sequencer set.
    sspGain_AnalogTap3,
    /// \brief Requests the <tt>Gain[GainSelector=AnalogTap4]</tt> property to be modified in a user selected sequencer set.
    sspGain_AnalogTap4,
    /// \brief Requests the <tt>OffsetX</tt> property to be modified in a user selected sequencer set.
    /**
     * \since 2.34.0, requires firmware version >= 2.35
     */
    sspOffsetX,
    /// \brief Requests the <tt>OffsetY</tt> property to be modified in a user selected sequencer set.
    /**
     * \since 2.34.0, requires firmware version >= 2.35
     */
    sspOffsetY,
    /// \brief Requests the <tt>Width</tt> property to be modified in a user selected sequencer set.
    /**
     * \since 2.36.0, requires firmware version >= 2.36
     */
    sspWidth,
    /// \brief Requests the <tt>Height</tt> property to be modified in a user selected sequencer set.
    /**
     * \since 2.36.0, requires firmware version >= 2.36
     */
    sspHeight,
    /// \brief Requests the <tt>BinningVertical</tt> property to be modified in a user selected sequencer set.
    /**
     * \since 2.45.0, requires firmware version >= 2.44
     */
    sspBinningVertical,
    /// \brief Requests the <tt>BinningHorizontal</tt> property to be modified in a user selected sequencer set.
    /**
     * \since 2.45.0, requires firmware version >= 2.44
     */
    sspBinningHorizontal,
    /// \brief Requests the <tt>DecimationVertical</tt> property to be modified in a user selected sequencer set.
    /**
     * \since 2.45.0, requires firmware version >= 2.44
     */
    sspDecimationVertical,
    /// \brief Requests the <tt>DecimationHorizontal</tt> property to be modified in a user selected sequencer set.
    /**
     * \since 2.45.0, requires firmware version >= 2.44
     */
    sspDecimationHorizontal
};

#if !defined(DOXYGEN_SHOULD_SKIP_THIS) && !defined(WRAP_ANY)
//-----------------------------------------------------------------------------
enum TCustomCommand
//-----------------------------------------------------------------------------
{
    ccSendFrame = 0x0001,
    ccUpdateSequencerSet = 0x0002,
    /// Private custom commands start here!
    ccLAST = 0x8000
};

//-----------------------------------------------------------------------------
struct CustomCommandProtocolHeader
//-----------------------------------------------------------------------------
{
    uint16_type interpreterVersionMajor;
    uint16_type interpreterVersionMinor;
    uint16_type totalMessageLength;
    uint16_type request_id;
} ATTR_PACK;

#define VARS_SFCPacketHeader    \
    uint16_type command;        \
    uint16_type messageLength;

#define VARS_SFCPacketUpdateSequencerSet    \
    uint16_type sequencerSet;               \
    uint16_type sequencerSetParameter;

//-----------------------------------------------------------------------------
struct CustomCommandHeader
//-----------------------------------------------------------------------------
{
    VARS_SFCPacketHeader
} ATTR_PACK;

//-----------------------------------------------------------------------------
struct CustomCommandSendFrame
//-----------------------------------------------------------------------------
{
    VARS_SFCPacketHeader
    uint32_type offsetX;
    uint32_type offsetY;
    uint32_type width;
    uint32_type height;
    uint32_type timestamp_high;
    uint32_type timestamp_low;
    uint32_type identifier;
} ATTR_PACK;

//-----------------------------------------------------------------------------
struct CustomCommandUpdateSequencerSet
//-----------------------------------------------------------------------------
{
    VARS_SFCPacketHeader
    VARS_SFCPacketUpdateSequencerSet
} ATTR_PACK;

//-----------------------------------------------------------------------------
struct CustomCommandUpdateSequencerSetI64
//-----------------------------------------------------------------------------
{
    VARS_SFCPacketHeader
    VARS_SFCPacketUpdateSequencerSet
    int32_type value_high;
    int32_type value_low;
} ATTR_PACK;

//-----------------------------------------------------------------------------
struct CustomCommandUpdateSequencerSetF
//-----------------------------------------------------------------------------
{
    VARS_SFCPacketHeader
    VARS_SFCPacketUpdateSequencerSet
    char data[8];
} ATTR_PACK;

typedef enum TRequestTransmissionMode TRequestTransmissionMode;
typedef enum TSequencerSetParameter TSequencerSetParameter;
typedef enum TCustomCommand TCustomCommand;
typedef struct CustomCommandProtocolHeader CustomCommandProtocolHeader;
typedef struct CustomCommandHeader CustomCommandHeader;
typedef struct CustomCommandSendFrame CustomCommandSendFrame;
typedef struct CustomCommandUpdateSequencerSet CustomCommandUpdateSequencerSet;
typedef struct CustomCommandUpdateSequencerSetI64 CustomCommandUpdateSequencerSetI64;
typedef struct CustomCommandUpdateSequencerSetF CustomCommandUpdateSequencerSetF;
#endif // #if !defined(DOXYGEN_SHOULD_SKIP_THIS) && !defined(WRAP_ANY)

// restore Borland compiler switch 'force enums to the size of integer'
#if !defined(DOXYGEN_SHOULD_SKIP_THIS) && !defined(WRAP_ANY)
#   ifdef _WIN32
#       ifdef __BORLANDC__
#           pragma option pop
#       endif // __BORLANDC__
#   endif // _WIN32
#endif // #if !defined(DOXYGEN_SHOULD_SKIP_THIS) && !defined(WRAP_ANY)

#if defined(MVIMPACT_ACQUIRE_CUSTOM_COMMANDS_H) || defined(DOXYGEN_CPP_DOCUMENTATION)
} // namespace GenICam
} // namespace acquire
} // namespace mvIMPACT
#endif // #if defined(MVIMPACT_ACQUIRE_CUSTOM_COMMANDS_H) || defined(DOXYGEN_CPP_DOCUMENTATION)

#ifdef __cplusplus
}
#endif // __cplusplus

#ifdef __GNUC__
#   undef ATTR_PACK
#elif defined _MSC_VER || defined __BORLANDC__
#   undef ATTR_PACK
#   pragma pack(pop) // restore previous structure alignment
#elif !defined(SWIG)
#   error restore your compiler specific structure alignment here if necessary
#endif

#endif /*MV_CUSTOM_COMMAND_DATA_TYPES_H*/
