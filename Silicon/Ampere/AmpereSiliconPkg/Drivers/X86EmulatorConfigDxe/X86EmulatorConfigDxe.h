/** @file

  Copyright (c) 2023, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef X86_EMULATOR_CONFIG_DXE_H_
#define X86_EMULATOR_CONFIG_DXE_H_

//
// This is the generated IFR binary data for each formset defined in VFR.
// This data array is ready to be used as input of HiiAddPackages() to
// create a packagelist (which contains Form packages, String packages, etc).
//
extern UINT8  X86EmulatorConfigVfrBin[];

//
// This is the generated String package data for all .UNI files.
// This data array is ready to be used as input of HiiAddPackages() to
// create a packagelist (which contains Form packages, String packages, etc).
//
extern UINT8  X86EmulatorConfigDxeStrings[];

#define X86_EMULATOR_CONFIG_FROM_THIS(a)  \
  CR (a, X86_EMULATOR_CONFIG_PRIVATE_DATA, DriverConfigAccess, X86_EMULATOR_CONFIG_PRIVATE_SIGNATURE)
#define X86_EMULATOR_CONFIG_PRIVATE_SIGNATURE  SIGNATURE_32 ('E', 'C', 'p', 's')

#pragma pack (1)

typedef struct {
  UINTN                                   Signature;
  EFI_HANDLE                              DriverHandle;
  EFI_HII_HANDLE                          DriverHiiHandle;
  X86_EMULATOR_CONFIG_VARSTORE_DATA       ConfigData;
  EDKII_PECOFF_IMAGE_EMULATOR_PROTOCOL    X86EmulatorProtocol;
  EFI_HII_CONFIG_ROUTING_PROTOCOL         *DriverHiiConfigRouting;
  EFI_HII_CONFIG_ACCESS_PROTOCOL          DriverConfigAccess;
} X86_EMULATOR_CONFIG_PRIVATE_DATA;

///
/// HII specific Vendor Device Path definition.
///
typedef struct {
  VENDOR_DEVICE_PATH          VendorDevicePath;
  EFI_DEVICE_PATH_PROTOCOL    End;
} HII_VENDOR_DEVICE_PATH;

#pragma pack ()

#endif /* X86_EMULATOR_CONFIG_DXE_H_ */
