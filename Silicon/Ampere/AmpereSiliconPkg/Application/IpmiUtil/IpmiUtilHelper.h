/** @file
  Ipmi utilities to test interaction with BMC via ipmi command

  Copyright (c) 2022, Ampere Computing. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef IPMI_COMMAND_H_
#define IPMI_COMMAND_H_

/**
 Handler for Ipmi commands

 @param ArgList          List entry of arguments.
 @param HiiPackageHandle Hii Handle which is registered by App or Driver.

 @retval EFI_SUCCESS Command successfully executed
 @retval EFI_INVALID_PARAMETER One or more parameters are invalid
 @retval EFI_ABORTED Command failed, should not continue execution
 **/
typedef
EFI_STATUS
( *SUB_COMMAND_HANDLER ) (
  IN LIST_ENTRY CONST *ArgList,
  IN EFI_HII_HANDLE   HiiPackageHandle
  );

/**
  Convert an array to string and add seperator.

  @param[in] Array Pointer to array which will be converted
  @param[in] ArraySize The array size in bytes
  @param[in] Seperator The character seperates the elements in array,
                       default is comm ','
  @param[out] ReturnArraySize They size in bytes of returned array
  @param[out] ReturnArray     Pointer to buffer which contains new string,
                              caller must prepare this buffer
  @retval EFI_SUCCESS         Successfully convert
  @retval EFI_INVALID_PARAMETER One or more paramters is invalid
**/
EFI_STATUS
EFIAPI
IpmiUtilNumberToHexJoin (
  IN     UINT8  Array[],
  IN     UINTN  ArraySize,
  IN     CHAR16 Seperator,
  OUT    UINTN  *ReturnArraySize,
  OUT    CHAR16 *ReturnArray
  );

/**
  Convert 1byte hex string to a number.
  The seperator is Null terminator or space character.
  Example:
     0xA -> 10.
     A   -> 10.
     a   -> 10.

  @param HexString The hex string to convert
  @param Number    The return value of the conversion

  @retval EFI_SUCCESS           The conversion was successful
  @retval EFI_INVALID_PARAMETER Some parameters are invalid
**/
EFI_STATUS
EFIAPI
IpmiUtilUniStrToNumber (
  IN CHAR16 *Strings,
  OUT UINT8 *Number
  );

/**
  Read completion code and check what it means.

  @param[in] HiiPackagehandle The Hii Handle of Caller
  @param[in] ResponseData     The completion code from ipmi response data

  @retval VOID
**/
VOID
EFIAPI
IpmiUtilErrorCatching (
  IN EFI_HII_HANDLE HiiPackageHandle,
  IN UINT8          *ResponseData
  );

/**
  Check the command line arguments and return arguments list.

  Caller must free this list after using.

  @param[in] HiiPackagehandle The Hii Handle of Caller

  @retval LIST_ENTRY The pointer to head of argument list
  @retval NULL       There are some error
**/
LIST_ENTRY
CONST *
EFIAPI
IpmiUtilInitializeArgumentList (
  IN EFI_HII_HANDLE HiiPackageHandle
  );

/**
  Destroy arguments list.

  @param[in] ArgList The pointer to head of argument list

  @retval EFI_SUCCESS Successfully free the list
**/
EFI_STATUS
EFIAPI
IpmiUtilDestroyArgumentList (
  IN LIST_ENTRY *ArgList
  );

/**
  Destroy arguments list.

  @param[in] ArgList The pointer to head of argument list

  @retval EFI_SUCCESS Successfully free the list
**/
EFI_HII_HANDLE
EFIAPI
InitializeHiiPackage (
  EFI_HANDLE ImageHandle
  );

/**
  If there are no commands handler successfully executed,
  show error message.

  @param ArgList          List entry of arguments.
  @param HiiPackageHandle Hii Handle which is registered by App or Driver.

  @retval EFI_SUCCESS     Successfully show error message.
  @retval Others          ShellPrintHiiEx failed.
**/
EFI_STATUS
EFIAPI
IpmiUtilHelpHandler (
  IN LIST_ENTRY CONST *ArgList,
  IN EFI_HII_HANDLE   HiiPackageHandle
  );

#endif /* IPMI_COMMAND_H_ */
