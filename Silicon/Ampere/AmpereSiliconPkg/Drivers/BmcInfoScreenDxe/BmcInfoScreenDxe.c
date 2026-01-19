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
#define MIN_VALID_EPOCH  1577836800  

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
  Decode and log the IPMI SEL time returned by the BMC.

  This function validates the IPMI completion code, converts the SEL
  timestamp from epoch seconds to EFI_TIME format, and prints both the
  raw epoch value and the decoded UTC time for debugging purposes.

  @param[in] Resp           Pointer to the IPMI Get SEL Time response
                            structure.

**/

STATIC
VOID
PrintIpmiSelTimeDecoded (
  IN IPMI_GET_SEL_TIME_RESPONSE *Resp
  )
{
  EFI_TIME  Time;
  UINT32    EpochSeconds;

  if (Resp->CompletionCode != IPMI_COMP_CODE_NORMAL) {
    DEBUG ((DEBUG_ERROR,
      "ADLINK1 Get SEL Time completion code = 0x%02x\n",
      Resp->CompletionCode));
    return;
  }

  EpochSeconds = Resp->Timestamp;
  EpochToEfiTime (EpochSeconds, &Time);

  DEBUG ((DEBUG_INFO,
    "ADLINK1 IPMI SEL EpochSeconds = %u (0x%08x)\n",
    EpochSeconds, EpochSeconds));

  DEBUG ((DEBUG_INFO,
    "ADLINK1 IPMI SEL Time decoded: %04u-%02u-%02u %02u:%02u:%02u UTC\n",
    Time.Year,
    Time.Month,
    Time.Day,
    Time.Hour,
    Time.Minute,
    Time.Second));
}

/**
  Synchronize the IPMI SEL time using the platform RTC.

  This function reads the current RTC time, validates it, compares it
  against the existing IPMI SEL time, and updates the SEL time only if
  the RTC time is newer. Time rollback and redundant updates are
  explicitly avoided. The SEL update is retried to tolerate transient
  BMC failures.

  @retval EFI_SUCCESS       SEL time is already up to date or updated
                            successfully.
  @retval EFI_NOT_READY     RTC time is invalid or not initialized.
  @retval EFI_DEVICE_ERROR  Failed to read or update IPMI SEL time.
  @retval Other             Error returned from RTC access.

**/

STATIC
EFI_STATUS
SyncRtcToIpmiSelTime (
  VOID
  )
{
  EFI_STATUS                Status;
  EFI_TIME                  Time;
  UINT32                    RtcEpoch;
  UINT32                    SelEpoch;
  IPMI_GET_SEL_TIME_RESPONSE SelResp;
  IPMI_SET_SEL_TIME_REQUEST  SetReq;
  UINT8                     CompletionCode;

  //
  // 1) Read RTC
  //
  Status = gRT->GetTime (&Time, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "GetTime failed: %r\n", Status));
    return Status;
  }

  RtcEpoch = (UINT32)EfiTimeToEpoch (&Time);

  //
  // 2) Validate RTC
  //
  if (RtcEpoch < MIN_VALID_EPOCH) {
    DEBUG ((DEBUG_WARN,
      "RTC not ready, epoch=%u skipping SEL sync\n",
      RtcEpoch));
    return EFI_NOT_READY;
  }

  //
  // 3) Read current SEL time
  //
  Status = IpmiGetSelTime (&SelResp);
  if (EFI_ERROR (Status) || SelResp.CompletionCode != IPMI_COMP_CODE_NORMAL) {
    DEBUG ((DEBUG_ERROR, "Get SEL Time failed\n"));
    return EFI_DEVICE_ERROR;
  }

  SelEpoch = SelResp.Timestamp;

  //
  // 4) Prevent backward or redundant updates
  //
  if (RtcEpoch <= SelEpoch) {
    DEBUG ((DEBUG_INFO,
      "SEL already up to date (SEL=%u RTC=%u)\n",
      SelEpoch, RtcEpoch));
    return EFI_SUCCESS;
  }

  //
  // 5) Write RTC SEL
  //
  SetReq.Timestamp = RtcEpoch;

  for (UINTN Retry = 0; Retry < IPMI_SET_SEL_RETRY_MAX; Retry++) {
    Status = IpmiSetSelTime (&SetReq, &CompletionCode);
    if (!EFI_ERROR (Status) && CompletionCode == IPMI_COMP_CODE_NORMAL) {
      DEBUG ((DEBUG_INFO,
        "SEL updated from RTC (epoch=%u)\n",
        RtcEpoch));
      return EFI_SUCCESS;
    }

    DEBUG ((DEBUG_WARN,
      "IpmiSetSelTime retry %u failed: %r CC=0x%02x\n",
      Retry + 1, Status, CompletionCode));

    MicroSecondDelay (1000000); // 1 second
  }

  return EFI_DEVICE_ERROR;
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
  Status = IpmiGetSelTime (&SelResp);
  if (!EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "IPMI SEL time BEFORE sync:\n"));
    PrintIpmiSelTimeDecoded (&SelResp);
  }
  
  //
  // ===== Sync RTC -> IPMI SEL =====
  //
  Status = SyncRtcToIpmiSelTime ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN,
      "%a: RTC to IPMI SEL sync failed: %r\n",
      __FUNCTION__, Status));
  }
  

  //
  // ===== AFTER SYNC: Read SEL time =====
  //
  Status = IpmiGetSelTime (&SelResp);
  if (!EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "IPMI SEL time AFTER sync:\n"));
    PrintIpmiSelTimeDecoded (&SelResp);
  }


  return Status;
}
