/** @file
  Ipmi utilities to test interaction with BMC via ipmi command

  Copyright (c) 2022, Ampere Computing. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HiiLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiHiiServicesLib.h>
#include <Library/UefiLib.h>
#include <Protocol/IpmiProtocol.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/ShellLib.h>

#include "IpmiUtilHelper.h"

/**
  Convert an array to string and add seperator.

  @param[in] Array Pointer to array which will be converted
  @param[in] ArraySize The array size in bytes
  @param[in] Seperator The character separates the elements in array,
                       default is comma ','
  @param[out] ReturnArraySize They size in bytes of returned array
  @param[out] ReturnArray     Pointer to buffer which contains new string,
                              caller must prepare this buffer
  @retval EFI_SUCCESS         Successfully convert
  @retval EFI_INVALID_PARAMETER One or more parameters is invalid
**/
EFI_STATUS
EFIAPI
IpmiUtilNumberToHexJoin (
  IN     UINT8          Array[],
  IN     UINTN          ArraySize,
  IN  OPTIONAL   CHAR16 Seperator,
  OUT    UINTN          *ReturnArraySize,
  OUT    CHAR16         *ReturnArray
  )
{
  CHAR16  _Buffer[3];
  CHAR16  _BufferHexString[(2 * ArraySize)];
  UINTN   Index, Index2;

  if (  (Array == NULL) || (ArraySize == 0)
     || (ReturnArray == NULL))
  {
    DEBUG ((DEBUG_ERROR, "IpmiUtil: Invalid param - %a", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  if (Seperator == 0) {
    Seperator = L',';
  }

  //
  // Size is included Null terminator
  //
  *ReturnArraySize = ((2 * ArraySize) * 2) * 2;

  //
  // Number -> Hex string
  //
  ZeroMem ((VOID *)_BufferHexString, sizeof (_BufferHexString));
  for (Index = 0; Index < ArraySize; ++Index) {
    UnicodeSPrint (_Buffer, sizeof (_Buffer), L"%02X", Array[Index]);
    CopyMem (
      (VOID *)&_BufferHexString[Index * 2],
      _Buffer,
      sizeof (CHAR16) * 2
      );
  }

  ZeroMem ((VOID *)ReturnArray, *ReturnArraySize);
  for (Index = 0, Index2 = 0;
       Index < sizeof (_BufferHexString) / sizeof (CHAR16);
       Index += 2, Index2 += 3)
  {
    ReturnArray[Index2]     = _BufferHexString[Index];
    ReturnArray[Index2 + 1] = _BufferHexString[Index + 1];
    if (!(Index + 2 < sizeof (_BufferHexString) / sizeof (CHAR16))) {
      ReturnArray[Index2 + 2] = L'\0';
    } else {
      ReturnArray[Index2 + 2] = Seperator;
    }
  }

  return EFI_SUCCESS;
}

/**
  Convert string to a number.
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
  )
{
  EFI_STATUS  Status;
  UINTN       Value;

  Status = StrHexToUintnS (Strings, NULL, &Value);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (Value > 255) {
    return EFI_UNSUPPORTED;
  }

  *Number = (UINT8)Value;

  return EFI_SUCCESS;
}

EFI_STATUS
IpmiUtilInitShell (
  VOID
  )
{
  EFI_STATUS  Status;

  Status = ShellInitialize ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Cannot prepare shell lib - %r\r\n", __FUNCTION__, Status));
    return EFI_ABORTED;
  }

  return Status;
}

/**
  Check the command line arguments and return arguments list.

  Caller must free this list after using.

  @param[in] HiiPackagehandle The Hii Handle of Caller

  @retval LIST_ENTRY The pointer to head of argument list.
  @retval NULL       There are some error.
**/
LIST_ENTRY
CONST *
IpmiUtilInitializeArgumentList (
  IN EFI_HII_HANDLE  HiiPackageHandle
  )
{
  EFI_STATUS  Status;
  LIST_ENTRY  *ParamList;
  CHAR16      *ProblemParam;

  SHELL_PARAM_ITEM  IpmiParamItemsType[] = {
    { L"-r", TypeMaxValue },
    { L"-v", TypeFlag     },
    { NULL,  TypeMax      }
  };

  Status = IpmiUtilInitShell ();
  if (EFI_ERROR (Status)) {
    ParamList = NULL;
    goto Done;
  }

  Status = ShellCommandLineParse (
             IpmiParamItemsType,
             &ParamList,
             &ProblemParam,
             TRUE
             );
  if (EFI_ERROR (Status)) {
    if ((Status == EFI_VOLUME_CORRUPTED) && (ProblemParam != NULL)) {
      ShellPrintHiiEx (
        -1,                 // Use current col
        -1,                 // Use current row
        NULL,               // English
        STRING_TOKEN (STR_GEN_PROBLEM),
        HiiPackageHandle,
        L"ipmiutil",
        ProblemParam
        );
      if (ProblemParam != NULL) {
        FreePool ((VOID *)ProblemParam);
      }

      goto Done;
    } else {
      if (ProblemParam != NULL) {
        FreePool ((VOID *)ProblemParam);
      }

      DEBUG ((DEBUG_ERROR, "Arguments passed are dirty !!!\r\n"));
      goto Done;
    }
  }

  //
  // Check parameters is valid
  //
  Status = ShellCommandLineCheckDuplicate (
             ParamList,
             &ProblemParam
             );
  if (EFI_ERROR (Status)) {
    ShellPrintHiiEx (
      -1,
      -1,
      NULL,
      STRING_TOKEN (STR_GEN_PARAM_DUPLICATE),
      HiiPackageHandle,
      L"ipmiutil",
      ProblemParam
      );
    if (ProblemParam != NULL) {
      FreePool ((VOID *)ProblemParam);
    }

    ParamList = NULL;

    goto Done;
  }

Done:
  return ParamList;
}

/**
  Destroy arguments list.

  @param[in] ArgList The pointer to head of argument list

  @retval EFI_SUCCESS Successfully free the list
**/
EFI_STATUS
IpmiUtilDestroyArgumentList (
  IN LIST_ENTRY  *ArgList
  )
{
  if (ArgList != NULL) {
    FreePool ((VOID *)ArgList);
  }

  return EFI_SUCCESS;
}

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
  )
{
  switch (*ResponseData) {
    case 0x00:
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (IPMI_COMP_CODE_COMMAND_SUCCESS_STRING), HiiPackageHandle, *ResponseData);
      break;
    case 0xC0:
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (IPMI_COMP_CODE_NODE_BUSY_STRING), HiiPackageHandle, *ResponseData);
      break;
    case 0xC1:
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (IPMI_COMP_CODE_INVALID_COMMAND_STRING), HiiPackageHandle, *ResponseData);
      break;

    case 0xC2:
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (IPMI_COMP_CODE_COMMAND_INVALID_FOR_LUN_STRING), HiiPackageHandle, *ResponseData);
      break;

    case 0xC3:
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (IPMI_COMP_CODE_COMMAND_TIMEOUT_STRING), HiiPackageHandle, *ResponseData);
      break;

    case 0xC4:
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (IPMI_COMP_CODE_OUT_OF_SPACE_STRING), HiiPackageHandle, *ResponseData);
      break;

    case 0xC5:
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (IPMI_COMP_CODE_RESERVATION_CANCELLED_STRING), HiiPackageHandle, *ResponseData);
      break;

    case 0xC6:
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (IPMI_COMP_CODE_REQUEST_DATA_TRUNCATED_STRING), HiiPackageHandle, *ResponseData);
      break;

    case 0xC7:
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (IPMI_COMP_CODE_REQUEST_DATA_LENGTH_INVALID_STRING), HiiPackageHandle, *ResponseData);
      break;

    case 0xC8:
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (IPMI_COMP_CODE_REQUEST_DATA_LENGTH_LIMIT_EXCEEDED_STRING), HiiPackageHandle, *ResponseData);
      break;

    case 0xC9:
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (IPMI_COMP_CODE_PARAMETER_OUT_OF_RANGE_STRING), HiiPackageHandle, *ResponseData);
      break;

    case 0xCA:
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (IPMI_COMP_CODE_CANNOT_RETURN_REQUESTED_NUMBER_OF_BYTES_STRING), HiiPackageHandle, *ResponseData);
      break;

    case 0xCB:
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (IPMI_COMP_CODE_REQUESTED_SENSOR_DATA_OR_RECORD_NOT_PRESENT_STRING), HiiPackageHandle, *ResponseData);
      break;

    case 0xCC:
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (IPMI_COMP_CODE_INVALID_DATA_FIELD_IN_REQUEST_STRING), HiiPackageHandle, *ResponseData);
      break;

    case 0xCD:
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (IPMI_COMP_CODE_COMMAND_ILLEGAL_FOR_SENSOR_OR_RECORD_TYPE_STRING), HiiPackageHandle, *ResponseData);
      break;
    case 0xCE:
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (IPMI_COMP_CODE_COMMAND_RESPONSE_COULD_NOT_BE_PROVIDED_STRING), HiiPackageHandle, *ResponseData);
      break;
    case 0xCF:
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (IPMI_COMP_CODE_CANNOT_EXECUTE_DUPLICATE_REQUEST_STRING), HiiPackageHandle, *ResponseData);
      break;

    case 0xD0:
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (IPMI_COMP_CODE_SDR_REPOSITORY_IN_UPDATE_MODE_STRING), HiiPackageHandle, *ResponseData);
      break;

    case 0xD1:
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (IPMI_COMP_CODE_DEVICE_IN_FIRMWARE_UPDATE_MODE_STRING), HiiPackageHandle, *ResponseData);
      break;
    case 0xD2:
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (IPMI_COMP_CODE_BMC_INITIALIZATION_IN_PROGRESS_STRING), HiiPackageHandle, *ResponseData);
      break;

    case 0xD3:
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (IPMI_COMP_CODE_DESTINATION_UNAVAILABLE_STRING), HiiPackageHandle, *ResponseData);
      break;

    case 0xD4:
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (IPMI_COMP_CODE_INSUFFICIENT_PRIVILEGE_LEVEL_STRING), HiiPackageHandle, *ResponseData);
      break;
    case 0xD5:
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (IPMI_COMP_CODE_REQUEST_PARAMETER_NOT_SUPPORTED_STRING), HiiPackageHandle, *ResponseData);
      break;

    case 0xD6:
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (IPMI_COMP_CODE_REQUEST_PARAMETER_ILLEGAL_STRING), HiiPackageHandle, *ResponseData);
      break;

    case 0xFF:
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (IPMI_COMP_CODE_UNSPECIFIED_ERROR_STRING), HiiPackageHandle, *ResponseData);
      break;

    default:
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (IPMI_COMP_CODE_OEM_CMD_SPECIFIC_STRING), HiiPackageHandle, *ResponseData);
      break;
  }

  return;
}

/**
  Hii Package List has been stored on PE Image in build time.
  Retrieve HII package list from ImageHandle and publish to HII database.

  @param[in] ImageHandle  The image handle of the process.

  @retval EFI_HII_HANDLE  The handle of the HII package.
**/
EFI_HII_HANDLE
EFIAPI
InitializeHiiPackage (
  EFI_HANDLE ImageHandle
  )
{
  EFI_STATUS                   Status;
  EFI_HII_PACKAGE_LIST_HEADER  *PackageList;
  EFI_HII_HANDLE               HiiHandle;

  if (ImageHandle == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Caller Handle is invalid\n", __FUNCTION__));
    return NULL;
  }

  //
  // Retrieve HII package list from ImageHandle
  //
  Status = gBS->OpenProtocol (
                  ImageHandle,
                  &gEfiHiiPackageListProtocolGuid,
                  (VOID **)&PackageList,
                  ImageHandle,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Uefi Driver opening protocol failed\r\n", __FUNCTION__));
    return NULL;
  }

  //
  // Publish HII package list to HII Database.
  //
  Status = gHiiDatabase->NewPackageList (
                           gHiiDatabase,
                           PackageList,
                           NULL,
                           &HiiHandle
                           );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Uefi Driver fail to register HII packages\r\n", __FUNCTION__));
    return NULL;
  }

  return HiiHandle;
}

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
  )
{
  return ShellPrintHiiEx (
           -1,
           -1,
           NULL,
           STRING_TOKEN (STR_GEN_CMD_INVALID),
           HiiPackageHandle
           );
}
