/** @file

  Copyright (c) 2020 - 2022, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiPei.h>
#include <Uefi.h>

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/FlashLib.h>
#include <Library/IpmiCommandLibExt.h>
#include <Library/NVParamLib.h>
#include <Library/PcdLib.h>
#include <Library/PeimEntryPoint.h>
#include <Library/ResetSystemLib.h>
#include <Library/AmpereCpuLib.h>

#define UUID_SIZE     PcdGetSize (PcdPlatformConfigUuid)

/**
  Entry point function for the PEIM

  @param FileHandle      Handle of the file being invoked.
  @param PeiServices     Describes the list of possible PEI Services.

  @return EFI_SUCCESS    If we installed our PPI

**/
EFI_STATUS
EFIAPI
FlashPeiEntryPoint (
  IN       EFI_PEI_FILE_HANDLE FileHandle,
  IN CONST EFI_PEI_SERVICES    **PeiServices
  )
{
  CHAR8                 BuildUuid[UUID_SIZE + sizeof(BOOLEAN)];
  CHAR8                 StoredUuid[sizeof (BuildUuid)];
  EFI_STATUS            Status;
  IPMI_BOOT_FLAGS_INFO  BootFlags;
  BOOLEAN               NeedToClearUserConfiguration;
  UINT32                FWNvRamSize;
  UINT32                NvRamSize;
  UINT32                UefiMiscSize;
  UINTN                 FWNvRamStartOffset;
  UINTN                 NvRamAddress;
  UINTN                 UefiMiscOffset;
  BOOLEAN               IsAc01;
  
  NeedToClearUserConfiguration = FALSE;

  IsAc01 = IsAc01Processor ();
  CopyMem ((VOID *)BuildUuid, PcdGetPtr (PcdPlatformConfigUuid), sizeof (BuildUuid));
  BuildUuid[sizeof (BuildUuid)-sizeof(IsAc01)]=IsAc01;

  NvRamAddress = PcdGet64 (PcdFlashNvStorageVariableBase64);
  NvRamSize = FixedPcdGet32 (PcdFlashNvStorageVariableSize) +
              FixedPcdGet32 (PcdFlashNvStorageFtwWorkingSize) +
              FixedPcdGet32 (PcdFlashNvStorageFtwSpareSize);

  DEBUG ((
    DEBUG_INFO,
    "%a: Using NV store FV in-memory copy at 0x%lx with size 0x%x\n",
    __FUNCTION__,
    NvRamAddress,
    NvRamSize
    ));

  Status = FlashGetNvRamInfo (&FWNvRamStartOffset, &FWNvRamSize, &UefiMiscOffset, &UefiMiscSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get Flash NVRAM info %r\n", __FUNCTION__, Status));
    return Status;
  }

  if (FWNvRamSize < (NvRamSize * 2)
      || UefiMiscSize < sizeof (BuildUuid)) {
    //
    // NVRAM size provided by FW is not enough
    //
    return EFI_INVALID_PARAMETER;
  }

  //
  // Get Boot Flags from BMC to determine if an NVRAM clear request has been made or not.
  //
  Status = IpmiGetBootFlags (&BootFlags);
  if (!EFI_ERROR (Status)) {
    if (BootFlags.IsCmosClear) {
      DEBUG ((DEBUG_INFO, "FlashPei: Clear-cmos option is selected\n"));
      NeedToClearUserConfiguration = TRUE;
    }

    Status = IpmiClearCmosBootFlags ();
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "FlashPei: Failed to clear clear-cmos boot flags - %r\n", Status));
      NeedToClearUserConfiguration = FALSE;
    }
  } else {
    DEBUG ((DEBUG_ERROR, "FlashPei: Failed to get Boot Flags via IPMI - %r\n", Status));
  }

  //
  // Get the Platform UUID stored in the very first bytes of the UEFI Misc.
  //
  Status = FlashReadCommand (
             UefiMiscOffset,
             (UINT8 *)StoredUuid,
             sizeof (StoredUuid)
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  DEBUG ((DEBUG_INFO, "StoredUuid = "));
  for (int i = 0; i < (sizeof (StoredUuid)); i++) {
    DEBUG ((DEBUG_INFO, "%x ", StoredUuid[i]));
  }
  DEBUG ((DEBUG_INFO, "\n"));
  DEBUG ((DEBUG_INFO, "stored IsAc01 = %x\n", StoredUuid[sizeof(StoredUuid)-sizeof(IsAc01)]));
  DEBUG ((DEBUG_INFO, "IsAc01 = %x\n", IsAc01));
  if (CompareMem ((VOID *)StoredUuid, (VOID *)BuildUuid, sizeof (BuildUuid)) != 0) {
    DEBUG ((DEBUG_INFO, "BUILD UUID Changed, Update Storage with NVRAM FV\n"));
    NeedToClearUserConfiguration = TRUE;
  }

  if (NeedToClearUserConfiguration) {

    Status = FlashEraseCommand (FWNvRamStartOffset, NvRamSize * 2 + sizeof (BuildUuid));
    if (EFI_ERROR (Status)) {
      return Status;
    }

    Status = FlashWriteCommand (
               FWNvRamStartOffset,
               (UINT8 *)NvRamAddress,
               NvRamSize
               );
    if (EFI_ERROR (Status)) {
      return Status;
    }

    //
    // Write new BUILD UUID to the Flash
    //
    Status = FlashEraseCommand (UefiMiscOffset, sizeof (BuildUuid));
    if (EFI_ERROR (Status)) {
      return Status;
    }

    Status = FlashWriteCommand (
               UefiMiscOffset,
               (UINT8 *)BuildUuid,
               sizeof (BuildUuid)
               );
    if (EFI_ERROR (Status)) {
      return Status;
    }

    Status = NVParamClrAll ();
    if (!EFI_ERROR (Status)) {
      //
      // Trigger reset to use default NVPARAM
      //
      ResetCold ();
    }
  } else {
    DEBUG ((DEBUG_INFO, "Identical UUID, copy stored NVRAM to RAM\n"));

    Status = FlashReadCommand (
               FWNvRamStartOffset,
               (UINT8 *)NvRamAddress,
               NvRamSize
               );
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  return EFI_SUCCESS;
}
