/** @file pa_utils_local.h
 *
 * Copyright (C) Sierra Wireless Inc.
 */

#ifndef LEGATO_PAUTILSLOCAL_INCLUDE_GUARD
#define LEGATO_PAUTILSLOCAL_INCLUDE_GUARD


//--------------------------------------------------------------------------------------------------
/**
 * Default buffer size for short string management
 */
//--------------------------------------------------------------------------------------------------
#define DEFAULT_AT_BUFFER_SHORT_BYTES   100


//--------------------------------------------------------------------------------------------------
/**
 * Compare two strings
 */
//--------------------------------------------------------------------------------------------------
#define FIND_STRING(stringToFind, intoString) \
                   (strncmp(stringToFind, intoString, sizeof(stringToFind)-1)==0)

//--------------------------------------------------------------------------------------------------
/**
 * Default Timeout for AT Command.
 */
//--------------------------------------------------------------------------------------------------
#define DEFAULT_AT_CMD_TIMEOUT          30000


//--------------------------------------------------------------------------------------------------
/**
 * Default Timeout for AT Command.
 */
//--------------------------------------------------------------------------------------------------
#define MAX_AT_CMD_TIMEOUT              120000


//--------------------------------------------------------------------------------------------------
/**
 * Default Expected AT Command Response.
 */
//--------------------------------------------------------------------------------------------------
#define DEFAULT_AT_RESPONSE             "OK|ERROR|+CME ERROR:"

//--------------------------------------------------------------------------------------------------
/**
 * Default Expected AT Command intermediate Response.
 */
//--------------------------------------------------------------------------------------------------
#define DEFAULT_EMPTY_INTERMEDIATE      "\0"

//--------------------------------------------------------------------------------------------------
/**
 * Define NULL char
 */
//--------------------------------------------------------------------------------------------------
#define NULL_CHAR           '\0'

//--------------------------------------------------------------------------------------------------
/**
 * Define Hexadecimale base
 */
//--------------------------------------------------------------------------------------------------
#define BASE_HEX            16


//--------------------------------------------------------------------------------------------------
/**
 * Define decimal base
 */
//--------------------------------------------------------------------------------------------------
#define BASE_DEC            10

//--------------------------------------------------------------------------------------------------
/**
 * Define range decimal value
 */
//--------------------------------------------------------------------------------------------------
#define MIN_VALUE_RANGE     -2000
#define MAX_VALUE_RANGE     2000


//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to count the number of parameters in a line, between ',' and ':' and to set
 * all ',' with '\0' and the new char after ':' to '\0'
 *
 * @return the number of parameter in the line
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED uint32_t pa_utils_CountAndIsolateLineParameters
(
    char* linePtr       ///< [IN/OUT] line to parse
);


//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to isolate a line parameter
 *
 * @return pointer to the isolate string in the line
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED char* pa_utils_IsolateLineParameter
(
    const char* linePtr,    ///< [IN] Line to read
    uint32_t    pos         ///< [IN] Position to read
);


//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to remove quotation at begining and ending in a string if present
 *
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void pa_utils_RemoveQuotationString
(
    char * stringToParsePtr   ///< [IN/OUT] String to remove quotation char if present.
);


//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to get an intermediate response string
 *
 * @return
 *  - LE_FAULT  Function failed.
 *  - LE_OK     Function succeeded.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t pa_utils_GetATIntermediateResponse
(
    const char * cmdStr,        ///< [IN] AT command to send
    const char * interStr,      ///< [IN] Intermediate response expected
    char * responseStr,         ///< [OUT] Intermediate response
    size_t responseSize         ///< [OUT] Buffer size > LE_ATDEFS_RESPONSE_MAX_BYTES
);


//--------------------------------------------------------------------------------------------------
/**
 * This function Convert Hexadecimal string to uint32_t type
 *
 * @return uint32_t value if Function succeeded,0 otherwize
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED uint32_t pa_utils_ConvertHexStringToUInt32
(
    const char * hexStringPtr   ///< [IN] Hexadecimale string to convert
);


//--------------------------------------------------------------------------------------------------
/**
 * This function conerted the <Act> field in +CREG / +CEREG string (3GPP 27.007 release 12)
 *  to le_mrc_Rat_t type
 *
 * @return LE_FAULT The function failed to conver the Radio Access Technology
 * @return LE_OK    The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t pa_utils_ConvertActToRat
(
    int  actValue,              ///< [IN] The Radio Access Technology <Act> in CREG/CEREG.
    le_mrc_Rat_t*   ratPtr      ///< [OUT] The Radio Access Technology.
);


//--------------------------------------------------------------------------------------------------
/**
 * This function remove space in the string
 *
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void pa_utils_RemoveSpaceInString
(
    char * stringStr
);


//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to count the number of COPS operators detected in a line,
 * between '(' and ')' and to set all '(' and ')' char to '\0'
 *
 * @return the number of operators in the line
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED uint32_t pa_utils_CountAndIsolateCopsParameters
(
    char* linePtr       ///< [IN/OUT] line to parse
);


//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to count the number of string occurence
  *
 * @return the number of operators in the line
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED uint32_t pa_utils_CountStringParameters
(
    char* stringStr,       ///< [IN/OUT] line to parse
    const char* tagStr     ///< [IN] string to count
);

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to count the number of parameters in a line,
 * between <separatorChar> and to set all <separatorChar> with '\0'.
 *
 * @return the number of parameters in the line
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED uint32_t pa_utils_CountAndIsolateLineParametersWithChar
(
    char* linePtr,       ///< [IN/OUT] line to parse
    char  separatorChar
);

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to get the CMEE mode
 *
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED int32_t  pa_utils_GetCmeeMode
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to set the CMEE mode
 *
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void  pa_utils_SetCmeeMode
(
    int32_t cmeeMode
);

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to send AT command and check the OK AT response string.
 *
 * @return
 *  - LE_FAULT  Function failed.
 *  - LE_OK     Function succeeded.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t  pa_utils_SendATCommandOK
(
    const char * cmdStr        ///< [IN] AT command to send
);

#endif // LEGATO_PAUTILSLOCAL_INCLUDE_GUARD
