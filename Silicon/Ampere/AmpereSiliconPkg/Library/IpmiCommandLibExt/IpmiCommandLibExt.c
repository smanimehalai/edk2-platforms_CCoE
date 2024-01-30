/** @file

  Copyright (c) 2021, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>

#include <IndustryStandard/Ipmi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/IpmiCommandLibExt.h>
#include <Library/IpmiLib.h>
#include <Library/MemoryAllocationLib.h>

/**
  Get BMC LAN Information of specific channel.

  @param[in]     BmcChannel            BMC Channel to retrieve LAN information.
  @param[out]    Info                  Pointer to return buffer.

  @retval EFI_SUCCESS                  The command byte stream was successfully submit to the device and a response was successfully received.
  @retval EFI_INVALID_PARAMETER        Info was NULL.
  @retval other                        Failed to get BMC LAN info.
**/
EFI_STATUS
EFIAPI
IpmiGetBmcLanInfo (
  IN  UINT8          BmcChannel,
  OUT BMC_LAN_INFO   *Info
  )
{
  EFI_STATUS                                     Status;
  IPMI_GET_CHANNEL_INFO_REQUEST                  GetChannelInfoRequest;
  IPMI_GET_CHANNEL_INFO_RESPONSE                 GetChannelInfoResponse;
  IPMI_GET_LAN_CONFIGURATION_PARAMETERS_REQUEST  GetConfigurationParametersRequest;
  IPMI_GET_LAN_CONFIGURATION_PARAMETERS_RESPONSE *GetConfigurationParametersResponse;
  UINT32                                         ResponseSize;

  if (Info == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Get Channel Information
  //
  ZeroMem (&GetChannelInfoRequest, sizeof (GetChannelInfoRequest));
  GetChannelInfoRequest.ChannelNumber.Bits.ChannelNo = BmcChannel;

  ResponseSize = sizeof (GetChannelInfoResponse);
  Status = IpmiSubmitCommand (
              IPMI_NETFN_APP,
              IPMI_APP_GET_CHANNEL_INFO,
              (VOID *)&GetChannelInfoRequest,
              sizeof (GetChannelInfoRequest),
              (VOID *)&GetChannelInfoResponse,
              &ResponseSize
              );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a Failed to submit IPMI command\n", __FUNCTION__));
    return Status;
  }

  //
  // Check for LAN interface
  //
  if (EFI_ERROR (Status)
      || GetChannelInfoResponse.CompletionCode != IPMI_COMP_CODE_NORMAL
      || GetChannelInfoResponse.MediumType.Bits.ChannelMediumType != IPMI_CHANNEL_MEDIA_TYPE_802_3_LAN)
  {
    return EFI_NOT_FOUND;
  }

  GetConfigurationParametersResponse = AllocateZeroPool (
                                         sizeof (*GetConfigurationParametersResponse)
                                         + sizeof (IPMI_LAN_IP_ADDRESS)
                                         );
  if (GetConfigurationParametersResponse == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  //
  // Get LAN IP Address
  //
  ZeroMem (&GetConfigurationParametersRequest, sizeof (GetConfigurationParametersRequest));
  GetConfigurationParametersRequest.ChannelNumber.Uint8 = BmcChannel;
  GetConfigurationParametersRequest.ParameterSelector = IpmiLanIpAddress;
  GetConfigurationParametersRequest.SetSelector = 0;
  GetConfigurationParametersRequest.BlockSelector = 0;

  ResponseSize = sizeof (*GetConfigurationParametersResponse) + sizeof (IPMI_LAN_IP_ADDRESS);

  Status = IpmiSubmitCommand (
             IPMI_NETFN_TRANSPORT,
             IPMI_TRANSPORT_GET_LAN_CONFIG_PARAMETERS,
             (VOID *)&GetConfigurationParametersRequest,
             sizeof (GetConfigurationParametersRequest),
             (VOID *)GetConfigurationParametersResponse,
             &ResponseSize
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a Failed to submit IPMI command\n", __FUNCTION__));
    goto Exit;
  }

  if (GetChannelInfoResponse.CompletionCode != IPMI_COMP_CODE_NORMAL) {
    Status = EFI_NOT_FOUND;
    goto Exit;
  }

  CopyMem (
    Info->IpAddress.IpAddress,
    GetConfigurationParametersResponse->ParameterData,
    sizeof (IPMI_LAN_IP_ADDRESS)
    );

  //
  // Get Subnet Mask
  //
  ZeroMem (&GetConfigurationParametersRequest, sizeof (GetConfigurationParametersRequest));
  GetConfigurationParametersRequest.ChannelNumber.Uint8 = BmcChannel;
  GetConfigurationParametersRequest.ParameterSelector = IpmiLanSubnetMask;
  GetConfigurationParametersRequest.SetSelector = 0;
  GetConfigurationParametersRequest.BlockSelector = 0;

  ResponseSize = sizeof (*GetConfigurationParametersResponse) + sizeof (IPMI_LAN_IP_ADDRESS);

  Status = IpmiSubmitCommand (
             IPMI_NETFN_TRANSPORT,
             IPMI_TRANSPORT_GET_LAN_CONFIG_PARAMETERS,
             (VOID *)&GetConfigurationParametersRequest,
             sizeof (GetConfigurationParametersRequest),
             (VOID *)GetConfigurationParametersResponse,
             &ResponseSize
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a Failed to send IPMI command\n", __FUNCTION__));
    goto Exit;
  }

  if (GetChannelInfoResponse.CompletionCode != IPMI_COMP_CODE_NORMAL) {
    Status = EFI_NOT_FOUND;
    goto Exit;
  }

  CopyMem (
    Info->SubnetMask.IpAddress,
    GetConfigurationParametersResponse->ParameterData,
    sizeof (IPMI_LAN_IP_ADDRESS)
    );

Exit:
  FreePool (GetConfigurationParametersResponse);
  return Status;
}

/**
  Set BMC Boot Options parameter.

  @param[in]    SetBootOptionsRequest      A pointer to an callee allocated buffer containing request data.
  @param[in]    SetBootOptionsRequestSize  Size of SetBootOptionsRequest.
  @param[out]   CompletionCode             Returned Completion Code from BMC.

  @retval EFI_SUCCESS                      The command byte stream was successfully submit to the device and a response was successfully received.
  @retval EFI_INVALID_PARAMETER            SetBootOptionsRequest or CompletionCode was NULL.
  @retval other                            Failed to write data to the device.
**/
EFI_STATUS
EFIAPI
IpmiSetSystemBootOptionsExt (
  IN  IPMI_SET_BOOT_OPTIONS_REQUEST  *SetBootOptionsRequest,
  IN  UINT32                         SetBootOptionsRequestSize,
  OUT UINT8                          *CompletionCode
  )
{
  EFI_STATUS                    Status;
  UINT32                        DataSize;

  if (SetBootOptionsRequest == NULL
      || CompletionCode == NULL)
  {
    return EFI_INVALID_PARAMETER;
  }

  DataSize = sizeof (*CompletionCode);
  Status = IpmiSubmitCommand (
             IPMI_NETFN_CHASSIS,
             IPMI_CHASSIS_SET_SYSTEM_BOOT_OPTIONS,
             (VOID *)SetBootOptionsRequest,
             SetBootOptionsRequestSize,
             (VOID *)CompletionCode,
             &DataSize
             );

  switch (*CompletionCode) {
  case IPMI_COMP_CODE_NORMAL:
    break;

  case IPMI_COMP_CODE_PARAM_UNSUPPORTED:
    Status = EFI_UNSUPPORTED;
    break;

  case IPMI_COMP_CODE_SET_IN_PROGRESS:
    Status = EFI_NOT_READY;
    break;

  case IPMI_COMP_CODE_READ_ONLY:
    Status = EFI_ACCESS_DENIED;
    break;

  default:
    Status = EFI_DEVICE_ERROR;
    break;
  }

  return Status;
}

/**
  Get BMC Boot Options parameter.

  @param[in]        ParameterSelector           Boot Options parameter which want to get.
  @param[out]       GetBootOptionsResponse      A pointer to an callee allocated buffer to store response data.
  @param[in, out]   GetBootOptionsResponseSize  Size of GetBootOptionsResponse.

  @retval EFI_SUCCESS                           The command byte stream was successfully submit to the device and a response was successfully received.
  @retval EFI_INVALID_PARAMETER                 GetBootOptionsResponse or GetBootOptionsResponseSize was NULL.
  @retval other                                 Failed to write data to the device.
**/
EFI_STATUS
EFIAPI
IpmiGetSystemBootOptionsExt (
  IN     UINT8                          ParameterSelector,
  OUT    IPMI_GET_BOOT_OPTIONS_RESPONSE *GetBootOptionsResponse,
  IN OUT UINT32                         *GetBootOptionsResponseSize
  )
{
  EFI_STATUS                    Status;
  IPMI_GET_BOOT_OPTIONS_REQUEST GetBootOptionsRequest;

  if (GetBootOptionsResponse == NULL
      || GetBootOptionsResponseSize == NULL)
  {
    return EFI_INVALID_PARAMETER;
  }

  GetBootOptionsRequest.ParameterSelector.Bits.ParameterSelector = ParameterSelector;
  GetBootOptionsRequest.ParameterSelector.Bits.Reserved = 0;
  GetBootOptionsRequest.SetSelector = 0;
  GetBootOptionsRequest.BlockSelector = 0;

  Status = IpmiSubmitCommand (
             IPMI_NETFN_CHASSIS,
             IPMI_CHASSIS_GET_SYSTEM_BOOT_OPTIONS,
             (VOID *)&GetBootOptionsRequest,
             sizeof (GetBootOptionsRequest),
             (VOID *)GetBootOptionsResponse,
             GetBootOptionsResponseSize
             );

  switch (GetBootOptionsResponse->CompletionCode) {
  case IPMI_COMP_CODE_NORMAL:
    break;

  case IPMI_COMP_CODE_PARAM_UNSUPPORTED:
    Status = EFI_UNSUPPORTED;
    break;

  default:
    Status = EFI_DEVICE_ERROR;
    break;
  }

  return Status;
}

/**
  Set Boot Info Acknowledge to notify BMC that the Boot Flags has been handled by UEFI.

  @retval EFI_SUCCESS                           The command byte stream was successfully submit to the device and a response was successfully received.
  @retval other                                 Failed to write data to the device.
**/
EFI_STATUS
EFIAPI
IpmiSetBootInfoAck (
  VOID
  )
{
  EFI_STATUS                             Status;
  IPMI_BOOT_OPTIONS_RESPONSE_PARAMETER_4 *ParameterData;
  IPMI_SET_BOOT_OPTIONS_REQUEST          *SetBootOptionsRequest;
  UINT32                                 SetBootOptionsRequestSize;
  UINT8                                  CompletionCode;

  SetBootOptionsRequestSize = sizeof (*SetBootOptionsRequest) + sizeof (*ParameterData);
  SetBootOptionsRequest = AllocateZeroPool (SetBootOptionsRequestSize);
  if (SetBootOptionsRequest == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  SetBootOptionsRequest->ParameterValid.Bits.ParameterSelector = IPMI_BOOT_OPTIONS_PARAMETER_BOOT_INFO_ACK;
  SetBootOptionsRequest->ParameterValid.Bits.MarkParameterInvalid = 0x0;

  ParameterData = (IPMI_BOOT_OPTIONS_RESPONSE_PARAMETER_4 *)&SetBootOptionsRequest->ParameterData;
  ParameterData->WriteMask = BIT0;
  ParameterData->BootInitiatorAcknowledgeData = 0x0;

  Status = IpmiSetSystemBootOptionsExt (
             SetBootOptionsRequest,
             SetBootOptionsRequestSize,
             &CompletionCode
             );

  FreePool (SetBootOptionsRequest);
  return Status;
}

/**
  Retrieve Boot Info Acknowledge from BMC.

  @param[out] BootInitiatorAcknowledgeData      Pointer to returned buffer.

  @retval EFI_SUCCESS                           The command byte stream was successfully submit to the device and a response was successfully received.
  @retval EFI_INVALID_PARAMETER                 BootInitiatorAcknowledgeData was NULL.
  @retval other                                 Failed to write data to the device.
**/
EFI_STATUS
EFIAPI
IpmiGetBootInfoAck (
  OUT UINT8 *BootInitiatorAcknowledgeData
  )
{
  EFI_STATUS                              Status;
  IPMI_BOOT_OPTIONS_RESPONSE_PARAMETER_4  *ParameterData;
  IPMI_GET_BOOT_OPTIONS_RESPONSE          *GetBootOptionsResponse;
  UINT32                                  GetBootOptionsResponseSize;

  if (BootInitiatorAcknowledgeData == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  GetBootOptionsResponseSize = sizeof (*GetBootOptionsResponse) + sizeof (*ParameterData);
  GetBootOptionsResponse = AllocateZeroPool (GetBootOptionsResponseSize);
  if (GetBootOptionsResponse == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  ParameterData = (IPMI_BOOT_OPTIONS_RESPONSE_PARAMETER_4 *)&GetBootOptionsResponse->ParameterData;

  Status = IpmiGetSystemBootOptionsExt (
             IPMI_BOOT_OPTIONS_PARAMETER_BOOT_INFO_ACK,
             GetBootOptionsResponse,
             &GetBootOptionsResponseSize
             );

  if (!EFI_ERROR (Status)) {
    *BootInitiatorAcknowledgeData = ParameterData->BootInitiatorAcknowledgeData;
  }

  FreePool (GetBootOptionsResponse);
  return Status;
}

/**
  Send command to clear BMC Boot Flags parameter.

  @retval EFI_SUCCESS                           The command byte stream was successfully submit to the device and a response was successfully received.
  @retval other                                 Failed to write data to the device.
**/
EFI_STATUS
EFIAPI
IpmiClearBootFlags (
  VOID
  )
{
  EFI_STATUS                              Status;
  IPMI_SET_BOOT_OPTIONS_REQUEST           *SetBootOptionsRequest;
  UINT32                                  SetBootOptionsRequestSize;
  UINT8                                   CompletionCode;

  SetBootOptionsRequestSize = sizeof (*SetBootOptionsRequest) + sizeof (IPMI_BOOT_OPTIONS_RESPONSE_PARAMETER_5);
  SetBootOptionsRequest = AllocateZeroPool (SetBootOptionsRequestSize);
  if (SetBootOptionsRequest == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  SetBootOptionsRequest->ParameterValid.Bits.ParameterSelector = IPMI_BOOT_OPTIONS_PARAMETER_BOOT_FLAGS;

  Status = IpmiSetSystemBootOptionsExt (
             SetBootOptionsRequest,
             SetBootOptionsRequestSize,
             &CompletionCode
             );

  FreePool (SetBootOptionsRequest);
  return Status;
}

/**
  Retrieve Boot Flags parameter from BMC.

  @param[out] BootFlags                         Pointer to returned buffer.

  @retval EFI_SUCCESS                           The command byte stream was successfully submit to the device and a response was successfully received.
  @retval EFI_INVALID_PARAMETER                 BootFlags was NULL.
  @retval other                                 Failed to write data to the device.
**/
EFI_STATUS
EFIAPI
IpmiGetBootFlags (
  OUT IPMI_BOOT_FLAGS_INFO *BootFlags
  )
{
  EFI_STATUS                              Status;
  IPMI_BOOT_OPTIONS_RESPONSE_PARAMETER_5  *ParameterData;
  IPMI_GET_BOOT_OPTIONS_RESPONSE          *GetBootOptionsResponse;
  UINT32                                  GetBootOptionsResponseSize;

  if (BootFlags == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  GetBootOptionsResponseSize = sizeof (*GetBootOptionsResponse) + sizeof (*ParameterData);
  GetBootOptionsResponse = AllocateZeroPool (GetBootOptionsResponseSize);
  if (GetBootOptionsResponse == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  ParameterData = (IPMI_BOOT_OPTIONS_RESPONSE_PARAMETER_5 *)&GetBootOptionsResponse->ParameterData;

  Status = IpmiGetSystemBootOptionsExt (
             IPMI_BOOT_OPTIONS_PARAMETER_BOOT_FLAGS,
             GetBootOptionsResponse,
             &GetBootOptionsResponseSize
             );

  if (!EFI_ERROR (Status)) {
    BootFlags->IsPersistent = ParameterData->Data1.Bits.PersistentOptions;
    BootFlags->IsBootFlagsValid = ParameterData->Data1.Bits.BootFlagValid;
    BootFlags->IsCmosClear = ParameterData->Data2.Bits.CmosClear;
    BootFlags->DeviceSelector = ParameterData->Data2.Bits.BootDeviceSelector;
    BootFlags->InstanceSelector = ParameterData->Data5.Bits.DeviceInstanceSelector;
  }

  FreePool (GetBootOptionsResponse);
  return Status;
}

/**
  Send command to clear BMC Clear Cmos in Boot Flags parameter.

  @retval EFI_SUCCESS      The command byte stream was successfully submit
                           to the device and a response was successfully received.
  @retval other            Failed to write data to the device.
**/
EFI_STATUS
EFIAPI
IpmiClearCmosBootFlags (
  VOID
  )
{
  EFI_STATUS                              Status;
  IPMI_SET_BOOT_OPTIONS_REQUEST           *SetBootOptionsRequest;
  UINT32                                  SetBootOptionsRequestSize;
  UINT8                                   CompletionCode;
  IPMI_BOOT_OPTIONS_RESPONSE_PARAMETER_5  *ParameterData;
  IPMI_GET_BOOT_OPTIONS_RESPONSE          *GetBootOptionsResponse;
  UINT32                                  GetBootOptionsResponseSize;

  //
  // Get current boot flag
  //
  GetBootOptionsResponseSize = sizeof (*GetBootOptionsResponse) + sizeof (*ParameterData);
  GetBootOptionsResponse     = AllocateZeroPool (GetBootOptionsResponseSize);
  if (GetBootOptionsResponse == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  ParameterData = (IPMI_BOOT_OPTIONS_RESPONSE_PARAMETER_5 *)&GetBootOptionsResponse->ParameterData;

  Status = IpmiGetSystemBootOptionsExt (
             IPMI_BOOT_OPTIONS_PARAMETER_BOOT_FLAGS,
             GetBootOptionsResponse,
             &GetBootOptionsResponseSize
             );
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  SetBootOptionsRequestSize = sizeof (*SetBootOptionsRequest) + sizeof (*ParameterData);
  SetBootOptionsRequest     = AllocateZeroPool (SetBootOptionsRequestSize);
  if (SetBootOptionsRequest == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  SetBootOptionsRequest->ParameterValid.Bits.ParameterSelector    = IPMI_BOOT_OPTIONS_PARAMETER_BOOT_FLAGS;
  SetBootOptionsRequest->ParameterValid.Bits.MarkParameterInvalid = 0x0;

  //
  // Get current boot flag
  //
  ParameterData->Data2.Bits.CmosClear = IPMI_BOOT_FLAG_CLEAR_CMOS_NO;
  CopyMem (&SetBootOptionsRequest->ParameterData, ParameterData, sizeof (IPMI_BOOT_OPTIONS_RESPONSE_PARAMETER_5));

  Status = IpmiSetSystemBootOptionsExt (
             SetBootOptionsRequest,
             SetBootOptionsRequestSize,
             &CompletionCode
             );

Exit:
  if (GetBootOptionsResponse != NULL) {
    FreePool (GetBootOptionsResponse);
  }

  if (SetBootOptionsRequest != NULL) {
    FreePool (SetBootOptionsRequest);
  }

  return Status;
}
