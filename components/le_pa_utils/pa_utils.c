/** @file pa_utils.c
 *
 * Copyright (C) Sierra Wireless Inc.
 */

#include "legato.h"
#include "interfaces.h"
#include "pa_utils.h"

//--------------------------------------------------------------------------------------------------
/**
 * Device reference used for sending AT commands
 */
//--------------------------------------------------------------------------------------------------
static le_atClient_DeviceRef_t AtDeviceRef = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * Device reference used for PPP session
 */
//--------------------------------------------------------------------------------------------------
static le_atClient_DeviceRef_t PppDeviceRef = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to count the number of parameters in a line,
 * between ',' and ':' and to set all ',' with '\0' and the new char after ':' to '\0'
 *
 * @return the number of parameters in the line
 */
//--------------------------------------------------------------------------------------------------
uint32_t pa_utils_CountAndIsolateLineParameters
(
    char* linePtr       ///< [IN/OUT] line to parse
)
{
    uint32_t cpt = 1;
    uint32_t lineSize = strlen(linePtr);

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
 * This function must be called to count the number of parameters in a line,
 * between <separatorChar> and to set all <separatorChar> with '\0'.
 *
 * @return the number of parameters in the line
 */
//--------------------------------------------------------------------------------------------------
uint32_t pa_utils_CountAndIsolateLineParametersWithChar
(
    char* linePtr,       ///< [IN/OUT] line to parse
    char  separatorChar
)
{
    uint32_t cpt = 1;
    uint32_t lineSize = strlen(linePtr);

    if (lineSize) {
        while (lineSize)
        {
            if ( linePtr[lineSize] == separatorChar )
            {
                linePtr[lineSize] = '\0';
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
 * This function must be called to count the number of COPS operators detected in a line,
 * between '(' and ')' and to set all '(' and ')' char to '\0'
 *
 * @return the number of operators in the line
 */
//--------------------------------------------------------------------------------------------------
uint32_t pa_utils_CountAndIsolateCopsParameters
(
    char* linePtr       ///< [IN/OUT] line to parse
)
{
    uint32_t cpt = 0;
    uint32_t lineSize = strnlen(linePtr, LE_ATDEFS_RESPONSE_MAX_BYTES);

    if (lineSize)
    {
        while (lineSize)
        {
            if ((linePtr[lineSize] == '(') || (linePtr[lineSize] == ')' ))
            {
                linePtr[lineSize] = NULL_CHAR;
                cpt++;
            }
            lineSize--;
        }

        if(cpt & 0x01)
        {
            LE_ERROR("Odd number of '(' ')' detected %" PRIu32 "!", cpt);
            return 0;
        }
    }

    cpt = cpt / 2;
    return cpt;
}


//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to count the number of string occurence
  *
 * @return the number of operators in the line
 */
//--------------------------------------------------------------------------------------------------
uint32_t pa_utils_CountStringParameters
(
    char* stringStr,       ///< [IN/OUT] line to parse
    const char* tagStr     ///< [IN] string to count
)
{
    int i = 0;
    uint32_t cpt = 0;
    uint32_t stringPtrLen = strnlen(stringStr, LE_ATDEFS_RESPONSE_MAX_BYTES);
    uint32_t tagStrLen = strnlen(tagStr, LE_ATDEFS_RESPONSE_MAX_BYTES);
    char * endStringPtr = stringStr + stringPtrLen + 1;
    char * shearchPtr = stringStr;
    char * newShearch;

    if ((0 == stringPtrLen) || (0 == tagStrLen))
    {
        return LE_FAULT;
    }

    for (i=0 ; ((shearchPtr) && (shearchPtr <= endStringPtr)); i++)
    {
        newShearch = strstr(shearchPtr, tagStr);
        if (newShearch && (newShearch != shearchPtr))
        {
            cpt++;
        }
        else
        {
            LE_DEBUG("Found nb %" PRIu32 " occurences", cpt);
            return cpt;
        }
        shearchPtr = newShearch+1;
    }

    return cpt;
}


//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to isolate a line parameter
 *
 * @return pointer to the isolated string in the line
 */
//--------------------------------------------------------------------------------------------------
char* pa_utils_IsolateLineParameter
(
    const char* linePtr,    ///< [IN] Line to read
    uint32_t    pos         ///< [IN] Position to read
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
 * This function must be called to remove quotation at begining and ending in a string if present
 *
 */
//--------------------------------------------------------------------------------------------------
void pa_utils_RemoveQuotationString
(
    char * stringToParsePtr   ///< [IN/OUT] String to remove quotation char if present.
)
{
    if(stringToParsePtr)
    {
        // stringToParsePtr = "1234"\0
        // stringlen = 6
        int stringlen = strnlen(stringToParsePtr, LE_ATDEFS_RESPONSE_MAX_BYTES);
        // Check minimum string len to have two quotations
        if(stringlen >= 2)
        {
            if ('"' == stringToParsePtr[0])
            {
                strncpy(stringToParsePtr, stringToParsePtr+1, stringlen);
                if ('"' == stringToParsePtr[stringlen-2])
                {
                    stringToParsePtr[stringlen-2] = NULL_CHAR;
                    // stringToParsePtr = 1234\0\0\0
                }
            }
        }
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to get an intermediate response string
 *
 * @return
 *  - LE_FAULT  Function failed.
 *  - LE_OK     Function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t  pa_utils_GetATIntermediateResponse
(
    const char * cmdStr,        ///< [IN] AT command to send
    const char * interStr,      ///< [IN] Intermediate response expected
    char * responseStr,         ///< [OUT] Intermediate response
    size_t responseSize         ///< [OUT] Buffer size > LE_ATDEFS_RESPONSE_MAX_BYTES
)
{
    le_result_t             res;
    le_atClient_CmdRef_t    cmdRef   = NULL;
    char                    finalResponseStr[PA_AT_LOCAL_STRING_SIZE] = {0};

    if(!cmdStr || !interStr || !responseStr)
    {
        LE_ERROR("Bad paramameters !!");
        return LE_FAULT;
    }

    res = le_atClient_SetCommandAndSend(&cmdRef,
        pa_utils_GetAtDeviceRef(),
        cmdStr,
        interStr,
        DEFAULT_AT_RESPONSE,
        MAX_AT_CMD_TIMEOUT);

    if (LE_OK != res)
    {
        LE_ERROR("Failed to send the command %s", cmdStr);
        return LE_FAULT;
    }

    res = le_atClient_GetFinalResponse(cmdRef,
        finalResponseStr,
        sizeof(finalResponseStr));
    if ((LE_OK != res) || (strcmp(finalResponseStr,"OK") != 0))
    {
        LE_ERROR("Failed to get the OK");
        le_atClient_Delete(cmdRef);
        return LE_FAULT;
    }

    responseStr[0] = NULL_CHAR;
    res = le_atClient_GetFirstIntermediateResponse(cmdRef,
        responseStr,
        responseSize);

    le_atClient_Delete(cmdRef);
    return res;
}



//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to send AT command and check the OK AT response string.
 *
 * @return
 *  - LE_FAULT  Function failed.
 *  - LE_OK     Function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t  pa_utils_SendATCommandOK
(
    const char * cmdStr        ///< [IN] AT command to send
)
{
    le_result_t             res;
    le_atClient_CmdRef_t    cmdRef   = NULL;
    char                    finalResponseStr[PA_AT_LOCAL_STRING_SIZE] = {0};

    if(!cmdStr)
    {
        LE_ERROR("Bad paramameters !!");
        return LE_FAULT;
    }

    res = le_atClient_SetCommandAndSend(&cmdRef,
        pa_utils_GetAtDeviceRef(),
        cmdStr,
        DEFAULT_EMPTY_INTERMEDIATE,
        DEFAULT_AT_RESPONSE,
        MAX_AT_CMD_TIMEOUT);

    if (LE_OK != res)
    {
        LE_ERROR("Failed to send the command");
        return LE_FAULT;
    }

    res = le_atClient_GetFinalResponse(cmdRef,
        finalResponseStr,
        sizeof(finalResponseStr));

    le_atClient_Delete(cmdRef);

    if ((LE_OK != res) || (strcmp(finalResponseStr,"OK") != 0))
    {
        LE_ERROR("Failed to get the OK");
        return LE_FAULT;
    }

    return res;
}


//--------------------------------------------------------------------------------------------------
/**
 * This function Convert Hexadecimal string to uint32_t type
 *
 * @return uint32_t value if Function succeeded,0 otherwize
 */
//--------------------------------------------------------------------------------------------------
uint32_t pa_utils_ConvertHexStringToUInt32
(
    const char * hexStringPtr   ///< [IN] Hexadecimale string to convert
)
{
    uint32_t valueUint32 = 0;

    if(hexStringPtr)
    {
        long value = strtol(hexStringPtr, NULL, BASE_HEX);
        if((value > 0) && (value < UINT32_MAX))
        {
            valueUint32 = value & UINT32_MAX;
        }
    }

    return valueUint32;
}


//--------------------------------------------------------------------------------------------------
/**
 * This function remove space in the string
 *
 */
//--------------------------------------------------------------------------------------------------
void pa_utils_RemoveSpaceInString
(
    char * stringStr
)
{
    int i, cpt;
    int len = strnlen(stringStr, LE_ATDEFS_RESPONSE_MAX_BYTES);
    for(i=0, cpt=0; i<len; i++)
    {
        if( ' ' != stringStr[i] )
        {
            stringStr[cpt++] = stringStr[i];
        }
    }
    stringStr[cpt] = NULL_CHAR;
}


//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to get the CMEE mode
 *
 */
//--------------------------------------------------------------------------------------------------
int32_t pa_utils_GetCmeeMode
(
    void
)
{
    int32_t cmeeMode = 0;
    char localBuffStr[PA_AT_LOCAL_SHORT_SIZE] = {0};
    // AT+CMEE?
    // +CMEE: 1
    // OK

    if( LE_OK == pa_utils_GetATIntermediateResponse("AT+CMEE?", "+CMEE",
        localBuffStr, sizeof(localBuffStr)))
    {
        LE_INFO("res %s", localBuffStr);
        cmeeMode = atoi(localBuffStr+strlen("+CMEE: "));
        if((cmeeMode < 0) || (cmeeMode > 2))
        {
            cmeeMode = 0;
        }
    }

    return cmeeMode;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to set the CMEE mode
 *
 */
//--------------------------------------------------------------------------------------------------
void pa_utils_SetCmeeMode
(
    int32_t cmeeMode
)
{
    char localBuffStr[PA_AT_LOCAL_SHORT_SIZE];
    snprintf(localBuffStr, sizeof(localBuffStr), "AT+CMEE=%" PRIi32, cmeeMode);

    pa_utils_GetATIntermediateResponse(localBuffStr, DEFAULT_EMPTY_INTERMEDIATE,
        localBuffStr, sizeof(localBuffStr));
}

//--------------------------------------------------------------------------------------------------
/**
 * This is used to set the device reference of the AT port.
 *
 **/
//--------------------------------------------------------------------------------------------------
void pa_utils_SetAtDeviceRef
(
    le_atClient_DeviceRef_t atDeviceRef
)
{
    AtDeviceRef = atDeviceRef;
}


//--------------------------------------------------------------------------------------------------
/**
 * This is used to get the device reference of the AT port.
 *
 **/
//--------------------------------------------------------------------------------------------------
le_atClient_DeviceRef_t pa_utils_GetAtDeviceRef
(
    void
)
{
    return AtDeviceRef;
}

//--------------------------------------------------------------------------------------------------
/**
 * This is used to set the device reference of the PPP port.
 *
 **/
//--------------------------------------------------------------------------------------------------
void pa_utils_SetPppDeviceRef
(
    le_atClient_DeviceRef_t pppDeviceRef
)
{
    PppDeviceRef = pppDeviceRef;
}

//--------------------------------------------------------------------------------------------------
/**
 * This is used to get the device reference of the PPP port.
 *
 **/
//--------------------------------------------------------------------------------------------------
le_atClient_DeviceRef_t pa_utils_GetPppDeviceRef
(
    void
)
{
    return PppDeviceRef;
}

COMPONENT_INIT
{
    // Do nothing.
}
