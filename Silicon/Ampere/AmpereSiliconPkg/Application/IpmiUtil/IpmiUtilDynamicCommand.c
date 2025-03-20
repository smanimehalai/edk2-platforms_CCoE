/** @file
  Ipmi utilities to test interaction with BMC via ipmi command

  Copyright (c) 2022, Ampere Computing. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/HiiLib.h>
#include <Library/ShellLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Protocol/HiiPackageList.h>
#include <Protocol/Shell.h>
#include <Protocol/ShellDynamicCommand.h>
#include <Protocol/ShellParameters.h>

#include "IpmiRawCommand.h"
#include "IpmiUtilHelper.h"

SUB_COMMAND_HANDLER  mSubCmdHandler[] = {
  IpmiUtilRawCommandHandler,
  IpmiUtilHelpHandler,
  NULL
};

EFI_HII_HANDLE  mIpmiUtilDynamicCommandHii = NULL;

/**
  This is the shell command handler function pointer callback type.  This
  function handles the command when it is invoked in the shell.

  @param[in] This                   The instance of the EFI_SHELL_DYNAMIC_COMMAND_PROTOCOL.
  @param[in] SystemTable            The pointer to the system table.
  @param[in] ShellParameters        The parameters associated with the command.
  @param[in] Shell                  The instance of the shell protocol used in the context
                                    of processing this command.

  @return EFI_SUCCESS               the operation was successful
  @return other                     the operation failed.
**/
SHELL_STATUS
IpmiUtilCommandHandler (
  IN EFI_SHELL_DYNAMIC_COMMAND_PROTOCOL *This,
  IN EFI_SYSTEM_TABLE                   *SystemTable,
  IN EFI_SHELL_PARAMETERS_PROTOCOL      *ShellParameters,
  IN EFI_SHELL_PROTOCOL                 *Shell
  )
{
  EFI_STATUS  Status;
  UINTN       Index;
  LIST_ENTRY  *ArgumentList;

  gEfiShellParametersProtocol = ShellParameters;
  gEfiShellProtocol = Shell;

  if (mIpmiUtilDynamicCommandHii == NULL) {
    return SHELL_ABORTED;
  }

  ArgumentList = (LIST_ENTRY *)IpmiUtilInitializeArgumentList (mIpmiUtilDynamicCommandHii);
  if (ArgumentList == NULL) {
    Status = SHELL_ABORTED;
    goto Cleanup;
  }

  for (Index = 0; mSubCmdHandler[Index] != NULL; ++Index) {
    Status = mSubCmdHandler[Index](ArgumentList, mIpmiUtilDynamicCommandHii);
    if ((Status == EFI_SUCCESS) || (Status == EFI_ABORTED)) {
      break;
    }
  }

Cleanup:
  IpmiUtilDestroyArgumentList (ArgumentList);

  return Status;
}

/**
  This is the command help handler function pointer callback type.  This
  function is responsible for displaying help information for the associated
  command.

  @param[in] This                   The instance of the EFI_SHELL_DYNAMIC_COMMAND_PROTOCOL.
  @param[in] Language               The pointer to the language string to use.

  @return string                    Pool allocated help string, must be freed by caller
**/
CHAR16 *
IpmiUtilCommandGetHelp (
  IN EFI_SHELL_DYNAMIC_COMMAND_PROTOCOL *This,
  IN CONST CHAR8                        *Language
  )
{
  return HiiGetString (mIpmiUtilDynamicCommandHii, STRING_TOKEN (STR_GET_HELP_IPMI_UTIL), Language);
}

EFI_SHELL_DYNAMIC_COMMAND_PROTOCOL  mIpmiUtilDynamicCommand = {
  L"ipmiutil",
  IpmiUtilCommandHandler,
  IpmiUtilCommandGetHelp
};

/**
  Entry point of IpmiUtil Dynamic Command.

  Produce the DynamicCommand protocol to handle "tftp" command.

  @param ImageHandle            The image handle of the process.
  @param SystemTable            The EFI System Table pointer.

  @retval EFI_SUCCESS           Tftp command is executed successfully.
  @retval EFI_ABORTED           HII package was failed to initialize.
  @retval others                Other errors when executing tftp command.
**/
EFI_STATUS
EFIAPI
IpmiUtilDynamicCommandEntryPoint (
  IN EFI_HANDLE       ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{
  EFI_STATUS  Status;

  //
  // Add Hii Package List to Database
  // Must be remove when this driver is unloaded
  //
  mIpmiUtilDynamicCommandHii = InitializeHiiPackage (ImageHandle);
  if (mIpmiUtilDynamicCommandHii == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Fail to initialize Hii package list for ipmiutil command\n", __FUNCTION__));
    return EFI_LOAD_ERROR;
  }

  Status = gBS->InstallProtocolInterface (
                  &ImageHandle,
                  &gEfiShellDynamicCommandProtocolGuid,
                  EFI_NATIVE_INTERFACE,
                  &mIpmiUtilDynamicCommand
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Cannot install IpmiUtil command - ipmiutil will be not available in shell\n"));
  }

  return Status;
}

/**
  IpmiUtil command driver unload handler.

  @param ImageHandle            The image handle of the process.

  @retval EFI_SUCCESS           The image is unloaded.
  @retval Others                Failed to unload the image.
**/
EFI_STATUS
EFIAPI
IpmiUtilDynamicCommandUnload (
  IN EFI_HANDLE ImageHandle
  )
{
  EFI_STATUS  Status;

  Status = gBS->UninstallProtocolInterface (
                  ImageHandle,
                  &gEfiShellDynamicCommandProtocolGuid,
                  &mIpmiUtilDynamicCommand
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (mIpmiUtilDynamicCommandHii != NULL) {
    HiiRemovePackages (mIpmiUtilDynamicCommandHii);
  }

  return EFI_SUCCESS;
}
