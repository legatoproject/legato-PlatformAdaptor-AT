/** @file pa_at_local.h
 *
 * Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
 */

#ifndef LEGATO_PAATLOCAL_INCLUDE_GUARD
#define LEGATO_PAATLOCAL_INCLUDE_GUARD

//--------------------------------------------------------------------------------------------------
/**
 * This is used to get the device reference of the AT port.
 *
 **/
//--------------------------------------------------------------------------------------------------
le_atClient_DeviceRef_t pa_at_GetAtDeviceRef
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * This is used to get the device reference of the PPP port.
 *
 **/
//--------------------------------------------------------------------------------------------------
le_atClient_DeviceRef_t pa_at_GetPppDeviceRef
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * This is used to get the path of the PPP port.
 *
 **/
//--------------------------------------------------------------------------------------------------
char* pa_at_GetPppPath
(
    void
);


#endif // LEGATO_PAATLOCAL_INCLUDE_GUARD
