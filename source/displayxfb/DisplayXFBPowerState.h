/** @file   com_tsoniq_driver_DisplayXFBPowerState.h
 *  @brief  Power management states.
 *
 *  Copyright (c) 2010 tsoniq. All rights reserved.
 */

#ifndef COM_TSONIQ_com_tsoniq_driver_DisplayXFBPowerState_H
#define COM_TSONIQ_com_tsoniq_driver_DisplayXFBPowerState_H   (1)

#include <IOKit/IOPlatformExpert.h>
#include "DisplayXFBNames.h"

// Power modes. These are indexes in to the DisplayXFBDriverPowerStates[] array.
static const unsigned kDisplayXFBPowerStateOff      =   0;      //! Device is off
static const unsigned kDisplayXFBPowerStateSleep    =   1;      //! Device is dozy
static const unsigned kDisplayXFBPowerStateWake     =   2;      //! Device at full power
static const unsigned kDisplayXFBNumPowerStates     =   3;      //! The total number of power states


extern IOPMPowerState DisplayXFBDriverPowerStates[kDisplayXFBNumPowerStates];

#endif      // COM_TSONIQ_com_tsoniq_driver_DisplayXFBPowerState_H
