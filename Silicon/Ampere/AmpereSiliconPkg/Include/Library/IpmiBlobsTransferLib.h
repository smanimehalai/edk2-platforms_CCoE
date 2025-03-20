/** @file
  This is a library for generic blob transfer.
  This mechanism can be leveraged to support firmware upgrade,
  smbios transfer,...

  Copyright (c) 2022, Ampere Computing. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef IPMI_BLOBS_TRANSFER_LIB_H_
#define IPMI_BLOBS_TRANSFER_LIB_H_

// IPMI spec v2.0 section 22.9
#define IPMI_BLOBS_TRANSFER_MAX_PACKET_SIZE  (255)
#define IANA_OEM_NUMBER_SIZE                 3

typedef enum {
  BmcBlobCmdCodeGetCount = 0,
  BmcBlobCmdCodeEnumerate,
  BmcBlobCmdCodeOpen,
  BmcBlobCmdCodeRead,
  BmcBlobCmdCodeWrite,
  BmcBlobCmdCodeCommit,
  BmcBlobCmdCodeClose,
  BmcBlobCmdCodeDelete,
  BmcBlobCmdCodeStat,
  BmcBlobCmdCodeSessionStat
} IPMI_BLOBS_TRANSFER_SUB_COMMAND;

#pragma pack (1)

//
//  BlobGetCount(0)
//  BlobCount: The BMC will return the number of enumerable blobs.
//
typedef struct {
  UINT32    BlobCount;
} IPMI_BLOB_COUNT_RX;

//
//  BlobEnumerate(1).
//  BlobIndex: 0-based index of blob to retrieve.
//
typedef struct {
  UINT32    BlobIndex;
} IPMI_BLOB_ENUMERATE_TX;

//
//  BlobEnumerate(1).
//  BlobId:  Blob name which is defined by BMC
//
typedef struct {
  CHAR8    BlobId[1];
} IPMI_BLOB_ENUMERATE_RX;

//
// Flags for blob open command.
//    <bits 2-7 reserved>
//    <bits 8-15 given blob-specific definitions>
//
#define IPMI_BLOB_OPEN_TO_READ   BIT0
#define IPMI_BLOB_OPEN_TO_WRITE  BIT1

//
//  BlobOpen(2).
//  Flags:  The flags field allows the caller to specify
//          whether the blob is being opened for reading or writing.
//  BlobId: Blob name which is defined by BMC
//
typedef struct {
  UINT16    Flags;
  CHAR8     BlobId[1];
} IPMI_BLOB_OPEN_TX;

//
//  BlobOpen(2)
//  SessionId:   The BMC allocates a unique session identifier,
//               and internally maps it to the blob identifier.
//
typedef struct {
  UINT16    SessionId;
} IPMI_BLOB_OPEN_RX;

//
//  BlobRead(3)
//  SessionId:   Returned from BmcBlobOpen.
//  Offset:      The byte sequence start, 0-based.
//  RequestSize: The number of bytes requested for reading.
//
typedef struct {
  UINT16    SessionId;
  UINT32    Offset;
  UINT32    RequestSize;
} IPMI_BLOB_READ_TX;

//
//  BlobRead(3)
//  Data:  Data read from BMC
//
typedef struct {
  UINT8    Data[1];
} IPMI_BLOB_READ_RX;

//
//  BlobWrite(4)
//  SessionId: Returned from BlobOpen.
//  Offset:    The byte sequence start, 0-based.
//  Data:      Data to write
//
typedef struct {
  UINT16    SessionId;
  UINT32    Offset;
  UINT8     Data[1];
} IPMI_BLOB_WRITE_TX;

#define IPMI_BLOB_WRITE_TX_HEADER_SIZE  OFFSET_OF (IPMI_BLOB_WRITE_TX, Data)

//
//  BlobCommit(5)
//  SessionId:        Returned from BlobOpen.
//  CommitDataLength: Length of optional data.
//  CommitData:       Optional blob-specific commit data.
//
typedef struct {
  UINT16    SessionId;
  UINT8     CommitDataLength;
  UINT8     CommitData[1];
} IPMI_BLOB_COMMIT_TX;

//
//  BlobClose(6).
//  SessionId:  Returned from BlobOpen.
//
typedef struct {
  UINT16    SessionId;
} IPMI_BLOB_CLOSE_TX;

//
//  BlobDelete(7)
//  BlobId: Blob name which is defined by BMC,
//          Must correspond to a valid blob.
//
typedef struct {
  CHAR8    BlobId[1];
} IPMI_BLOB_DELETE_TX;

//
// Flags for blob state
//    <bits 5-7 reserved>
//    <bits 8-15 given blob-specific definitions>
//
#define IPMI_BLOB_STATE_OPEN_TO_READ   BIT0
#define IPMI_BLOB_STATE_OPEN_TO_WRITE  BIT1
#define IPMI_BLOB_STATE_COMMITTING     BIT2
#define IPMI_BLOB_STATE_COMMITED       BIT3
#define IPMI_BLOB_STATE_COMMIT_ERROR   BIT4

//
//  BlobStat(8).
//  BlobId: Blob name which is defined by BMC,
//          Must correspond to a valid blob.
//
typedef struct {
  CHAR8    BlobId[1];
} IPMI_BLOB_STAT_TX;

//
//  BlobSessionStat(9)
//  SessionId:  Returned from BlobOpen.
//
typedef struct {
  UINT16    SessionId;
} IPMI_BLOB_SESSION_STAT_TX;

//
//  BlobStat(8) and BlobSessionStat(9)
//  BlobState: statistics about a blob, bit field is defined in IPMI_BLOB_STATE_FLAGS.
//  Size: The size in byte of blob.
//  MetaDataLength: Optional data length in byte.
//  MetaData: Optional data.
//
typedef struct {
  UINT16    BlobState;
  UINT32    Size;
  UINT8     MetaDataLength;
  UINT8     MetaData[1];
} IPMI_BLOB_STAT_RX;

//
//  BlobWriteMeta(10)
//  SessionId:  Returned from BlobOpen.
//  Offset: The byte sequence start, 0-based.
//  Data: Data to write
//
typedef struct {
  UINT16    SessionId;
  UINT32    Offset;
  UINT8     Data[1];
} IPMI_BLOB_WRITE_META_TX;

//
// OemNumber: IANA OEM Number
// SubCommand: is defined in the blob specification
// Crc16 of data in payload, dont use this field if payload is null
//
typedef struct {
  UINT8     IanaNumber[IANA_OEM_NUMBER_SIZE];
  UINT8     SubCommand;
  UINT16    Crc16;
} IPMI_BLOB_TRANSFER_TX_HEADER;

//
// Completion Code: This is IPMI completion code
// OemNumber: is defined in the blob specification
// Crc16 of data in payload, dont use this field if payload is null
//
typedef struct {
  UINT8     CompletionCode;
  UINT8     IANANumber[IANA_OEM_NUMBER_SIZE];
  UINT16    Crc16;
} IPMI_BLOB_TRANSFER_RX_HEADER;

typedef struct {
  union {
    struct {
      IPMI_BLOB_TRANSFER_TX_HEADER    Header;
      UINT8                           Data[1];
    } Tx;
    struct {
      IPMI_BLOB_TRANSFER_RX_HEADER    Header;
      UINT8                           Data[1];
    } Rx;
    UINT8    Data[IPMI_BLOBS_TRANSFER_MAX_PACKET_SIZE];
  } Payload;
  UINTN    PayloadSize;
} IPMI_BLOB_TRANSFER_PACKET;

#pragma pack ()

/**
  This function returns the max size of IPMI blob packet in bytes.

  This size included the header.

  @retval The max size of packet.

**/
UINT32
EFIAPI
IpmiBlobGetMaxRequestPacketSize (
  VOID
  );

/**
  Override the max size in bytes of packet.

  @param[in] PayloadSize  The max size of packet if successfully set.

  @retval The current max size of packet.

**/
UINT32
EFIAPI
IpmiBlobSetMaxRequestPacketSize (
  IN UINTN  PayloadSize
  );

/**
  This function returns the max size of IPMI blob response
  packet in bytes.

  This size included the header.

  @return The max size of payload.

**/
UINT32
IpmiBlobGetMaxResponsePacketSize (
  VOID
  );

/**
  Make a transaction with BMC via IPMI and return the response data.

  @param[in, out] Packet  As input, it contains data to send.
                          As output, it contains response data.

  @return EFI_SUCCESS   The transaction is successful.
  @return Others        Some error has occurred during the transaction.
**/
EFI_STATUS
EFIAPI
IpmiBlobExecute (
  IN OUT IPMI_BLOB_TRANSFER_PACKET  *Packet
  );

/**
  Query BMC to get the number of enumerable blobs.

  @param[in]  IanaNumber  IANA OEM number.
  @param[out] Pointer    Pointer of variable which stores the number of enumerable blobs.

  @retval EFI_SUCCESS   The transaction is successful.
  @retval Others        Some error has occurred during the transaction.
**/
EFI_STATUS
EFIAPI
IpmiBlobGetCount (
  IN  UINT8   *IanaNumber,
  OUT UINT32  *NumberOfEnumerableBlobs
  );

/**
  Get the blob ID from BMC by index; 0 is the base index.
  The blob ID is defined by BMC.

  @param[in]  IanaNumber IANA OEM number
  @param[in]  BlobIndex  Index of the blob.
  @param[out] BlobId     Pointer to the pool containing the blob ID.
  @param[out] BlobSize   Size of the pool.

  @retval EFI_SUCCESS   The transaction is successful.
  @retval Others        Some error has occurred during the transaction.
**/
EFI_STATUS
EFIAPI
IpmiBlobEnumerate (
  IN UINT8       *IanaNumber,
  IN UINT32      BlobIndex,
  OUT CHAR8      *BlobId OPTIONAL,
  IN OUT UINT32  *BlobIdSize
  );

/**
  Notify to BMC whether the blob is being opened for reading or writing.

  @param[in]  IanaNumber IANA OEM number.
  @param[in]  Flags      The flags field allows the caller to specify whether the blob
                         is being opened for reading or writing.
  @param[in]  BlobId     Pointer to Blob ID (ascii string).
  @param[in]  BlobSize   Size of the string include null-terminated.
  @param[out] SessionId  A unique session identifier (BMC auto gen),
                         and internally maps it to the blob identifier.

  @retval EFI_SUCCESS   The transaction is successful.
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
  );

/**
  Send content of blobs to the BMC.

  @param[in]  IanaNumber  IANA OEM number.
  @param[in]  SessionId  A unique session identifier (BMC auto gen),
                         Usually, we get it from Open command.
  @param[in]  Data       Pointer to pool contain data.
  @param[in]  DataSize   The pool size.
  @param[in]  Offset     Offset in blob.

  @retval EFI_SUCCESS   The transaction is successful.
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
  );

/**
  Notify to BMC that blob write operation is completed.

  When have no date to commit, just leave it with NULL,
  the size will be ignored.

  @param[in]  IanaNumber  IANA OEM number.
  @param[in]  SessionId  A unique session identifier (BMC auto gen),
                         Usually, we get it from Open command.
  @param[in] Data        Optional data for BMC (some blobs do not this data).
  @param[in] DataSize    Optional data size.

  @retval EFI_SUCCESS   The transaction is successful.
  @retval Others        Some error has occurred during the transaction.
**/
EFI_STATUS
EFIAPI
IpmiBlobCommit (
  IN UINT8   *IanaNumber,
  IN UINT16  SessionId,
  IN UINT8   *Data OPTIONAL,
  IN UINT8   DataSize OPTIONAL
  );

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

  @retval EFI_SUCCESS   The transaction is successful.
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
  );

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
  );

/**
  Finish the session.

  Command must be called after commit-polling has finished,
  regardless of the result

  @param[in]  IanaNumber  IANA OEM number.
  @param[in]  SessionId  A unique session identifier (BMC auto gen),
                         Usually, we get it from Open command.

  @retval EFI_SUCCESS   The transaction is successful.
  @retval Others        Some error has occurred during the transaction.
**/
EFI_STATUS
EFIAPI
IpmiBlobClose (
  IN UINT8   *IanaNumber,
  IN UINT16  SessionId
  );

#endif /* IPMI_BLOBS_TRANSFER_LIB_H_ */
