/** @file
  Ipmi utilities to test interaction with BMC via ipmi command

  Copyright (c) 2022, Ampere Computing. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>

#include <Library/HiiLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Protocol/ShellParameters.h>

#include "IpmiRawCommand.h"
#include "IpmiUtilHelper.h"

GLOBAL_REMOVE_IF_UNREFERENCED EFI_STRING_ID  mStringHelpTokenId = STRING_TOKEN (STR_GET_HELP_IPMI_UTIL);

SUB_COMMAND_HANDLER  mAppSubCmdHandler[] = {
  IpmiUtilRawCommandHandler,
  IpmiUtilHelpHandler,
  NULL
};
EFI_HII_HANDLE       mIpmiUtilAppHii = NULL;

/**
  The user Entry Point for Application. The user code starts with this function
  as the real entry point for the application.

  @param[in] ImageHandle    The firmware allocated handle for the EFI image.
  @param[in] SystemTable    A pointer to the EFI System Table.

  @retval EFI_SUCCESS       The entry point is executed successfully.
  @retval other             Some error occurs when executing this entry point.

**/
EFI_STATUS
EFIAPI
IpmiUtilApplicationEntryPoint (
  IN EFI_HANDLE       ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{
  EFI_STATUS  Status;
  UINTN       Index;
  LIST_ENTRY  *ArgumentList = NULL;

  mIpmiUtilAppHii = InitializeHiiPackage (ImageHandle);
  if (mIpmiUtilAppHii == NULL) {
    Status = EFI_ABORTED;
    goto Cleanup;
  }

  ArgumentList = (LIST_ENTRY *)IpmiUtilInitializeArgumentList (mIpmiUtilAppHii);

  if (ArgumentList == NULL) {
    Status = EFI_ABORTED;
    goto Cleanup;
  }

  for (Index = 0; mAppSubCmdHandler[Index] != NULL; ++Index) {
    Status = mAppSubCmdHandler[Index](ArgumentList, mIpmiUtilAppHii);
    if ((Status == EFI_SUCCESS) || (Status == EFI_ABORTED)) {
      break;
    }
  }

Cleanup:
  IpmiUtilDestroyArgumentList (ArgumentList);

  if (mIpmiUtilAppHii != NULL) {
    HiiRemovePackages (mIpmiUtilAppHii);
  }

  return Status;
}
