/**
 * @file pa_mdc.c
 *
 * AT implementation of c_pa_mdc API.
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#include "legato.h"

#include "pa_mdc.h"

#ifdef MK_ATPROXY_CONFIG_CLIB
#include "le_atClientIF.h"
#endif

#include "pa_utils.h"
#include "pa_mdc_local.h"
#include "pa_mdc_utils_local.h"

#include <sys/wait.h>

//--------------------------------------------------------------------------------------------------
/**
 * Mapping between profileIndex and PDP <cid>
 */
//--------------------------------------------------------------------------------------------------
static uint8_t ProfileIndexCidMapping[PA_MDC_MAX_PROFILE+1] = {0};


//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to compute default IPV6 Gateway.
 * @return
 *      - LE_OK on success
 *      - LE_OVERFLOW if the IP address would not fit in gatewayAddrStr
 *      - LE_FAULT for all other errors
 */
//--------------------------------------------------------------------------------------------------
static le_result_t GetIpv6DefaultGateway
(
    uint32_t                profileIndex,      ///< [IN]  The profile to use
    char*                   gatewayAddrStr,    ///< [OUT] The gateway IP address in dotted format
    size_t                  gatewayAddrStrSize ///< [IN]  The size in bytes of the address buffer
)
{
    LE_UNUSED(profileIndex);
    le_result_t res;
    res = le_utf8_Copy(gatewayAddrStr, "::", gatewayAddrStrSize, NULL);
    LE_WARN("Default IPV6 Gw %s", gatewayAddrStr);
    return res;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the gateway IP address for the given profile, if the data session is connected.
 *
 * @return
 *      - LE_OK on success
 *      - LE_BAD_PARAMETER on null of profileIndex
 *      - LE_OVERFLOW if the IP address would not fit in gatewayAddrStr
 *      - LE_FAULT for all other errors
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mdc_GetGatewayAddress
(
    uint32_t                profileIndex,      ///< [IN]  The profile to use
    le_mdmDefs_IpVersion_t  ipVersion,         ///< [IN]  IP Version
    char*                   gatewayAddrStr,    ///< [OUT] The gateway IP address in dotted format
    size_t                  gatewayAddrStrSize ///< [IN]  The size in bytes of the address buffer
)
{
    le_atClient_CmdRef_t cmdRef   = NULL;
    le_result_t          res      = LE_FAULT;
    static const char    cgcontrdpStr[]="AT+CGCONTRDP=";
    char                 commandStr[sizeof(cgcontrdpStr)+PA_AT_COMMAND_PADDING] ;
    char                 responseStr[PA_AT_LOCAL_LONG_STRING_SIZE];
    char                 gwStr[DEFAULT_AT_BUFFER_SHORT_BYTES];

    if (!profileIndex)
    {
        LE_DEBUG("One parameter is NULL");
        return LE_BAD_PARAMETER;
    }
    gatewayAddrStr[0] = NULL_CHAR;

    // AT+CGCONTRDP command used to retrieve the IPv4 GW
    // profileIndex is the CID
    snprintf(commandStr, sizeof(commandStr),"%s%"PRIu32, cgcontrdpStr, profileIndex);
    snprintf(responseStr, sizeof(responseStr),"+CGCONTRDP: %"PRIu32, profileIndex);

    res = le_atClient_SetCommandAndSend(&cmdRef,
                                        pa_utils_GetAtDeviceRef(),
                                        commandStr,
                                        responseStr,
                                        DEFAULT_AT_RESPONSE,
                                        DEFAULT_AT_CMD_TIMEOUT);

    if (res != LE_OK)
    {
        LE_ERROR("Failed to send the command");
        return res;
    }

    responseStr[0] = NULL_CHAR;
    res = le_atClient_GetFinalResponse(cmdRef,
                                       responseStr,
                                       sizeof(responseStr));

    if (res != LE_OK)
    {
        LE_ERROR("Failed to get the final response");
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
    // Get First response
    res = le_atClient_GetFirstIntermediateResponse(cmdRef,
                                                   responseStr,
                                                   sizeof(responseStr));
    if (res != LE_OK)
    {
        LE_ERROR("Failed to get the intermediate response");
        le_atClient_Delete(cmdRef);
        return LE_FAULT;
    }

    // Extract the IP address from the response
    res = pa_mdc_util_GetGWAddr(responseStr, gwStr, sizeof(gwStr));
    if(LE_OK != res)
    {
        le_atClient_Delete(cmdRef);
        return LE_FAULT;
    }

    // Check address type
    if(pa_mdc_util_CheckConvertIPAddressFormat(gwStr, ipVersion))
    {
        le_atClient_Delete(cmdRef);
        res = le_utf8_Copy(gatewayAddrStr, gwStr, gatewayAddrStrSize, NULL);
        return res;
    }

    responseStr[0] = NULL_CHAR;
    // Get Second response if the first one doen't match
    res = le_atClient_GetNextIntermediateResponse(cmdRef,
                                                  responseStr,
                                                  sizeof(responseStr));
    le_atClient_Delete(cmdRef);

    if (res != LE_OK)
    {
        if(LE_MDMDEFS_IPV6 == ipVersion)
        {
            res = GetIpv6DefaultGateway(profileIndex, gatewayAddrStr, gatewayAddrStrSize);
            LE_WARN("No Gw found %s", gatewayAddrStr);
            return res;
        }
        LE_WARN("Failed to get the second intermediate response");
        return res;
    }

    // Extract the IP address from the response
    res = pa_mdc_util_GetGWAddr(responseStr, gwStr, sizeof(gwStr));
    if(LE_OK != res)
    {
        return res;
    }

    // Check address type
    if(pa_mdc_util_CheckConvertIPAddressFormat(gwStr, ipVersion))
    {
        res = le_utf8_Copy(gatewayAddrStr, gwStr, gatewayAddrStrSize, NULL);
    }
    else
    {
        if(LE_MDMDEFS_IPV6 == ipVersion)
        {
            res = GetIpv6DefaultGateway(profileIndex, gatewayAddrStr, gatewayAddrStrSize);
            return res;
        }
        res = LE_FAULT;
    }

    return res;
}


//--------------------------------------------------------------------------------------------------
/**
 * Reject a MT-PDP data session for the given profile
 *
 * @return
 *      - LE_OK on success
 *      - LE_BAD_PARAMETER if the input parameter is not valid
 *      - LE_FAULT for other failures
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mdc_RejectMtPdpSession
(
    uint32_t profileIndex
)
{
    LE_UNUSED(profileIndex);
    return LE_FAULT;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the index of the default profile for Bearer Independent Protocol (link to the platform)
 *
 * @return
 *      - LE_OK on success
 *      - LE_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mdc_GetBipDefaultProfileIndex
(
    uint32_t* profileIndexPtr   ///< [OUT] index of the profile.
)
{
   *profileIndexPtr = 2;
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get session type for the given profile (IPV4 or IPV6)
 *
 * @return
 *      - LE_OK on success
 *      - LE_FAULT for other failures
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mdc_GetSessionType
(
    uint32_t              profileIndex,     ///< [IN] The profile to use
    pa_mdc_SessionType_t* sessionIpPtr      ///< [OUT] IP family session
)
{
    le_result_t ipv4Res = LE_FAULT;
    le_result_t ipv6Res = LE_FAULT;
    char addr[50];

    ipv4Res = pa_mdc_GetIPAddress(profileIndex, (le_mdmDefs_IpVersion_t)PA_MDC_SESSION_IPV4, addr, sizeof(addr));
    ipv6Res = pa_mdc_GetIPAddress(profileIndex, (le_mdmDefs_IpVersion_t)PA_MDC_SESSION_IPV6, addr, sizeof(addr));

    if((LE_OK == ipv4Res) && (LE_OK == ipv6Res))
    {
        *sessionIpPtr = PA_MDC_SESSION_IPV4V6;
    }
    else if(LE_OK == ipv4Res)
    {
        *sessionIpPtr = PA_MDC_SESSION_IPV4;
    }
    else if(LE_OK == ipv6Res)
    {
        *sessionIpPtr = PA_MDC_SESSION_IPV6;
    }
    else
    {
        return LE_FAULT;
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Map a profile on a network interface
 *
 * * @return
 *      - LE_OK on success
 *      - LE_UNSUPPORTED if not supported by the target
 *      - LE_FAULT for all other errors
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mdc_MapProfileOnNetworkInterface
(
    uint32_t         profileIndex,         ///< [IN] The profile to use
    const char*      interfaceNamePtr      ///< [IN] Network interface name
)
{
    LE_UNUSED(profileIndex);
    LE_UNUSED(interfaceNamePtr);

    return LE_UNSUPPORTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the list of all profiles
 *
 * @return
 *      - LE_OK on success
 *      - LE_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mdc_GetProfileList
(
    le_mdc_ProfileInfo_t* profileList, ///< [OUT] list of available profiles
    size_t* listSize                   ///< [INOUT] list size
)
{
    LE_UNUSED(profileList);
    LE_UNUSED(listSize);

    LE_ERROR("Unsupported function called");
    return LE_UNSUPPORTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get profileIndex from PDP cid
 *
 * @return The profileIndex (session ID) mapped on cid (pdp context)
 */
//--------------------------------------------------------------------------------------------------
uint32_t pa_mdc_local_GetProfileIndexFromCid
(
    uint32_t cid                ///< [IN]  The cid (pdp context)
)
{
    if ((!cid) || (cid > PA_MDC_MAX_PROFILE))
    {
        LE_ERROR("Wrong cid");
        return 0;
    }

    for(int i=1; i<=PA_MDC_MAX_PROFILE; i++)
    {
        if(cid == ProfileIndexCidMapping[i])
        {
            return i;
        }
    }
    return 0;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set PDP cid from profileIndex (session ID)
 */
//--------------------------------------------------------------------------------------------------
void pa_mdc_local_SetCidFromProfileIndex
(
    uint32_t profileIndex,      ///< [IN]  The session ID (=profileIndex)
    uint32_t cid                ///< [IN]  The cid (pdp context)
)
{
    LE_DEBUG("Add Id%" PRIu32 ",Cid %" PRIu32, profileIndex, cid);

    if (profileIndex > PA_MDC_MAX_PROFILE)
    {
        LE_ERROR("Wrong session ID");
        return;
    }
    if (cid > PA_MDC_MAX_PROFILE)
    {
        LE_ERROR("Wrong Cid");
        return;
    }
    ProfileIndexCidMapping[profileIndex] = cid;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get PDP cid from profileIndex
 *
 * @return The cid (pdp context) mapped on profileIndex (session ID)
 */
//--------------------------------------------------------------------------------------------------
uint32_t pa_mdc_local_GetCidFromProfileIndex
(
    uint32_t profileIndex       ///< [IN]  The session ID (=profileIndex)
)
{
    if ((!profileIndex) || (profileIndex > PA_MDC_MAX_PROFILE))
    {
        LE_ERROR("Wrong session ID");
        return 0;
    }
    return ProfileIndexCidMapping[profileIndex];
}
