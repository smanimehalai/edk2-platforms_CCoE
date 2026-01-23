/** @file
  BMC Management setup screen

  Copyright (c) 2021, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>

#include <Guid/BmcInfoScreenHii.h>
#include <Guid/MdeModuleHii.h>
#include <IndustryStandard/Ipmi.h>
#include <IndustryStandard/IpmiNetFnApp.h>
#include <IndustryStandard/IpmiNetFnTransport.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HiiLib.h>
#include <Library/IpmiCommandLib.h>
#include <Library/IpmiCommandLibExt.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/RealTimeClockLib.h>
#include <Library/TimeBaseLib.h>
#include <Library/TimerLib.h>
#include <IndustryStandard/IpmiNetFnStorage.h>

#include "BmcInfoScreenDxe.h"

#define IPMI_SET_SEL_RETRY_MAX  10
#define MIN_VALID_EPOCH  946684800  // 2000-01-01
#define DBG_TIME DEBUG_INFO
#define DBG_ERR  DEBUG_ERROR
#define DBG_WARN DEBUG_WARN

/* --------------------------------------------------------------------
 * NEW: Sync direction enum
 * ------------------------------------------------------------------*/
typedef enum {
  SelSyncNone = 0,
  SelSyncRtcToSel,
  SelSyncSelToRtc
} SEL_SYNC_DIRECTION;



STATIC BOOLEAN mTimeSyncSelLogged = FALSE;


//
// HII Handle for BMC Info Screen package
//
EFI_HII_HANDLE mHiiHandle;

/**
  This function updates info for Main form.

  @param[in] VOID

  @retval EFI_SUCCESS       The entry point is executed successfully.
  @retval Other             Some error occurs when executing this entry point.

**/
EFI_STATUS
UpdateMainForm (
  VOID
  )
{
  EFI_IFR_GUID_LABEL          *EndLabel;
  EFI_IFR_GUID_LABEL          *StartLabel;
  EFI_STATUS                  Status;
  IPMI_GET_DEVICE_ID_RESPONSE DeviceId;
  BMC_LAN_INFO                BmcLanInfo;
  UINT16                      StrBuf[MAX_STRING_SIZE];
  UINT8                       BmcChannel;
  VOID                        *EndOpCodeHandle;
  VOID                        *StartOpCodeHandle;

  Status = IpmiGetDeviceId (&DeviceId);
  if (!EFI_ERROR (Status)
      && DeviceId.CompletionCode == IPMI_COMP_CODE_NORMAL)
  {
    //
    // Firmware Revision
    //
    UnicodeSPrint (
      StrBuf,
      sizeof (StrBuf),
      L"%d.%02d",
      DeviceId.FirmwareRev1.Bits.MajorFirmwareRev,
      BcdToDecimal8 (DeviceId.MinorFirmwareRev)
      );
    HiiSetString (mHiiHandle, STRING_TOKEN (STR_BMC_FIRMWARE_REV_VALUE), StrBuf, NULL);

    //
    // IPMI Version
    //
    UnicodeSPrint (
      StrBuf,
      sizeof (StrBuf),
      L"%d.%d",
      DeviceId.SpecificationVersion & 0x0F,
      (DeviceId.SpecificationVersion >> 4) & 0x0F
      );
    HiiSetString (mHiiHandle, STRING_TOKEN (STR_BMC_IPMI_VER_VALUE), StrBuf, NULL);
  }

  //
  // Initialize the container for dynamic opcodes
  //
  StartOpCodeHandle = HiiAllocateOpCodeHandle ();
  ASSERT (StartOpCodeHandle != NULL);

  EndOpCodeHandle = HiiAllocateOpCodeHandle ();
  ASSERT (EndOpCodeHandle != NULL);

  //
  // Create Hii Extend Label OpCode as the start opcode
  //
  StartLabel = (EFI_IFR_GUID_LABEL *)HiiCreateGuidOpCode (
                                       StartOpCodeHandle,
                                       &gEfiIfrTianoGuid,
                                       NULL,
                                       sizeof (EFI_IFR_GUID_LABEL)
                                       );
  StartLabel->ExtendOpCode = EFI_IFR_EXTEND_OP_LABEL;
  StartLabel->Number       = LABEL_UPDATE;

  //
  // Create Hii Extend Label OpCode as the end opcode
  //
  EndLabel = (EFI_IFR_GUID_LABEL *)HiiCreateGuidOpCode (
                                     EndOpCodeHandle,
                                     &gEfiIfrTianoGuid,
                                     NULL,
                                     sizeof (EFI_IFR_GUID_LABEL)
                                     );
  EndLabel->ExtendOpCode = EFI_IFR_EXTEND_OP_LABEL;
  EndLabel->Number       = LABEL_END;

  for (BmcChannel = 0; BmcChannel < BMC_MAX_CHANNEL; BmcChannel++) {
    ZeroMem (&BmcLanInfo, sizeof (BmcLanInfo));
    Status = IpmiGetBmcLanInfo (BmcChannel, &BmcLanInfo);
    if (EFI_ERROR (Status)
        || (BmcLanInfo.IpAddress.IpAddress[0] == 0))
    {
      continue;
    }

    UnicodeSPrint (
      StrBuf,
      sizeof (StrBuf),
      L"%d.%d.%d.%d",
      BmcLanInfo.IpAddress.IpAddress[0],
      BmcLanInfo.IpAddress.IpAddress[1],
      BmcLanInfo.IpAddress.IpAddress[2],
      BmcLanInfo.IpAddress.IpAddress[3]
      );

    HiiCreateTextOpCode (
      StartOpCodeHandle,
      STRING_TOKEN (STR_BMC_IP_ADDRESS_LABEL),
      STRING_TOKEN (STR_BMC_IP_ADDRESS_LABEL),
      HiiSetString (mHiiHandle, 0, StrBuf, NULL)
      );

    HiiUpdateForm (
      mHiiHandle,                 // HII handle
      &gBmcInfoScreenFormSetGuid, // Formset GUID
      MAIN_FORM_ID,               // Form ID
      StartOpCodeHandle,          // Label for where to insert opcodes
      EndOpCodeHandle             // Insert data
      );

    break;
  }

  return Status;
}

/**
  Print RTC time in human-readable format along with epoch value.

  This helper converts an EFI_TIME structure into epoch seconds and
  logs the decoded date/time and epoch value for debugging purposes.

  @param[in] Time    Pointer to EFI_TIME structure representing RTC time.

**/

STATIC
VOID
PrintRtcTime (
  IN EFI_TIME *Time
  )
{
  UINT32 Epoch;

  Epoch = (UINT32)EfiTimeToEpoch (Time);

  DEBUG ((DBG_TIME,
    "RTC Time: %04u-%02u-%02u %02u:%02u:%02u (epoch=%u)\n",
    Time->Year, Time->Month, Time->Day,
    Time->Hour, Time->Minute, Time->Second,
    Epoch));
}

/**
  Decode and print IPMI SEL time.

  This helper decodes the raw SEL timestamp returned by the BMC,
  converts it to EFI_TIME format, and logs both decoded time and
  raw epoch value. If the completion code is not normal, the
  function exits without decoding.

  @param[in] Resp    Pointer to IPMI_GET_SEL_TIME_RESPONSE structure.

**/

STATIC
VOID
PrintIpmiSelTimeDecoded (
  IN IPMI_GET_SEL_TIME_RESPONSE *Resp
  )
{
  EFI_TIME Time;

  DEBUG ((DBG_TIME,
    "Get SEL Time CC=0x%02x RawEpoch=%u\n",
    Resp->CompletionCode,
    Resp->Timestamp));

  if (Resp->CompletionCode != IPMI_COMP_CODE_NORMAL) {
    return;
  }

  EpochToEfiTime (Resp->Timestamp, &Time);

  DEBUG ((DBG_TIME,
    "SEL Time: %04u-%02u-%02u %02u:%02u:%02u (epoch=%u)\n",
    Time.Year, Time.Month, Time.Day,
    Time.Hour, Time.Minute, Time.Second,
    Resp->Timestamp));
}

/**
  Add an OEM System Event Log (SEL) entry for time synchronization.

  This function creates and submits a System Event Record (type 0x02)
  indicating a time synchronization operation between RTC and SEL.
  The entry records the epoch time and direction of synchronization.

  @param[in] Epoch       Epoch timestamp associated with the sync event.
  @param[in] Direction   Direction of time synchronization
                          (RTC to SEL or SEL to RTC).

  @retval EFI_SUCCESS        SEL entry added successfully.
  @retval EFI_DEVICE_ERROR  Failed to add SEL entry.

**/

STATIC
EFI_STATUS
AddTimeSyncSelEntry (
  IN UINT32 Epoch,
  IN SEL_SYNC_DIRECTION Direction
  )
{
  IPMI_ADD_SEL_ENTRY_REQUEST   Req;
  IPMI_ADD_SEL_ENTRY_RESPONSE  Resp;
  EFI_STATUS                   Status;

  ZeroMem (&Req, sizeof (Req));
  ZeroMem (&Resp, sizeof (Resp));

  //
  // Fill System Event Record (per your IPMI_SEL_EVENT_RECORD_DATA)
  //
  Req.RecordData.RecordId      = 0x0000;  // Ignored by BMC
  Req.RecordData.RecordType    = IPMI_SEL_SYSTEM_RECORD; // 0x02
  Req.RecordData.TimeStamp     = Epoch;
  Req.RecordData.GeneratorId   = IPMI_GENERATOR_ID (IPMI_SWID_BIOS_RANGE_START, 0);
  Req.RecordData.EvMRevision   = IPMI_EVM_REVISION;

  //
  // Match existing SEL format
  // 
  //
  Req.RecordData.SensorType    = IPMI_SENSOR_TYPE_EVENT_CODE_DISCRETE; // 0x6F
  Req.RecordData.SensorNumber  = 0xA8;

  //
  // EventDirType:
  //  Bit7 = Assertion (0)
  //  Bit6:0 = Sensor-specific (0x6F)
  //
  Req.RecordData.EventDirType  =
    (IPMI_SEL_EVENT_DIR_ASSERTION_EVENT << 7) |
    IPMI_SEL_EVENT_TYPE_SENSOR_SPECIFIC;

  //
  // OEM event payload
  //
  Req.RecordData.OEMEvData1 = 0xA1;                 // Time Sync event ID
  Req.RecordData.OEMEvData2 = (UINT8)Direction;     // 1=RTC to SEL, 2=SEL to RTC
  Req.RecordData.OEMEvData3 = 0x00;

  Status = IpmiAddSelEntry (&Req, &Resp);

  if (EFI_ERROR (Status) ||
      Resp.CompletionCode != IPMI_COMP_CODE_NORMAL) {
    DEBUG ((DEBUG_WARN,
      "AddTimeSyncSelEntry failed: %r CC=0x%02x\n",
      Status, Resp.CompletionCode));
    return EFI_DEVICE_ERROR;
  }

  DEBUG ((DEBUG_INFO,
    "Time Sync SEL added: RecordId=0x%04x Epoch=%u\n",
    Resp.RecordId, Epoch));

  return EFI_SUCCESS;
}

/**
  Synchronize RTC time with IPMI SEL time.

  This function compares RTC time and BMC SEL time, selects the most
  recent valid timestamp, and updates the SEL time accordingly.
  The synchronization direction is returned to the caller.

  Retry logic is applied when setting SEL time to ensure reliability.

  @param[out] SyncDir   Pointer to receive the synchronization direction.

  @retval EFI_SUCCESS       Time synchronization completed successfully.
  @retval EFI_NOT_READY     Both RTC and SEL times are invalid.
  @retval EFI_DEVICE_ERROR  Failed to get or set SEL time.

**/

STATIC
EFI_STATUS
SyncRtcToIpmiSelTime (
  OUT SEL_SYNC_DIRECTION *SyncDir
  )
{
  EFI_STATUS                 Status;
  EFI_TIME                   Time;
  UINT32                     RtcEpoch = 0;
  UINT32                     SelEpoch = 0;
  UINT32                     NewEpoch;
  IPMI_GET_SEL_TIME_RESPONSE SelResp;
  IPMI_SET_SEL_TIME_REQUEST  SetReq;
  UINT8                      CompletionCode;
  BOOLEAN                    SetSelSuccess = FALSE;

  *SyncDir = SelSyncNone;

  DEBUG ((DBG_TIME, "==== SyncRtcToIpmiSelTime ENTRY ====\n"));

  /* ---------------- RTC (read only) ---------------- */
  Status = gRT->GetTime (&Time, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DBG_WARN, "GetTime failed: %r\n", Status));
  } else {
    RtcEpoch = (UINT32)EfiTimeToEpoch (&Time);
    PrintRtcTime (&Time);
  }

  /* ---------------- SEL (before) ---------------- */
  Status = IpmiGetSelTime (&SelResp);
  if (EFI_ERROR (Status) ||
      SelResp.CompletionCode != IPMI_COMP_CODE_NORMAL) {
    DEBUG ((DBG_ERR, "IpmiGetSelTime failed: %r CC=0x%02x\n",
            Status, SelResp.CompletionCode));
    return EFI_DEVICE_ERROR;
  }

  DEBUG ((DBG_TIME, "SEL time BEFORE SetSelTime\n"));
  PrintIpmiSelTimeDecoded (&SelResp);
  SelEpoch = SelResp.Timestamp;

  /* ---------------- Validate ---------------- */
  if (RtcEpoch < MIN_VALID_EPOCH &&
      SelEpoch < MIN_VALID_EPOCH) {

    DEBUG ((DBG_WARN,
      "Both RTC (%u) and SEL (%u) invalid (< MIN_VALID_EPOCH)\n",
      RtcEpoch, SelEpoch));

    return EFI_NOT_READY;
  }

  /* ---------------- Choose latest ---------------- */
  if (RtcEpoch >= SelEpoch && RtcEpoch >= MIN_VALID_EPOCH) {
    NewEpoch = RtcEpoch;
    *SyncDir = SelSyncRtcToSel;

    DEBUG ((DBG_TIME,
      "RTC is newer setting SEL to RTC epoch=%u\n",
      NewEpoch));
  } else {
    NewEpoch = SelEpoch;
    *SyncDir = SelSyncSelToRtc;

    DEBUG ((DBG_TIME,
      "SEL is newer re-setting SEL to epoch=%u\n",
      NewEpoch));
  }

  /* ---------------- Set SEL Time ---------------- */
  DEBUG ((DBG_TIME,
    "Calling IpmiSetSelTime(epoch=%u)\n",
    NewEpoch));

  ZeroMem (&SetReq, sizeof (SetReq));
  SetReq.Timestamp = NewEpoch;

  for (UINTN Retry = 0; Retry < IPMI_SET_SEL_RETRY_MAX; Retry++) {
    Status = IpmiSetSelTime (&SetReq, &CompletionCode);

    DEBUG ((DBG_TIME,
      "SetSelTime retry=%u Status=%r CC=0x%02x\n",
      Retry, Status, CompletionCode));

    if (!EFI_ERROR (Status) &&
        CompletionCode == IPMI_COMP_CODE_NORMAL) {
      SetSelSuccess = TRUE;
      DEBUG ((DBG_TIME, "SetSelTime SUCCESS\n"));
      break;
    }

    MicroSecondDelay (1000000);
  }

  if (!SetSelSuccess) {
    DEBUG ((DBG_ERR,
      "SetSelTime FAILED after %u retries\n",
      IPMI_SET_SEL_RETRY_MAX));
    return EFI_DEVICE_ERROR;
  }

  /* ---------------- SEL (after) ---------------- */
  Status = IpmiGetSelTime (&SelResp);
  if (!EFI_ERROR (Status)) {
    DEBUG ((DBG_TIME, "SEL time AFTER SetSelTime\n"));
    PrintIpmiSelTimeDecoded (&SelResp);
  } else {
    DEBUG ((DBG_WARN,
      "IpmiGetSelTime (after) failed: %r\n",
      Status));
  }

  DEBUG ((DBG_TIME,
    "==== SyncRtcToIpmiSelTime EXIT dir=%u ====\n",
    *SyncDir));

  return EFI_SUCCESS;
}

/**
  The user Entry Point for the BMC Screen driver.

  @param[in] ImageHandle    The firmware allocated handle for the EFI image.
  @param[in] SystemTable    A pointer to the EFI System Table.

  @retval EFI_SUCCESS       The entry point is executed successfully.
  @retval Other             Some error occurs when executing this entry point.

**/
EFI_STATUS
EFIAPI
BmcInfoScreenEntry (
  IN EFI_HANDLE       ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{
  EFI_STATUS Status;
  EFI_HANDLE DriverHandle;

  Status = EFI_SUCCESS;
  DriverHandle = NULL;

 
  IPMI_GET_SEL_TIME_RESPONSE  SelResp;
  SEL_SYNC_DIRECTION         SyncDir;
  

  //
  // Publish our HII data
  //
  mHiiHandle = HiiAddPackages (
                 &gBmcInfoScreenFormSetGuid,
                 DriverHandle,
                 BmcInfoScreenDxeStrings,
                 VfrBin,
                 NULL
                 );

  ASSERT (mHiiHandle != NULL);

  Status = UpdateMainForm ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a Failed to update BMC Info Screen\n", __FUNCTION__));
  }

  //
  // ===== BEFORE SYNC: Read SEL time =====
  //
  if (!EFI_ERROR (IpmiGetSelTime (&SelResp))) {
    DEBUG ((DEBUG_INFO, "SEL time BEFORE sync\n"));
    PrintIpmiSelTimeDecoded (&SelResp);
  }

  Status = SyncRtcToIpmiSelTime (&SyncDir);
  //
  // ===== AFTER SYNC: Read SEL time =====
  //
  if (!EFI_ERROR (IpmiGetSelTime (&SelResp))) {
    DEBUG ((DEBUG_INFO, "SEL time AFTER sync\n"));
    PrintIpmiSelTimeDecoded (&SelResp);
  }

  if (!EFI_ERROR (Status) &&
      SyncDir != SelSyncNone &&
      !mTimeSyncSelLogged) {

    AddTimeSyncSelEntry (SelResp.Timestamp, SyncDir);
    mTimeSyncSelLogged = TRUE;
  }

  return Status;
}
