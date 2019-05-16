/** @file pa_sim_utils_local.h
 *
 * APIs to translate Sim string code
 *
 * Copyright (C) Sierra Wireless Inc.
 */

#ifndef LEGATO_PASIMUTILSLOCAL_INCLUDE_GUARD
#define LEGATO_PASIMUTILSLOCAL_INCLUDE_GUARD


//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to translate the code status for +CMS ERROR parsing.
 *
 */
//--------------------------------------------------------------------------------------------------
void pa_sim_utils_CheckCmsErrorCode
(
    const char*       valPtr,   ///< [IN] the value to change
    le_sim_States_t*  statePtr  ///< [OUT] SIM state
);


//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to translate the code status for +CME ERROR parsing
 *
 */
//--------------------------------------------------------------------------------------------------
void pa_sim_utils_CheckCmeErrorCode
(
    const char*      valPtr,   ///< [IN] the value to change
    le_sim_States_t* statePtr  ///< [OUT] SIM state
);

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to translate the code status for +CPIN parsing
 *
 */
//--------------------------------------------------------------------------------------------------
void pa_sim_utils_CheckCpinCode
(
    const char*      valPtr,   ///< [IN] the value to change
    le_sim_States_t* statePtr  ///< [OUT] SIM state
);

//--------------------------------------------------------------------------------------------------
/**
 * This function is called to check the status of a line received
 *
 * @return true and filed statePtr
 * @return false
 */
//--------------------------------------------------------------------------------------------------
bool pa_sim_utils_CheckStatus
(
    const char*      lineStr,   ///< [IN] line to parse
    le_sim_States_t* statePtr   ///< [OUT] SIM state
);


#endif // LEGATO_PASIMUTILSLOCAL_INCLUDE_GUARD
