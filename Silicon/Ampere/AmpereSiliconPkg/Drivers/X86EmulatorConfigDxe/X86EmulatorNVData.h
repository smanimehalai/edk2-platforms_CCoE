/** @file

  Copyright (c) 2023, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef X86_EMULATOR_NV_DATA_H_
#define X86_EMULATOR_NV_DATA_H_

#define X86_EMULATOR_VARSTORE_ID  0x2000

#define MAIN_FORM_ID  0x1000

#define X86_EMULATOR_ENABLE       1
#define X86_EMULATOR_DISABLE      0
#define X86_EMULATOR_PRESENT      1
#define X86_EMULATOR_NOT_PRESENT  0

#pragma pack (1)

typedef struct {
  UINT8    X86EmulatorIsPresent;
  UINT8    EnableX86Emulator;
} X86_EMULATOR_CONFIG_VARSTORE_DATA;

#pragma pack ()

#define X86_EMULATOR_CONFIG_VARIABLE_NAME  L"X86EmulatorConfigNVData"

#endif /* X86_EMULATOR_NV_DATA_H_ */
