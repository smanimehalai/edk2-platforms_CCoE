/** @file

  Copyright (c) 2022, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>

#include <Guid/SmbiosBlobsTransfer.h>
#include <IndustryStandard/SmBios.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/IpmiBlobsTransferLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/TimerLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

#define SMBIOS_COMMIT_POLL_INTERVAL    (200 * 1000) // 200ms
#define RETRY_COUNTER                  10
#define SMBIOS_BLOBS_ID                "/smbios"
#define SMBIOS_BLOB_TRANSFER_VAR_NAME  L"SmbiosBlobTransfer"

UINT8  OpenBmcIanaNumber[IANA_OEM_NUMBER_SIZE] = { 0xcf, 0xc2, 0x00 };

STATIC UINT8  *mIanaNumber[] = {
  OpenBmcIanaNumber,
  NULL
};

EFI_STATUS
GetSmbiosTable (
  OUT UINT8  **TableEntry,
  OUT UINTN  *TableEntrySize,
  OUT UINT8  **DataAddress,
  OUT UINTN  *DataSize
  )
{
  EFI_STATUS                    Status;
  SMBIOS_TABLE_3_0_ENTRY_POINT  *SmbiosTable30EntryPoint;
  SMBIOS_TABLE_ENTRY_POINT      *SmbiosTableEntryPoint;

  Status = EfiGetSystemConfigurationTable (&gEfiSmbios3TableGuid, (VOID **)&SmbiosTable30EntryPoint);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "SMBIOS Transfer: SMBIOS 3.x Table - %r\nTry with SMBIOS 2.x Table\n", Status));
    Status = EfiGetSystemConfigurationTable (&gEfiSmbiosTableGuid, (VOID **)&SmbiosTableEntryPoint);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "SMBIOS Transfer: SMBIOS 2.x Table - %r\n", Status));
      return Status;
    }

    *TableEntry     = (UINT8 *)SmbiosTableEntryPoint;
    *TableEntrySize = sizeof (SMBIOS_TABLE_ENTRY_POINT);
    *DataAddress    = (UINT8 *)((UINTN)SmbiosTableEntryPoint->TableAddress);
    *DataSize       = SmbiosTableEntryPoint->TableLength;
  } else {
    *TableEntry     = (UINT8 *)SmbiosTable30EntryPoint;
    *TableEntrySize = sizeof (SMBIOS_TABLE_3_0_ENTRY_POINT);
    *DataAddress    = (UINT8 *)SmbiosTable30EntryPoint->TableAddress;
    *DataSize       = SmbiosTable30EntryPoint->TableMaximumSize;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
SmbiosBlobsDiscover (
  OUT UINT8  *IanaNumber
  )
{
  CHAR8       BlobId[IPMI_BLOBS_TRANSFER_MAX_PACKET_SIZE];
  CHAR8       SmbiosBlobId[] = SMBIOS_BLOBS_ID;
  EFI_STATUS  Status;
  UINT32      BlobIdSize;
  UINT32      NumberOfEnumerableBlobs;
  UINTN       IanaIdx;
  UINTN       Index;

  BlobIdSize = sizeof (BlobId);

  for (IanaIdx = 0; mIanaNumber[IanaIdx] != NULL; IanaIdx++) {
    Status = IpmiBlobGetCount (mIanaNumber[IanaIdx], &NumberOfEnumerableBlobs);
    if (EFI_ERROR (Status)) {
      return EFI_NOT_FOUND;
    }

    for (Index = 0; Index < NumberOfEnumerableBlobs; Index++) {
      Status = IpmiBlobEnumerate (mIanaNumber[IanaIdx], Index, BlobId, &BlobIdSize);
      if (EFI_ERROR (Status)) {
        continue;
      }

      if (  (BlobIdSize == sizeof (SmbiosBlobId))
         && (CompareMem (SmbiosBlobId, BlobId, BlobIdSize) == 0))
      {
        CopyMem (IanaNumber, mIanaNumber[IanaIdx], IANA_OEM_NUMBER_SIZE);
        return EFI_SUCCESS;
      }
    }
  }

  return EFI_NOT_FOUND;
}

EFI_STATUS
CommitSmbiosData (
  IN UINT8   *IanaNumber,
  IN UINT16  SessionId
  )
{
  UINTN   RetryCount;
  UINT16  BlobStatistics;

  //
  // Notify the BMC that no more data is transferred
  //
  IpmiBlobCommit (IanaNumber, SessionId, NULL, 0);

  //
  // Check completion
  //
  for (RetryCount = 0, BlobStatistics = 0;
       RetryCount < RETRY_COUNTER;
       RetryCount++, BlobStatistics = 0)
  {
    IpmiBlobGetStatistics (
      IanaNumber,
      (VOID *)SMBIOS_BLOBS_ID,
      sizeof (SMBIOS_BLOBS_ID),
      &BlobStatistics,
      NULL,
      NULL,
      NULL
      );

    MicroSecondDelay (SMBIOS_COMMIT_POLL_INTERVAL);

    if ((BlobStatistics & IPMI_BLOB_STATE_COMMITTING) != 0) {
      continue;
    }

    if ((BlobStatistics & IPMI_BLOB_STATE_COMMITED) != 0) {
      return EFI_SUCCESS;
    }

    if ((BlobStatistics & IPMI_BLOB_STATE_COMMIT_ERROR) != 0) {
      return EFI_UNSUPPORTED;
    }
  }

  return EFI_UNSUPPORTED;
}

EFI_STATUS
WriteSmbiosDataToBmc (
  IN UINT8   *IanaNumber,
  IN UINT16  SessionId
  )
{
  EFI_STATUS  Status;
  UINT32      Offset;
  UINT32      PayloadSize;
  UINT8       *SmbiosTableAddress;
  UINT8       *SmbiosTableEntry;
  UINTN       Index;
  UINTN       MaxPayloadDataSize;
  UINTN       NumberOfPayload;
  UINTN       SmbiosTableEntrySize;
  UINTN       SmbiosTableSize;
  VOID        *Payload;

  //
  // Get SMBIOS table from system configuration table
  //
  Status = GetSmbiosTable (
             &SmbiosTableEntry,
             &SmbiosTableEntrySize,
             &SmbiosTableAddress,
             &SmbiosTableSize
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Fragment data
  //
  MaxPayloadDataSize = IpmiBlobGetMaxRequestPacketSize ()
                       - sizeof (IPMI_BLOB_TRANSFER_TX_HEADER)
                       - IPMI_BLOB_WRITE_TX_HEADER_SIZE;

  NumberOfPayload = SmbiosTableSize / MaxPayloadDataSize;
  if ((SmbiosTableSize % MaxPayloadDataSize) != 0) {
    NumberOfPayload++;
  }

  for (Index = 0, Offset = 0; Index < NumberOfPayload; Index++) {
    Payload = (UINT8 *)SmbiosTableAddress + (MaxPayloadDataSize * Index);

    if (MaxPayloadDataSize * (Index + 1) > SmbiosTableSize) {
      PayloadSize = SmbiosTableSize - (MaxPayloadDataSize * Index);
    } else {
      PayloadSize = MaxPayloadDataSize;
    }

    Status = IpmiBlobWrite (
               IanaNumber,
               SessionId,
               (UINT8 *)Payload,
               PayloadSize,
               Offset
               );
    if (EFI_ERROR (Status)) {
      return Status;
    }

    Offset += PayloadSize;
  }

  //
  // Send SMBIOS entry
  //
  Status = IpmiBlobWrite (
             IanaNumber,
             SessionId,
             SmbiosTableEntry,
             SmbiosTableEntrySize,
             Offset
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return EFI_UNSUPPORTED;
}

/**
  This function try to transfer SMBIOS table via IPMI Blob
  which is available in System Configuration Table.

  @param[in]   Event    The event handle returned when create event.
  @param[in]   Context  The parameter context for this function.
                        Not used in this function.

  @retval VOID
**/
VOID
SmbiosBlobTransfer (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EFI_STATUS  Status;
  UINT16      SessionId;
  UINT8       IanaNumber[IANA_OEM_NUMBER_SIZE];
  UINTN       SessionIdSize;

  Status = SmbiosBlobsDiscover (IanaNumber);
  if (EFI_ERROR (Status)) {
    return;
  }

  Status = IpmiBlobOpen (
             IanaNumber,
             IPMI_BLOB_OPEN_TO_WRITE,
             SMBIOS_BLOBS_ID,
             sizeof (SMBIOS_BLOBS_ID),
             &SessionId
             );
  if (EFI_ERROR (Status)) {
    //
    // There may sometimes be an old session that has previously started.
    // We can not open a new session until BMC kills the old one (typically 1 minute).
    // To recover the interface, we save the opened session to the UEFI variable
    // and then try to close it when needed.
    //
    SessionIdSize = sizeof (SessionId);
    Status        = gRT->GetVariable (
                           SMBIOS_BLOB_TRANSFER_VAR_NAME,
                           &gSmbiosBlobsTransferGuid,
                           NULL,
                           &SessionIdSize,
                           &SessionId
                           );
    if (EFI_ERROR (Status)) {
      return;
    }

    IpmiBlobClose (IanaNumber, SessionId);
    Status = IpmiBlobOpen (
               IanaNumber,
               IPMI_BLOB_OPEN_TO_WRITE,
               SMBIOS_BLOBS_ID,
               sizeof (SMBIOS_BLOBS_ID),
               &SessionId
               );
    if (EFI_ERROR (Status)) {
      return;
    }
  }

  //
  // Save this session id for next boot recover.
  //
  gRT->SetVariable (
         SMBIOS_BLOB_TRANSFER_VAR_NAME,
         &gSmbiosBlobsTransferGuid,
         EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS,
         sizeof (UINT16),
         &SessionId
         );

  WriteSmbiosDataToBmc (IanaNumber, SessionId);

  Status = CommitSmbiosData (IanaNumber, SessionId);
  if (!EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INIT, "SMBIOS Transfer successfully executes\n"));
  }

  //
  // Need to close
  //
  Status = IpmiBlobClose (IanaNumber, SessionId);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "SMBIOS Transfer: Can't close session %x\nYou may need close this session manually\n",
      SessionId
      ));
  }
}

EFI_STATUS
EFIAPI
SmbiosBlobsTransferEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_EVENT   SmbiosBlobTransferEvent;
  EFI_STATUS  Status;

  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_NOTIFY,
                  SmbiosBlobTransfer,
                  NULL,
                  &gEfiEventExitBootServicesGuid,
                  &SmbiosBlobTransferEvent
                  );
  ASSERT_EFI_ERROR (Status);

  return EFI_SUCCESS;
}
