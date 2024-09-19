## @file
#
# Copyright (c) 2020 - 2022, Ampere Computing LLC. All rights reserved.<BR>
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
##

################################################################################
#
# Defines Section - statements that will be processed to create a Makefile.
#
################################################################################
[Defines]
  PLATFORM_NAME                  = Jade
  PLATFORM_GUID                  = 7BDD00C0-68F3-4CC1-8775-F0F00572019F
  PLATFORM_VERSION               = 0.1
  DSC_SPECIFICATION              = 0x0001001B
  OUTPUT_DIRECTORY               = Build/Jade
  SUPPORTED_ARCHITECTURES        = AARCH64
  BUILD_TARGETS                  = DEBUG|RELEASE
  SKUID_IDENTIFIER               = DEFAULT
  FLASH_DEFINITION               = Platform/Ampere/JadePkg/JadeLinuxBoot.fdf

  #
  # Defines for default states.  These can be changed on the command line.
  # -D FLAG=VALUE
  #
  DEFINE DEBUG_PRINT_ERROR_LEVEL = 0x8000000F
  DEFINE FIRMWARE_VER            = 0.01.001
  DEFINE EDK2_SKIP_PEICORE       = TRUE

!include MdePkg/MdeLibs.dsc.inc

# Include default Ampere Platform DSC file
!include Silicon/Ampere/AmpereAltraPkg/AmpereAltraLinuxBootPkg.dsc.inc

#
# Specific Platform Library
#
[LibraryClasses.common]
  #
  # ACPI Libraries
  #
  AcpiLib|EmbeddedPkg/Library/AcpiLib/AcpiLib.inf
  AcpiHelperLib|Platform/Ampere/AmperePlatformPkg/Library/AcpiHelperLib/AcpiHelperLib.inf

  #
  # Pcie Board
  #
  BoardPcieLib|Platform/Ampere/JadePkg/Library/BoardPcieLib/BoardPcieLib.inf

  IOExpanderLib|Platform/Ampere/JadePkg/Library/IOExpanderLib/IOExpanderLib.inf

  PlatformBmcReadyLib|Platform/Ampere/JadePkg/Library/PlatformBmcReadyLib/PlatformBmcReadyLib.inf

[LibraryClasses.common.PEIM]
  SmbusLib|MdePkg/Library/PeiSmbusLibSmbus2Ppi/PeiSmbusLibSmbus2Ppi.inf

[LibraryClasses.common.DXE_RUNTIME_DRIVER]
  #
  # RTC Library: Common RTC
  #
  RealTimeClockLib|Platform/Ampere/JadePkg/Library/PCF85063RealTimeClockLib/PCF85063RealTimeClockLib.inf

[LibraryClasses.common.UEFI_DRIVER, LibraryClasses.common.UEFI_APPLICATION, LibraryClasses.common.DXE_RUNTIME_DRIVER, LibraryClasses.common.DXE_DRIVER]
  SmbusLib|MdePkg/Library/DxeSmbusLib/DxeSmbusLib.inf

[PcdsFixedAtBuild.common]
  gAmpereTokenSpaceGuid.PcdSmbiosTables0MajorVersion|$(MAJOR_VER)
  gAmpereTokenSpaceGuid.PcdSmbiosTables0MinorVersion|$(MINOR_VER)
!ifdef $(FIRMWARE_VER)
  gEfiMdeModulePkgTokenSpaceGuid.PcdFirmwareVersionString|L"$(FIRMWARE_VER)"
!endif

  # Clearing BIT0 in this PCD prevents installing a 32-bit SMBIOS entry point,
  # if the entry point version is >= 3.0. AARCH64 OSes cannot assume the
  # presence of the 32-bit entry point anyway (because many AARCH64 systems
  # don't have 32-bit addressable physical RAM), and the additional allocations
  # below 4 GB needlessly fragment the memory map. So expose the 64-bit entry
  # point only, for entry point versions >= 3.0.
  gEfiMdeModulePkgTokenSpaceGuid.PcdSmbiosEntryPointProvideMethod|0x2

#
# Specific Platform Component
#
[Components.common]

  #
  # ACPI
  #
  MdeModulePkg/Universal/Acpi/AcpiTableDxe/AcpiTableDxe.inf
  Platform/Ampere/JadePkg/Drivers/AcpiPlatformDxe/AcpiPlatformDxe.inf
  Silicon/Ampere/AmpereAltraPkg/AcpiCommonTables/AcpiCommonTables.inf
  Platform/Ampere/JadePkg/AcpiTables/AcpiTables.inf
  Platform/Ampere/JadePkg/Ac02AcpiTables/Ac02AcpiTables.inf

  #
  # SMBIOS
  #
  MdeModulePkg/Universal/SmbiosDxe/SmbiosDxe.inf
  ArmPkg/Universal/Smbios/ProcessorSubClassDxe/ProcessorSubClassDxe.inf
  ArmPkg/Universal/Smbios/SmbiosMiscDxe/SmbiosMiscDxe.inf
  Platform/Ampere/JadePkg/Drivers/SmbiosPlatformDxe/SmbiosPlatformDxe.inf

  #
  # NVParamRuntimeDxe
  #
  Silicon/Ampere/AmpereAltraPkg/Drivers/NVParamRuntimeDxe/NVParamRuntimeDxe.inf

