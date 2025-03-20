/** @file

  Copyright (c) 2023, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>

#include <Guid/MdeModuleHii.h>
#include <Guid/X86EmulatorConfigGuid.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/HiiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Protocol/HiiConfigAccess.h>
#include <Protocol/HiiConfigRouting.h>
#include <Protocol/PeCoffImageEmulator.h>

#include "X86EmulatorNVData.h"
#include "X86EmulatorConfigDxe.h"

X86_EMULATOR_CONFIG_PRIVATE_DATA  *mDriverPrivateData;
EFI_EVENT                         mX86EmulatorConfigEvent;

HII_VENDOR_DEVICE_PATH  mX86EmulatorHiiVendorDevicePath = {
  {
    {
      HARDWARE_DEVICE_PATH,
      HW_VENDOR_DP,
      {
        (UINT8)(sizeof (VENDOR_DEVICE_PATH)),
        (UINT8)((sizeof (VENDOR_DEVICE_PATH)) >> 8)
      }
    },
    X86_EMULATOR_CONFIG_FORMSET_GUID
  },
  {
    END_DEVICE_PATH_TYPE,
    END_ENTIRE_DEVICE_PATH_SUBTYPE,
    {
      (UINT8)(END_DEVICE_PATH_LENGTH),
      (UINT8)((END_DEVICE_PATH_LENGTH) >> 8)
    }
  }
};

/**
  This function allows a caller to extract the current configuration for one
  or more named elements from the target driver.

  @param  This                   Points to the EFI_HII_CONFIG_ACCESS_PROTOCOL.
  @param  Request                A null-terminated Unicode string in
                                 <ConfigRequest> format.
  @param  Progress               On return, points to a character in the Request
                                 string. Points to the string's null terminator if
                                 request was successful. Points to the most recent
                                 '&' before the first failing name/value pair (or
                                 the beginning of the string if the failure is in
                                 the first name/value pair) if the request was not
                                 successful.
  @param  Results                A null-terminated Unicode string in
                                 <ConfigAltResp> format which has all values filled
                                 in for the names in the Request string. String to
                                 be allocated by the called function.

  @retval EFI_SUCCESS            The Results is filled with the requested values.
  @retval EFI_OUT_OF_RESOURCES   Not enough memory to store the results.
  @retval EFI_INVALID_PARAMETER  Request is illegal syntax, or unknown name.
  @retval EFI_NOT_FOUND          Routing data doesn't match any storage in this
                                 driver.

**/
EFI_STATUS
EFIAPI
DriverExtractConfig (
  IN  CONST EFI_HII_CONFIG_ACCESS_PROTOCOL *This,
  IN  CONST EFI_STRING                     Request,
  OUT EFI_STRING                           *Progress,
  OUT EFI_STRING                           *Results
  )
{
  BOOLEAN                           AllocatedRequest;
  EFI_HII_CONFIG_ROUTING_PROTOCOL   *HiiConfigRouting;
  EFI_STATUS                        Status;
  EFI_STRING                        ConfigRequest;
  EFI_STRING                        ConfigRequestHdr;
  UINTN                             BufferSize;
  UINTN                             VariableSize;
  X86_EMULATOR_CONFIG_PRIVATE_DATA  *DriverPrivateData;

  if ((Progress == NULL) || (Results == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  AllocatedRequest  = FALSE;
  ConfigRequest     = NULL;
  ConfigRequestHdr  = NULL;
  DriverPrivateData = X86_EMULATOR_CONFIG_FROM_THIS (This);
  HiiConfigRouting  = DriverPrivateData->DriverHiiConfigRouting;
  VariableSize = sizeof (X86_EMULATOR_CONFIG_VARSTORE_DATA);

  if (  (Request != NULL)
     && (!HiiIsConfigHdrMatch (Request, &gX86EmulatorConfigFormSetGuid, X86_EMULATOR_CONFIG_VARIABLE_NAME)))
  {
    return EFI_NOT_FOUND;
  }

  Status = gRT->GetVariable (
                  X86_EMULATOR_CONFIG_VARIABLE_NAME,
                  &gX86EmulatorConfigFormSetGuid,
                  NULL,
                  &VariableSize,
                  &(DriverPrivateData->ConfigData)
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Convert Block Data to <ConfigResp> by helper function BlockToConfig ()
  //
  ConfigRequest = Request;
  if ((Request == NULL) || ((StrStr (Request, L"OFFSET")) == NULL)) {
    //
    // Request has no request element, construct full request string.
    // Allocate and fill a buffer large enough to hold the <ConfigHdr> template
    // followed by "&OFFSET=0&WIDTH=WWWWWWWWWWWWWWWW" followed by a Null-terminator
    //
    ConfigRequestHdr = HiiConstructConfigHdr (
                         &gX86EmulatorConfigFormSetGuid,
                         X86_EMULATOR_CONFIG_VARIABLE_NAME,
                         DriverPrivateData->DriverHandle
                         );
    ASSERT (ConfigRequestHdr != NULL);
    BufferSize    = (StrLen (ConfigRequestHdr) + 32 + 1) * sizeof (CHAR16);
    ConfigRequest = AllocateZeroPool (BufferSize);
    ASSERT (ConfigRequest != NULL);
    if (ConfigRequest == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }

    AllocatedRequest = TRUE;
    UnicodeSPrint (
      ConfigRequest,
      BufferSize,
      L"%s&OFFSET=0&WIDTH=%016LX",
      ConfigRequestHdr,
      (UINT64)VariableSize
      );
    FreePool (ConfigRequestHdr);
  }

  //
  // Convert buffer data to <ConfigResp> by helper function BlockToConfig()
  //
  Status = HiiConfigRouting->BlockToConfig (
                               HiiConfigRouting,
                               ConfigRequest,
                               (UINT8 *)&(DriverPrivateData->ConfigData),
                               VariableSize,
                               Results,
                               Progress
                               );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Free the allocated config request string.
  //
  if (AllocatedRequest) {
    FreePool (ConfigRequest);
    ConfigRequest = NULL;
  }

  //
  // Set Progress string to the original request string.
  //
  if (Request == NULL) {
    *Progress = NULL;
  } else if (StrStr (Request, L"OFFSET") == NULL) {
    *Progress = Request + StrLen (Request);
  }

  return EFI_SUCCESS;
}

/**
  This function processes the results of changes in configuration.

  @param  This                   Points to the EFI_HII_CONFIG_ACCESS_PROTOCOL.
  @param  Configuration          A null-terminated Unicode string in <ConfigResp>
                                 format.
  @param  Progress               A pointer to a string filled in with the offset of
                                 the most recent '&' before the first failing
                                 name/value pair (or the beginning of the string if
                                 the failure is in the first name/value pair) or
                                 the terminating NULL if all was successful.

  @retval EFI_SUCCESS            The Results is processed successfully.
  @retval EFI_INVALID_PARAMETER  Configuration is NULL.
  @retval EFI_NOT_FOUND          Routing data doesn't match any storage in this
                                 driver.

**/
EFI_STATUS
EFIAPI
DriverRouteConfig (
  IN CONST EFI_HII_CONFIG_ACCESS_PROTOCOL *This,
  IN CONST EFI_STRING                     Configuration,
  OUT      EFI_STRING                     *Progress
  )
{
  EFI_HII_CONFIG_ROUTING_PROTOCOL   *HiiConfigRouting;
  EFI_STATUS                        Status;
  UINTN                             VariableSize;
  X86_EMULATOR_CONFIG_PRIVATE_DATA  *DriverPrivateData;

  if ((Configuration == NULL) || (Progress == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  DriverPrivateData = X86_EMULATOR_CONFIG_FROM_THIS (This);
  HiiConfigRouting  = DriverPrivateData->DriverHiiConfigRouting;
  *Progress = Configuration;

  //
  // Check routing data in <ConfigHdr>.
  // Note: if only one Storage is used, then this checking could be skipped.
  //
  if (!HiiIsConfigHdrMatch (Configuration, &gX86EmulatorConfigFormSetGuid, X86_EMULATOR_CONFIG_VARIABLE_NAME)) {
    return EFI_NOT_FOUND;
  }

  VariableSize = sizeof (X86_EMULATOR_CONFIG_VARSTORE_DATA);
  Status = gRT->GetVariable (
                  X86_EMULATOR_CONFIG_VARIABLE_NAME,
                  &gX86EmulatorConfigFormSetGuid,
                  NULL,
                  &VariableSize,
                  &DriverPrivateData->ConfigData
                  );
  if (EFI_ERROR (Status)) {
    return EFI_NOT_FOUND;
  }

  //
  // Convert <ConfigResp> to buffer data by helper function ConfigToBlock()
  //
  VariableSize = sizeof (X86_EMULATOR_CONFIG_VARSTORE_DATA);
  Status = HiiConfigRouting->ConfigToBlock (
                               HiiConfigRouting,
                               Configuration,
                               (UINT8 *)&DriverPrivateData->ConfigData,
                               &VariableSize,
                               Progress
                               );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = gRT->SetVariable (
                  X86_EMULATOR_CONFIG_VARIABLE_NAME,
                  &gX86EmulatorConfigFormSetGuid,
                  EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                  sizeof (X86_EMULATOR_CONFIG_VARSTORE_DATA),
                  &DriverPrivateData->ConfigData
                  );
  if (EFI_ERROR (Status)) {
    return EFI_NOT_FOUND;
  }

  return Status;
}

/**
  This function processes the results of changes in configuration.

  @param  This                   Points to the EFI_HII_CONFIG_ACCESS_PROTOCOL.
  @param  Action                 Specifies the type of action taken by the browser.
  @param  QuestionId             A unique value which is sent to the original
                                 exporting driver so that it can identify the type
                                 of data to expect.
  @param  Type                   The type of value for the question.
  @param  Value                  A pointer to the data being sent to the original
                                 exporting driver.
  @param  ActionRequest          On return, points to the action requested by the
                                 callback function.

  @retval  EFI_SUCCESS           The callback successfully handled the action.
  @retval  EFI_INVALID_PARAMETER The setup browser call this function with invalid parameters.

**/
EFI_STATUS
EFIAPI
DriverCallback (
  IN CONST EFI_HII_CONFIG_ACCESS_PROTOCOL *This,
  IN       EFI_BROWSER_ACTION             Action,
  IN       EFI_QUESTION_ID                QuestionId,
  IN       UINT8                          Type,
  IN       EFI_IFR_TYPE_VALUE             *Value,
  OUT      EFI_BROWSER_ACTION_REQUEST     *ActionRequest
  )
{
  if (Action != EFI_BROWSER_ACTION_CHANGING) {
    //
    // Do nothing for other UEFI Action. Only do call back when data is changed.
    //
    return EFI_UNSUPPORTED;
  }

  if (  (  (Value == NULL)
        && (Action != EFI_BROWSER_ACTION_FORM_OPEN)
        && (Action != EFI_BROWSER_ACTION_FORM_CLOSE))
     || (ActionRequest == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  return EFI_SUCCESS;
}

/**
  This function forces to remove the X86 Emulator support.

  @param[in] This         This pointer for EDKII_PECOFF_IMAGE_EMULATOR_PROTOCOL
                          structure
  @param[in] ImageType    Whether the image is an application, a boot time
                          driver or a runtime driver.
  @param[in] DevicePath   Path to device where the image originated
                          (e.g., a PCI option ROM)

  @retval TRUE            The image is supported by the emulator
  @retval FALSE           The image is not supported by the emulator.

**/
BOOLEAN
EFIAPI
ForceToStopSupportingX86Image (
  IN  EDKII_PECOFF_IMAGE_EMULATOR_PROTOCOL *This,
  IN  UINT16                               ImageType,
  IN  EFI_DEVICE_PATH_PROTOCOL             *DevicePath   OPTIONAL
  )
{
  return FALSE;
}

VOID
X86EmulatorEndOfDxeCallback (
  IN  EFI_EVENT Event,
  IN  VOID      *Context
  )
{
  EFI_STATUS                            Status;
  UINTN                                 VariableSize;
  UINTN                                 NumberOfHandles;
  UINTN                                 Index;
  EFI_HANDLE                            *HandleBuffer;
  X86_EMULATOR_CONFIG_VARSTORE_DATA     ConfigurationData;
  EDKII_PECOFF_IMAGE_EMULATOR_PROTOCOL  *X86Emulator;

  VariableSize = sizeof (X86_EMULATOR_CONFIG_VARSTORE_DATA);

  Status = gRT->GetVariable (
                  X86_EMULATOR_CONFIG_VARIABLE_NAME,
                  &gX86EmulatorConfigFormSetGuid,
                  NULL,
                  &VariableSize,
                  &ConfigurationData
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get NVRAM - %r\n", __FUNCTION__, Status));
    goto Exit;
  }

  ConfigurationData.X86EmulatorIsPresent = X86_EMULATOR_NOT_PRESENT;
  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEdkiiPeCoffImageEmulatorProtocolGuid,
                  NULL,
                  &NumberOfHandles,
                  &HandleBuffer
                  );
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  for (Index = 0; Index < NumberOfHandles; Index++) {
    Status = gBS->HandleProtocol (
                    HandleBuffer[Index],
                    &gEdkiiPeCoffImageEmulatorProtocolGuid,
                    (VOID **)&X86Emulator
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Failed to locate PE/COFF Image Emulator Protocol - %r\n",
        __FUNCTION__,
        Status
        ));
      continue;
    }

    //
    // Only overwrite x86 emulator services
    //
    if (X86Emulator->MachineType == EFI_IMAGE_MACHINE_X64) {
      if (ConfigurationData.X86EmulatorIsPresent != X86_EMULATOR_PRESENT) {
        ConfigurationData.X86EmulatorIsPresent = X86_EMULATOR_PRESENT;
        //
        // Set variable back
        //
        Status = gRT->SetVariable (
                        X86_EMULATOR_CONFIG_VARIABLE_NAME,
                        &gX86EmulatorConfigFormSetGuid,
                        EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS,
                        sizeof (X86_EMULATOR_CONFIG_VARSTORE_DATA),
                        &ConfigurationData
                        );
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "Failed to set X86 Config Variable - %r\n", __FUNCTION__, Status));
        }
      }
    } else {
      continue;
    }

    if (ConfigurationData.EnableX86Emulator == X86_EMULATOR_ENABLE) {
      continue;
    }

    X86Emulator->IsImageSupported = ForceToStopSupportingX86Image;
  }

Exit:
  if (HandleBuffer != NULL) {
    FreePool (HandleBuffer);
  }

  gBS->CloseEvent (mX86EmulatorConfigEvent);
}

VOID
DriverUnload (
  X86_EMULATOR_CONFIG_PRIVATE_DATA *DriverData
  )
{
  if (DriverData == NULL) {
    return;
  }

  //
  // Unload protocol
  //
  if (DriverData->DriverHandle != NULL) {
    gBS->UninstallMultipleProtocolInterfaces (
           DriverData->DriverHandle,
           &gEfiDevicePathProtocolGuid,
           &mX86EmulatorHiiVendorDevicePath,
           &gEfiHiiConfigAccessProtocolGuid,
           &DriverData->DriverConfigAccess,
           NULL
           );
    DriverData->DriverHandle = NULL;
  }

  //
  // Free HII Database
  //
  if (DriverData->DriverHiiHandle != NULL) {
    HiiRemovePackages (DriverData->DriverHiiHandle);
  }

  //
  // Free variable
  //
  FreePool (DriverData);
}

EFI_STATUS
X86EmulatorConfigDxeEntry (
  IN EFI_HANDLE       ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{
  EFI_STATUS                       Status;
  EFI_HII_CONFIG_ROUTING_PROTOCOL  *HiiConfigRouting;
  UINTN                            VariableSize;
  EFI_STRING                       ConfigRequestHeader;
  BOOLEAN                          ActionFlag;

  //
  // Initialize the Driver Private Data
  //
  mDriverPrivateData = AllocateZeroPool (sizeof (X86_EMULATOR_CONFIG_PRIVATE_DATA));
  if (mDriverPrivateData == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  mDriverPrivateData->DriverHandle = NULL;
  mDriverPrivateData->Signature    = X86_EMULATOR_CONFIG_PRIVATE_SIGNATURE;
  mDriverPrivateData->DriverConfigAccess.ExtractConfig = DriverExtractConfig;
  mDriverPrivateData->DriverConfigAccess.RouteConfig   = DriverRouteConfig;
  mDriverPrivateData->DriverConfigAccess.Callback = DriverCallback;

  //
  // Locate ConfigRouting protocol
  //
  Status = gBS->LocateProtocol (&gEfiHiiConfigRoutingProtocolGuid, NULL, (VOID **)&HiiConfigRouting);
  if (EFI_ERROR (Status)) {
    DriverUnload (mDriverPrivateData);
    return Status;
  }

  mDriverPrivateData->DriverHiiConfigRouting = HiiConfigRouting;

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &(mDriverPrivateData->DriverHandle),
                  &gEfiDevicePathProtocolGuid,
                  &mX86EmulatorHiiVendorDevicePath,
                  &gEfiHiiConfigAccessProtocolGuid,
                  &mDriverPrivateData->DriverConfigAccess,
                  NULL
                  );
  ASSERT_EFI_ERROR (Status);
  if (EFI_ERROR (Status)) {
    DriverUnload (mDriverPrivateData);
    return Status;
  }

  //
  // Publish our HII data
  //
  mDriverPrivateData->DriverHiiHandle = HiiAddPackages (
                                          &gX86EmulatorConfigFormSetGuid,
                                          mDriverPrivateData->DriverHandle,
                                          X86EmulatorConfigDxeStrings,
                                          X86EmulatorConfigVfrBin,
                                          NULL
                                          );
  if (mDriverPrivateData->DriverHiiHandle == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Can not add Package List to HII Database, system run out of resources\n",
      __FUNCTION__
      ));
    DriverUnload (mDriverPrivateData);
    ASSERT_EFI_ERROR (Status);
    return EFI_OUT_OF_RESOURCES;
  }

  //
  // Retrieve the default settings
  //
  ConfigRequestHeader = HiiConstructConfigHdr (
                          &gX86EmulatorConfigFormSetGuid,
                          X86_EMULATOR_CONFIG_VARIABLE_NAME,
                          mDriverPrivateData->DriverHandle
                          );
  ASSERT (ConfigRequestHeader != NULL);
  VariableSize = sizeof (X86_EMULATOR_CONFIG_VARSTORE_DATA);
  Status = gRT->GetVariable (
                  X86_EMULATOR_CONFIG_VARIABLE_NAME,
                  &gX86EmulatorConfigFormSetGuid,
                  NULL,
                  &VariableSize,
                  &(mDriverPrivateData->ConfigData)
                  );
  if (Status == EFI_NOT_FOUND) {
    mDriverPrivateData->ConfigData.X86EmulatorIsPresent = X86_EMULATOR_NOT_PRESENT;
    mDriverPrivateData->ConfigData.EnableX86Emulator    = X86_EMULATOR_DISABLE;
    Status = gRT->SetVariable (
                    X86_EMULATOR_CONFIG_VARIABLE_NAME,
                    &gX86EmulatorConfigFormSetGuid,
                    EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                    sizeof (X86_EMULATOR_CONFIG_VARSTORE_DATA),
                    &mDriverPrivateData->ConfigData
                    );
    ASSERT_EFI_ERROR (Status);

    //
    // EFI variable for NV config doesn't exit, we should build this variable
    // based on default values stored in IFR
    //
    ActionFlag = HiiSetToDefaults (ConfigRequestHeader, EFI_HII_DEFAULT_CLASS_STANDARD);
    if (!ActionFlag) {
      DriverUnload (mDriverPrivateData);
      return EFI_INVALID_PARAMETER;
    }
  } else if (EFI_ERROR (Status)) {
    DriverUnload (mDriverPrivateData);
    return Status;
  } else {
    //
    // EFI variable does exist and Validate Current Setting
    //
    ActionFlag = HiiValidateSettings (ConfigRequestHeader);
    if (!ActionFlag) {
      DriverUnload (mDriverPrivateData);
      return EFI_INVALID_PARAMETER;
    }
  }

  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  X86EmulatorEndOfDxeCallback,
                  NULL,
                  &gEfiEndOfDxeEventGroupGuid,
                  &mX86EmulatorConfigEvent
                  );
  ASSERT_EFI_ERROR (Status);

  return EFI_SUCCESS;
}
