/** @file
  Ipmi utilities to test interaction with BMC via ipmi command

  Copyright (c) 2022, Ampere Computing. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef IPMI_RAW_COMMAND_H_
#define IPMI_RAW_COMMAND_H_

/**
  Wrapper Ipmi protocol
**/
EFI_STATUS
EFIAPI
IpmiUtilSummitCommand (
  IN     UINT8  NetFunction,
  IN     UINT8  Command,
  IN     UINT8  *RequestData,
  IN     UINT32 RequestDataSize,
  OUT    UINT8  *ResponseData,
  IN OUT UINT32 *ResponseDataSize
  );

/**
 Handler for Ipmi command with raw option.

 @param ArgList          List entry of arguments.
 @param HiiPackageHandle Hii Handle which is registered by App or Driver.

 @retval EFI_SUCCESS Command successfully executed
 @retval EFI_INVALID_PARAMETER One or more parameters are invalid
 @retval EFI_ABORTED Command failed
 @retval Other       Error unexpected
**/
EFI_STATUS
EFIAPI
IpmiUtilRawCommandHandler (
  IN LIST_ENTRY CONST *ArgList,
  IN EFI_HII_HANDLE   HiiPackageHandle
  );

#endif /* IPMI_RAW_COMMAND_H_ */
