/** @file pa_common_local.h
 *
 * Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
 */

#ifndef LEGATO_PACOMMONLOCAL_INCLUDE_GUARD
#define LEGATO_PACOMMONLOCAL_INCLUDE_GUARD


#include "legato.h"

#define FIND_STRING(stringToFind, intoString) \
            (strncmp(stringToFind,intoString,sizeof(stringToFind)-1)==0)

#define COMMAND_LEN_MAX             50
#define DEFAULT_TIMEOUT             30000

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to initialize the mcc module
 *
 * @return LE_FAULT         The function failed to initialize the module.
 * @return LE_OK            The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_common_Init
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * This function gets the SierraWireless proprietary indicator +WIND
 *
 * @return LE_FAULT         The function failed.
 * @return LE_TIMEOUT       No response was received.
 * @return LE_OK            The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_common_GetWindIndicator
(
    uint32_t*  windPtr
);

//--------------------------------------------------------------------------------------------------
/**
 * This function sets the SierraWireless proprietary indicator +WIND
 *
 * @return LE_FAULT         The function failed.
 * @return LE_TIMEOUT       No response was received.
 * @return LE_OK            The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_common_SetWindIndicator
(
    uint32_t  wind
);



//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to count the number of paramter between ',' and ':' and to set
 * all ',' with '\0' and the new char after ':' to '\0'
 *
 * @return the number of paremeter in the line
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED uint32_t le_atClient_cmd_CountLineParameter
(
    char        *linePtr       ///< [IN/OUT] line to parse
);

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to count the get the @ref pos parameter of Line
 *
 *
 * @return pointer to the string at pos position in Line
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED char* le_atClient_cmd_GetLineParameter
(
    const char* linePtr,   ///< [IN] Line to read
    uint32_t    pos     ///< [IN] Position to read
);

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to copy string from inBuffer to outBuffer without '"'
 *
 *
 * @return number of char really copy
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED uint32_t le_atClient_cmd_CopyStringWithoutQuote
(
    char*       outBufferPtr,  ///< [OUT] final buffer
    const char* inBufferPtr,   ///< [IN] initial buffer
    uint32_t    inBufferSize ///< [IN] Max char to copy from inBuffer to outBuffer
);



#endif // LEGATO_PACOMMONLOCAL_INCLUDE_GUARD
