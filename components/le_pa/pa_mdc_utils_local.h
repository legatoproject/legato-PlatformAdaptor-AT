/** @file pa_mdc_utils_local.h
 *
 *
 * Copyright (C) Sierra Wireless Inc.
 */

#ifndef LEGATO_PAMDCUTILSLOCAL_INCLUDE_GUARD
#define LEGATO_PAMDCUTILSLOCAL_INCLUDE_GUARD


//--------------------------------------------------------------------------------------------------
/**
 * Attach or detach from the Packet Domain service.
 *
 * @return LE_OK            modem is attached or detached from the Packet domain service
 * @return LE_FAULT         modem could not attach or detach from the Packet domain service
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mdc_utils_AttachPS
(
    bool toAttach               ///< [IN] boolean value
);

//--------------------------------------------------------------------------------------------------
/**
 * This function return true if profileIndex PDP context is activated
 *
 */
//--------------------------------------------------------------------------------------------------
bool pa_mdc_utils_IsConnected
(
    uint32_t profileIndex           ///< [IN]  The profile to use
);

//--------------------------------------------------------------------------------------------------
/**
 * This function return the DNS1 and DNS2 addresses from +CGCONTRDP response.
 *
 * @return LE_FAULT         The function failed.
 * @return LE_OK            The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mdc_util_GetDNSAddr
(
    const char*  inputStringStr,    ///< [IN] Input string to parse
    char*  dns1Str,                 ///< [OUT] DNS1 address string
    size_t dns1StrLen,              ///< [IN] DNS1 buffer size
    char*  dns2Str,                 ///< [OUT] DNS2 address string
    size_t dns2StrLen               ///< [IN] DNS2 buffer size
);

//--------------------------------------------------------------------------------------------------
/**
 * This function check IPv4/IPv6 string format. Convert DOT IPV6 address to hex format if detected
 *
 * @return true         The inputStringStr string is a ipVersion type
 * @return false        The inputStringStr doesn't match with the ipVersion type.
 *
 */
//--------------------------------------------------------------------------------------------------
bool pa_mdc_util_CheckConvertIPAddressFormat
(
    char* ipAddressStr,                       ///< [IN/OUT] IP address
    le_mdmDefs_IpVersion_t  ipVersion         ///< [IN] IP Version
);
#endif // LEGATO_PAMDCUTILSLOCAL_INCLUDE_GUARD
