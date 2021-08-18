/** @file pa_mrc_local.h
 *
 * Copyright (C) Sierra Wireless Inc.
 */

#ifndef LEGATO_PAMRCLOCAL_INCLUDE_GUARD
#define LEGATO_PAMRCLOCAL_INCLUDE_GUARD


#include "legato.h"

//--------------------------------------------------------------------------------------------------
/**
 * Define PA MRC local string size
 */
//--------------------------------------------------------------------------------------------------
#define PA_MRC_MEAS_LOCAL_STRING_SIZE    150


//--------------------------------------------------------------------------------------------------
/**
 * Define value for +COPS string management
 *
 * +COPS?
 * +COPS: <mode>[,<format>,<oper>[,<AcT>]]
 */
//--------------------------------------------------------------------------------------------------
#define COPS_PARAM_MODE_COUNT_ID        2
#define COPS_PARAM_FORMAT_COUNT_ID      3
#define COPS_PARAM_OPERATOR_COUNT_ID    4


//--------------------------------------------------------------------------------------------------
/**
 * Define value for +CREG string management
 *
 * +CREG?
 * +CREG:  <n>,<stat>[,[<lac>],[<ci>],[<AcT>]
 * +CEREG?
 * +CEREG: <n>,<stat>[,<tac>,<ci>[,<AcT>]]
 *  <n>: integer type
 *  0    disable network registration unsolicited result code
 *  1    enable network registration unsolicited result code +CREG: <stat>
 *  2    enable network registration and location information unsolicited result code
 */
//--------------------------------------------------------------------------------------------------
#define REG_PARAM_MODE_DISABLE         0
#define REG_PARAM_MODE_UNSO            1
#define REG_PARAM_MODE_VERBOSE         2
#define REG_PARAM_POS_ACT_COUNT_ID     6


//--------------------------------------------------------------------------------------------------
/**
 * 3GPP 27.007 release 12 <format> definition
 *
 * +COPS?
 * +COPS: <mode>[,<format>,<oper>[,<AcT>]]
 *
 * <format> integer type
 * 0    long    format    alphanumeric <oper>
 * 1    short format alphanumeric <oper>
 * 2    numeric <oper>
 */
//--------------------------------------------------------------------------------------------------
#define COPS_LONG_FORMAT_VAL            0
#define COPS_SHORT_FORMAT_VAL           1
#define COPS_NUMERIC_FORMAT_VAL         2

//--------------------------------------------------------------------------------------------------
/**
 * Base 10 value for strtol() API
 *
 */
//--------------------------------------------------------------------------------------------------
#define BASE_10                         10

//--------------------------------------------------------------------------------------------------
/**
 * Registration type
 *
 */
//--------------------------------------------------------------------------------------------------
typedef enum
{
    PA_MRC_REG_NETWORK,        ///< Network registration.
    PA_MRC_REG_PACKET_SWITCH   ///< Packet switch registration.
}
pa_mrc_RegistrationType_t;

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to initialize the mrc module
 *
 * @return LE_OK            The function succeeded.
 * @return LE_BAD_PARAMETER Bad parameter passed to the function
 * @return LE_TIMEOUT       No response was received.
 * @return LE_FAULT         The function failed to initialize the module.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t pa_mrc_Init
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * This function allocated memory to store Cell information
 *
 * @return pointer         The function failed to initialize the module.
 *
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED pa_mrc_CellInfo_t* pa_mrc_local_AllocateCellInfo
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
* This function must be called to get the unsolicited string to subscribe
*
* @return The unsolicited string
*/
//--------------------------------------------------------------------------------------------------
LE_SHARED const char * pa_mrc_local_GetRegisterUnso
(
  void
);

//--------------------------------------------------------------------------------------------------
/**
 * This function parse network scan response
 *
 * @return LE_FAULT         The function failed.
 * @return LE_TIMEOUT       No response was received.
 * @return LE_COMM_ERROR    Radio link failure occurred.
 * @return LE_OK            The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t pa_mrc_local_ParseNetworkScan
(
    char * responseStr,                        ///< [IN] Resposne to parse
    le_mrc_RatBitMask_t ratMask,               ///< [IN] The network mask
    pa_mrc_ScanType_t   scanType,              ///< [IN] the scan type
    le_dls_List_t*      scanInformationListPtr ///< [OUT] list of pa_mrc_ScanInformation_t
);

//--------------------------------------------------------------------------------------------------
/**
 * This function get the network operator mode (text or number mode).
 *
 * @return LE_FAULT         The function failed.
 * @return LE_OK            The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t pa_mrc_local_GetOperatorTextMode
(
    bool*    textMode
);

//--------------------------------------------------------------------------------------------------
/**
 * This function set the network operator mode (text or number mode).
 *
 * @return LE_OK            The function succeeded.
 * @return LE_TIMEOUT       No response was received.
 * @return LE_FAULT         The function failed.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t pa_mrc_local_SetOperatorTextMode
(
    bool    textMode
);


//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to set the CREG mode
 *
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void pa_mrc_local_SetCregMode
(
    int32_t cregMode
);


//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to set the CEREG mode
 *
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void pa_mrc_local_SetCeregMode
(
    int32_t ceregMode
);


//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to get the serving cell information.
 *
 * @return
 *  - LE_FAULT  Function failed.
 *  - LE_OK     Function succeeded.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t pa_mrc_local_GetServingCellInfo
(
    char * servinCellStr,       ///< [OUT] The serving cell string information.
    size_t servingCellStrSize   ///< [IN] Buffer size.
);

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to get the serving cell information for EPS_ONLY attach.
 *
 * @return
 *  - LE_FAULT  Function failed.
 *  - LE_OK     Function succeeded.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t pa_mrc_local_GetServingCellInfoEPS
(
    char * servinCellStr,       ///< [OUT] The serving cell string information.
    size_t servingCellStrSize   ///< [IN] Buffer size.
);

//--------------------------------------------------------------------------------------------------
/**
 * This function converts the <Act> field in +CREG / +CEREG string (3GPP 27.007 release 12)
 *  to le_mrc_Rat_t type
 *
 * @return LE_FAULT The function failed to convert the Radio Access Technology
 * @return LE_OK    The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t pa_mrc_local_ConvertActToRat
(
    int  actValue,              ///< [IN] The Radio Access Technology <Act> in CREG/CEREG.
    le_mrc_Rat_t*   ratPtr      ///< [OUT] The Radio Access Technology.
);

//--------------------------------------------------------------------------------------------------
/**
 * Check if the current device RAT is configured in GSM
 *
 * @return
        - true      If currently configured in GSM mode
        - false     Otherwise
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED bool pa_mrc_local_IsGSMMode
(
    void
);

#endif // LEGATO_PAMRCLOCAL_INCLUDE_GUARD
