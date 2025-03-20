/** @file

  Copyright (c) 2024, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>

#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/MemoryAttribute.h>

/**
  Remove gEfiMemoryAttributeProtocolGuid.

  @param  ImageHandle   ImageHandle of the loaded driver.
  @param  SystemTable   Pointer to the EFI System Table.

  @retval EFI_SUCCESS   Always return successfully.
**/
EFI_STATUS
EFIAPI
RemoveMemoryAttributeProtocolLibConstructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                     Status;
  EFI_MEMORY_ATTRIBUTE_PROTOCOL  *MemoryAttribute;
  UINTN                          NumHandles;
  EFI_HANDLE                     *Handle;

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiMemoryAttributeProtocolGuid,
                  NULL,
                  &NumHandles,
                  &Handle
                  );
  if (EFI_ERROR (Status) || (NumHandles == 0) || (Handle == NULL)) {
    DEBUG ((DEBUG_ERROR, "ERROR: Could not find gEfiMemoryAttributeProtocolGuid\n"));
    return EFI_SUCCESS;
  }

  // It should have only one EFI_MEMORY_ATTRIBUTE_PROTOCOL installed
  if (NumHandles > 1) {
    DEBUG ((DEBUG_ERROR, "ERROR: Found multiple gEfiMemoryAttributeProtocolGuid\n"));
    return EFI_SUCCESS;
  }

  Status = gBS->HandleProtocol (
                  Handle[0],
                  &gEfiMemoryAttributeProtocolGuid,
                  (VOID **)&MemoryAttribute
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ERROR: Could not handle gEfiMemoryAttributeProtocolGuid\n"));
    return EFI_SUCCESS;
  }

  Status = gBS->UninstallProtocolInterface (
                  Handle[0],
                  &gEfiMemoryAttributeProtocolGuid,
                  MemoryAttribute
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ERROR: Could not uninstall gEfiMemoryAttributeProtocolGuid\n"));
    return EFI_SUCCESS;
  }

  return EFI_SUCCESS;
}
