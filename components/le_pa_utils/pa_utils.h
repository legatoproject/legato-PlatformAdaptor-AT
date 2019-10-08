/** @file pa_utils.h
 *
 * Copyright (C) Sierra Wireless Inc.
 */

#ifndef LEGATO_PAUTILS_INCLUDE_GUARD
#define LEGATO_PAUTILS_INCLUDE_GUARD


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
#define NULL_CHAR                       '\0'

//--------------------------------------------------------------------------------------------------
/**
 * Define Hexadecimale base
 */
//--------------------------------------------------------------------------------------------------
#define BASE_HEX                        16


//--------------------------------------------------------------------------------------------------
/**
 * Define decimal base
 */
//--------------------------------------------------------------------------------------------------
#define BASE_DEC                        10

//--------------------------------------------------------------------------------------------------
/**
 * Define range decimal value
 */
//--------------------------------------------------------------------------------------------------
#define MIN_VALUE_RANGE                 -2000
#define MAX_VALUE_RANGE                 2000

//--------------------------------------------------------------------------------------------------
/**
 * Define PA AT command padding size
 */
//--------------------------------------------------------------------------------------------------
#define PA_AT_COMMAND_PADDING           6

//--------------------------------------------------------------------------------------------------
/**
 * Define PA local string size
 */
//--------------------------------------------------------------------------------------------------
#define PA_AT_LOCAL_STRING_SIZE         100

//--------------------------------------------------------------------------------------------------
/**
 * Define PA at local long string size
 */
//--------------------------------------------------------------------------------------------------
#define PA_AT_LOCAL_LONG_STRING_SIZE    200


//--------------------------------------------------------------------------------------------------
/**
 * Define PA MRC local string size
 */
//--------------------------------------------------------------------------------------------------
#define PA_AT_LOCAL_SHORT_SIZE          50

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

//--------------------------------------------------------------------------------------------------
/**
 * This is used to get the device reference of the AT port.
 *
 **/
//--------------------------------------------------------------------------------------------------
LE_SHARED le_atClient_DeviceRef_t pa_utils_GetAtDeviceRef
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * This is used to get the device reference of the PPP port.
 *
 **/
//--------------------------------------------------------------------------------------------------
LE_SHARED le_atClient_DeviceRef_t pa_utils_GetPppDeviceRef
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * This is used to set the device reference of the AT port.
 *
 **/
//--------------------------------------------------------------------------------------------------
LE_SHARED void pa_utils_SetAtDeviceRef
(
    le_atClient_DeviceRef_t atDeviceRef
);

//--------------------------------------------------------------------------------------------------
/**
 * This is used to set the device reference of the PPP port.
 *
 **/
//--------------------------------------------------------------------------------------------------
LE_SHARED void pa_utils_SetPppDeviceRef
(
    le_atClient_DeviceRef_t pppDeviceRef
);

//--------------------------------------------------------------------------------------------------
/**
 * This is used to get the path of the PPP port.
 *
 **/
//--------------------------------------------------------------------------------------------------
LE_SHARED char* pa_utils_GetPppPath
(
    void
);

#endif // LEGATO_PAUTILS_INCLUDE_GUARD
