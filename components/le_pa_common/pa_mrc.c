/** @file pa_mrc.c
 *
 * AT implementation of c_pa_mrc API.
 *
 * Copyright (C) Sierra Wireless Inc.
 */

#include "legato.h"
#include "interfaces.h"
#include "pa_mrc.h"
#include "pa_utils.h"
#include "pa_mrc_local.h"
#ifdef MK_CONFIG_MRC_LISTEN_ATSWI_READY
#include "signals/signals.h"
#endif

//--------------------------------------------------------------------------------------------------
/**
 * Memory pool default value for network registration state
 */
//--------------------------------------------------------------------------------------------------
#define DEFAULT_REGSTATE_POOL_SIZE  8

//--------------------------------------------------------------------------------------------------
/**
 * Memory pool default value for packet switch state
 */
//--------------------------------------------------------------------------------------------------
#define DEFAULT_PSSTATE_POOL_SIZE  8

//--------------------------------------------------------------------------------------------------
/**
 * Maximum number of expected networks
 */
//--------------------------------------------------------------------------------------------------
#define HIGH_SCAN_INFO_COUNT 1


//--------------------------------------------------------------------------------------------------
/**
 * Maximum number of expected cells.
 */
//--------------------------------------------------------------------------------------------------
#define HIGH_CELL_INFO_COUNT       6


//--------------------------------------------------------------------------------------------------
/**
 * Define static pool for registration state
 */
//--------------------------------------------------------------------------------------------------
LE_MEM_DEFINE_STATIC_POOL(RegStatePool,
                          DEFAULT_REGSTATE_POOL_SIZE,
                          sizeof(le_mrc_NetRegState_t));

//--------------------------------------------------------------------------------------------------
/**
 * Define static pool for Packet Switched state
 */
//--------------------------------------------------------------------------------------------------
LE_MEM_DEFINE_STATIC_POOL(PSStatePool,
                          DEFAULT_PSSTATE_POOL_SIZE,
                          sizeof(le_mrc_NetRegState_t));

//--------------------------------------------------------------------------------------------------
/**
 * Define static pool for scan information
 */
//--------------------------------------------------------------------------------------------------
LE_MEM_DEFINE_STATIC_POOL(ScanInformationPool,
                          HIGH_SCAN_INFO_COUNT,
                          sizeof(pa_mrc_ScanInformation_t));


//--------------------------------------------------------------------------------------------------
/**
 * Define static pool for cell information
 */
//--------------------------------------------------------------------------------------------------
LE_MEM_DEFINE_STATIC_POOL(CellInfoPool,
                          HIGH_CELL_INFO_COUNT,
                          sizeof(pa_mrc_CellInfo_t));


//--------------------------------------------------------------------------------------------------
/**
 * Memory pool reference for network registration state
 */
//--------------------------------------------------------------------------------------------------
static le_mem_PoolRef_t           RegStatePoolRef = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * Memory pool reference for packet switch state
 */
//--------------------------------------------------------------------------------------------------
static le_mem_PoolRef_t           PSStatePoolRef = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * The pa_mrc_ScanInformation_t pool
 */
//--------------------------------------------------------------------------------------------------
static le_mem_PoolRef_t            ScanInformationPool = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * Pool for cells information.
 */
//--------------------------------------------------------------------------------------------------
static le_mem_PoolRef_t             CellInfoPool = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * Network registering event identifier
 */
//--------------------------------------------------------------------------------------------------
static le_event_Id_t                NetworkRegEventId = NULL;

//--------------------------------------------------------------------------------------------------
/**
 *Packet Switched state event identifier
 */
//--------------------------------------------------------------------------------------------------
static le_event_Id_t                PSStateEventId = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * Network registering mode
 */
//--------------------------------------------------------------------------------------------------
static pa_mrc_NetworkRegSetting_t RegNotification = PA_MRC_DISABLE_REG_NOTIFICATION;

//--------------------------------------------------------------------------------------------------
/**
 * Packet Switched state.
 */
//--------------------------------------------------------------------------------------------------
static le_mrc_NetRegState_t PSState = LE_MRC_REG_UNKNOWN;

//--------------------------------------------------------------------------------------------------
/**
 * Unsolicited +CEREG references
 */
//--------------------------------------------------------------------------------------------------
static le_atClient_UnsolicitedResponseHandlerRef_t UnsolCeregRef = NULL;

#ifdef MK_CONFIG_MRC_LISTEN_ATSWI_READY
//--------------------------------------------------------------------------------------------------
/**
 * Callback for AtSwi ready
 *
 * @return none
 */
//--------------------------------------------------------------------------------------------------
static void AtSwiReadyHandlerFunc(const sig_event_msg_t * const psig_event_msg)
{
    if ( psig_event_msg != NULL )
    {
        switch ( psig_event_msg->sig_usr_event )
        {
            case SIG_ATCMD_READY:
                if ( psig_event_msg->logportApp == LEGATO )
                {
                    le_mrc_NetRegState_t* stateArgPtr = (le_mrc_NetRegState_t*)
                                                        psig_event_msg->user_arg;
                    le_mrc_NetRegState_t* statePtr = le_mem_ForceAlloc(RegStatePoolRef);
                    *statePtr = *stateArgPtr;
                    LE_INFO("AtSwiReadyHandlerFunc state = %d", *stateArgPtr);
                    le_event_ReportWithRefCounting(NetworkRegEventId, statePtr);
                }
                break;
            case SIG_ATCMD_REG_FAILURE:
                break;
            default:
                break;
        }
    }
}
#endif

//--------------------------------------------------------------------------------------------------
/**
 * Report a network state and PS state change to event loop
 *
 * @return none
 */
//--------------------------------------------------------------------------------------------------
static void ReportNetworkPSStateUpdate
(
    int stateNum        ///< [IN] State number from URC
)
{
    le_mrc_NetRegState_t  state;
    le_mrc_NetRegState_t* statePtr;

    switch(stateNum)
    {
        case 0:
            state = LE_MRC_REG_NONE;
            break;
        case 1:
            state = LE_MRC_REG_HOME;
            break;
        case 2:
            state = LE_MRC_REG_SEARCHING;
            break;
        case 3:
            state = LE_MRC_REG_DENIED;
            break;
        case 4:
            state = LE_MRC_REG_UNKNOWN;
            break;
        case 5:
            state = LE_MRC_REG_ROAMING;
            break;
        default:
            state = LE_MRC_REG_UNKNOWN;
            break;
    }

    LE_DEBUG("Send Event with state %d", state);

    statePtr = le_mem_ForceAlloc(RegStatePoolRef);
    *statePtr = state;

#ifdef MK_CONFIG_MRC_LISTEN_ATSWI_READY
    // Workaround AtSwi not ready but report NetworkRegEventId event
    static le_mrc_NetRegState_t atswi_ready_state = LE_MRC_REG_NONE;
    if(state == LE_MRC_REG_HOME || state == LE_MRC_REG_ROAMING)
    {
        if (LE_MRC_REG_NONE == atswi_ready_state)
        {
            atswi_ready_state = state;
            sig_client_id sig_swi_id = sig_event_cb_register(SIGUSR, AtSwiReadyHandlerFunc,
                                                             (void*)&atswi_ready_state);
            if ( sig_swi_id < 0 )
            {
                LE_ERROR("SIGUSR signal event registration error");
            }
        }
    }
#endif

    le_event_ReportWithRefCounting(NetworkRegEventId, statePtr);

    statePtr = le_mem_ForceAlloc(PSStatePoolRef);
    *statePtr = state;
    le_event_ReportWithRefCounting(PSStateEventId, statePtr);

    PSState = state;
}


//--------------------------------------------------------------------------------------------------
/**
 * The handler for a new Network Registration Notification.
 *
 */
//--------------------------------------------------------------------------------------------------
static void CeregUnsolHandler
(
    const char* unsolPtr,
    void* contextPtr
)
{
    uint32_t numParam = 0;
    char unsolStr[LE_ATDEFS_UNSOLICITED_MAX_BYTES];

    if(!unsolPtr)
    {
        LE_ERROR("unsolPtr NULL");
        return;
    }

    strncpy(unsolStr, unsolPtr, LE_ATDEFS_UNSOLICITED_MAX_BYTES);
    numParam = pa_utils_CountAndIsolateLineParameters((char*) unsolPtr);
    LE_INFO("CeregUnsolHandler mode(%d) nb(%d) %s", (int) RegNotification,(int) numParam, unsolStr);

    if (numParam >= 2)
    {
        ReportNetworkPSStateUpdate(atoi(pa_utils_IsolateLineParameter(unsolPtr, 2)));
    }
    else
    {
        LE_WARN("this Response pattern is not expected -%s-", unsolStr);
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * This function registers a callback to the URC which indicates the network state or PS switched
 * state change.
 *
 */
//--------------------------------------------------------------------------------------------------
static void SubscribeUnsolCreg
(
    pa_mrc_NetworkRegSetting_t  mode ///< [IN] The selected Network registration mode.
)
{

    if (UnsolCeregRef)
    {
        le_atClient_RemoveUnsolicitedResponseHandler(UnsolCeregRef);
        UnsolCeregRef = NULL;
    }

    if ((PA_MRC_ENABLE_REG_NOTIFICATION == mode) ||
        (PA_MRC_ENABLE_REG_LOC_NOTIFICATION) == mode)
    {
        UnsolCeregRef = le_atClient_AddUnsolicitedResponseHandler(
            pa_mrc_local_GetRegisterUnso(),
            pa_utils_GetAtDeviceRef(),
            CeregUnsolHandler,
            NULL,
            1);
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * This function gets information of the Network or the packet switch registration.
 *
 * @return LE_OK            The function succeeded.
 * @return LE_BAD_PARAMETER The parameters are invalid.
 * @return LE_TIMEOUT       No response was received.
 * @return LE_FAULT         The function failed.
 */
//--------------------------------------------------------------------------------------------------
static le_result_t GetRegistration
(
    pa_mrc_RegistrationType_t  regType,   ///< Network or packet switch
    bool        mode,                  ///< true -> mode, false -> state
    int32_t*    valuePtr               ///< value that will be return
)
{
    le_atClient_CmdRef_t cmdRef   = NULL;
    le_result_t          res      = LE_FAULT;
    char*                tokenPtr = NULL;
    char*                savePtr  = NULL;
    char                 responseStr[PA_AT_LOCAL_STRING_SIZE] = {0};
    le_mrc_RatBitMask_t  ratMask = 0;

    if (!valuePtr)
    {
        LE_ERROR("One parameter is NULL");
        return LE_BAD_PARAMETER;
    }

    if (LE_OK != pa_mrc_GetRatPreferences(&ratMask))
    {
        return LE_FAULT;
    }

    if ((LE_MRC_BITMASK_RAT_LTE & ratMask) ||
        (LE_MRC_BITMASK_RAT_CATM1 & ratMask) ||
        (LE_MRC_BITMASK_RAT_NB1 & ratMask))
    {
        res = le_atClient_SetCommandAndSend(&cmdRef,
                                            pa_utils_GetAtDeviceRef(),
                                            "AT+CEREG?",
                                            "+CEREG:",
                                            DEFAULT_AT_RESPONSE,
                                            DEFAULT_AT_CMD_TIMEOUT);
    }
    else
    {
        if (PA_MRC_REG_NETWORK == regType)
        {
            res = le_atClient_SetCommandAndSend(&cmdRef,
                                                pa_utils_GetAtDeviceRef(),
                                                "AT+CREG?",
                                                "+CREG:",
                                                DEFAULT_AT_RESPONSE,
                                                DEFAULT_AT_CMD_TIMEOUT);
        }
        else if (PA_MRC_REG_PACKET_SWITCH == regType)
        {
            res = le_atClient_SetCommandAndSend(&cmdRef,
                                                pa_utils_GetAtDeviceRef(),
                                                "AT+CGREG?",
                                                "+CGREG:",
                                                DEFAULT_AT_RESPONSE,
                                                DEFAULT_AT_CMD_TIMEOUT);
        }
        else
        {
            LE_ERROR("Wrong registration domain");
            return LE_FAULT;
        }
    }

    if (res != LE_OK)
    {
        LE_ERROR("Failed to send the command");
        return res;
    }

    res = le_atClient_GetFinalResponse(cmdRef,
                                       responseStr,
                                       sizeof(responseStr));
    if (res != LE_OK)
    {
        LE_ERROR("Failed to get the OK");
        le_atClient_Delete(cmdRef);
        return res;
    }
    else if (strcmp(responseStr,"OK") != 0)
    {
        LE_ERROR("Final response is not OK");
        le_atClient_Delete(cmdRef);
        return LE_FAULT;
    }

    responseStr[0] = NULL_CHAR;
    res = le_atClient_GetFirstIntermediateResponse(cmdRef,
                            responseStr,
                            sizeof(responseStr));
    le_atClient_Delete(cmdRef);

    if (res != LE_OK)
    {
        LE_ERROR("Failed to get the response");
    }
    else
    {
        if ((LE_MRC_BITMASK_RAT_LTE & ratMask) ||
            (LE_MRC_BITMASK_RAT_CATM1 & ratMask) ||
            (LE_MRC_BITMASK_RAT_NB1 & ratMask))
        {
            tokenPtr = strtok_r(responseStr + strlen("+CEREG:"), ",", &savePtr);
        }
        else
        {
            if (PA_MRC_REG_NETWORK == regType)
            {
                tokenPtr = strtok_r(responseStr + strlen("+CREG:"), ",", &savePtr);
            }
            else if (PA_MRC_REG_PACKET_SWITCH == regType)
            {
                tokenPtr = strtok_r(responseStr + strlen("+CGREG:"), ",", &savePtr);
            }
        }

        if (tokenPtr)
        {
            if (mode)
            {
                switch(atoi(tokenPtr))
                {
                    case 0:
                        *valuePtr = PA_MRC_DISABLE_REG_NOTIFICATION;
                        break;
                    case 1:
                        *valuePtr = PA_MRC_ENABLE_REG_NOTIFICATION;
                        break;
                    case 2:
                        *valuePtr = PA_MRC_ENABLE_REG_LOC_NOTIFICATION;
                        break;
                    default:
                        LE_ERROR("Failed to get the response");
                        break;
                }
            }
            else
            {
                tokenPtr = strtok_r(NULL, ",", &savePtr);
                *valuePtr = LE_MRC_REG_UNKNOWN;
                if(tokenPtr)
                {
                    switch(atoi(tokenPtr))
                    {
                        case 0:
                            *valuePtr = LE_MRC_REG_NONE;
                            break;
                        case 1:
                            *valuePtr = LE_MRC_REG_HOME;
                            break;
                        case 2:
                            *valuePtr = LE_MRC_REG_SEARCHING;
                            break;
                        case 3:
                            *valuePtr = LE_MRC_REG_DENIED;
                            break;
                        case 4:
                            *valuePtr = LE_MRC_REG_UNKNOWN;
                            break;
                        case 5:
                            *valuePtr = LE_MRC_REG_ROAMING;
                            break;
                        default:
                            *valuePtr = LE_MRC_REG_UNKNOWN;
                            break;
                    }
                }
            }
        }
        else
        {
            res = LE_FAULT;
        }
    }
    return res;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function extract a plmn information from string
 * <stat>,long alphanumeric<oper>,shortalphanumeric<oper>,numeric<oper>[,<AcT>]
 *
 * @return LE_FAULT         The function failed.
 * @return LE_OK            The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
static le_result_t ExtractCopsPlmn
(
    char *ptrString,
    char *mccStr,
    char *mncStr,
    le_mrc_Rat_t* ratPtr,
    uint32_t * statePtr
)
{
    char tempString[DEFAULT_AT_BUFFER_SHORT_BYTES];
    char parseString[DEFAULT_AT_BUFFER_SHORT_BYTES];
    uint32_t nbField;
    char * subStringPtr;
    int i;

    strncpy(tempString, ptrString, sizeof(tempString));
    pa_utils_RemoveSpaceInString(tempString);
    memset(mccStr, 0, LE_MRC_MCC_BYTES);
    memset(mncStr, 0, LE_MRC_MNC_BYTES);
    nbField = pa_utils_CountAndIsolateLineParameters(tempString);

    /* +COPS: [list of supported(<stat>,long alphanumeric<oper>,
    *          shortalphanumeric<oper>,numeric<oper>[,<AcT>])s]
    *          [,,(list ofsupported<mode>s),(list of supported<format>s)]
    */

    *statePtr = 0;
    *ratPtr = LE_MRC_RAT_UNKNOWN;

    // Extract fields
    // 2,"RADIOLINJA","RL","24405",7\0
    // 0,"TELE","TELE","24491",7\0
    for(i=1; i<=nbField; i++)
    {
        subStringPtr = pa_utils_IsolateLineParameter(tempString, i);
        // 2\0"RADIOLINJA"\0"RL"\0"24405"\07\0
        // 0\0"TELE"\0"TELE"\0"24491"\07\0
        if(subStringPtr)
        {
            switch(i)
            {
                // network <stat>
                case 1:
                {
                    int stat = strtol(subStringPtr, NULL, BASE_DEC);
                    *statePtr = stat;
                }
                break;
                // long alphanumeric<oper>
                case 2: break;
                // shortalphanumeric<oper>
                case 3: break;
                // numeric<oper>
                case 4:
                {
                    //Extract PLMN
                    strncpy(parseString, subStringPtr,sizeof(parseString));
                    pa_utils_RemoveQuotationString(parseString);

                    int stringLen = strnlen(parseString, LE_MRC_MCC_LEN + LE_MRC_MNC_LEN);
                    if(stringLen >= (LE_MRC_MCC_LEN + LE_MRC_MNC_LEN - 1))
                    {
                        strncpy(mccStr, parseString, LE_MRC_MCC_LEN);
                        mccStr[LE_MRC_MCC_LEN] = NULL_CHAR;
                        strncpy(mncStr, parseString+LE_MRC_MCC_LEN, LE_MRC_MNC_BYTES);
                        mncStr[LE_MRC_MNC_LEN] = NULL_CHAR;
                    }
                }
                break;
                case 5:
                {
                    //Extract RAT
                    int state = strtol(subStringPtr, NULL, BASE_DEC);
                    pa_mrc_local_ConvertActToRat( state, ratPtr);
                }
                break;
                default:
                    break;
            }
        }
    }
    return LE_OK;
}


//--------------------------------------------------------------------------------------------------
/**
 * Initialize a pa_mrc_ScanInformation_t
 *
 */
//--------------------------------------------------------------------------------------------------
static void InitializeScanInformation
(
    pa_mrc_ScanInformation_t* scanInformationPtr    ///< [IN] the Scan information to initialize.
)
{
    memset(scanInformationPtr,0,sizeof(*scanInformationPtr));
    scanInformationPtr->link = LE_DLS_LINK_INIT;
}


//--------------------------------------------------------------------------------------------------
/**
 * Find a Scan information given the mcc and mnc.
 *
 * @return
 *      - pointer on pa_mrc_ScanInformation_t
 *      - NULL if not found
 */
//--------------------------------------------------------------------------------------------------
static pa_mrc_ScanInformation_t* FindScanInformation
(
    le_dls_List_t *scanInformationListPtr,
    char * mccStr,
    char * mncStr,
    le_mrc_Rat_t rat
)
{
    pa_mrc_ScanInformation_t* nodePtr;
    le_dls_Link_t *linkPtr;

    linkPtr = le_dls_Peek(scanInformationListPtr);

    do
    {
        if (linkPtr == NULL)
        {
            break;
        }
        // Get the node from MsgList
        nodePtr = CONTAINER_OF(linkPtr, pa_mrc_ScanInformation_t, link);

        if (    (memcmp(nodePtr->mobileCode.mcc,mccStr,LE_MRC_MCC_BYTES)==0)
             && (memcmp(nodePtr->mobileCode.mnc,mncStr,LE_MRC_MNC_BYTES)==0)
             && (nodePtr->rat == rat)
           )
        {
            LE_DEBUG("Found scan information for [%s,%s]", mccStr, mncStr);
            return nodePtr;
        }

        // Get Next element
        linkPtr = le_dls_PeekNext(scanInformationListPtr,linkPtr);

    } while (linkPtr != NULL);

    LE_DEBUG("Cannot find scan information for [%s,%s]",mccStr, mncStr);
    return NULL;
}

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
le_result_t pa_mrc_Init
(
    void
)
{
    le_result_t res;
    int32_t psState;
    NetworkRegEventId = le_event_CreateIdWithRefCounting("NetworkRegEventId");

    RegStatePoolRef = le_mem_InitStaticPool(RegStatePool,
                                            DEFAULT_REGSTATE_POOL_SIZE,
                                            sizeof(le_mrc_NetRegState_t));

    PSStateEventId = le_event_CreateIdWithRefCounting("PSStateEventId");

    PSStatePoolRef = le_mem_InitStaticPool(PSStatePool,
                                            DEFAULT_PSSTATE_POOL_SIZE,
                                            sizeof(le_mrc_NetRegState_t));

    ScanInformationPool=le_mem_InitStaticPool(ScanInformationPool,
                                              HIGH_SCAN_INFO_COUNT,
                                              sizeof(pa_mrc_ScanInformation_t));
    // Create the pool for cells information.
    CellInfoPool = le_mem_InitStaticPool(CellInfoPool,
                                         HIGH_CELL_INFO_COUNT,
                                         sizeof(pa_mrc_CellInfo_t));

    if(!NetworkRegEventId || !RegStatePoolRef || !PSStateEventId || !PSStatePoolRef
       || !ScanInformationPool || !CellInfoPool)
    {
        return LE_FAULT;
    }

    res = pa_mrc_ConfigureNetworkReg(PA_MRC_ENABLE_REG_LOC_NOTIFICATION);
    if(LE_OK != res)
    {
        return res;
    }

    res = pa_mrc_GetNetworkRegConfig(&RegNotification);
    if(LE_OK != res)
    {
        return res;
    }

    GetRegistration(PA_MRC_REG_PACKET_SWITCH, false, &psState);
    PSState = (le_mrc_NetRegState_t)psState;

    SubscribeUnsolCreg(PA_MRC_ENABLE_REG_LOC_NOTIFICATION);
    return res;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to register on a mobile network [mcc;mnc]
 *
 * @return LE_FAULT         The function failed to register.
 * @return LE_OK            The function succeeded to register,
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mrc_RegisterNetwork
(
    const char *mccPtr,   ///< [IN] Mobile Country Code
    const char *mncPtr    ///< [IN] Mobile Network Code
)
{
    return LE_FAULT;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to perform a network scan.
 *
 * @return LE_FAULT         The function failed.
 * @return LE_TIMEOUT       No response was received.
 * @return LE_COMM_ERROR    Radio link failure occurred.
 * @return LE_OK            The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mrc_PerformNetworkScan
(
    le_mrc_RatBitMask_t ratMask,               ///< [IN] The network mask
    pa_mrc_ScanType_t   scanType,              ///< [IN] the scan type
    le_dls_List_t*      scanInformationListPtr ///< [OUT] list of pa_mrc_ScanInformation_t
)
{
    le_result_t             res = LE_OK;
    char                    responseStr[LE_ATDEFS_RESPONSE_MAX_BYTES] = {0};

    //at+cops=?
    //+COPS: (1,,,"00101",7),,(0-4),(0-2)
    res = pa_utils_GetATIntermediateResponse("AT+COPS=?", "+COPS:", responseStr, sizeof(responseStr));
    if (LE_OK == res)
    {
        res = pa_mrc_local_ParseNetworkScan(responseStr, ratMask, scanType, scanInformationListPtr);
    }

    return res;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to perform a network scan.
 *
 * @return
 *      - LE_OK on success
 *      - LE_OVERFLOW if the operator name would not fit in buffer
 *      - LE_FAULT for all other errors
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mrc_GetScanInformationName
(
    pa_mrc_ScanInformation_t* scanInformationPtr,   ///< [IN] The scan information
    char*                     namePtr,              ///< [OUT] Name of operator
    size_t                    nameSize              ///< [IN] The size in bytes of the namePtr buffer
)
{
    return LE_FAULT;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to register a handler to report network reject code.
 *
 * @return A handler reference, which is only needed for later removal of the handler.
 *
 * @note Doesn't return on failure, so there's no need to check the return value for errors.
 */
//--------------------------------------------------------------------------------------------------
le_event_HandlerRef_t pa_mrc_AddNetworkRejectIndHandler
(
    pa_mrc_NetworkRejectIndHdlrFunc_t networkRejectIndHandler, ///< [IN] The handler function to
                                                               ///< report network reject
                                                               ///< indication.
    void*                             contextPtr               ///< [IN] The context to be given to
                                                               ///< the handler.
)
{
    return NULL;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to unregister the handler for network reject indication handling.
 *
 */
//--------------------------------------------------------------------------------------------------
void pa_mrc_RemoveNetworkRejectIndHandler
(
    le_event_HandlerRef_t handlerRef
)
{
    return;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to set the power of the Radio Module.
 *
 * @return LE_BAD_PARAMETER Bad power mode specified.
 * @return LE_FAULT         The function failed.
 * @return LE_TIMEOUT       No response was received.
 * @return LE_OK            The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mrc_SetRadioPower
(
    le_onoff_t    power   ///< [IN] The power state.
)
{
    char                 responseStr[PA_AT_LOCAL_SHORT_SIZE];
    le_result_t          res    = LE_FAULT;
    le_atClient_CmdRef_t cmdRef = NULL;

    if ((power != LE_ON) && (power != LE_OFF))
    {
        return LE_BAD_PARAMETER;
    }

    res = le_atClient_SetCommandAndSend(&cmdRef,
                                        pa_utils_GetAtDeviceRef(),
                                        (LE_ON == power ? "AT+CFUN=1" : "AT+CFUN=4"),
                                        DEFAULT_EMPTY_INTERMEDIATE,
                                        DEFAULT_AT_RESPONSE,
                                        DEFAULT_AT_CMD_TIMEOUT);

    if (res != LE_OK)
    {
        LE_ERROR("Failed to send the command");
        return res;
    }

    res = le_atClient_GetFinalResponse(cmdRef,responseStr,PA_AT_LOCAL_SHORT_SIZE);
    le_atClient_Delete(cmdRef);

    if ((res != LE_OK) || (strcmp(responseStr,"OK") != 0))
    {
        LE_ERROR("Failed to get the OK response");
        res = LE_FAULT;
    }

    return res;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to get the Radio Module power state.
 *
 * @return LE_FAULT         The function failed.
 * @return LE_TIMEOUT       No response was received.
 * @return LE_OK            The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mrc_GetRadioPower
(
     le_onoff_t*    powerPtr   ///< [OUT] The power state.
)
{
    le_atClient_CmdRef_t cmdRef = NULL;
    le_result_t          res    = LE_FAULT;
    char                 responseStr[PA_AT_LOCAL_STRING_SIZE] = {0} ;

    res = le_atClient_SetCommandAndSend(&cmdRef,
                                        pa_utils_GetAtDeviceRef(),
                                        "AT+CFUN?",
                                        "+CFUN:",
                                        DEFAULT_AT_RESPONSE,
                                        DEFAULT_AT_CMD_TIMEOUT);
    if (res != LE_OK)
    {
        LE_ERROR("Failed to send the command");
        return res;
    }

    res = le_atClient_GetFinalResponse(cmdRef,
                                       responseStr,
                                       PA_AT_LOCAL_STRING_SIZE);
    if (res != LE_OK)
    {
        LE_ERROR("Failed to get the response");
        le_atClient_Delete(cmdRef);
        return res;
    }
    else if (strcmp(responseStr,"OK") != 0)
    {
        LE_ERROR("Final response is not OK");
        le_atClient_Delete(cmdRef);
        return LE_FAULT;
    }


    res = le_atClient_GetFirstIntermediateResponse(cmdRef,
                                                   responseStr,
                                                   PA_AT_LOCAL_STRING_SIZE);
    le_atClient_Delete(cmdRef);

    if (res != LE_OK)
    {
        LE_DEBUG("Failed to get the response");
    }
    else
    {
        if(atoi(&responseStr[strlen("+CFUN: ")]) == 1)
        {
            *powerPtr = LE_ON;
        }
        else
        {
            *powerPtr = LE_OFF;
        }
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to register a handler for Radio Access Technology change handling.
 *
 * @return A handler reference, which is only needed for later removal of the handler.
 *
 * @note Doesn't return on failure, so there's no need to check the return value for errors.
 */
//--------------------------------------------------------------------------------------------------
le_event_HandlerRef_t pa_mrc_SetRatChangeHandler
(
    pa_mrc_RatChangeHdlrFunc_t handlerFuncPtr ///< [IN] The handler function.
)
{
    return NULL;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to unregister the handler for Radio Access Technology change
 * handling.
 *
 * @note Doesn't return on failure, so there's no need to check the return value for errors.
 */
//--------------------------------------------------------------------------------------------------
void pa_mrc_RemoveRatChangeHandler
(
    le_event_HandlerRef_t handlerRef
)
{
    return;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to register a handler for Network registration state handling.
 *
 * @return A handler reference, which is only needed for later removal of the handler.
 *
 * @note Doesn't return on failure, so there's no need to check the return value for errors.
 */
//--------------------------------------------------------------------------------------------------
le_event_HandlerRef_t pa_mrc_AddNetworkRegHandler
(
    pa_mrc_NetworkRegHdlrFunc_t regStateHandler ///< [IN] The handler function to handle the
                                                 ///       Network registration state.
)
{
    if (regStateHandler == NULL)
    {
        LE_FATAL("Network registration handler is NULL");
    }

    return (le_event_AddHandler("NewRegStateHandler",
                                NetworkRegEventId,
                                (le_event_HandlerFunc_t) regStateHandler));
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to unregister the handler for Network registration state handling.
 *
 * @note Doesn't return on failure, so there's no need to check the return value for errors.
 */
//--------------------------------------------------------------------------------------------------
void pa_mrc_RemoveNetworkRegHandler
(
    le_event_HandlerRef_t handlerRef
)
{
    le_event_RemoveHandler(handlerRef);
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to register a handler for Packet Switched change handling.
 *
 * @return A handler reference, which is only needed for later removal of the handler.
 *
 * @note Doesn't return on failure, so there's no need to check the return value for errors.
 */
//--------------------------------------------------------------------------------------------------
le_event_HandlerRef_t pa_mrc_SetPSChangeHandler
(
    pa_mrc_ServiceChangeHdlrFunc_t PSChangeHandler ///< [IN] The handler function.
)
{
    if (PSChangeHandler == NULL)
    {
        LE_FATAL("PS state handler is NULL");
    }

    return (le_event_AddHandler("PSStateHandler",
                                PSStateEventId,
                                (le_event_HandlerFunc_t) PSChangeHandler));
}


//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to set and activate the delta for signal strength indications.
 *
 * @return
 *  - LE_FAULT  Function failed.
 *  - LE_OK     Function succeeded.
 *  - LE_BAD_PARAMETER  Bad parameters.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mrc_SetSignalStrengthIndDelta
(
    le_mrc_Rat_t rat,    ///< [IN] Radio Access Technology
    uint16_t     delta   ///< [IN] Signal delta in units of 0.1 dB
)
{
    switch(rat)
    {
        case LE_MRC_RAT_GSM:
        case LE_MRC_RAT_UMTS:
        case LE_MRC_RAT_TDSCDMA:
        case LE_MRC_RAT_LTE:
        case LE_MRC_RAT_CDMA:
            break;

        case LE_MRC_RAT_UNKNOWN:
        default:
            LE_ERROR("Bad parameter!");
            return LE_FAULT;
    }

    return LE_OK;
}
//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to unregister the handler for Packet Switched change
 * handling.
 *
 */
//--------------------------------------------------------------------------------------------------
void pa_mrc_RemovePSChangeHandler
(
    le_event_HandlerRef_t handlerRef
)
{
    LE_WARN("Unsupported function called");
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to register a handler for Signal Strength change handling.
 *
 * @return A handler reference, which is only needed for later removal of the handler.
 *
 * @note Doesn't return on failure, so there's no need to check the return value for errors.
 */
//--------------------------------------------------------------------------------------------------
le_event_HandlerRef_t pa_mrc_AddSignalStrengthIndHandler
(
    pa_mrc_SignalStrengthIndHdlrFunc_t ssIndHandler, ///< [IN] The handler function to handle the
                                                     ///        Signal Strength change indication.
    void*                              contextPtr    ///< [IN] The context to be given to the handler.
)
{
    return NULL;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to unregister the handler for Signal Strength change handling.
 *
 * @note Doesn't return on failure, so there's no need to check the return value for errors.
 */
//--------------------------------------------------------------------------------------------------
void pa_mrc_RemoveSignalStrengthIndHandler
(
    le_event_HandlerRef_t handlerRef
)
{
    return;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to set and activate the signal strength thresholds for signal
 * strength indications
 *
 * @return
 *  - LE_FAULT  Function failed.
 *  - LE_OK     Function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mrc_SetSignalStrengthIndThresholds
(
    le_mrc_Rat_t rat,                 ///< Radio Access Technology
    int32_t      lowerRangeThreshold, ///< [IN] lower-range threshold in dBm
    int32_t      upperRangeThreshold  ///< [IN] upper-range strength threshold in dBm
)
{
    return LE_FAULT;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to get the serving cell Identifier.
 *
 * @return
 *  - LE_FAULT  Function failed.
 *  - LE_OK     Function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mrc_GetServingCellId
(
    uint32_t* cellIdPtr ///< [OUT] main Cell Identifier.
)
{
    uint32_t   numParam = 0;
    char       responseStr[PA_AT_LOCAL_STRING_SIZE] = {0};
    char*      ptr;

    if (LE_OK != pa_mrc_local_GetServingCellInfo(responseStr, sizeof(responseStr)))
    {
        LE_ERROR("No match %s", responseStr);
        return LE_FAULT;
    }

    numParam = pa_utils_CountAndIsolateLineParameters((char*)responseStr);
    if (numParam >= 5)
    {
        // Extract <ci> field
        ptr = pa_utils_IsolateLineParameter(responseStr, 5);
        if (ptr)
        {
            // Remove quotaion if present
            pa_utils_RemoveQuotationString(ptr);

            uint32_t cellIdValue = pa_utils_ConvertHexStringToUInt32(ptr);

            if (cellIdValue)
            {
                *cellIdPtr = cellIdValue;
                return LE_OK;
            }
        }
    }
    else if (LE_OK == pa_mrc_local_GetServingCellInfoEPS(responseStr, sizeof(responseStr)))
    {
        //check for EPS_ONLY attach case
        numParam = pa_utils_CountAndIsolateLineParameters((char*)responseStr);
        if (numParam >= 5)
        {
            // Extract <ci> field
            ptr = pa_utils_IsolateLineParameter(responseStr, 5);
            if (ptr)
            {
                // Remove quotaion if present
                pa_utils_RemoveQuotationString(ptr);

                uint32_t cellIdValue = pa_utils_ConvertHexStringToUInt32(ptr);

                if (cellIdValue)
                {
                    *cellIdPtr = cellIdValue;
                    return LE_OK;
                }
            }
        }
    }

    LE_ERROR("No match %s", responseStr);
    return LE_FAULT;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to get the Tracking Area Code of the serving cell.
 *
 * @return
 *  - LE_FAULT  Function failed.
 *  - LE_OK     Function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mrc_GetServingCellLteTracAreaCode
(
    uint16_t* tacPtr ///< [OUT] Tracking Area Code of the serving cell.
)
{
    le_result_t res;
    uint32_t    numParam = 0;
    char        responseStr[PA_AT_LOCAL_STRING_SIZE] = {0};
    const char            commandStr[] = "AT+CEREG?";
    const char            interStr[] = "+CEREG:";

    if(!tacPtr)
    {
        return LE_FAULT;
    }

    // Get +CEREG response
    res = pa_utils_GetATIntermediateResponse(commandStr, interStr,
        responseStr, sizeof(responseStr));

    if(LE_OK == res)
    {
        //+CEREG:  <n>,<stat>[,[<tac>],[<ci>],[<AcT>]
        numParam = pa_utils_CountAndIsolateLineParameters(responseStr);
        if (numParam >= 3)
        {
            // Extract mode <n>
            char * ptr = pa_utils_IsolateLineParameter(responseStr, 2);
            pa_utils_RemoveQuotationString(responseStr);
            uint32_t setting = atoi(ptr);

            if (setting != REG_PARAM_MODE_VERBOSE)
            {
                pa_mrc_local_SetCeregMode(REG_PARAM_MODE_VERBOSE);

                // Get +CEREG response
                responseStr[0] = NULL_CHAR;
                res = pa_utils_GetATIntermediateResponse(commandStr, interStr,
                    responseStr, sizeof(responseStr));

                pa_mrc_local_SetCeregMode(setting);

                if(LE_OK != res)
                {
                    LE_ERROR("No Match %s", responseStr);
                    return res;
                }
                numParam = pa_utils_CountAndIsolateLineParameters(responseStr);
            }

            if (numParam >= 4)
            {
                // Extract <tac> field
                char * ptr = pa_utils_IsolateLineParameter(responseStr, 4);
                if(ptr)
                {
                    // Remove quotation if present
                    pa_utils_RemoveQuotationString(ptr);
                    // Convert Hexadecimal string to uint32_t type
                    uint32_t tac = pa_utils_ConvertHexStringToUInt32(ptr);
                    if(tac)
                    {
                        *tacPtr = tac;
                        return LE_OK;
                    }
                }
            }
        }
    }
    else
    {
        LE_WARN("No enougth param %" PRIu32, numParam);
    }

    LE_ERROR("No match %s", responseStr);
    return LE_FAULT;
}


//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to get the Location Area Code of the serving cell.
 *
 * @return LE_OK            The function succeeded.
 * @return LE_BAD_PARAMETER The parameters are invalid.
 * @return LE_TIMEOUT       No response was received.
 * @return LE_FAULT         The function failed.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mrc_GetServingCellLocAreaCode
(
    uint32_t* lacPtr ///< [OUT] Location Area Code of the serving cell.
)
{
    le_result_t res;
    uint32_t    numParam = 0;
    char        responseStr[PA_AT_LOCAL_STRING_SIZE] = {0};

    res = pa_mrc_local_GetServingCellInfo(responseStr, sizeof(responseStr));
    if(LE_OK != res)
    {
        LE_ERROR("No match %s", responseStr);
        return res;
    }

    if(!lacPtr)
    {
        return LE_FAULT;
    }

    if(LE_OK == res)
    {
        //+CREG:  <n>,<stat>[,[<lac>],[<ci>],[<AcT>]
        numParam = pa_utils_CountAndIsolateLineParameters(responseStr);
        if (numParam >= 4)
        {
            // Extract <lac> field
            char * ptr = pa_utils_IsolateLineParameter(responseStr, 4);
            if(ptr)
            {
                // Remove quotaion if present
                pa_utils_RemoveQuotationString(ptr);
                // Convert Hexadecimal string to uint type
                uint32_t lac = pa_utils_ConvertHexStringToUInt32(ptr);
                if(lac)
                {
                    *lacPtr = lac;
                    return LE_OK;
                }
            }
        }
    }
    else
    {
        LE_WARN("No enougth parameters %" PRIu32, numParam);
    }

    LE_ERROR("No match %s", responseStr);
    return LE_FAULT;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the Band capabilities
 *
 * @return
 *  - LE_OK              on success
 *  - LE_FAULT           on failure
 *  - LE_UNSUPPORTED     The platform does not support this operation.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mrc_GetBandCapabilities
(
    le_mrc_BandBitMask_t* bandsPtr ///< [OUT] A bit mask to get the Band capabilities.
)
{
    le_atClient_CmdRef_t cmdRef  = NULL;
    le_result_t          res     = LE_FAULT;
    char*                bandPtr = NULL;
    le_mrc_BandBitMask_t bands   = 0;
    int                  bitMask = 0;
    char                 intermediateResponse[LE_ATDEFS_RESPONSE_MAX_BYTES];
    char                 finalResponse[LE_ATDEFS_RESPONSE_MAX_BYTES];

    res = le_atClient_SetCommandAndSend(&cmdRef,
                                        pa_utils_GetAtDeviceRef(),
                                        "AT+KBND?",
                                        "+KBND:",
                                        DEFAULT_AT_RESPONSE,
                                        DEFAULT_AT_CMD_TIMEOUT);
    if (res != LE_OK)
    {
        LE_ERROR("Failed to send the command");
        return res;
    }

    res = le_atClient_GetFinalResponse(cmdRef,
                                       finalResponse,
                                       LE_ATDEFS_RESPONSE_MAX_BYTES);
    if ((res != LE_OK) || (strcmp(finalResponse,"OK") != 0))
    {
        LE_ERROR("Failed to get the response");
        le_atClient_Delete(cmdRef);
        return LE_FAULT;
    }

    res = le_atClient_GetFirstIntermediateResponse(cmdRef,
                                                   intermediateResponse,
                                                   LE_ATDEFS_RESPONSE_MAX_BYTES);
    if (res != LE_OK)
    {
        LE_ERROR("Failed to get the response");
        res = LE_FAULT;
    }
    else
    {
        bandPtr = intermediateResponse+strlen("+KBND: ");
        bitMask = atoi(bandPtr);

        if (bitMask & 0x00)
        {
            LE_ERROR("Band capabilities not available !");
            res = LE_FAULT;
        }
        if (bitMask == 1)
        {
            bands |= LE_MRC_BITMASK_BAND_GSM_850;
        }
        if (bitMask == 2)
        {
            bands |= LE_MRC_BITMASK_BAND_EGSM_900;
        }
        if (bitMask == 4)
        {
            bands |= LE_MRC_BITMASK_BAND_GSM_DCS_1800;
        }
        if (bitMask == 8)
        {
            bands |= LE_MRC_BITMASK_BAND_GSM_PCS_1900;
        }
        if (bitMask == 10)
        {
            bands |= LE_MRC_BITMASK_BAND_WCDMA_EU_J_CH_IMT_2100;
        }
        if (bitMask == 20)
        {
            bands |= LE_MRC_BITMASK_BAND_WCDMA_US_PCS_1900;
        }
        if (bitMask == 40)
        {
            bands |= LE_MRC_BITMASK_BAND_WCDMA_US_850;
        }
        if (bitMask == 80)
        {
            bands |= LE_MRC_BITMASK_BAND_WCDMA_J_800;
        }
        if (bitMask == 100)
        {
            bands |= LE_MRC_BITMASK_BAND_WCDMA_EU_J_900;
        }
        if (bitMask == 200)
        {
            bands |= LE_MRC_BITMASK_BAND_WCDMA_J_800;
        }

        if (bandsPtr)
        {
            *bandsPtr = bands;
        }
    }

    le_atClient_Delete(cmdRef);
    return res;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the LTE Band capabilities
 *
 * @return
 *  - LE_OK              on success
 *  - LE_FAULT           on failure
 *  - LE_UNSUPPORTED     The platform does not support this operation.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mrc_GetLteBandCapabilities
(
    le_mrc_LteBandBitMask_t* bandsPtr ///< [OUT] Bit mask to get the LTE Band capabilities.
)
{
    LE_WARN("LTE not available");
    return LE_UNSUPPORTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the TD-SCDMA Band capabilities
 *
 * @return
 *  - LE_OK              on success
 *  - LE_FAULT           on failure
 *  - LE_UNSUPPORTED     The platform does not support this operation.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mrc_GetTdScdmaBandCapabilities
(
    le_mrc_TdScdmaBandBitMask_t* bandsPtr ///< [OUT] Bit mask to get the TD-SCDMA Band capabilities.
)
{
    LE_WARN("CDMA not available");
    return LE_UNSUPPORTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function gets the Network registration setting.
 *
 * @return LE_BAD_PARAMETER Bad parameter passed to the function
 * @return LE_FAULT         The function failed.
 * @return LE_TIMEOUT       No response was received.
 * @return LE_OK            The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mrc_GetNetworkRegConfig
(
    pa_mrc_NetworkRegSetting_t* settingPtr   ///< [OUT] The selected Network registration setting.
)
{
    int32_t val;

    if (!settingPtr)
    {
        LE_WARN("One parameter is NULL");
        return LE_BAD_PARAMETER;
    }

    GetRegistration(PA_MRC_REG_NETWORK, true, &val);
    *settingPtr = (pa_mrc_NetworkRegSetting_t)val;
    RegNotification = val;

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the platform specific network registration error code.
 *
 * @return the platform specific registration error code
 *
 */
//--------------------------------------------------------------------------------------------------
int32_t pa_mrc_GetPlatformSpecificRegistrationErrorCode
(
    void
)
{
    return 0;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function gets the Network registration state.
 *
 * @return LE_BAD_PARAMETER Bad parameter passed to the function
 * @return LE_FAULT         The function failed.
 * @return LE_TIMEOUT       No response was received.
 * @return LE_OK            The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mrc_GetNetworkRegState
(
    le_mrc_NetRegState_t* statePtr  ///< [OUT] The network registration state.
)
{
    int32_t val;

    if (!statePtr)
    {
        LE_WARN("One parameter is NULL");
        return LE_BAD_PARAMETER;
    }

    GetRegistration(PA_MRC_REG_NETWORK, false, &val);
    *statePtr = (le_mrc_NetRegState_t)val;

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to register automatically on network
 *
 * @return LE_OK            The function succeeded.
 * @return LE_BAD_PARAMETER The parameters are invalid.
 * @return LE_TIMEOUT       No response was received.
 * @return LE_FAULT         The function failed.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mrc_SetAutomaticNetworkRegistration
(
    void
)
{
    le_atClient_CmdRef_t cmdRef = NULL;
    le_result_t          res    = LE_FAULT;
    char                 finalResponse[LE_ATDEFS_RESPONSE_MAX_BYTES];

    res = le_atClient_SetCommandAndSend(&cmdRef,
                                        pa_utils_GetAtDeviceRef(),
                                        "AT+CREG=1",
                                        "",
                                        DEFAULT_AT_RESPONSE,
                                        DEFAULT_AT_CMD_TIMEOUT);
    if (res != LE_OK)
    {
        LE_ERROR("Failed to send the command !");
        return res;
    }

    res = le_atClient_GetFinalResponse(cmdRef,
                                       finalResponse,
                                       LE_ATDEFS_RESPONSE_MAX_BYTES);
    if (res != LE_OK)
    {
        LE_ERROR("Function failed !");
        le_atClient_Delete(cmdRef);
        return res;
    }
    else if (strcmp(finalResponse,"OK") != 0)
    {
        LE_ERROR("Final response is not OK");
        le_atClient_Delete(cmdRef);
        return LE_FAULT;
    }

    LE_DEBUG("Set automatic network registration.");
    le_atClient_Delete(cmdRef);
    return res;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to get current registration mode
 *
 * @return LE_OK            The function succeeded.
 * @return LE_BAD_PARAMETER The parameters are invalid.
 * @return LE_TIMEOUT       No response was received.
 * @return LE_FAULT         The function failed.
*/
//--------------------------------------------------------------------------------------------------
le_result_t pa_mrc_GetNetworkRegistrationMode
(
    bool*   isManualPtr,  ///< [OUT] true if the scan mode is manual, false if it is automatic.
    char*   mccPtr,       ///< [OUT] Mobile Country Code
    size_t  mccPtrSize,   ///< [IN] mccPtr buffer size
    char*   mncPtr,       ///< [OUT] Mobile Network Code
    size_t  mncPtrSize    ///< [IN] mncPtr buffer size
)
{
    char*                tokenPtr   = NULL;
    char*                restPtr    = NULL;
    char*                savePtr    = NULL;
    le_atClient_CmdRef_t cmdRef     = NULL;
    le_result_t          res        = LE_FAULT;
    char                 responseStr[PA_AT_LOCAL_STRING_SIZE];

    res = le_atClient_SetCommandAndSend(&cmdRef,
                                        pa_utils_GetAtDeviceRef(),
                                        "AT+CREG?",
                                        "+CREG:",
                                        DEFAULT_AT_RESPONSE,
                                        DEFAULT_AT_CMD_TIMEOUT);
    if (res != LE_OK)
    {
        LE_ERROR("Failed to send the command !");
        return res;
    }

    res = le_atClient_GetFinalResponse(cmdRef,
                                       responseStr,
                                       PA_AT_LOCAL_STRING_SIZE);
    if ((res != LE_OK) || (strcmp(responseStr, "OK") != 0))
    {
        LE_ERROR("Function failed !");
        le_atClient_Delete(cmdRef);
        return LE_FAULT;
    }

    res = le_atClient_GetFirstIntermediateResponse(cmdRef,
                                                   responseStr,
                                                   PA_AT_LOCAL_STRING_SIZE);
    if (res != LE_OK)
    {
        LE_ERROR("Failed to get the response !");
        le_atClient_Delete(cmdRef);
        return res;
    }

    restPtr  = responseStr+strlen("+CREG: ");
    tokenPtr = strtok_r(restPtr, ",", &savePtr);

    if (tokenPtr == NULL)
    {
        LE_ERROR("Failed to get isManual");
        return LE_FAULT;
    }

    if (atoi(tokenPtr) == 1)
    {
        *isManualPtr = false;
    }
    else
    {
        *isManualPtr = true;
    }

    res = pa_mrc_GetCurrentNetwork(NULL, 0, mccPtr, mccPtrSize, mncPtr, mncPtrSize);

    le_atClient_Delete(cmdRef);
    return res;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set the Radio Access Technology Preferences
 *
 * @return LE_OK            The function succeeded.
 * @return LE_BAD_PARAMETER The parameters are invalid.
 * @return LE_FAULT         The function failed.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mrc_SetRatPreferences
(
    le_mrc_RatBitMask_t ratMask ///< [IN] A bit mask to set the Radio Access Technology preferences.
)
{
    le_atClient_CmdRef_t cmdRef = NULL;
    le_result_t          res    = LE_FAULT;
    char                 finalResponse[LE_ATDEFS_RESPONSE_MAX_BYTES];

    if (ratMask == LE_MRC_BITMASK_RAT_GSM)
    {
        res = le_atClient_SetCommandAndSend(&cmdRef,
                                            pa_utils_GetAtDeviceRef(),
                                            "AT+KSRAT=1",
                                            "",
                                            DEFAULT_AT_RESPONSE,
                                            DEFAULT_AT_CMD_TIMEOUT);
    }
    else if (ratMask == LE_MRC_BITMASK_RAT_UMTS)
    {
        res = le_atClient_SetCommandAndSend(&cmdRef,
                                            pa_utils_GetAtDeviceRef(),
                                            "AT+KSRAT=2",
                                            "",
                                            DEFAULT_AT_RESPONSE,
                                            DEFAULT_AT_CMD_TIMEOUT);
    }
    else if (ratMask == LE_MRC_BITMASK_RAT_ALL)
    {
        res = le_atClient_SetCommandAndSend(&cmdRef,
                                            pa_utils_GetAtDeviceRef(),
                                            "AT+KSRAT=4",
                                            "",
                                            DEFAULT_AT_RESPONSE,
                                            DEFAULT_AT_CMD_TIMEOUT);
    }
    else
    {
        LE_ERROR("Impossible to set the Radio Access technology");
        return LE_FAULT;
    }

    if (res != LE_OK)
    {
        LE_ERROR("Failed to send the command");
        return res;
    }

    res = le_atClient_GetFinalResponse(cmdRef,
                                       finalResponse,
                                       LE_ATDEFS_RESPONSE_MAX_BYTES);
    le_atClient_Delete(cmdRef);
    if (res != LE_OK)
    {
        LE_ERROR("Failed to get the response");
        return res;
    }
    else if (strcmp(finalResponse,"OK") != 0)
    {
        LE_ERROR("Final response is not OK");
        return LE_FAULT;
    }

    return res;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set the Rat Automatic Radio Access Technology Preference
 *
 * @return LE_OK            The function succeeded.
 * @return LE_BAD_PARAMETER The parameters are invalid.
 * @return LE_FAULT         The function failed.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mrc_SetAutomaticRatPreference
(
    void
)
{
    le_atClient_CmdRef_t cmdRef = NULL;
    le_result_t          res    = LE_FAULT;
    char                 finalResponse[LE_ATDEFS_RESPONSE_MAX_BYTES];

    res = le_atClient_SetCommandAndSend(&cmdRef,
                                        pa_utils_GetAtDeviceRef(),
                                        "AT+KSRAT=4",
                                        "",
                                        DEFAULT_AT_RESPONSE,
                                        DEFAULT_AT_CMD_TIMEOUT);
    if (res != LE_OK)
    {
        LE_ERROR("Failed to send the command");
        return res;
    }

    res = le_atClient_GetFinalResponse(cmdRef,
                                       finalResponse,
                                       LE_ATDEFS_RESPONSE_MAX_BYTES);
    le_atClient_Delete(cmdRef);
    if (res != LE_OK)
    {
        LE_ERROR("Failed to get the response");
        return res;
    }
    else if (strcmp(finalResponse,"OK") != 0)
    {
        LE_ERROR("Final response is not OK");
        return LE_FAULT;
    }

    return res;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set the Band Preferences
 *
 * @return
 * - LE_OK              on success
 * - LE_FAULT           on failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mrc_SetBandPreferences
(
    le_mrc_BandBitMask_t bands ///< [IN] A bit mask to set the Band preferences.
)
{
    return LE_FAULT;
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the Band Preferences
 *
 * @return
 * - LE_FAULT           on success
 * - LE_FAULT           on failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mrc_GetBandPreferences
(
    le_mrc_BandBitMask_t* bandsPtr ///< [OUT] A bit mask to get the Band preferences.
)
{
    return LE_FAULT;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set the LTE Band Preferences
 *
 * @return
 * - LE_FAULT           on success
 * - LE_FAULT           on failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mrc_SetLteBandPreferences
(
    le_mrc_LteBandBitMask_t bands ///< [IN] A bit mask to set the LTE Band preferences.
)
{
    return LE_FAULT;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the LTE Band Preferences
 *
 * @return
 * - LE_FAULT           on success
 * - LE_FAULT           on failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mrc_GetLteBandPreferences
(
    le_mrc_LteBandBitMask_t* bandsPtr ///< [OUT] A bit mask to get the LTE Band preferences.
)
{
    return LE_FAULT;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set the TD-SCDMA Band Preferences
 *
 * @return
 * - LE_FAULT           on success
 * - LE_FAULT           on failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mrc_SetTdScdmaBandPreferences
(
    le_mrc_TdScdmaBandBitMask_t bands ///< [IN] A bit mask to set the TD-SCDMA Band Preferences.
)
{
    return LE_FAULT;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the TD-SCDMA Band Preferences
 *
 * @return
 * - LE_FAULT           on success
 * - LE_FAULT           on failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mrc_GetTdScdmaBandPreferences
(
    le_mrc_TdScdmaBandBitMask_t* bandsPtr ///< [OUT] A bit mask to set the TD-SCDMA Band
                                          ///<  preferences.
)
{
    return LE_FAULT;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to get the current network information.
 *
 * @return
 *      - LE_OK on success
 *      - LE_OVERFLOW if the current network name can't fit in nameStr
 *      - LE_FAULT on any other failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mrc_GetCurrentNetwork
(
    char*       nameStr,               ///< [OUT] the home network Name
    size_t      nameStrSize,           ///< [IN]  the nameStr size
    char*       mccStr,                ///< [OUT] the mobile country code
    size_t      mccStrNumElements,     ///< [IN]  the mccStr size
    char*       mncStr,                ///< [OUT] the mobile network code
    size_t      mncStrNumElements      ///< [IN]  the mncStr size
)
{
    le_result_t          res      = LE_FAULT;
    char                 responseStr[PA_AT_LOCAL_STRING_SIZE] = {0};
    bool                 textMode = true;

    pa_mrc_local_GetOperatorTextMode(&textMode);

    if (nameStr != NULL)
    {
        res = pa_mrc_local_SetOperatorTextMode(true);
    }
    else if ((mccStr != NULL) && (mncStr != NULL))
    {
        res = pa_mrc_local_SetOperatorTextMode(false);
    }
    else
    {
        LE_ERROR("One parameter is NULL");
        return LE_BAD_PARAMETER;
    }

    if (res != LE_OK)
    {
        LE_ERROR("Failed to set the command");
        return res;
    }


    res = pa_utils_GetATIntermediateResponse("AT+COPS?", "+COPS:",
        responseStr, sizeof(responseStr));

    if (res != LE_OK)
    {
        LE_ERROR("Failed to get the response");
    }
    else
    {
        if (nameStr != NULL)
        {
            // +COPS?
            // +COPS: <mode>[,<format>,<oper>[,<AcT>]]
            // <oper> string type;
            //  <format> indicates if the format is alphanumeric or numeric;
            // long alphanumeric format can be upto 16 characters long
            // responseStr = "+COPS: 0,0,\"Test Usim\",7"
            int nbParam = pa_utils_CountAndIsolateLineParameters(responseStr);
            if(nbParam >= COPS_PARAM_OPERATOR_COUNT_ID)
            {
                char * charPtr = pa_utils_IsolateLineParameter(responseStr,
                    COPS_PARAM_FORMAT_COUNT_ID);
                int value = atoi(charPtr);
                // Check if it cops return long format <format> = 0 (27.007)
                if( COPS_LONG_FORMAT_VAL == value)
                {
                    charPtr = pa_utils_IsolateLineParameter(responseStr,
                        COPS_PARAM_OPERATOR_COUNT_ID);
                    if (charPtr)
                    {
                        pa_utils_RemoveQuotationString(charPtr);
                        strncpy(nameStr, charPtr, nameStrSize);
                        return LE_OK;
                    }
                }
                else
                {
                    LE_ERROR("Bad <format> %d: +COPS: <mode>[,<format>,<oper>[,<AcT>]]", value);
                }
            }
        }
        else
        {
            // PLMN Numerique
            // +COPS?
            // +COPS: <mode>[,<format>,<oper>[,<AcT>]]
            // responseStr = "+COPS: 0,2,\"00101\",7"
            int nbParam = pa_utils_CountAndIsolateLineParameters(responseStr);
            if(nbParam >= COPS_PARAM_OPERATOR_COUNT_ID)
            {
                char * charPtr = pa_utils_IsolateLineParameter(responseStr,
                    COPS_PARAM_FORMAT_COUNT_ID);
                int value = atoi(charPtr);
                // Check if it cops return numeric format <format> = 2 (27.007)
                if( COPS_NUMERIC_FORMAT_VAL == value)
                {
                    charPtr = pa_utils_IsolateLineParameter(responseStr,
                        COPS_PARAM_OPERATOR_COUNT_ID);
                    if (charPtr)
                    {
                        pa_utils_RemoveQuotationString(charPtr);
                        int lenSring = strnlen(charPtr, LE_MRC_MNC_BYTES + LE_MRC_MCC_BYTES);
                        if (lenSring >= (LE_MRC_MNC_LEN+LE_MRC_MCC_LEN-1))
                        {
                            // Mcc "001101", Mnc 101"
                            if ((mccStrNumElements < LE_MRC_MCC_BYTES)
                                || (mncStrNumElements < LE_MRC_MNC_BYTES))
                            {
                                return LE_OVERFLOW;
                            }
                            memset(mccStr, 0, mccStrNumElements);
                            memset(mncStr, 0, mncStrNumElements);
                            strncpy(mccStr, charPtr, LE_MRC_MCC_LEN);
                            strncpy(mncStr, charPtr + LE_MRC_MCC_LEN, mncStrNumElements);
                        }
                    }
                }
                else
                {
                    LE_ERROR("Bad <format> %d: +COPS: <mode>[,<format>,<oper>[,<AcT>]]", value);
                }
            }
        }
    }

    pa_mrc_local_SetOperatorTextMode(textMode);
    return res;
}


//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to delete the list of Scan Information
 *
 */
//--------------------------------------------------------------------------------------------------
void pa_mrc_DeleteScanInformation
(
    le_dls_List_t *scanInformationListPtr ///< [IN] list of pa_mrc_ScanInformation_t
)
{
    pa_mrc_ScanInformation_t* nodePtr;
    le_dls_Link_t *linkPtr;

    while ((linkPtr=le_dls_Pop(scanInformationListPtr)) != NULL)
    {
        nodePtr = CONTAINER_OF(linkPtr, pa_mrc_ScanInformation_t, link);
        le_mem_Release(nodePtr);
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to delete the list of neighboring cells information.
 *
 */
//--------------------------------------------------------------------------------------------------
void pa_mrc_DeleteNeighborCellsInfo
(
    le_dls_List_t* cellInfoListPtr   ///< [IN] list of pa_mrc_CellInfo_t
)
{
    pa_mrc_CellInfo_t* nodePtr;
    le_dls_Link_t *linkPtr;

    while ((linkPtr=le_dls_Pop(cellInfoListPtr)) != NULL)
    {
        nodePtr = CONTAINER_OF(linkPtr, pa_mrc_CellInfo_t, link);
        le_mem_Release(nodePtr);
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * This function gets the Packet switch registration state.
 *
 * @return LE_BAD_PARAMETER Bad parameter passed to the function
 * @return LE_FAULT         The function failed.
 * @return LE_TIMEOUT       No response was received.
 * @return LE_OK            The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mrc_GetPacketSwitchRegState
(
    le_mrc_NetRegState_t* statePtr  ///< [OUT] The network registration state.
)
{
    int32_t val;

    if (!statePtr)
    {
        LE_WARN("One parameter is NULL");
        return LE_BAD_PARAMETER;
    }

    GetRegistration(PA_MRC_REG_PACKET_SWITCH, false, &val);
    *statePtr = (le_mrc_NetRegState_t)val;

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to get the serving cell information.
 *
 * @return
 *  - LE_FAULT  Function failed.
 *  - LE_OK     Function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mrc_local_GetServingCellInfo
(
    char * servinCellStr,       ///< [OUT] The serving cell string information.
    size_t servingCellStrSize   ///< [IN] Buffer size.
)
{
    uint32_t setting, numParam;
    char * ptr;
    const char commandStr[] = "AT+CREG?";
    const char interStr[] = "+CREG:";

    // Get +CREG responses
    if (LE_OK != pa_utils_GetATIntermediateResponse(commandStr, interStr, servinCellStr,
                                                    servingCellStrSize))
    {
        LE_ERROR("No match");
        return LE_FAULT;
    }

    //+CREG:  <n>,<stat>[,[<lac>],[<ci>],[<AcT>]
    //+CREG: 2,1,"0001","01A2D001",7
    numParam = pa_utils_CountAndIsolateLineParameters((char*)servinCellStr);
    if (numParam >= 3)
    {
        // Extract mode <n>
        ptr = pa_utils_IsolateLineParameter(servinCellStr, 2);
        pa_utils_RemoveQuotationString(servinCellStr);
        setting = atoi(ptr);
        if ((REG_PARAM_MODE_DISABLE == setting) || (REG_PARAM_MODE_UNSO == setting))
        {
            // If the mode is REG_PARAM_MODE_DISABLE or REG_PARAM_MODE_UNSO send at+creg=2 to get
            // the results, then restore the CREG mode.
            pa_mrc_local_SetCregMode(REG_PARAM_MODE_VERBOSE);
            servinCellStr[0] = NULL_CHAR;
            pa_utils_GetATIntermediateResponse(commandStr, interStr, servinCellStr,
                                               servingCellStrSize);
            pa_mrc_local_SetCregMode(setting);
        }
        else
        {
            // Get +CREG responses
            pa_utils_GetATIntermediateResponse(commandStr, interStr, servinCellStr,
                                               servingCellStrSize);
        }
    }
    else
    {
        LE_ERROR("Error in +CREG answer %d", (int)numParam);
        return LE_FAULT;
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to get the serving cell information for EPS_ONLY attach.
 *
 * @return
 *  - LE_FAULT  Function failed.
 *  - LE_OK     Function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mrc_local_GetServingCellInfoEPS
(
    char * servinCellStr,       ///< [OUT] The serving cell string information.
    size_t servingCellStrSize   ///< [IN] Buffer size.
)
{
    uint32_t setting, numParam;
    char * ptr;
    const char commandStr[] = "AT+CEREG?";
    const char interStr[] = "+CEREG:";

    // Get +CEREG responses
    if (LE_OK != pa_utils_GetATIntermediateResponse(commandStr, interStr, servinCellStr,
                                                    servingCellStrSize))
    {
        LE_ERROR("No match");
        return LE_FAULT;
    }

    //+CEREG:  <n>,<stat>[,[<tac>],[<ci>],[<AcT>]
    //+CEREG: 2,1,"0001","01A2D001",7
    numParam = pa_utils_CountAndIsolateLineParameters((char*)servinCellStr);
    if (numParam >= 3)
    {
        // Extract mode <n>
        ptr = pa_utils_IsolateLineParameter(servinCellStr, 2);
        pa_utils_RemoveQuotationString(servinCellStr);
        setting = atoi(ptr);
        if ((REG_PARAM_MODE_DISABLE == setting) || (REG_PARAM_MODE_UNSO == setting))
        {
            // If the mode is REG_PARAM_MODE_DISABLE or REG_PARAM_MODE_UNSO send at+cereg=2 to get
            // the results, then restore the CEREG mode.
            pa_mrc_local_SetCeregMode(REG_PARAM_MODE_VERBOSE);
            servinCellStr[0] = NULL_CHAR;
            pa_utils_GetATIntermediateResponse(commandStr, interStr, servinCellStr,
                                               servingCellStrSize);
            pa_mrc_local_SetCeregMode(setting);
        }
        else
        {
            // Get +CEREG responses
            pa_utils_GetATIntermediateResponse(commandStr, interStr, servinCellStr,
                                               servingCellStrSize);
        }
    }
    else
    {
        LE_ERROR("Error in +CEREG answer %d", (int)numParam);
        return LE_FAULT;
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function allocated memory to store Cell information
 *
 * @return pointer         The function failed to initialize the module.
 *
 */
//--------------------------------------------------------------------------------------------------
pa_mrc_CellInfo_t* pa_mrc_local_AllocateCellInfo
(
    void
)
{
    pa_mrc_CellInfo_t* cellInfoPtr =
        (pa_mrc_CellInfo_t*)le_mem_ForceAlloc(CellInfoPool);

    if(cellInfoPtr)
    {
        memset(cellInfoPtr, 0, sizeof(pa_mrc_CellInfo_t));
    }

    return cellInfoPtr;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function converts the <Act> field in +CREG / +CEREG string (3GPP 27.007 release 12)
 *  to le_mrc_Rat_t type
 *
 * @return LE_FAULT The function failed to convert the Radio Access Technology
 * @return LE_OK    The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mrc_local_ConvertActToRat
(
    int  actValue,              ///< [IN] The Radio Access Technology <Act> in CREG/CEREG.
    le_mrc_Rat_t*   ratPtr      ///< [OUT] The Radio Access Technology.
)
{
    /*
     * 3GPP 27.007 release 14
     * <AcT> : integer type; access technology of the serving cell
    0    GSM
    1    GSM    Compact
    2    UTRAN
    3    GSM w/EGPRS
    4    UTRAN w/HSDPA
    5    UTRAN w/HSUPA
    6    UTRAN w/HSDPA and HSUPA
    7    E-UTRAN
    8    EC-GSM-IoT (A/Gb mode)
    9    E-UTRAN (NB-S1 mode)
     */
    switch(actValue)
    {
        case 0 :
        case 1 :
        case 3 :
            *ratPtr = LE_MRC_RAT_GSM;
            return LE_OK;

        case 2 :
        case 4 :
        case 5 :
        case 6 :
            *ratPtr = LE_MRC_RAT_UMTS;
            return LE_OK;

        // NB1 network are considered as LTE network
        case 9 :
        case 7 :
            *ratPtr = LE_MRC_RAT_LTE;
            return LE_OK;

        default :
            LE_ERROR("Debug <Act> = %d", actValue);
            *ratPtr = LE_MRC_RAT_UNKNOWN;
            break;
    }

    return LE_FAULT;
}

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
le_result_t pa_mrc_local_ParseNetworkScan
(
    char * responseStr,                        ///< [IN] Resposne to parse
    le_mrc_RatBitMask_t ratMask,               ///< [IN] The network mask
    pa_mrc_ScanType_t   scanType,              ///< [IN] the scan type
    le_dls_List_t*      scanInformationListPtr ///< [OUT] list of pa_mrc_ScanInformation_t
)
{
    le_result_t             res = LE_OK;
    int                     nbNetwork;
    char                    mncStr[LE_MRC_MNC_BYTES];
    char                    mccStr[LE_MRC_MCC_BYTES];
    char *                  charPtr = NULL;
    /* AT+COPS=? 3GPP 27.007 Release 12 response:
     * +COPS: [list of supported(<stat>,long alphanumeric<oper>,
     *          shortalphanumeric<oper>,numeric<oper>[,<AcT>])s]
     *          [,,(list ofsupported<mode>s),(list of supported<format>s)]
     * AT+COPS=?
     * +COPS: (2,"RADIOLINJA","RL","24405",7),(0,"TELE","TELE","24491",7)
     * +COPS: (2,"T-Mobile USA","TMO","310260"),,(0-4),(0-2)
     * +COPS: (1,,,"00101",7),,(0-4),(0-2)
     * OK
     */
    if(NULL == responseStr)
    {
        return LE_FAULT;
    }

    // Remove  [,,(list ofsupported<mode>s),(list of supported<format>s)] If present
    // +COPS: (2,"T-Mobile USA","TMO","310260"),,(0-4),(0-2)
    charPtr = strstr(responseStr, ",,(");
    if(charPtr)
    {
        // +COPS: (2,"T-Mobile USA","TMO","310260")\0
        *charPtr = NULL_CHAR;
    }

    // +COPS: (2,"RADIOLINJA","RL","24405",7),(0,"TELE","TELE","24491",7)
    nbNetwork = pa_utils_CountAndIsolateCopsParameters(responseStr);
    // +COPS: \02,"RADIOLINJA","RL","24405",7\0,\00,"TELE","TELE","24491",7\0

    if ( (LE_OK == res) && nbNetwork )
    {
        pa_mrc_ScanInformation_t* newScanInformation = NULL;
        int i;
        le_mrc_Rat_t rat;
        uint32_t networkStatus;

        for (i=1; i<=nbNetwork; i++)
        {
            // 2,"RADIOLINJA","RL","24405",7\0
            // 0,"TELE","TELE","24491",7\0
            // +COPS: \02,\"RADIOLINJA\",\"RL\",\"24405\",7\0,\00,\"TELE\",\"TELE\",\"24491\",7\0"
            char * plmnPtr = pa_utils_IsolateLineParameter(responseStr, i * 2);

            // Extract fields
            // 2,"RADIOLINJA","RL","24405",7\0
            // 0,"TELE","TELE","24491",7\0
            res = ExtractCopsPlmn(plmnPtr, mccStr, mncStr, &rat, &networkStatus);

            newScanInformation = FindScanInformation(scanInformationListPtr, mccStr, mncStr, rat);
            if (newScanInformation == NULL)
            {
                newScanInformation = le_mem_ForceAlloc(ScanInformationPool);
                InitializeScanInformation(newScanInformation);
                le_dls_Queue(scanInformationListPtr,&(newScanInformation->link));

                memcpy(newScanInformation->mobileCode.mcc, mccStr, LE_MRC_MCC_BYTES);
                memcpy(newScanInformation->mobileCode.mnc, mncStr, LE_MRC_MNC_BYTES);
                newScanInformation->rat = rat;
                /* 3GPP 27.007 Release 12 values definition
                 * <stat> : integer type
                 * 0    unknown
                 * 1    available
                 * 2    current
                 * 3    forbidden
                 */

                //     bool  isInUse;        ///< network is in use
                //     bool  isAvailable;    ///< network can be connected
                //     bool  isHome;         ///< home status
                //     bool  isForbidden;    ///< forbidden status
                switch(networkStatus)
                {
                    default:
                    case 0:
                        break;
                    case 1:
                    {
                        newScanInformation->isAvailable = true;
                    }
                    break;
                    case 2:
                    {
                        newScanInformation->isInUse = true;
                        newScanInformation->isAvailable = true;
                    }
                    break;
                    case 3:
                    {
                        newScanInformation->isForbidden = true;
                    }
                    break;
                }
                LE_DEBUG("MCC %s, MNS %s, rat %d",
                    newScanInformation->mobileCode.mcc,
                    newScanInformation->mobileCode.mnc,
                    newScanInformation->rat);
            }
        }
    }
    return res;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function get the network operator mode (text or number mode).
 *
 * @return LE_FAULT         The function failed.
 * @return LE_OK            The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mrc_local_GetOperatorTextMode
(
    bool*    textMode
)
{
    le_result_t          res    = LE_FAULT;
    char                 responseStr[PA_AT_LOCAL_SHORT_SIZE];

    res = pa_utils_GetATIntermediateResponse("AT+COPS?", "+COPS", responseStr, sizeof(responseStr));

    if (res != LE_OK)
    {
        LE_ERROR("Failed to send the command");
        return res;
    }

    // responseStr = "+COPS: 0,0,\"Test Usim\",7"
    int nbParam = pa_utils_CountAndIsolateLineParameters(responseStr);
    if(nbParam >= COPS_PARAM_MODE_COUNT_ID)
    {
        char * charPtr = pa_utils_IsolateLineParameter(responseStr,
            COPS_PARAM_MODE_COUNT_ID);
        int value = atoi(charPtr);
        if(value == 0)
        {
            *textMode = true;
        }
        else
        {
            *textMode = false;
        }
    }

    return res;
}
