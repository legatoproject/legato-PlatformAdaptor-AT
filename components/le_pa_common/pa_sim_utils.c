/** @file pa_sim_utils.c
 *
 * APIs to translate Sim string code
 *
 * Copyright (C) Sierra Wireless Inc.
 */

#include "legato.h"
#include "interfaces.h"

#ifdef MK_ATPROXY_CONFIG_CLIB
#include "le_atClientIF.h"
#endif

#include "pa_utils.h"
#include "pa_sim_utils_local.h"

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
)
{
    switch (atoi(valPtr))
    {
        case 310:   /* SIM not inserted */
        {
            *statePtr = LE_SIM_ABSENT;
            break;
        }
        case 311:   /* SIM PIN required */
        case 312:   /* PH-SIM PIN required */
        case 317:   /* SIM PIN2 required */
        {
            *statePtr = LE_SIM_INSERTED;
            break;
        }
        case 515:   /* Please wait, init or command processing in progress. */
        {
            *statePtr = LE_SIM_BUSY;
            break;
        }
        case 316:   /* SIM PUK required */
        case 318:   /* SIM PUK2 required */
        {
            *statePtr = LE_SIM_BLOCKED;
            break;
        }
        case 313:   /* SIM failure */
        default:
        {
            *statePtr = LE_SIM_STATE_UNKNOWN;
        }
    }
}

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
)
{
    switch (atoi(valPtr))
    {

        case 5:     /* PH-SIM PIN required (SIM lock) */
        case 11:    /* SIM PIN required */
        case 16:    /* Incorrect password Bad user pin */
        case 17:    /* SIM PIN2 required */
        {
            *statePtr = LE_SIM_INSERTED;
            break;
        }
        case 10:    /* SIM not inserted */
        {
            *statePtr = LE_SIM_ABSENT;
            break;
        }
        case 12:    /* SIM PUK required */
        case 18:    /* SIM PUK2 required */
        {
            *statePtr = LE_SIM_BLOCKED;
            break;
        }
        default:
        {
            *statePtr = LE_SIM_STATE_UNKNOWN;
        }
    }
}

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
)
{
    if          (strcmp(valPtr, "READY") == 0)
    {
        *statePtr = LE_SIM_READY;
    }
    else if (
                (strcmp(valPtr, "SIM PIN") == 0)
                ||
                (strcmp(valPtr, "PH-SIM PIN") == 0)
                ||
                (strcmp(valPtr, "SIM PIN2") == 0)
            )
    {
        *statePtr = LE_SIM_INSERTED;
    }
    else if (
                (strcmp(valPtr, "SIM PUK") == 0)
                ||
                (strcmp(valPtr, "SIM PUK2") == 0)
            )
    {
        *statePtr = LE_SIM_BLOCKED;
    }
    else
    {
        *statePtr = LE_SIM_STATE_UNKNOWN;
    }
}

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
)
{
    bool result = true;

    #define MAXLINE     18

    char line[MAXLINE+1];
    memcpy(line, lineStr, MAXLINE);

    line[MAXLINE] = '\0';
    *statePtr = LE_SIM_STATE_UNKNOWN;

    pa_utils_CountAndIsolateLineParameters(line);

    if (FIND_STRING("+CME ERROR:", pa_utils_IsolateLineParameter(line, 1)))
    {
        pa_sim_utils_CheckCmeErrorCode(pa_utils_IsolateLineParameter(line, 2), statePtr);
    }
    else if (FIND_STRING("+CMS ERROR:", pa_utils_IsolateLineParameter(line, 1)))
    {
        pa_sim_utils_CheckCmsErrorCode(pa_utils_IsolateLineParameter(line, 2), statePtr);
    }
    else if (FIND_STRING("+CPIN:", pa_utils_IsolateLineParameter(line, 1)))
    {
        pa_sim_utils_CheckCpinCode(pa_utils_IsolateLineParameter(line, 2), statePtr);
    }
    else
    {
        LE_DEBUG("this pattern is not expected -%s-", line);
        *statePtr = LE_SIM_STATE_UNKNOWN;
        result = false;
    }

    LE_DEBUG("SIM Card Status %d", *statePtr);

    return result;
}
