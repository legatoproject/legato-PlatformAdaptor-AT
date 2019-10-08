/** @file pa_mdc_local.h
 *
 * Copyright (C) Sierra Wireless Inc.
 */

#ifndef LEGATO_PAMDCLOCAL_INCLUDE_GUARD
#define LEGATO_PAMDCLOCAL_INCLUDE_GUARD


#include "legato.h"

//--------------------------------------------------------------------------------------------------
/**
 * Define pa mdc local string size
 */
//--------------------------------------------------------------------------------------------------
#define PA_MDC_LOCAL_STRING_SIZE    100

//--------------------------------------------------------------------------------------------------
/**
 * Define pa mdc local long response string size
 */
//--------------------------------------------------------------------------------------------------
#define PA_MDC_LOCAL_STRING_EXTRA_LONG_SIZE    500

//--------------------------------------------------------------------------------------------------
/**
 * Define dot char
 */
//--------------------------------------------------------------------------------------------------
#define DOT_CHAR            '.'

//--------------------------------------------------------------------------------------------------
/**
 * Define comma char
 */
//--------------------------------------------------------------------------------------------------
#define COMMA_CHAR          ','

//--------------------------------------------------------------------------------------------------
/**
 * Define colons char
 */
//--------------------------------------------------------------------------------------------------
#define COLONS_CHAR         ':'

//--------------------------------------------------------------------------------------------------
/**
 * NB dot char in IPV6 string address
 */
//--------------------------------------------------------------------------------------------------
#define NB_DOT_IPV4_ADDR    3

//--------------------------------------------------------------------------------------------------
/**
 * NB dot char in IPV4 string address
 */
//--------------------------------------------------------------------------------------------------
#define NB_DOT_IPV6_ADDR    15


//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to initialize the mdc module
 *
 * @return LE_FAULT         The function failed to initialize the module.
 * @return LE_OK            The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mdc_Init
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Get profileIndex from PDP cid
 *
 * @return The profileIndex (session ID) mapped on cid (pdp context)
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED uint32_t pa_mdc_local_GetProfileIndexFromCid
(
    uint32_t cid                ///< [IN]  The cid (pdp context)
);

//--------------------------------------------------------------------------------------------------
/**
 * Set PDP cid from profileIndex
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void pa_mdc_local_SetCidFromProfileIndex
(
    uint32_t profileIndex,      ///< [IN]  The session ID (=profileIndex)
    uint32_t cid                ///< [IN]  The cid (pdp context)
);

//--------------------------------------------------------------------------------------------------
/**
 * Get PDP cid from profileIndex
 *
 * @return The cid (pdp context) mapped on profileIndex
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED uint32_t pa_mdc_local_GetCidFromProfileIndex
(
    uint32_t profileIndex       ///< [IN]  The session ID (=profileIndex)
);

#endif // LEGATO_PAMDCLOCAL_INCLUDE_GUARD
