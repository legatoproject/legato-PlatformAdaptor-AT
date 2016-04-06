/**
 *
 * Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
 */


/** @file pa_common.c
 *
 * Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
 */


#include "legato.h"
#include "at/inc/le_atClient.h"
#include "pa_common_local.h"

//--------------------------------------------------------------------------------------------------
// APIs.
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to initialize the common module.
 *
 * @return LE_FAULT         The function failed to initialize the module.
 * @return LE_OK            The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_common_Init
(
    void
)
{
    return LE_OK;
}

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
)
{
    const char*          commandPtr   = "AT+WIND?";
    const char*          interRespPtr = "+WIND:";
    const char*          respPtr      = "OK|ERROR|+CME ERROR:";
    le_atClient_CmdRef_t cmdRef       = NULL;
    le_result_t          res          = LE_OK;
    char                 firstResponse[50];
    char                 finalResponse[50];

    LE_ASSERT(windPtr);

    le_atClient_SetCommandAndSend(&cmdRef,commandPtr,interRespPtr,respPtr,DEFAULT_TIMEOUT);

    res = le_atClient_GetFinalResponse(cmdRef,finalResponse,50);
    if ((res != LE_OK) || (strcmp(finalResponse, "OK") != 0))
    {
        LE_ERROR("Function failed !");
        le_atClient_Delete(cmdRef);
        return LE_FAULT;
    }

    res = le_atClient_GetFirstIntermediateResponse(cmdRef,firstResponse,50);

    if (sscanf(firstResponse,"+WIND: %d",windPtr) != 1)
    {
        LE_DEBUG("Cannot set wind indicator");
        res = LE_FAULT;
    }

    le_atClient_Delete(cmdRef);
    return res;
}

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
)
{
    char                 command[LE_ATCLIENT_CMD_SIZE_MAX_LEN];
    const char*          interRespPtr = "\0";
    const char*          respPtr      = "OK|ERROR|+CME ERROR:";
    le_atClient_CmdRef_t cmdRef       = NULL;
    le_result_t          res          = LE_OK;

    snprintf(command,LE_ATCLIENT_CMD_SIZE_MAX_LEN,"AT+WIND=%d",wind);

    res = le_atClient_SetCommandAndSend(&cmdRef,command,interRespPtr,respPtr,DEFAULT_TIMEOUT);

    le_atClient_Delete(cmdRef);
    return res;
}



//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to count the number of paramter between ',' and ':' and to set
 * all ',' with '\0' and the new char after ':' to '\0'
 *
 * @return true if the line start with '+', or false
 */
//--------------------------------------------------------------------------------------------------
uint32_t le_atClient_cmd_CountLineParameter
(
    char        *linePtr       ///< [IN/OUT] line to parse
)
{
    uint32_t  cpt=1;
    uint32_t  lineSize = strlen(linePtr);

    if (lineSize) {
        while (lineSize)
        {
            if ( linePtr[lineSize] == ',' ) {
                linePtr[lineSize] = '\0';
                cpt++;
            } else if (linePtr[lineSize] == ':' ) {
                linePtr[lineSize+1] = '\0';
                cpt++;
            }

            lineSize--;
        }
        return cpt;
    }
    return 0;
}



//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to count the get the @ref pos parameter of Line
 *
 *
 * @return pointer to the string at pos position in Line
 */
//--------------------------------------------------------------------------------------------------
char* le_atClient_cmd_GetLineParameter
(
    const char* linePtr,   ///< [IN] Line to read
    uint32_t    pos     ///< [IN] Position to read
)
{
    uint32_t i;
    char*    pCurr = (char*)linePtr;

    for(i=1;i<pos;i++)
    {
        pCurr=pCurr+strlen(pCurr)+1;
    }

    return pCurr;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to copy string from inBuffer to outBuffer without '"'
 *
 *
 * @return number of char really copy
 */
//--------------------------------------------------------------------------------------------------
uint32_t le_atClient_cmd_CopyStringWithoutQuote
(
    char*       outBufferPtr,  ///< [OUT] final buffer
    const char* inBufferPtr,   ///< [IN] initial buffer
    uint32_t    inBufferSize ///< [IN] Max char to copy from inBuffer to outBuffer
)
{
    uint32_t i,idx;

    for(i=0,idx=0;i<inBufferSize;i++)
    {
        if (inBufferPtr[i]=='\0') {
            break;
        }

        if (inBufferPtr[i] != '"') {
            outBufferPtr[idx++]=inBufferPtr[i];
        }
    }
    outBufferPtr[idx]='\0';

    return idx;
}
