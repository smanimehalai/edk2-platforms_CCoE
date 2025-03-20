/** @file
*  OemMiscLib.c
*
*  Copyright (c) 2022, Ampere Computing LLC. All rights reserved.
*  Copyright (c) 2021, NUVIA Inc. All rights reserved.
*  Copyright (c) 2018, Hisilicon Limited. All rights reserved.
*  Copyright (c) 2018, Linaro Limited. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <CpuConfigNVDataStruc.h>
#include <NVParamDef.h>
#include <PiPei.h>
#include <Uefi.h>
#include <Library/AmpereCpuLib.h>
#include <Library/ArmLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HiiLib.h>
#include <Library/HobLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/OemMiscLib.h>
#include <Library/PrintLib.h>
#include <Guid/PlatformInfoHob.h>
#include <IndustryStandard/ArmCache.h>

#include "IpmiFruInfo.h"

#define CACHE_SOCKETED_SHIFT          3
#define CACHE_LOCATION_SHIFT          5
#define CACHE_ENABLED_SHIFT           7
#define CACHE_OPERATION_MODE_SHIFT    8

#define PROCESSOR_VERSION_ALTRA       L"Ampere(R) Altra(R) Processor"
#define PROCESSOR_VERSION_ALTRA_MAX   L"Ampere(R) Altra(R) Max Processor"

#define VOLTAGE_SCALE_FACTOR          1000

#define SCP_VERSION_STRING_MAX_LENGTH 32

typedef enum {
  CacheModeWriteThrough = 0,  ///< Cache is write-through
  CacheModeWriteBack,         ///< Cache is write-back
  CacheModeVariesWithAddress, ///< Cache mode varies by address
  CacheModeUnknown,           ///< Cache mode is unknown
  CacheModeMax
} CACHE_OPERATION_MODE;

typedef enum {
  CacheLocationInternal = 0, ///< Cache is internal to the processor
  CacheLocationExternal,     ///< Cache is external to the processor
  CacheLocationReserved,     ///< Reserved
  CacheLocationUnknown,      ///< Cache location is unknown
  CacheLocationMax
} CACHE_LOCATION;

/**
  Gets the Cache Operational Mode via CCSIDR register.

  @param    CacheLevel     The cache level to get information for.
  @param    DataCache      Whether the cache is a data cache.
  @param    UnifiedCache   Whether the cache is a unified cache.

  @return   UINT8          Type of Cache Operational Mode.
**/
UINT8
GetCacheOperationalMode (
  IN UINT8   CacheLevel,
  IN BOOLEAN DataCache,
  IN BOOLEAN UnifiedCache
  )
{
  CSSELR_DATA  Csselr;
  UINT64       CcsidrValue;
  BOOLEAN      SupportWB;
  BOOLEAN      SupportWT;

  Csselr.Data = 0;
  Csselr.Bits.Level = CacheLevel - 1;
  Csselr.Bits.InD   = (!DataCache && !UnifiedCache);

  CcsidrValue = ReadCCSIDR (Csselr.Data);
  SupportWT = (CcsidrValue & BIT31) ? TRUE : FALSE;
  SupportWB = (CcsidrValue & BIT30) ? TRUE : FALSE;
  if (SupportWT && SupportWB) {
    return CacheModeVariesWithAddress;
  }

  if (SupportWT) {
    return CacheModeWriteThrough;
  }

  return CacheModeWriteBack;
}

/**
  Gets the CPU frequency of the specified processor.

  @param    ProcessorIndex   Index of the processor to get the frequency for.

  @return   UINTN            CPU frequency in Hz.
**/
UINTN
EFIAPI
OemGetCpuFreq (
  IN UINT8 ProcessorIndex
  )
{
  return CpuGetCurrentFreq ();
}

/**
  Gets information about the specified processor and stores it in
  the structures provided.

  @param    ProcessorIndex             Index of the processor to get the information for.
  @param    ProcessorStatus            Processor status.
  @param    ProcessorCharacteristics   Processor characteritics.
  @param    MiscProcessorData          Miscellaneous processor information.

  @return   BOOLEAN                    TRUE on success
                                       FALSE on failure.
**/
BOOLEAN
EFIAPI
OemGetProcessorInformation (
  IN UINTN                              ProcessorIndex,
  IN OUT PROCESSOR_STATUS_DATA          *ProcessorStatus,
  IN OUT PROCESSOR_CHARACTERISTIC_FLAGS *ProcessorCharacteristics,
  IN OUT OEM_MISC_PROCESSOR_DATA        *MiscProcessorData
  )
{
  if (!OemIsProcessorPresent (ProcessorIndex)) {
    ProcessorStatus->Data = 0;
    return FALSE;
  }

  /* Processor Status */
  ProcessorStatus->Bits.SocketPopulated = OemIsProcessorPresent (ProcessorIndex);
  ProcessorStatus->Bits.CpuStatus       = IsCpuEnabled (ProcessorIndex);

  /* Processor Charateristics */
  ProcessorCharacteristics->ProcessorReserved1              = 0;
  ProcessorCharacteristics->ProcessorUnknown                = 0;
  ProcessorCharacteristics->Processor64BitCapable           = 1;
  ProcessorCharacteristics->ProcessorMultiCore              = 1;
  ProcessorCharacteristics->ProcessorHardwareThread         = 0;
  ProcessorCharacteristics->ProcessorExecuteProtection      = 1;
  ProcessorCharacteristics->ProcessorEnhancedVirtualization = 1;
  ProcessorCharacteristics->ProcessorPowerPerformanceCtrl   = 1;
  ProcessorCharacteristics->Processor128BitCapable          = 0;
  ProcessorCharacteristics->ProcessorReserved2              = 0;

  /* Oem Misc Procesor Data */
  MiscProcessorData->MaxSpeed     = (UINT16)CpuGetMaxFreq (ProcessorIndex);
  MiscProcessorData->CoresEnabled = GetNumberOfActiveCoresPerSocket (ProcessorIndex);
  MiscProcessorData->CoreCount    = GetMaximumNumberOfCores ();
  MiscProcessorData->ThreadCount  = GetMaximumNumberOfCores ();
  MiscProcessorData->CurrentSpeed = CpuGetCurrentFreq ();
  // BIT7: the remaining seven bits of the field are set to
  // contain the processor’s current voltage times 10.
  MiscProcessorData->Voltage      = (UINT8)(BIT7 | ((CpuGetVoltage (ProcessorIndex) / VOLTAGE_SCALE_FACTOR) * 10));

  return TRUE;
}

/**
  Gets information about the cache at the specified cache level.

  @param    ProcessorIndex     The processor to get information for.
  @param    CacheLevel         The cache level to get information for.
  @param    DataCache          Whether the cache is a data cache.
  @param    UnifiedCache       Whether the cache is a unified cache.
  @param    SmbiosCacheTable   The SMBIOS Type7 cache information structure.

  @return   BOOLEAN            TRUE on success.
                               FALSE on failure.
**/
BOOLEAN
EFIAPI
OemGetCacheInformation (
  IN UINT8                  ProcessorIndex,
  IN UINT8                  CacheLevel,
  IN BOOLEAN                DataCache,
  IN BOOLEAN                UnifiedCache,
  IN OUT SMBIOS_TABLE_TYPE7 *SmbiosCacheTable
  )
{
  UINT16       CacheSize16;
  UINT32       CacheSize32;
  UINT64       CacheSize64;
  UINT8        Granularity32;

  if (!OemIsProcessorPresent (ProcessorIndex)) {
    return FALSE;
  }

  /* SRAM Information */
  SmbiosCacheTable->SupportedSRAMType.Unknown     = 0;
  SmbiosCacheTable->SupportedSRAMType.Synchronous = 1;
  SmbiosCacheTable->CurrentSRAMType.Unknown       = 0;
  SmbiosCacheTable->CurrentSRAMType.Synchronous   = 1;

  /* Error Correction Type */
  if (CacheLevel == CpuCacheL1) {
    SmbiosCacheTable->ErrorCorrectionType = CacheErrorParity;
  } else {
    SmbiosCacheTable->ErrorCorrectionType = CacheErrorSingleBit;
  }

  /* Cache Configuration */
  SmbiosCacheTable->CacheConfiguration = 0;
  SmbiosCacheTable->CacheConfiguration |= (UINT16)(GetCacheOperationalMode (
                                                     CacheLevel,
                                                     DataCache,
                                                     UnifiedCache
                                                     ) << CACHE_OPERATION_MODE_SHIFT);
  SmbiosCacheTable->CacheConfiguration |= (UINT16)(CacheLocationInternal << CACHE_LOCATION_SHIFT);
  SmbiosCacheTable->CacheConfiguration |= (UINT16)(1 << CACHE_ENABLED_SHIFT);
  SmbiosCacheTable->CacheConfiguration |= (UINT16)(CacheLevel - 1);

  /* Cache Size */
  CacheSize16 = SmbiosCacheTable->MaximumCacheSize;
  CacheSize32 = SmbiosCacheTable->MaximumCacheSize2;

  Granularity32 = CacheSize32 >> 31;
  if (Granularity32 == 0) {
    CacheSize64 = CacheSize32;
  } else {
    CacheSize64 = (CacheSize32 & (~BIT31)) * 64;
  }

  CacheSize64 *= GetNumberOfActiveCoresPerSocket (ProcessorIndex);
  if (CacheSize64 < MAX_INT16) {
    CacheSize16 = CacheSize64;
    CacheSize32 = CacheSize16;
  } else if ((CacheSize64 / 64) < MAX_INT16) {
    CacheSize16 = (UINT16)(BIT15 | (CacheSize64 / 64));
    CacheSize32 = (UINT32)(BIT31 | (CacheSize64 / 64));
  } else {
    if ((CacheSize64 / 1024) <= 2047) {
      CacheSize32 = CacheSize64;
    } else {
      CacheSize32 = (UINT32)(BIT31 | (CacheSize64 / 64));
    }

    CacheSize16 = 0xFFFF;
  }

  SmbiosCacheTable->MaximumCacheSize  = CacheSize16;
  SmbiosCacheTable->InstalledSize     = CacheSize16;
  SmbiosCacheTable->MaximumCacheSize2 = CacheSize32;
  SmbiosCacheTable->InstalledSize2    = CacheSize32;

  return TRUE;
}

/**
  Gets the maximum number of processors supported by the platform.

  @return   UINT8   The maximum number of processors.
**/
UINT8
EFIAPI
OemGetMaxProcessors (
  VOID
  )
{
  return GetNumberOfSupportedSockets ();
}

/**
  Gets the type of chassis for the system.

  @return   MISC_CHASSIS_TYPE   The type of the chassis.
**/
MISC_CHASSIS_TYPE
EFIAPI
OemGetChassisType (
  VOID
  )
{
  return MiscChassisTypeRackMountChassis;
}

/**
  Returns whether the specified processor is present or not.

  @param    ProcessIndex   The processor index to check.

  @return   BOOLEAN        TRUE is the processor is present.
                           FALSE is the processor is not present.
**/
BOOLEAN
EFIAPI
OemIsProcessorPresent (
  IN UINTN ProcessorIndex
  )
{
  if (ProcessorIndex == 0) {
    return TRUE;
  }

  return IsSlaveSocketActive ();
}

/**
  Update the firmware version in SMBIOS Type 0.
  This is the combination of UEFI and Ampere system firmware version.

**/
VOID
UpdateFirmwareVersionString (
  OUT CHAR16  *Version
  )
{
  UINT8   UnicodeStrLen;
  UINT8   FirmwareVersionStrLen;
  UINT8   FirmwareVersionStrSize;
  UINT8   *ScpVersion;
  UINT8   *ScpBuild;
  CHAR16  UnicodeStr[SMBIOS_STRING_MAX_LENGTH * sizeof (CHAR16)];
  CHAR16  *FirmwareVersionPcdPtr;

  FirmwareVersionStrLen = 0;
  ZeroMem (UnicodeStr, sizeof (UnicodeStr));
  FirmwareVersionPcdPtr  = (CHAR16 *)FixedPcdGetPtr (PcdFirmwareVersionString);
  FirmwareVersionStrSize = SMBIOS_STRING_MAX_LENGTH * sizeof (CHAR16);

  //
  // Format of PcdFirmwareVersionString is
  // "(MAJOR_VER).(MINOR_VER).(BUILD) Build YYYY.MM.DD", we only need
  // "(MAJOR_VER).(MINOR_VER).(BUILD)" showed in BIOS version. Using
  // space character to determine this string. Another case uses null
  // character to end while loop.
  //
  while (*FirmwareVersionPcdPtr != ' ' && *FirmwareVersionPcdPtr != '\0') {
    FirmwareVersionStrLen++;
    FirmwareVersionPcdPtr++;
  }

  FirmwareVersionPcdPtr = (CHAR16 *)FixedPcdGetPtr (PcdFirmwareVersionString);
  UnicodeStrLen         = FirmwareVersionStrLen * sizeof (CHAR16);
  CopyMem (UnicodeStr, FirmwareVersionPcdPtr, UnicodeStrLen);

  GetScpVersion (&ScpVersion);
  GetScpBuild (&ScpBuild);
  if ((ScpVersion == NULL) || (ScpBuild == NULL)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a:%d: Fail to get SMpro/PMpro information\n",
      __FUNCTION__,
      __LINE__
      ));
    UnicodeSPrint (
      Version,
      FirmwareVersionStrSize,
      L"TianoCore %.*s (SYS: 0.00.00000000)",
      FirmwareVersionStrLen,
      (UINT16 *)UnicodeStr
      );
  } else {
    UnicodeSPrint (
      Version,
      FirmwareVersionStrSize,
      L"TianoCore %.*s (SYS: %a.%a)",
      FirmwareVersionStrLen,
      (UINT16 *)UnicodeStr,
      ScpVersion,
      ScpBuild
      );
  }
}
/**
  Updates the HII string for the specified field.

  @param   HiiHandle       The HII handle.
  @param   TokenToUpdate   The string to update.
  @param   Field           The field to get information about.
**/
VOID
EFIAPI
OemUpdateSmbiosInfo (
  IN EFI_HII_HANDLE                   HiiHandle,
  IN EFI_STRING_ID                    TokenToUpdate,
  IN OEM_MISC_SMBIOS_HII_STRING_FIELD Field
  )
{
  EFI_STRING UnicodeString;
  UINT8      StringLength;
  CHAR8      *AsciiString;
  UINT32     *Ecid;

  StringLength = SMBIOS_STRING_MAX_LENGTH * sizeof (CHAR16);
  UnicodeString = AllocatePool (StringLength);
  if (UnicodeString == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a:%d: There is not enough memory remaining to satisfy the request\n",
      __FUNCTION__,
      __LINE__));

    goto Exit;
  }

  switch (Field) {
    case ProductNameType01:
      AsciiString = IpmiFruInfoGet (FruProductName);
      if (AsciiString != NULL) {
        StringLength = AsciiStrLen (AsciiString) + 1;
        AsciiStrToUnicodeStrS (AsciiString, UnicodeString, StringLength);
      }

      break;

    case SystemManufacturerType01:
      AsciiString = IpmiFruInfoGet (FruProductManufacturerName);
      if (AsciiString != NULL) {
        StringLength = AsciiStrLen (AsciiString) + 1;
        AsciiStrToUnicodeStrS (AsciiString, UnicodeString, StringLength);
      }

      break;

    case VersionType01:
      AsciiString = IpmiFruInfoGet (FruProductVersion);
      if (AsciiString != NULL) {
        StringLength = AsciiStrLen (AsciiString) + 1;
        AsciiStrToUnicodeStrS (AsciiString, UnicodeString, StringLength);
      }

      break;

    case SerialNumType01:
      AsciiString = IpmiFruInfoGet (FruProductSerialNumber);
      if (AsciiString != NULL) {
        StringLength = AsciiStrLen (AsciiString) + 1;
        AsciiStrToUnicodeStrS (AsciiString, UnicodeString, StringLength);
      }

      break;

    case SkuNumberType01:
      AsciiString = IpmiFruInfoGet (FruProductExtra);
      if (AsciiString != NULL) {
        StringLength = AsciiStrLen (AsciiString) + 1;
        AsciiStrToUnicodeStrS (AsciiString, UnicodeString, StringLength);
      }

      break;

    case FamilyType01:
      UnicodeSPrint (
        UnicodeString,
        StringLength,
        IsAc01Processor () ? L"Altra\0" : L"Altra Max\0"
        );

      break;

    case ProductNameType02:
      AsciiString = IpmiFruInfoGet (FruBoardProductName);
      if (AsciiString != NULL) {
        StringLength = AsciiStrLen (AsciiString) + 1;
        AsciiStrToUnicodeStrS (AsciiString, UnicodeString, StringLength);
      }

      break;

    case AssetTagType02:
      UnicodeSPrint (
        UnicodeString,
        StringLength,
        L"Not Set"
        );

      break;

    case VersionType02:
      AsciiString = IpmiFruInfoGet (FruBoardPartNumber);
      if (AsciiString != NULL) {
        StringLength = AsciiStrLen (AsciiString) + 1;
        AsciiStrToUnicodeStrS (AsciiString, UnicodeString, StringLength);
      }

      break;

    case SerialNumberType02:
      AsciiString = IpmiFruInfoGet (FruBoardSerialNumber);
      if (AsciiString != NULL) {
        StringLength = AsciiStrLen (AsciiString) + 1;
        AsciiStrToUnicodeStrS (AsciiString, UnicodeString, StringLength);
      }

      break;

    case BoardManufacturerType02:
      AsciiString = IpmiFruInfoGet (FruBoardManufacturerName);
      if (AsciiString != NULL) {
        StringLength = AsciiStrLen (AsciiString) + 1;
        AsciiStrToUnicodeStrS (AsciiString, UnicodeString, StringLength);
      }

      break;

    case ChassisLocationType02:
      UnicodeSPrint (
        UnicodeString,
        StringLength,
        L"Base of Chassis"
        );

      break;

    case SerialNumberType03:
      AsciiString = IpmiFruInfoGet (FruChassisSerialNumber);
      if (AsciiString != NULL) {
        StringLength = AsciiStrLen (AsciiString) + 1;
        AsciiStrToUnicodeStrS (AsciiString, UnicodeString, StringLength);
      }

      break;

    case VersionType03:
      AsciiString = IpmiFruInfoGet (FruChassisPartNumber);
      if (AsciiString != NULL) {
        StringLength = AsciiStrLen (AsciiString) + 1;
        AsciiStrToUnicodeStrS (AsciiString, UnicodeString, StringLength);
      }

      break;

    case ManufacturerType03:
      AsciiString = IpmiFruInfoGet (FruBoardManufacturerName);
      if (AsciiString != NULL) {
        StringLength = AsciiStrLen (AsciiString) + 1;
        AsciiStrToUnicodeStrS (AsciiString, UnicodeString, StringLength);
      }

      break;

    case AssetTagType03:
      AsciiString = IpmiFruInfoGet (FruProductAssetTag);
      if (AsciiString != NULL) {
        StringLength = AsciiStrLen (AsciiString) + 1;
        AsciiStrToUnicodeStrS (AsciiString, UnicodeString, StringLength);
      }

      break;

    case SkuNumberType03:
      AsciiString = IpmiFruInfoGet (FruChassisExtra);
      if (AsciiString != NULL) {
        StringLength = AsciiStrLen (AsciiString) + 1;
        AsciiStrToUnicodeStrS (AsciiString, UnicodeString, StringLength);
      }

      break;

    case ProcessorVersionType04:
      if (IsAc01Processor ()){
        UnicodeSPrint (
          UnicodeString,
          StringLength,
          PROCESSOR_VERSION_ALTRA
        );
      } else {
        UnicodeSPrint (
          UnicodeString,
          StringLength,
          PROCESSOR_VERSION_ALTRA_MAX
          );
      }

      break;

    //
    // FIXME: Information of Serial number and Part number should be
    // read from respective socket (S0 and S1).
    // To do that, "ArmPkg/Include/Library/OemMiscLib.h" file need to
    // add the input parameter (ProcessorIndex) for function "OemUpdateSmbiosInfo ()"
    // and minor update at "ArmPkg/Universal/Smbios/ProcessorSubClassDxe/ProcessorSubClass.c".
    //
    case ProcessorSerialNumType04:
      CpuGetEcid (0, &Ecid);
      UnicodeSPrint (
        UnicodeString,
        StringLength,
        L"%08X%08X%08X%08X",
        Ecid[0],
        Ecid[1],
        Ecid[2],
        Ecid[3]
        );

      break;

    case ProcessorPartNumType04:
      if (IsAc01Processor ()) {
        UnicodeSPrint (
          UnicodeString,
          StringLength,
          L"Q%02d-%02X",
          GetSkuMaxCore (0),
          GetSkuMaxTurbo (0)
          );
      } else {
        UnicodeSPrint (
          UnicodeString,
          StringLength,
          L"M%02d-%02X",
          GetSkuMaxCore (0),
          GetSkuMaxTurbo (0)
          );
      }

      break;

    case BiosVersionType00:
      UpdateFirmwareVersionString (UnicodeString);
      break;

    default:
      UnicodeSPrint (
        UnicodeString,
        StringLength,
        L"Not Specified"
        );
  }

  // Update string value for respective token.
  HiiSetString (HiiHandle, TokenToUpdate, UnicodeString, NULL);

Exit:
  FreePool (UnicodeString);
}

/**
  Fetches the Type 32 boot information status.

  @return   MISC_BOOT_INFORMATION_STATUS_DATA_TYPE   Boot status.
**/
MISC_BOOT_INFORMATION_STATUS_DATA_TYPE
EFIAPI
OemGetBootStatus (
  VOID
  )
{
  return BootInformationStatusNoError;
}

/**
  Fetches the chassis status when it was last booted.

  @return   MISC_CHASSIS_STATE   Chassis status.
**/
MISC_CHASSIS_STATE
EFIAPI
OemGetChassisBootupState (
  VOID
  )
{
  return ChassisStateSafe;
}

/**
  Fetches the chassis power supply/supplies status when last booted.

  @return   MISC_CHASSIS_STATE   Chassis power supply/supplies status.
**/
MISC_CHASSIS_STATE
EFIAPI
OemGetChassisPowerSupplyState (
  VOID
  )
{
  return ChassisStateSafe;
}

/**
  Fetches the chassis thermal status when last booted.

  @return   MISC_CHASSIS_STATE   Chassis thermal status.
**/
MISC_CHASSIS_STATE
EFIAPI
OemGetChassisThermalState (
  VOID
  )
{
  return ChassisStateSafe;
}

/**
  Fetches the chassis security status when last booted.

  @return   MISC_CHASSIS_SECURITY_STATE   Chassis security status.
**/
MISC_CHASSIS_SECURITY_STATE
EFIAPI
OemGetChassisSecurityStatus (
  VOID
  )
{
  return ChassisSecurityStatusNone;
}

/**
  Fetches the chassis height in RMUs (Rack Mount Units).

  @return    UINT8   The height of the chassis.
**/
UINT8
EFIAPI
OemGetChassisHeight (
  VOID
  )
{
  return 2;
}

/**
  Fetches the number of power cords.

  @return    UINT8   The number of power cords.
**/
UINT8
EFIAPI
OemGetChassisNumPowerCords (
  VOID
  )
{
  return 2;
}

/** Fetches the BIOS release.

  @return The BIOS release.
**/
UINT16
EFIAPI
OemGetBiosRelease (
  VOID
  )
{
  UINT16 BiosRelease;

  BiosRelease = (UINT16)(((PcdGet8 (PcdSmbiosTables0MajorVersion)) << 8)
                         | PcdGet8 (PcdSmbiosTables0MinorVersion));

  return BiosRelease;
}

/**
  Fetches the embedded controller firmware release.

  @return   UINT16   The embedded controller firmware release.
**/
UINT16
EFIAPI
OemGetEmbeddedControllerFirmwareRelease (
  VOID
  )
{
  CHAR8  AsciiScpVer[SCP_VERSION_STRING_MAX_LENGTH];
  UINT8  *ScpVer = NULL;
  UINT8  Index;
  UINT16 FirmwareRelease;

  GetScpVersion (&ScpVer);
  if (ScpVer == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a:%d: Fail to get SMpro/PMpro information\n",
      __FUNCTION__,
      __LINE__));

      return 0xFFFF;
  }

  CopyMem ((VOID *)AsciiScpVer, (VOID *)ScpVer, AsciiStrLen ((CHAR8 *)ScpVer));
  /* The AsciiVersion is formated as "major.minor" */
  for (Index = 0; Index < (UINTN)AsciiStrLen (AsciiScpVer); Index++) {
    if (AsciiScpVer[Index] == '.') {
      AsciiScpVer[Index] = '\0';
      break;
    }
  }

  FirmwareRelease = ((UINT8)AsciiStrDecimalToUintn (AsciiScpVer) << 8)
                    + (UINT8)AsciiStrDecimalToUintn (AsciiScpVer + Index + 1);

  return FirmwareRelease;
}

VOID
ConvertIpmiGuidToSmbiosGuid (
  IN OUT UINT8 *SmbiosGuid,
  IN     UINT8 *IpmiGuid
  )
{
  UINT8 Index;

  //
  // Node and clock seq field within the GUID
  // are stored most-significant byte first in
  // SMBIOS spec but LSB first in IPMI spec
  // ->change its offset and byte-order
  //
  for (Index = 0; Index < 8; Index++) {
    *(SmbiosGuid + 15 - Index) = *(IpmiGuid + Index);
  }
  //
  // Time high, time mid and time low field
  // are stored LSB first in both IPMI spec
  // and SMBIOS spec
  // ->only need change offset
  //
  *(SmbiosGuid + 6) = *(IpmiGuid + 8);
  *(SmbiosGuid + 7) = *(IpmiGuid + 9);
  *(SmbiosGuid + 4) = *(IpmiGuid + 10);
  *(SmbiosGuid + 5) = *(IpmiGuid + 11);
  *SmbiosGuid       = *(IpmiGuid + 12);
  *(SmbiosGuid + 1) = *(IpmiGuid + 13);
  *(SmbiosGuid + 2) = *(IpmiGuid + 14);
  *(SmbiosGuid + 3) = *(IpmiGuid + 15);
}

/**
  Fetches the system UUID.

  @param[out]   SystemUuid   The pointer to the buffer to store the System UUID.
**/
VOID
EFIAPI
OemGetSystemUuid (
  OUT GUID  *SystemUuid
  )
{
  EFI_STATUS  Status;
  EFI_GUID    Uuid;

  if (SystemUuid == NULL) {
    return;
  }

  Status = IpmiFruGetSystemUuid (&Uuid);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a %d Can not get System UUID!\n", __FUNCTION__, __LINE__));
  }

  ConvertIpmiGuidToSmbiosGuid ((UINT8 *)SystemUuid, (UINT8 *)&Uuid);
}
