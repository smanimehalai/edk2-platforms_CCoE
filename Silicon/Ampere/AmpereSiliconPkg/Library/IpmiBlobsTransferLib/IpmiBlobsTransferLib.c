/** @file

  Copyright (c) 2022, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/IpmiBlobsTransferLib.h>
#include <Library/IpmiLib.h>
#include <Library/MemoryAllocationLib.h>

#define CRC16_CCITT_INIT_VALUE          (0x1D0F)
#define IPMI_BLOBS_TRANSFER_OEM_CMD     (0x80)
#define IPMI_BLOBS_TRANSFER_OEM_NET_FN  (0x2e)

IPMI_GET_SYSTEM_INTERFACE_SSIF_CAPABILITIES_RESPONSE  mIpmiCapabilities;
STATIC UINTN                                          mMaxPayLoadSize;

GLOBAL_REMOVE_IF_UNREFERENCED STATIC CONST UINT16  mCrc16LookupTable[] = {
  0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
  0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
  0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
  0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
  0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
  0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
  0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
  0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
  0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
  0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
  0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
  0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
  0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
  0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
  0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
  0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
  0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
  0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
  0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
  0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
  0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
  0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
  0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
  0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
  0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
  0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
  0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
  0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
  0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
  0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
  0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
  0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0
};

/**
   Calculates the CRC16-CCITT checksum of the given buffer.
   @param[in]      Buffer        Pointer to the buffer.
   @param[in]      Length        Length of the buffer, in bytes.
   @param[in]      InitialValue  Initial value of the CRC.
   @return The CRC16-CCITT checksum.
**/
UINT16
EFIAPI
CalculateCrc16Ccitt (
  IN CONST VOID  *Buffer,
  IN UINTN       Length,
  IN UINT16      InitialValue
  )
{
  CONST UINT8  *Data;
  UINT16       Crc16;
  UINTN        Index;

  if ((Buffer == NULL) || (Length == 0)) {
    return InitialValue;
  }

  Data  = Buffer;
  Crc16 = InitialValue;

  for (Index = 0; Index < Length; Index++) {
    Crc16  = (Crc16 << 8) ^ (mCrc16LookupTable[(Crc16 >> 8) ^ Data[Index]]);
    Crc16 &= 0xFFFFU;
  }

  return Crc16;
}

/**
  This function returns the max size of IPMI blob request
  packet in bytes.

  This size included the header.

  @retval The max size of packet.

**/
UINT32
EFIAPI
IpmiBlobGetMaxRequestPacketSize (
  VOID
  )
{
  //
  // Need to exclude NetFn, Cmd and PEC
  //
  if (mMaxPayLoadSize > 3) {
    return mMaxPayLoadSize - 3;
  }

  return 0;
}

/**
  Override the max size in bytes of request packet.

  @param[in] PayloadSize  The max size of packet if successfully set.

  @retval The current max size of packet.

**/
UINT32
EFIAPI
IpmiBlobSetMaxRequestPacketSize (
  IN UINTN  PayloadSize
  )
{
  if ((PayloadSize == 0) || (PayloadSize > mIpmiCapabilities.InputMsgSize)) {
    return mMaxPayLoadSize;
  }

  mMaxPayLoadSize = PayloadSize;
  return mMaxPayLoadSize;
}

/**
  This function returns the max size of IPMI blob response
  packet in bytes.

  This size included the header.

  @return The max size of payload.

**/
UINT32
IpmiBlobGetMaxResponsePacketSize (
  VOID
  )
{
  return mIpmiCapabilities.OutputMsgSize;
}

/**
  Make a transaction with BMC via IPMI and return the response data.

  @param[in, out] Packet  As input, it contains data to send.
                                As output, it contains response data.

  @return EFI_SUCCESS   The transaction is successfully.
  @return Others        Some error has occurred during the transaction.
**/
EFI_STATUS
EFIAPI
IpmiBlobExecute (
  IN OUT IPMI_BLOB_TRANSFER_PACKET  *Packet
  )
{
  EFI_STATUS                    Status;
  IPMI_BLOB_TRANSFER_RX_HEADER  *RxHeader;
  IPMI_BLOB_TRANSFER_TX_HEADER  *TxHeader;
  UINT16                        Crc16;
  UINT32                        IpmiBlobResponseSize;
  UINT8                         IanaNumber[3];
  UINTN                         PacketSize;

  if (Packet == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  TxHeader             = &(Packet->Payload.Tx.Header);
  RxHeader             = &(Packet->Payload.Rx.Header);
  IpmiBlobResponseSize = IpmiBlobGetMaxResponsePacketSize ();

  if (Packet->PayloadSize
      > IpmiBlobGetMaxRequestPacketSize ()
      - sizeof (IPMI_BLOB_TRANSFER_TX_HEADER))
  {
    return EFI_UNSUPPORTED;
  }

  CopyMem (
    IanaNumber,
    TxHeader->IanaNumber,
    sizeof (IanaNumber)
    );

  DEBUG_CODE (
    UINTN  Index;

    DEBUG ((DEBUG_INFO, "IPMI blob transfer request:\n"));
    DEBUG ((
      DEBUG_INFO,
      "  - Oem Number: 0x%02x 0x%02x 0x%02x\n",
      TxHeader->IanaNumber[0],
      TxHeader->IanaNumber[1],
      TxHeader->IanaNumber[2]
      ));
    DEBUG ((DEBUG_INFO, "  - Subcommand: 0x%02x\n", TxHeader->SubCommand));
    DEBUG ((DEBUG_INFO, "  - Crc16: 0x%04x\n", TxHeader->Crc16));

    if (Packet->PayloadSize != 0) {
      DEBUG ((DEBUG_INFO, "  - Payload:"));
      for (Index = 0; Index < Packet->PayloadSize; Index++) {
        if (Index % 0x10 == 0) {
          DEBUG ((DEBUG_INFO, "\n"));
        }

        DEBUG ((DEBUG_INFO, "%02X ", *(Packet->Payload.Tx.Data + Index)));
      }

      DEBUG ((DEBUG_INFO, "\n\n"));
    }
  );

  if (Packet->PayloadSize != 0) {
    PacketSize = OFFSET_OF (IPMI_BLOB_TRANSFER_PACKET, Payload.Tx.Data) + Packet->PayloadSize;
  } else {
    PacketSize = OFFSET_OF (IPMI_BLOB_TRANSFER_PACKET, Payload.Tx.Header.Crc16);
  }

  Status = IpmiSubmitCommand (
             IPMI_BLOBS_TRANSFER_OEM_NET_FN,
             IPMI_BLOBS_TRANSFER_OEM_CMD,
             (UINT8 *)Packet,
             PacketSize,
             (UINT8 *)Packet,
             &IpmiBlobResponseSize
             );
  if (EFI_ERROR (Status) || (RxHeader->CompletionCode != IPMI_COMP_CODE_NORMAL)) {
    return EFI_UNSUPPORTED;
  }

  if (IpmiBlobResponseSize < OFFSET_OF (IPMI_BLOB_TRANSFER_PACKET, Payload.Rx.Header.Crc16)) {
    DEBUG ((DEBUG_WARN, "IpmiBlob: Response message is too small\n"));
    return EFI_UNSUPPORTED;
  }

  if (IpmiBlobResponseSize <= sizeof (IPMI_BLOB_TRANSFER_RX_HEADER)) {
    Packet->PayloadSize = 0;
  } else {
    //
    // Response message contains payload
    //
    Packet->PayloadSize = IpmiBlobResponseSize - OFFSET_OF (IPMI_BLOB_TRANSFER_PACKET, Payload.Rx.Data);
  }

  // check IANA OEM Number
  if (0 != CompareMem (
             IanaNumber,
             RxHeader->IANANumber,
             sizeof (IanaNumber)
             ))
  {
    DEBUG ((
      DEBUG_WARN,
      "IpmiBlob: Returned IANA OEM number (0x%02X 0x%02X 0x%02X) does not match request (0x%02X 0x%02X 0x%02X)\n",
      RxHeader->IANANumber[0],
      RxHeader->IANANumber[1],
      RxHeader->IANANumber[2],
      TxHeader->IanaNumber[0],
      TxHeader->IanaNumber[1],
      TxHeader->IanaNumber[2]
      ));
    return EFI_UNSUPPORTED;
  }

  Crc16 = CalculateCrc16Ccitt (
            Packet->Payload.Rx.Data,
            Packet->PayloadSize,
            CRC16_CCITT_INIT_VALUE
            );

  if (Packet->PayloadSize != 0) {
    if (RxHeader->Crc16 != Crc16) {
      DEBUG ((DEBUG_WARN, "Transfer blob fail !!! Crc error %x != %x\n", RxHeader->Crc16, Crc16));
      return EFI_UNSUPPORTED;
    }
  }

  return EFI_SUCCESS;
}

/**
  Query BMC to get the number of enumerable blobs.

  @param[in]  IanaNumber  IANA OEM number.
  @param[out] Pointer    Pointer of variable which stores the number of enumerable blobs.

  @retval EFI_SUCCESS   The transaction is successfully.
  @retval Others        Some error has occurred during the transaction.
**/
EFI_STATUS
EFIAPI
IpmiBlobGetCount (
  IN  UINT8   *IanaNumber,
  OUT UINT32  *NumberOfEnumerableBlobs
  )
{
  EFI_STATUS                    Status;
  IPMI_BLOB_COUNT_RX            *RxCountBlob;
  IPMI_BLOB_TRANSFER_PACKET     Packet;
  IPMI_BLOB_TRANSFER_TX_HEADER  *TxHeader;

  if ((NumberOfEnumerableBlobs == NULL) || (IanaNumber == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  TxHeader    = (IPMI_BLOB_TRANSFER_TX_HEADER *)&Packet;
  RxCountBlob =
    (IPMI_BLOB_COUNT_RX *)((UINT8 *)&Packet + sizeof (IPMI_BLOB_TRANSFER_TX_HEADER));

  CopyMem (
    TxHeader->IanaNumber,
    IanaNumber,
    sizeof (TxHeader->IanaNumber)
    );
  TxHeader->SubCommand = BmcBlobCmdCodeGetCount;
  //
  // This command does not contain payload data
  //
  TxHeader->Crc16 = CRC16_CCITT_INIT_VALUE;

  Packet.PayloadSize = 0;

  Status = IpmiBlobExecute (&Packet);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Get Count fail - %r\n", Status));
    return Status;
  }

  if (Packet.PayloadSize == 0) {
    DEBUG ((DEBUG_ERROR, "%a: Returned data is deprecated.\n", __FUNCTION__));
    return EFI_UNSUPPORTED;
  } else {
    *NumberOfEnumerableBlobs = RxCountBlob->BlobCount;
  }

  return EFI_SUCCESS;
}

/**
  Get the blob ID from BMC by index; 0 is the base index.
  The blob ID is defined by BMC.

  @param[in]  IanaNumber IANA OEM number
  @param[in]  BlobIndex  Index of the blob.
  @param[out] BlobId     Pointer to the pool containing the blob ID.
  @param[out] BlobSize   Size of the pool.

  @retval EFI_SUCCESS   The transaction is successfully.
  @retval Others        Some error has occurred during the transaction.
**/
EFI_STATUS
EFIAPI
IpmiBlobEnumerate (
  IN UINT8       *IanaNumber,
  IN UINT32      BlobIndex,
  OUT CHAR8      *BlobId,
  IN OUT UINT32  *BlobIdSize
  )
{
  EFI_STATUS                    Status;
  IPMI_BLOB_ENUMERATE_RX        *RxBlob;
  IPMI_BLOB_ENUMERATE_TX        *TxBlob;
  IPMI_BLOB_TRANSFER_PACKET     Packet;
  IPMI_BLOB_TRANSFER_TX_HEADER  *TxHeader;

  if ((IanaNumber == NULL) || (BlobIdSize == NULL) || (BlobId == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  TxHeader = (IPMI_BLOB_TRANSFER_TX_HEADER *)&Packet;
  TxBlob   = (IPMI_BLOB_ENUMERATE_TX *)((UINT8 *)&Packet + sizeof (IPMI_BLOB_TRANSFER_TX_HEADER));
  RxBlob   = (IPMI_BLOB_ENUMERATE_RX *)((UINT8 *)&Packet + sizeof (IPMI_BLOB_TRANSFER_RX_HEADER));

  CopyMem (
    TxHeader->IanaNumber,
    IanaNumber,
    sizeof (TxHeader->IanaNumber)
    );

  TxHeader->SubCommand = BmcBlobCmdCodeEnumerate;
  //
  // Payload data contains blob index
  //
  Packet.PayloadSize = sizeof (TxBlob->BlobIndex);
  if (Packet.PayloadSize
      > IpmiBlobGetMaxRequestPacketSize ()
      - sizeof (IPMI_BLOB_TRANSFER_TX_HEADER))
  {
    return EFI_UNSUPPORTED;
  }

  TxBlob->BlobIndex = BlobIndex;
  TxHeader->Crc16   = CalculateCrc16Ccitt (
                        Packet.Payload.Tx.Data,
                        Packet.PayloadSize,
                        CRC16_CCITT_INIT_VALUE
                        );

  Status = IpmiBlobExecute (&Packet);
  if (!EFI_ERROR (Status)) {
    if (Packet.PayloadSize != 0) {
      if (*BlobIdSize < Packet.PayloadSize) {
        *BlobIdSize = Packet.PayloadSize;
        return EFI_BUFFER_TOO_SMALL;
      }

      CopyMem (
        BlobId,
        RxBlob->BlobId,
        Packet.PayloadSize
        );

      *BlobIdSize = Packet.PayloadSize;
      return EFI_SUCCESS;
    } else {
      DEBUG ((DEBUG_ERROR, "%a: Returned data is deprecated.\n", __FUNCTION__));
      return EFI_UNSUPPORTED;
    }
  } else {
    DEBUG ((DEBUG_ERROR, "Enumerate blob fail - %r\n", Status));
  }

  return Status;
}

/**
  Notify to BMC whether the blob is being opened for reading or writing.

  @param[in]  IanaNumber IANA OEM number.
  @param[in]  Flags      The flags field allows the caller to specify whether the blob
                         is being opened for reading or writing.
  @param[in]  BlobId     Pointer to Blob ID (ascii string).
  @param[in]  BlobSize   Size of the string include null-terminated.
  @param[out] SessionId  A unique session identifier (BMC auto gen),
                         and internally maps it to the blob identifier.

  @retval EFI_SUCCESS   The transaction is successfully.
  @retval Others        Some error has occurred during the transaction.
**/
EFI_STATUS
EFIAPI
IpmiBlobOpen (
  IN UINT8    *IanaNumber,
  IN UINT16   Flags,
  IN CHAR8    *BlobId,
  IN UINT32   BlobIdSize,
  OUT UINT16  *SessionId
  )
{
  CHAR16                        TempString[128];
  EFI_STATUS                    Status;
  IPMI_BLOB_OPEN_RX             *RxBlob;
  IPMI_BLOB_OPEN_TX             *TxBlob;
  IPMI_BLOB_TRANSFER_PACKET     Packet;
  IPMI_BLOB_TRANSFER_TX_HEADER  *TxHeader;

  if ((IanaNumber == NULL) || (BlobId == NULL) || (SessionId == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  TxHeader = (IPMI_BLOB_TRANSFER_TX_HEADER *)&Packet;
  TxBlob   = (IPMI_BLOB_OPEN_TX *)((UINT8 *)&Packet + sizeof (IPMI_BLOB_TRANSFER_TX_HEADER));
  RxBlob   = (IPMI_BLOB_OPEN_RX *)((UINT8 *)&Packet + sizeof (IPMI_BLOB_TRANSFER_TX_HEADER));

  CopyMem (
    TxHeader->IanaNumber,
    IanaNumber,
    sizeof (TxHeader->IanaNumber)
    );
  TxHeader->SubCommand = BmcBlobCmdCodeOpen;

  //
  // Payload contains the open flags and blob ID
  //
  Packet.PayloadSize = sizeof (TxBlob->Flags) + BlobIdSize;
  if (Packet.PayloadSize
      > IpmiBlobGetMaxRequestPacketSize ()
      - sizeof (IPMI_BLOB_TRANSFER_TX_HEADER))
  {
    return EFI_UNSUPPORTED;
  }

  TxBlob->Flags = Flags;

  CopyMem (
    TxBlob->BlobId,
    BlobId,
    BlobIdSize
    );

  TxHeader->Crc16 = CalculateCrc16Ccitt (
                      Packet.Payload.Tx.Data,
                      Packet.PayloadSize,
                      CRC16_CCITT_INIT_VALUE
                      );

  Status = IpmiBlobExecute (&Packet);
  if (!EFI_ERROR (Status)) {
    if (Packet.PayloadSize != 0) {
      *SessionId = RxBlob->SessionId;
    } else {
      DEBUG ((DEBUG_ERROR, "%a: Returned data is deprecated.\n", __FUNCTION__));
      return EFI_UNSUPPORTED;
    }
  } else {
    AsciiStrToUnicodeStrS (BlobId, TempString, sizeof (TempString));
    DEBUG ((DEBUG_ERROR, "Open blob %s fail - %r\n", TempString, Status));
  }

  return Status;
}

/**
  Send content of blobs to the BMC.

  @param[in]  IanaNumber  IANA OEM number.
  @param[in]  SessionId  A unique session identifier (BMC auto gen),
                         Usually, we get it from Open command.
  @param[in]  Data       Pointer to pool contain data.
  @param[in]  DataSize   The pool size.
  @param[in]  Offset     Offset in blob.

  @retval EFI_SUCCESS   The transaction is successfully.
  @retval Others        Some error has occurred during the transaction.
**/
EFI_STATUS
EFIAPI
IpmiBlobWrite (
  IN UINT8   *IanaNumber,
  IN UINT16  SessionId,
  IN UINT8   *Data,
  IN UINTN   DataSize,
  IN UINT32  Offset
  )
{
  EFI_STATUS                    Status;
  IPMI_BLOB_TRANSFER_PACKET     Packet;
  IPMI_BLOB_TRANSFER_TX_HEADER  *TxHeader;
  IPMI_BLOB_WRITE_TX            *TxBlob;

  if ((IanaNumber == NULL) || (Data == NULL) || (DataSize == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  TxHeader = (IPMI_BLOB_TRANSFER_TX_HEADER *)&Packet;
  TxBlob   = (IPMI_BLOB_WRITE_TX *)((UINT8 *)&Packet + sizeof (IPMI_BLOB_TRANSFER_TX_HEADER));

  CopyMem (
    TxHeader->IanaNumber,
    IanaNumber,
    sizeof (TxHeader->IanaNumber)
    );
  TxHeader->SubCommand = BmcBlobCmdCodeWrite;

  //
  // Payload contains Session ID, Offset of data and data size
  //
  Packet.PayloadSize = OFFSET_OF (IPMI_BLOB_WRITE_TX, Data) + DataSize;

  if (Packet.PayloadSize
      > IpmiBlobGetMaxRequestPacketSize ()
      - sizeof (IPMI_BLOB_TRANSFER_TX_HEADER))
  {
    return EFI_UNSUPPORTED;
  }

  TxBlob->SessionId = SessionId;
  TxBlob->Offset    = Offset;

  CopyMem (
    TxBlob->Data,
    Data,
    DataSize
    );

  TxHeader->Crc16 = CalculateCrc16Ccitt (
                      Packet.Payload.Tx.Data,
                      Packet.PayloadSize,
                      CRC16_CCITT_INIT_VALUE
                      );

  Status = IpmiBlobExecute (&Packet);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Write blob session %x fail - %r\n", SessionId, Status));
  }

  return Status;
}

/**
  Notify to BMC that blob write operation is completed.

  When have no date to commit, just leave it with NULL,
  the size will be ignored.

  @param[in]  IanaNumber  IANA OEM number.
  @param[in]  SessionId   A unique session identifier (BMC auto gen),
                          Usually, we get it from Open command.
  @param[in]  Data        Optional data for BMC (some blobs do not this data).
  @param[in]  DataSize    Optional data size.

  @retval EFI_SUCCESS   The transaction is successfully.
  @retval Others        Some error has occurred during the transaction.
**/
EFI_STATUS
EFIAPI
IpmiBlobCommit (
  IN UINT8   *IanaNumber,
  IN UINT16  SessionId,
  IN UINT8   *Data OPTIONAL,
  IN UINT8   DataSize OPTIONAL
  )
{
  EFI_STATUS                    Status;
  IPMI_BLOB_COMMIT_TX           *TxBlob;
  IPMI_BLOB_TRANSFER_PACKET     Packet;
  IPMI_BLOB_TRANSFER_TX_HEADER  *TxHeader;

  if (IanaNumber == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  TxHeader = (IPMI_BLOB_TRANSFER_TX_HEADER *)&Packet;
  TxBlob   = (IPMI_BLOB_COMMIT_TX *)((UINT8 *)&Packet + sizeof (IPMI_BLOB_TRANSFER_TX_HEADER));

  CopyMem (
    TxHeader->IanaNumber,
    IanaNumber,
    sizeof (TxHeader->IanaNumber)
    );
  TxHeader->SubCommand = BmcBlobCmdCodeCommit;

  //
  // Payload data contains Session ID, and optional data
  //
  if ((Data == NULL) || (DataSize == 0)) {
    Data     = NULL;
    DataSize = 0;
  }

  Packet.PayloadSize = OFFSET_OF (IPMI_BLOB_COMMIT_TX, CommitData) + DataSize;

  if (Packet.PayloadSize
      > IpmiBlobGetMaxRequestPacketSize ()
      - sizeof (IPMI_BLOB_TRANSFER_TX_HEADER))
  {
    return EFI_UNSUPPORTED;
  }

  TxBlob->SessionId        = SessionId;
  TxBlob->CommitDataLength = DataSize;

  if (DataSize != 0) {
    CopyMem (
      TxBlob->CommitData,
      Data,
      DataSize
      );
  }

  TxHeader->Crc16 = CalculateCrc16Ccitt (
                      Packet.Payload.Tx.Data,
                      Packet.PayloadSize,
                      CRC16_CCITT_INIT_VALUE
                      );

  Status = IpmiBlobExecute (&Packet);
  if (EFI_ERROR (Status)) {
    //
    // If BMC is busy, this command may not succeed.
    //
    DEBUG ((DEBUG_WARN, "Commit blob %x fail - %r\n", SessionId, Status));
  }

  return Status;
}

/**
  Delete a blob, not all bobs support this command.

  @param[in]  IanaNumber  IANA OEM number.
  @param[in]  BlobId      Pointer to Blob ID (ascii string).
  @param[in]  BlobSize    Size of the string include null-terminated.

  @retval EFI_SUCCESS   The transaction is successful.
  @retval Others        Some error has occurred during the transaction.
**/
EFI_STATUS
EFIAPI
IpmiBlobDelete (
  IN UINT8   *IanaNumber,
  IN CHAR8   *BlobId,
  IN UINT32  BlobIdSize
  )
{
  CHAR16                        TempString[128];
  EFI_STATUS                    Status;
  IPMI_BLOB_DELETE_TX           *TxBlob;
  IPMI_BLOB_TRANSFER_PACKET     Packet;
  IPMI_BLOB_TRANSFER_TX_HEADER  *TxHeader;

  if ((IanaNumber == NULL) || (BlobId == NULL) || (BlobIdSize == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  TxHeader = (IPMI_BLOB_TRANSFER_TX_HEADER *)&Packet;
  TxBlob   = (IPMI_BLOB_DELETE_TX *)((UINT8 *)&Packet + sizeof (IPMI_BLOB_TRANSFER_TX_HEADER));

  CopyMem (
    TxHeader->IanaNumber,
    IanaNumber,
    sizeof (TxHeader->IanaNumber)
    );
  TxHeader->SubCommand = BmcBlobCmdCodeDelete;

  //
  // This command contains blob ID
  //
  Packet.PayloadSize = OFFSET_OF (IPMI_BLOB_DELETE_TX, BlobId) + BlobIdSize;
  if (Packet.PayloadSize
      > IpmiBlobGetMaxRequestPacketSize ()
      - sizeof (IPMI_BLOB_TRANSFER_TX_HEADER))
  {
    return EFI_UNSUPPORTED;
  }

  CopyMem (TxBlob->BlobId, BlobId, BlobIdSize);
  TxHeader->Crc16 = CalculateCrc16Ccitt (
                      Packet.Payload.Tx.Data,
                      Packet.PayloadSize,
                      CRC16_CCITT_INIT_VALUE
                      );

  Status = IpmiBlobExecute (&Packet);
  if (EFI_ERROR (Status)) {
    AsciiStrToUnicodeStrS (BlobId, TempString, sizeof (TempString));
    DEBUG ((DEBUG_WARN, "Delete blob %s fail - %r\n", TempString, Status));
    return Status;
  }

  return EFI_SUCCESS;
}

/**
  Retrieve statistics about a blob. When blob is not opened, this service
  will fail.

  This function supports both blob ID (ascii) and session ID.

  @param[in]  IanaNumber   IANA OEM number.
  @param[in]  Id           Blob identifier (ascii string). In case BlobIdSize is zero,
                           we know it as SessionId.
  @param[in]  BlobIdSize   Size of BlobId, include null terminated char.
                           In case we use session ID, it must be zero.
  @param[out] BlobState    The state of blob in BMC.
  @param[out] BlobSize     The size in bytes of the blob.
  @param[out] MetaData     Optional blob-specific metadata.
  @param[out] MetaDataSize The size in bytes of optional blob-specific metadata.

  @retval EFI_SUCCESS   The transaction is successfully.
  @retval Others        Some error has occurred during the transaction.
**/
EFI_STATUS
EFIAPI
IpmiBlobGetStatistics (
  IN UINT8    *IanaNumber,
  IN VOID     *Id,
  IN UINTN    BlobIdSize,
  OUT UINT16  *BlobState,
  OUT UINT32  *BlobSize OPTIONAL,
  OUT UINT8   *MetaData OPTIONAL,
  OUT UINT8   *MetaDataSize OPTIONAL
  )
{
  CHAR16                        TempString[128];
  EFI_STATUS                    Status;
  IPMI_BLOB_SESSION_STAT_TX     *BlobSessionTx;
  IPMI_BLOB_STAT_RX             *RxBlob;
  IPMI_BLOB_STAT_TX             *TxBlob;
  IPMI_BLOB_TRANSFER_PACKET     Packet;
  IPMI_BLOB_TRANSFER_TX_HEADER  *TxHeader;

  if (  (IanaNumber == NULL)
     || ((Id == NULL))
     || (BlobState == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  TxHeader      = (IPMI_BLOB_TRANSFER_TX_HEADER *)&Packet;
  TxBlob        = (IPMI_BLOB_STAT_TX *)((UINT8 *)&Packet + sizeof (IPMI_BLOB_TRANSFER_TX_HEADER));
  BlobSessionTx = (IPMI_BLOB_SESSION_STAT_TX *)((UINT8 *)&Packet + sizeof (IPMI_BLOB_TRANSFER_TX_HEADER));
  RxBlob        = (IPMI_BLOB_STAT_RX *)((UINT8 *)&Packet + sizeof (IPMI_BLOB_TRANSFER_RX_HEADER));

  CopyMem (
    TxHeader->IanaNumber,
    IanaNumber,
    sizeof (TxHeader->IanaNumber)
    );
  TxHeader->SubCommand = BmcBlobCmdCodeStat;

  if (BlobIdSize != 0) {
    //
    // Using Blob ID
    //
    Packet.PayloadSize = OFFSET_OF (IPMI_BLOB_STAT_TX, BlobId) + BlobIdSize;
    CopyMem (TxBlob->BlobId, Id, BlobIdSize);
    TxHeader->Crc16 = CalculateCrc16Ccitt (
                        Packet.Payload.Tx.Data,
                        Packet.PayloadSize,
                        CRC16_CCITT_INIT_VALUE
                        );
  } else {
    //
    // Using session ID
    //
    Packet.PayloadSize       = sizeof (BlobSessionTx->SessionId);
    BlobSessionTx->SessionId = ReadUnaligned16 ((UINT16 *)Id);
    TxHeader->Crc16          = CalculateCrc16Ccitt (
                                 Packet.Payload.Tx.Data,
                                 Packet.PayloadSize,
                                 CRC16_CCITT_INIT_VALUE
                                 );
  }

  Status =  IpmiBlobExecute (&Packet);
  if (!EFI_ERROR (Status)) {
    if (Packet.PayloadSize != 0) {
      *BlobState = RxBlob->BlobState;

      if (BlobSize != NULL) {
        *BlobSize = RxBlob->Size;
      }

      if (MetaDataSize != NULL) {
        if (*MetaDataSize < RxBlob->MetaDataLength) {
          *MetaDataSize = RxBlob->MetaDataLength;
          return EFI_BUFFER_TOO_SMALL;
        } else {
          CopyMem (
            MetaData,
            RxBlob->MetaData,
            RxBlob->MetaDataLength
            );
          *MetaDataSize = RxBlob->MetaDataLength;
          return EFI_SUCCESS;
        }
      }
    } else {
      DEBUG ((DEBUG_WARN, "%a: Returned data is deprecated.\n", __FUNCTION__));
      Status = EFI_UNSUPPORTED;
    }
  } else {
    //
    // This command is not supported by all blobs.
    //
    if (BlobIdSize != 0) {
      AsciiStrToUnicodeStrS ((CHAR8 *)Id, TempString, sizeof (TempString));
      DEBUG ((
        DEBUG_WARN,
        "Get statistics info of blob id %s fail - %r\n",
        TempString,
        Status
        ));
    } else {
      DEBUG ((
        DEBUG_WARN,
        "Get statistics info of session %x fail - %r\n",
        ReadUnaligned16 (Id),
        Status
        ));
    }
  }

  return Status;
}

/**
  Finish the session.

  Command must be called after commit-polling has finished,
  regardless of the result

  @param[in]  IanaNumber  IANA OEM number.
  @param[in]  SessionId  A unique session identifier (BMC auto gen),
                         Usually, we get it from Open command.

  @retval EFI_SUCCESS   The transaction is successfully.
  @retval Others        Some error has occurred during the transaction.
**/
EFI_STATUS
EFIAPI
IpmiBlobClose (
  IN UINT8   *IanaNumber,
  IN UINT16  SessionId
  )
{
  EFI_STATUS                    Status;
  IPMI_BLOB_CLOSE_TX            *TxBlob;
  IPMI_BLOB_TRANSFER_PACKET     Packet;
  IPMI_BLOB_TRANSFER_TX_HEADER  *TxHeader;

  if (IanaNumber == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  TxHeader = (IPMI_BLOB_TRANSFER_TX_HEADER *)&Packet;
  TxBlob   = (IPMI_BLOB_CLOSE_TX *)((UINT8 *)&Packet + sizeof (IPMI_BLOB_TRANSFER_TX_HEADER));

  CopyMem (
    TxHeader->IanaNumber,
    IanaNumber,
    sizeof (TxHeader->IanaNumber)
    );

  TxHeader->SubCommand = BmcBlobCmdCodeClose;
  //
  // Payload data contains session ID
  //
  Packet.PayloadSize = sizeof (TxBlob->SessionId);
  TxBlob->SessionId  = SessionId;

  TxHeader->Crc16 = CalculateCrc16Ccitt (
                      Packet.Payload.Tx.Data,
                      Packet.PayloadSize,
                      CRC16_CCITT_INIT_VALUE
                      );

  Status = IpmiBlobExecute (&Packet);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Close session %x fail - %r\n", SessionId, Status));
  }

  return Status;
}

EFI_STATUS
EFIAPI
IpmiBlobsTransferLibConstructor (
  VOID
  )
{
  EFI_STATUS  Status;
  UINT8       RequestTypeSsif;
  UINT32      ResponseSize;

  RequestTypeSsif = IPMI_GET_SYSTEM_INTERFACE_CAPABILITIES_INTERFACE_TYPE_SSIF;
  ResponseSize    = sizeof (IPMI_GET_SYSTEM_INTERFACE_SSIF_CAPABILITIES_RESPONSE);

  Status =  IpmiSubmitCommand (
              IPMI_NETFN_APP,
              IPMI_APP_GET_SYSTEM_INTERFACE_CAPABILITIES,
              (UINT8 *)&RequestTypeSsif,
              sizeof (UINT8),
              (UINT8 *)&mIpmiCapabilities,
              &ResponseSize
              );
  if (EFI_ERROR (Status)) {
    return EFI_UNSUPPORTED;
  }

  mMaxPayLoadSize = mIpmiCapabilities.InputMsgSize;

  return EFI_SUCCESS;
}
