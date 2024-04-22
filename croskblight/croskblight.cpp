#define DESCRIPTOR_DEF
#include "croskblight.h"
#include <acpiioct.h>
#include <ntstrsafe.h>
#define NOTVM 1

static ULONG CrosKBLightDebugLevel = 100;
static ULONG CrosKBLightDebugCatagories = DBG_INIT || DBG_PNP || DBG_IOCTL;

NTSTATUS
DriverEntry(
	__in PDRIVER_OBJECT  DriverObject,
	__in PUNICODE_STRING RegistryPath
	)
{
	NTSTATUS               status = STATUS_SUCCESS;
	WDF_DRIVER_CONFIG      config;
	WDF_OBJECT_ATTRIBUTES  attributes;

	CrosKBLightPrint(DEBUG_LEVEL_INFO, DBG_INIT,
		"Driver Entry");

	WDF_DRIVER_CONFIG_INIT(&config, CrosKBLightEvtDeviceAdd);

	WDF_OBJECT_ATTRIBUTES_INIT(&attributes);

	//
	// Create a framework driver object to represent our driver.
	//

	status = WdfDriverCreate(DriverObject,
		RegistryPath,
		&attributes,
		&config,
		WDF_NO_HANDLE
		);

	if (!NT_SUCCESS(status))
	{
		CrosKBLightPrint(DEBUG_LEVEL_ERROR, DBG_INIT,
			"WdfDriverCreate failed with status 0x%x\n", status);
	}

	return status;
}

#define MAX_DEVICE_REG_VAL_LENGTH 0x100
NTSTATUS GetSmbiosName(WCHAR systemProductName[MAX_DEVICE_REG_VAL_LENGTH]) {
	NTSTATUS status = STATUS_UNSUCCESSFUL;
	HANDLE parentKey = NULL;
	UNICODE_STRING ParentKeyName;
	OBJECT_ATTRIBUTES  ObjectAttributes;
	RtlInitUnicodeString(&ParentKeyName, L"\\Registry\\Machine\\Hardware\\DESCRIPTION\\System\\BIOS");

	InitializeObjectAttributes(&ObjectAttributes,
		&ParentKeyName,
		OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
		NULL,    // handle
		NULL);

	status = ZwOpenKey(&parentKey, KEY_READ, &ObjectAttributes);
	if (!NT_SUCCESS(status)) {
		return status;
	}

	ULONG ResultLength;
	PKEY_VALUE_PARTIAL_INFORMATION KeyValueInfo = (PKEY_VALUE_PARTIAL_INFORMATION)ExAllocatePoolZero(NonPagedPool, sizeof(KEY_VALUE_PARTIAL_INFORMATION) + MAX_DEVICE_REG_VAL_LENGTH, CROSKBLIGHT_POOL_TAG);
	if (!KeyValueInfo) {
		status = STATUS_NO_MEMORY;
		goto exit;
	}

	UNICODE_STRING SystemProductNameValue;
	RtlInitUnicodeString(&SystemProductNameValue, L"SystemProductName");
	status = ZwQueryValueKey(parentKey, &SystemProductNameValue, KeyValuePartialInformation, KeyValueInfo, sizeof(KEY_VALUE_PARTIAL_INFORMATION) + MAX_DEVICE_REG_VAL_LENGTH, &ResultLength);
	if (!NT_SUCCESS(status)) {
		goto exit;
	}

	if (KeyValueInfo->DataLength > MAX_DEVICE_REG_VAL_LENGTH) {
		status = STATUS_BUFFER_OVERFLOW;
		goto exit;
	}

	RtlZeroMemory(systemProductName, sizeof(systemProductName));
	RtlCopyMemory(systemProductName, &KeyValueInfo->Data, KeyValueInfo->DataLength);

exit:
	if (KeyValueInfo) {
		ExFreePoolWithTag(KeyValueInfo, CROSKBLIGHT_POOL_TAG);
	}
	return status;
}

#if NOTVM
NTSTATUS ConnectToEc(
	_In_ WDFDEVICE FxDevice
) {
	PCROSKBLIGHT_CONTEXT pDevice = GetDeviceContext(FxDevice);
	WDF_OBJECT_ATTRIBUTES objectAttributes;

	WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
	objectAttributes.ParentObject = FxDevice;

	NTSTATUS status = WdfIoTargetCreate(FxDevice,
		&objectAttributes,
		&pDevice->busIoTarget
	);
	if (!NT_SUCCESS(status))
	{
		CrosKBLightPrint(
			DEBUG_LEVEL_ERROR,
			DBG_IOCTL,
			"Error creating IoTarget object - 0x%x\n",
			status);
		if (pDevice->busIoTarget)
			WdfObjectDelete(pDevice->busIoTarget);
		return status;
	}

	DECLARE_CONST_UNICODE_STRING(busDosDeviceName, L"\\DosDevices\\GOOG0004");

	WDF_IO_TARGET_OPEN_PARAMS openParams;
	WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(
		&openParams,
		&busDosDeviceName,
		(GENERIC_READ | GENERIC_WRITE));

	openParams.ShareAccess = FILE_SHARE_READ | FILE_SHARE_WRITE;
	openParams.CreateDisposition = FILE_OPEN;
	openParams.FileAttributes = FILE_ATTRIBUTE_NORMAL;

	CROSEC_INTERFACE_STANDARD CrosEcInterface;
	RtlZeroMemory(&CrosEcInterface, sizeof(CrosEcInterface));

	status = WdfIoTargetOpen(pDevice->busIoTarget, &openParams);
	if (!NT_SUCCESS(status))
	{
		CrosKBLightPrint(
			DEBUG_LEVEL_ERROR,
			DBG_IOCTL,
			"Error opening IoTarget object - 0x%x\n",
			status);
		WdfObjectDelete(pDevice->busIoTarget);
		return status;
	}

	status = WdfIoTargetQueryForInterface(pDevice->busIoTarget,
		&GUID_CROSEC_INTERFACE_STANDARD,
		(PINTERFACE)&CrosEcInterface,
		sizeof(CrosEcInterface),
		1,
		NULL);
	WdfIoTargetClose(pDevice->busIoTarget);
	pDevice->busIoTarget = NULL;
	if (!NT_SUCCESS(status)) {
		CrosKBLightPrint(DEBUG_LEVEL_ERROR, DBG_PNP,
			"WdfFdoQueryForInterface failed 0x%x\n", status);
		return status;
	}

	pDevice->CrosEcBusContext = CrosEcInterface.InterfaceHeader.Context;
	pDevice->CrosEcCmdXferStatus = CrosEcInterface.CmdXferStatus;
	return status;
}

static NTSTATUS send_ec_command(
	_In_ PCROSKBLIGHT_CONTEXT pDevice,
	UINT32 cmd,
	UINT32 version,
	UINT8* out,
	size_t outSize,
	UINT8* in,
	size_t inSize)
{
	PCROSEC_COMMAND msg = (PCROSEC_COMMAND)ExAllocatePoolWithTag(NonPagedPool, sizeof(CROSEC_COMMAND) + max(outSize, inSize), CROSKBLIGHT_POOL_TAG);
	if (!msg) {
		return STATUS_NO_MEMORY;
	}
	msg->Version = version;
	msg->Command = cmd;
	msg->OutSize = outSize;
	msg->InSize = inSize;

	if (outSize)
		memcpy(msg->Data, out, outSize);

	NTSTATUS status = (*pDevice->CrosEcCmdXferStatus)(pDevice->CrosEcBusContext, msg);
	if (!NT_SUCCESS(status)) {
		goto exit;
	}

	if (in && inSize) {
		memcpy(in, msg->Data, inSize);
	}

exit:
	ExFreePoolWithTag(msg, CROSKBLIGHT_POOL_TAG);
	return status;
}

/**
 * Get the versions of the command supported by the EC.
 *
 * @param cmd		Command
 * @param pmask		Destination for version mask; will be set to 0 on
 *			error.
 */
static NTSTATUS cros_ec_get_cmd_versions(PCROSKBLIGHT_CONTEXT pDevice, int cmd, UINT32* pmask) {
	struct ec_params_get_cmd_versions_v1 pver_v1;
	struct ec_params_get_cmd_versions pver;
	struct ec_response_get_cmd_versions rver;
	NTSTATUS status;

	*pmask = 0;

	pver_v1.cmd = cmd;
	status = send_ec_command(pDevice, EC_CMD_GET_CMD_VERSIONS, 1, (UINT8*)&pver_v1, sizeof(pver_v1),
		(UINT8*)&rver, sizeof(rver));

	if (!NT_SUCCESS(status)) {
		pver.cmd = cmd;
		status = send_ec_command(pDevice, EC_CMD_GET_CMD_VERSIONS, 0, (UINT8*)&pver, sizeof(pver),
			(UINT8*)&rver, sizeof(rver));
	}

	*pmask = rver.version_mask;
	return status;
}

/**
 * Return non-zero if the EC supports the command and version
 *
 * @param cmd		Command to check
 * @param ver		Version to check
 * @return non-zero if command version supported; 0 if not.
 */
BOOLEAN cros_ec_cmd_version_supported(PCROSKBLIGHT_CONTEXT pDevice, int cmd, int ver)
{
	UINT32 mask = 0;

	if (NT_SUCCESS(cros_ec_get_cmd_versions(pDevice, cmd, &mask)))
		return false;

	return (mask & EC_VER_MASK(ver)) ? true : false;
}

NTSTATUS
CrosKBLightGetBacklight(
	_In_ PCROSKBLIGHT_CONTEXT pDevice,
	UINT8* PBacklight
)
{
	struct ec_response_pwm_get_keyboard_backlight backlightParams;
	NTSTATUS status = send_ec_command(pDevice, EC_CMD_PWM_GET_KEYBOARD_BACKLIGHT, 0, NULL, 0, (UINT8*)&backlightParams, sizeof(struct ec_response_pwm_get_keyboard_backlight));
	if (!NT_SUCCESS(status))
		return status;
	if (PBacklight) {
		if (backlightParams.enabled) {
			*PBacklight = backlightParams.percent;
		}
		else {
			*PBacklight = 0;
		}
	}
	return status;
}

NTSTATUS
CrosKBLightSetBacklight(
	_In_ PCROSKBLIGHT_CONTEXT pDevice,
	UINT8 Backlight
)
{
	struct ec_params_pwm_set_keyboard_backlight backlightParams;
	backlightParams.percent = Backlight;
	return send_ec_command(pDevice, EC_CMD_PWM_SET_KEYBOARD_BACKLIGHT, 0, (UINT8*)&backlightParams, sizeof(struct ec_params_pwm_set_keyboard_backlight), NULL, 0);
}
#endif

BOOLEAN validateAndLoad(
	_In_ PCROSKBLIGHT_CONTEXT pDevice,
	_In_ firmware *fw
) {
	if (fw->size < sizeof(CROSKBLIGHT_INFO)) { //Must be base info size at minimum
		return FALSE;
	}

	CROSKBLIGHT_INFO* info = (CROSKBLIGHT_INFO*)fw->data;
	UINT32 LampCount = info->LampCount;
	if (LampCount < 1) {
		DbgPrint("RGB Keyboard must have at least 1 Lamp!\n");
		return FALSE;
	}

	size_t sz = sizeof(CROSKBLIGHT_INFO) + LampCount * sizeof(CROSKBLIGHT_KEY_INFO);
	if (fw->size < sz) {
		DbgPrint("Size is too small: %lld (expected %lld)\n", fw->size, sz);
		return FALSE;
	}

	pDevice->RGBLedInfo = (CROSKBLIGHT_INFO*)ExAllocatePoolZero(NonPagedPool, sz, CROSKBLIGHT_POOL_TAG);
	RtlCopyMemory(pDevice->RGBLedInfo, info, sz);
	return TRUE;
}

NTSTATUS
OnPrepareHardware(
	_In_  WDFDEVICE     FxDevice,
	_In_  WDFCMRESLIST  FxResourcesRaw,
	_In_  WDFCMRESLIST  FxResourcesTranslated
)
/*++

Routine Description:

This routine caches the SPB resource connection ID.

Arguments:

FxDevice - a handle to the framework device object
FxResourcesRaw - list of translated hardware resources that
the PnP manager has assigned to the device
FxResourcesTranslated - list of raw hardware resources that
the PnP manager has assigned to the device

Return Value:

Status

--*/
{
	PCROSKBLIGHT_CONTEXT pDevice = GetDeviceContext(FxDevice);
	NTSTATUS status = STATUS_SUCCESS;

	UNREFERENCED_PARAMETER(FxResourcesRaw);
	UNREFERENCED_PARAMETER(FxResourcesTranslated);

	pDevice->RGBLedInfo = NULL;

#if NOTVM
	status = ConnectToEc(FxDevice);
	if (!NT_SUCCESS(status)) {
		return status;
	}

	(*pDevice->CrosEcCmdXferStatus)(pDevice->CrosEcBusContext, NULL);

	BOOLEAN supportsRGBKBD = FALSE;
	if (!cros_ec_cmd_version_supported(pDevice, EC_CMD_RGBKBD, 0)) {
		ec_params_rgbkbd kbdCmd = { 0 };
		kbdCmd.subcmd = EC_RGBKBD_SUBCMD_GET_CONFIG;
		ec_response_rgbkbd kbdResp = { 0 };
		if (NT_SUCCESS(send_ec_command(pDevice, EC_CMD_RGBKBD, 0, (UINT8*)&kbdCmd, sizeof(kbdCmd), (UINT8*)&kbdResp, sizeof(kbdResp)))) {
			if (kbdResp.rgbkbd_type > EC_RGBKBD_TYPE_UNKNOWN) {
				DbgPrint("Got RGB Keyboard Type: %d\n", kbdResp.rgbkbd_type);

				WCHAR fwPath[MAX_DEVICE_REG_VAL_LENGTH];

				WCHAR SystemProductName[MAX_DEVICE_REG_VAL_LENGTH];
				status = GetSmbiosName(SystemProductName);
				if (NT_SUCCESS(status)) {
					status = RtlStringCbPrintfW(fwPath, MAX_DEVICE_REG_VAL_LENGTH, L"\\SystemRoot\\system32\\DRIVERS\\croskbrgb_%s.bin", SystemProductName);
					if (NT_SUCCESS(status)) {
						struct firmware* fw = NULL;
						status = request_firmware(&fw, fwPath);
						if (NT_SUCCESS(status)) {
							validateAndLoad(pDevice, fw);
							free_firmware(fw);
						}
						else {
							DbgPrint("Failed to load %ws\n", fwPath);
						}
					}
				}

				if (!pDevice->RGBLedInfo) {
					status = RtlStringCbPrintfW(fwPath, MAX_DEVICE_REG_VAL_LENGTH, L"\\SystemRoot\\system32\\DRIVERS\\croskbrgb_generic_%d.bin", kbdResp.rgbkbd_type);
					if (NT_SUCCESS(status)) {
						struct firmware* fw = NULL;
						status = request_firmware(&fw, fwPath);
						if (NT_SUCCESS(status)) {
							validateAndLoad(pDevice, fw);
							free_firmware(fw);
						}
						else {
							DbgPrint("Failed to load %ws\n", fwPath);
						}
					}
					else {
						DbgPrint("Failed create fwPath\n");
					}
				}

				supportsRGBKBD = TRUE;
			}
		}
	}
	#endif

	firmware* fw = NULL;
	NTSTATUS checkForceRGB = request_firmware(&fw, L"\\SystemRoot\\system32\\DRIVERS\\croskbrgb_forcergb.txt");
	if (fw) {
		free_firmware(fw);
	}
	if (NT_SUCCESS(checkForceRGB)) {
		DbgPrint("Force enabling RGB on non-RGB keyboard as grayscale!\n");
	}

	if (!pDevice->RGBLedInfo){
		UINT8 keyCount = 1;

		pDevice->RGBLedInfo = (CROSKBLIGHT_INFO *)ExAllocatePoolZero(NonPagedPool, sizeof(CROSKBLIGHT_INFO) + (keyCount * sizeof(CROSKBLIGHT_KEY_INFO)), CROSKBLIGHT_POOL_TAG);
		if (!pDevice->RGBLedInfo) {
			return STATUS_NO_MEMORY;
		}
		pDevice->RGBLedInfo->LampCount = keyCount;
		pDevice->RGBLedInfo->Width = 15000; //1.5 cm
		pDevice->RGBLedInfo->Height = 15000; //1.5 cm
		pDevice->RGBLedInfo->Depth = 300; //0.3 cm

		pDevice->RGBLedInfo->Keys[0].Pos_X = 0;
		pDevice->RGBLedInfo->Keys[0].Pos_Y = 0;
		pDevice->RGBLedInfo->Keys[0].Pos_Z = 0;
		pDevice->RGBLedInfo->Keys[0].RedCount = 255;
		pDevice->RGBLedInfo->Keys[0].GreenCount = 255;
		pDevice->RGBLedInfo->Keys[0].BlueCount = 255;
		pDevice->RGBLedInfo->Keys[0].IntensityCount = 100;
		pDevice->RGBLedInfo->Keys[0].IsProgrammable = 0;
	}

	if (pDevice->RGBLedInfo->LampCount == 1 && pDevice->RGBLedInfo->Keys[0].IsProgrammable == 0 && !NT_SUCCESS(checkForceRGB)) {
		pDevice->SupportsRGB = FALSE;
#if NOTVM
		CrosKBLightGetBacklight(pDevice, &pDevice->currentBrightness);
#endif
	}
	else {
		pDevice->SupportsRGB = TRUE;
		pDevice->currentBrightness = 100;
		DbgPrint("RGB Supported! Keys: %d\n", pDevice->RGBLedInfo->LampCount);
	}

	return status;
}

NTSTATUS
OnReleaseHardware(
	_In_  WDFDEVICE     FxDevice,
	_In_  WDFCMRESLIST  FxResourcesTranslated
	)
	/*++

	Routine Description:

	Arguments:

	FxDevice - a handle to the framework device object
	FxResourcesTranslated - list of raw hardware resources that
	the PnP manager has assigned to the device

	Return Value:

	Status

	--*/
{
	PCROSKBLIGHT_CONTEXT pDevice = GetDeviceContext(FxDevice);
	NTSTATUS status = STATUS_SUCCESS;

	UNREFERENCED_PARAMETER(FxResourcesTranslated);

	if (pDevice->RGBLedInfo) {
		ExFreePool(pDevice->RGBLedInfo);
		pDevice->RGBLedInfo = NULL;
	}

	return status;
}

NTSTATUS
OnD0Entry(
	_In_  WDFDEVICE               FxDevice,
	_In_  WDF_POWER_DEVICE_STATE  FxPreviousState
	)
	/*++

	Routine Description:

	This routine allocates objects needed by the driver.

	Arguments:

	FxDevice - a handle to the framework device object
	FxPreviousState - previous power state

	Return Value:

	Status

	--*/
{
	UNREFERENCED_PARAMETER(FxPreviousState);

	PCROSKBLIGHT_CONTEXT pDevice = GetDeviceContext(FxDevice);
	NTSTATUS status = STATUS_SUCCESS;

#if NOTVM
	CrosKBLightSetBacklight(pDevice, pDevice->currentBrightness);
#endif

	CrosKBLightCompleteIdleIrp(pDevice);

	return status;
}

NTSTATUS
OnD0Exit(
	_In_  WDFDEVICE               FxDevice,
	_In_  WDF_POWER_DEVICE_STATE  FxTargetState
	)
	/*++

	Routine Description:

	This routine destroys objects needed by the driver.

	Arguments:

	FxDevice - a handle to the framework device object
	FxTargetState - target power state

	Return Value:

	Status

	--*/
{
	PCROSKBLIGHT_CONTEXT pDevice = GetDeviceContext(FxDevice);

	if (FxTargetState != WdfPowerDeviceD3Final &&
		FxTargetState != WdfPowerDevicePrepareForHibernation) {
#if NOTVM
		CrosKBLightSetBacklight(pDevice, 0);
#endif
	}

	return STATUS_SUCCESS;
}

static void update_brightness(PCROSKBLIGHT_CONTEXT pDevice, BYTE brightness) {
	_CROSKBLIGHT_GETLIGHT_REPORT report;
	report.ReportID = REPORTID_KBLIGHT;
	report.Brightness = brightness;

	size_t bytesWritten;
	CrosKBLightProcessVendorReport(pDevice, &report, sizeof(report), &bytesWritten);
}

NTSTATUS
CrosKBLightEvtDeviceAdd(
	IN WDFDRIVER       Driver,
	IN PWDFDEVICE_INIT DeviceInit
	)
{
	NTSTATUS                      status = STATUS_SUCCESS;
	WDF_IO_QUEUE_CONFIG           queueConfig;
	WDF_OBJECT_ATTRIBUTES         attributes;
	WDFDEVICE                     device;
	WDF_INTERRUPT_CONFIG interruptConfig;
	WDFQUEUE                      queue;
	UCHAR                         minorFunction;
	PCROSKBLIGHT_CONTEXT               devContext;

	UNREFERENCED_PARAMETER(Driver);

	PAGED_CODE();

	CrosKBLightPrint(DEBUG_LEVEL_INFO, DBG_PNP,
		"CrosKBLightEvtDeviceAdd called\n");

	//
	// Tell framework this is a filter driver. Filter drivers by default are  
	// not power policy owners. This works well for this driver because
	// HIDclass driver is the power policy owner for HID minidrivers.
	//

	WdfFdoInitSetFilter(DeviceInit);

	{
		WDF_PNPPOWER_EVENT_CALLBACKS pnpCallbacks;
		WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpCallbacks);

		pnpCallbacks.EvtDevicePrepareHardware = OnPrepareHardware;
		pnpCallbacks.EvtDeviceReleaseHardware = OnReleaseHardware;
		pnpCallbacks.EvtDeviceD0Entry = OnD0Entry;
		pnpCallbacks.EvtDeviceD0Exit = OnD0Exit;

		WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpCallbacks);
	}

	//
	// Setup the device context
	//

	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, CROSKBLIGHT_CONTEXT);

	//
	// Create a framework device object.This call will in turn create
	// a WDM device object, attach to the lower stack, and set the
	// appropriate flags and attributes.
	//

	status = WdfDeviceCreate(&DeviceInit, &attributes, &device);

	if (!NT_SUCCESS(status))
	{
		CrosKBLightPrint(DEBUG_LEVEL_ERROR, DBG_PNP,
			"WdfDeviceCreate failed with status code 0x%x\n", status);

		return status;
	}

	{
		WDF_DEVICE_STATE deviceState;
		WDF_DEVICE_STATE_INIT(&deviceState);

		deviceState.NotDisableable = WdfFalse;
		WdfDeviceSetDeviceState(device, &deviceState);
	}

	WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchParallel);

	queueConfig.EvtIoInternalDeviceControl = CrosKBLightEvtInternalDeviceControl;

	status = WdfIoQueueCreate(device,
		&queueConfig,
		WDF_NO_OBJECT_ATTRIBUTES,
		&queue
		);

	if (!NT_SUCCESS(status))
	{
		CrosKBLightPrint(DEBUG_LEVEL_ERROR, DBG_PNP,
			"WdfIoQueueCreate failed 0x%x\n", status);

		return status;
	}

	//
	// Create manual I/O queue to take care of hid report read requests
	//

	devContext = GetDeviceContext(device);
	devContext->FxDevice = device;

	WDF_IO_QUEUE_CONFIG_INIT(&queueConfig, WdfIoQueueDispatchManual);

	queueConfig.PowerManaged = WdfTrue;

	status = WdfIoQueueCreate(device,
		&queueConfig,
		WDF_NO_OBJECT_ATTRIBUTES,
		&devContext->ReportQueue
		);

	if (!NT_SUCCESS(status))
	{
		CrosKBLightPrint(DEBUG_LEVEL_ERROR, DBG_PNP,
			"WdfIoQueueCreate failed 0x%x\n", status);

		return status;
	}

	//
	// Create manual I/O queue to take care of idle power requests
	//

	WDF_IO_QUEUE_CONFIG_INIT(&queueConfig, WdfIoQueueDispatchManual);

	queueConfig.PowerManaged = WdfFalse;

	status = WdfIoQueueCreate(device,
		&queueConfig,
		WDF_NO_OBJECT_ATTRIBUTES,
		&devContext->IdleQueue
	);

	if (!NT_SUCCESS(status))
	{
		CrosKBLightPrint(DEBUG_LEVEL_ERROR, DBG_PNP,
			"WdfIoQueueCreate failed 0x%x\n", status);

		return status;
	}

	return status;
}

void
CrosKBLightIdleIrpWorkItem
(
	IN WDFWORKITEM IdleWorkItem
)
{
	NTSTATUS status;
	PIDLE_WORKITEM_CONTEXT idleWorkItemContext;
	PCROSKBLIGHT_CONTEXT deviceContext;
	PHID_SUBMIT_IDLE_NOTIFICATION_CALLBACK_INFO idleCallbackInfo;

	idleWorkItemContext = GetIdleWorkItemContext(IdleWorkItem);
	NT_ASSERT(idleWorkItemContext != NULL);

	deviceContext = GetDeviceContext(idleWorkItemContext->FxDevice);
	NT_ASSERT(deviceContext != NULL);

	//
	// Get the idle callback info from the workitem context
	//
	PIRP irp = WdfRequestWdmGetIrp(idleWorkItemContext->FxRequest);
	PIO_STACK_LOCATION stackLocation = IoGetCurrentIrpStackLocation(irp);

	idleCallbackInfo = (PHID_SUBMIT_IDLE_NOTIFICATION_CALLBACK_INFO)
		(stackLocation->Parameters.DeviceIoControl.Type3InputBuffer);

	//
	// idleCallbackInfo is validated already, so invoke idle callback
	//
	idleCallbackInfo->IdleCallback(idleCallbackInfo->IdleContext);

	//
	// Park this request in our IdleQueue and mark it as pending
	// This way if the IRP was cancelled, WDF will cancel it for us
	//
	status = WdfRequestForwardToIoQueue(
		idleWorkItemContext->FxRequest,
		deviceContext->IdleQueue);

	if (!NT_SUCCESS(status))
	{
		//
		// IdleQueue is a manual-dispatch, non-power-managed queue. This should
		// *never* fail.
		//

		NT_ASSERTMSG("WdfRequestForwardToIoQueue to IdleQueue failed!", FALSE);

		CrosKBLightPrint(DEBUG_LEVEL_INFO, DBG_IOCTL,
			"Error forwarding idle notification Request:0x%p to IdleQueue:0x%p - %!STATUS!",
			idleWorkItemContext->FxRequest,
			deviceContext->IdleQueue,
			status);

		//
		// Complete the request if we couldnt forward to the Idle Queue
		//
		WdfRequestComplete(idleWorkItemContext->FxRequest, status);
	}
	else
	{
		CrosKBLightPrint(DEBUG_LEVEL_INFO, DBG_IOCTL,
			"Forwarded idle notification Request:0x%p to IdleQueue:0x%p - %!STATUS!",
			idleWorkItemContext->FxRequest,
			deviceContext->IdleQueue,
			status);
	}

	//
	// Delete the workitem since we're done with it
	//
	WdfObjectDelete(IdleWorkItem);

	return;
}

NTSTATUS
CrosKBLightProcessIdleRequest(
	IN PCROSKBLIGHT_CONTEXT pDevice,
	IN WDFREQUEST Request,
	OUT BOOLEAN* Complete
)
{
	PHID_SUBMIT_IDLE_NOTIFICATION_CALLBACK_INFO idleCallbackInfo;
	PIRP irp;
	PIO_STACK_LOCATION irpSp;
	NTSTATUS status;

	NT_ASSERT(Complete != NULL);
	*Complete = TRUE;

	//
	// Retrieve request parameters and validate
	//
	irp = WdfRequestWdmGetIrp(Request);
	irpSp = IoGetCurrentIrpStackLocation(irp);

	if (irpSp->Parameters.DeviceIoControl.InputBufferLength <
		sizeof(HID_SUBMIT_IDLE_NOTIFICATION_CALLBACK_INFO))
	{
		status = STATUS_INVALID_BUFFER_SIZE;

		CrosKBLightPrint(DEBUG_LEVEL_INFO, DBG_IOCTL,
			"Error: Input buffer is too small to process idle request - %!STATUS!",
			status);

		goto exit;
	}

	//
	// Grab the callback
	//
	idleCallbackInfo = (PHID_SUBMIT_IDLE_NOTIFICATION_CALLBACK_INFO)
		irpSp->Parameters.DeviceIoControl.Type3InputBuffer;

	NT_ASSERT(idleCallbackInfo != NULL);

	if (idleCallbackInfo == NULL || idleCallbackInfo->IdleCallback == NULL)
	{
		status = STATUS_NO_CALLBACK_ACTIVE;
		CrosKBLightPrint(DEBUG_LEVEL_INFO, DBG_IOCTL,
			"Error: Idle Notification request %p has no idle callback info - %!STATUS!",
			Request,
			status);
		goto exit;
	}

	{
		//
		// Create a workitem for the idle callback
		//
		WDF_OBJECT_ATTRIBUTES workItemAttributes;
		WDF_WORKITEM_CONFIG workitemConfig;
		WDFWORKITEM idleWorkItem;
		PIDLE_WORKITEM_CONTEXT idleWorkItemContext;

		WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&workItemAttributes, IDLE_WORKITEM_CONTEXT);
		workItemAttributes.ParentObject = pDevice->FxDevice;

		WDF_WORKITEM_CONFIG_INIT(&workitemConfig, CrosKBLightIdleIrpWorkItem);

		status = WdfWorkItemCreate(
			&workitemConfig,
			&workItemAttributes,
			&idleWorkItem
		);

		if (!NT_SUCCESS(status)) {
			CrosKBLightPrint(DEBUG_LEVEL_INFO, DBG_IOCTL,
				"Error creating creating idle work item - %!STATUS!",
				status);
			goto exit;
		}

		//
		// Set the workitem context
		//
		idleWorkItemContext = GetIdleWorkItemContext(idleWorkItem);
		idleWorkItemContext->FxDevice = pDevice->FxDevice;
		idleWorkItemContext->FxRequest = Request;

		//
		// Enqueue a workitem for the idle callback
		//
		WdfWorkItemEnqueue(idleWorkItem);

		//
		// Mark the request as pending so that 
		// we can complete it when we come out of idle
		//
		*Complete = FALSE;
	}

exit:

	return status;
}

VOID
CrosKBLightCompleteIdleIrp(
	IN PCROSKBLIGHT_CONTEXT FxDeviceContext
)
/*++
Routine Description:
	This is invoked when we enter D0.
	We simply complete the Idle Irp if it hasn't been cancelled already.
Arguments:
	FxDeviceContext -  Pointer to Device Context for the device
Return Value:
--*/
{
	NTSTATUS status;
	WDFREQUEST request = NULL;

	//
	// Lets try to retrieve the Idle IRP from the Idle queue
	//
	status = WdfIoQueueRetrieveNextRequest(
		FxDeviceContext->IdleQueue,
		&request);

	//
	// We did not find the Idle IRP, maybe it was cancelled
	// 
	if (!NT_SUCCESS(status) || (request == NULL))
	{
		CrosKBLightPrint(DEBUG_LEVEL_INFO, DBG_IOCTL,
			"Error finding idle notification request in IdleQueue:0x%p - %!STATUS!",
			FxDeviceContext->IdleQueue,
			status);
	}
	else
	{
		//
		// Complete the Idle IRP
		//
		WdfRequestComplete(request, status);

		CrosKBLightPrint(DEBUG_LEVEL_INFO, DBG_IOCTL,
			"Completed idle notification Request:0x%p from IdleQueue:0x%p - %!STATUS!",
			request,
			FxDeviceContext->IdleQueue,
			status);
	}

	return;
}

VOID
CrosKBLightEvtInternalDeviceControl(
	IN WDFQUEUE     Queue,
	IN WDFREQUEST   Request,
	IN size_t       OutputBufferLength,
	IN size_t       InputBufferLength,
	IN ULONG        IoControlCode
	)
{
	NTSTATUS            status = STATUS_SUCCESS;
	WDFDEVICE           device;
	PCROSKBLIGHT_CONTEXT     devContext;
	BOOLEAN             completeRequest = TRUE;

	UNREFERENCED_PARAMETER(OutputBufferLength);
	UNREFERENCED_PARAMETER(InputBufferLength);

	device = WdfIoQueueGetDevice(Queue);
	devContext = GetDeviceContext(device);

	CrosKBLightPrint(DEBUG_LEVEL_INFO, DBG_IOCTL,
		"%s, Queue:0x%p, Request:0x%p\n",
		DbgHidInternalIoctlString(IoControlCode),
		Queue,
		Request
		);

	//
	// Please note that HIDCLASS provides the buffer in the Irp->UserBuffer
	// field irrespective of the ioctl buffer type. However, framework is very
	// strict about type checking. You cannot get Irp->UserBuffer by using
	// WdfRequestRetrieveOutputMemory if the ioctl is not a METHOD_NEITHER
	// internal ioctl. So depending on the ioctl code, we will either
	// use retreive function or escape to WDM to get the UserBuffer.
	//

	switch (IoControlCode)
	{

	case IOCTL_HID_GET_DEVICE_DESCRIPTOR:
		//
		// Retrieves the device's HID descriptor.
		//
		status = CrosKBLightGetHidDescriptor(device, Request);
		break;

	case IOCTL_HID_GET_DEVICE_ATTRIBUTES:
		//
		//Retrieves a device's attributes in a HID_DEVICE_ATTRIBUTES structure.
		//
		status = CrosKBLightGetDeviceAttributes(Request);
		break;

	case IOCTL_HID_GET_REPORT_DESCRIPTOR:
		//
		//Obtains the report descriptor for the HID device.
		//
		status = CrosKBLightGetReportDescriptor(device, Request);
		break;

	case IOCTL_HID_GET_STRING:
		//
		// Requests that the HID minidriver retrieve a human-readable string
		// for either the manufacturer ID, the product ID, or the serial number
		// from the string descriptor of the device. The minidriver must send
		// a Get String Descriptor request to the device, in order to retrieve
		// the string descriptor, then it must extract the string at the
		// appropriate index from the string descriptor and return it in the
		// output buffer indicated by the IRP. Before sending the Get String
		// Descriptor request, the minidriver must retrieve the appropriate
		// index for the manufacturer ID, the product ID or the serial number
		// from the device extension of a top level collection associated with
		// the device.
		//
		status = CrosKBLightGetString(Request);
		break;

	case IOCTL_HID_WRITE_REPORT:
	case IOCTL_HID_SET_OUTPUT_REPORT:
		//
		//Transmits a class driver-supplied report to the device.
		//
		status = CrosKBLightWriteReport(devContext, Request);
		break;

	case IOCTL_HID_READ_REPORT:
	case IOCTL_HID_GET_INPUT_REPORT:
		//
		// Returns a report from the device into a class driver-supplied buffer.
		// 
		status = CrosKBLightReadReport(devContext, Request, &completeRequest);
		break;

	case IOCTL_HID_SET_FEATURE:
		//
		// This sends a HID class feature report to a top-level collection of
		// a HID class device.
		//
		status = CrosKBLightSetFeature(devContext, Request, &completeRequest);
		break;

	case IOCTL_HID_GET_FEATURE:
		//
		// returns a feature report associated with a top-level collection
		//
		status = CrosKBLightGetFeature(devContext, Request, &completeRequest);
		break;

	case IOCTL_HID_SEND_IDLE_NOTIFICATION_REQUEST:
		//Handle HID Idle request
		status = CrosKBLightProcessIdleRequest(devContext, Request, &completeRequest);
		break;

	case IOCTL_HID_ACTIVATE_DEVICE:
		//
		// Makes the device ready for I/O operations.
		//
	case IOCTL_HID_DEACTIVATE_DEVICE:
		//
		// Causes the device to cease operations and terminate all outstanding
		// I/O requests.
		//
	default:
		status = STATUS_NOT_SUPPORTED;
		break;
	}

	if (completeRequest)
	{
		WdfRequestComplete(Request, status);

		CrosKBLightPrint(DEBUG_LEVEL_INFO, DBG_IOCTL,
			"%s completed, Queue:0x%p, Request:0x%p\n",
			DbgHidInternalIoctlString(IoControlCode),
			Queue,
			Request
			);
	}
	else
	{
		CrosKBLightPrint(DEBUG_LEVEL_INFO, DBG_IOCTL,
			"%s deferred, Queue:0x%p, Request:0x%p\n",
			DbgHidInternalIoctlString(IoControlCode),
			Queue,
			Request
			);
	}

	return;
}

NTSTATUS
CrosKBLightGetHidDescriptor(
	IN WDFDEVICE Device,
	IN WDFREQUEST Request
	)
{
	NTSTATUS            status = STATUS_SUCCESS;
	size_t              bytesToCopy = 0;
	WDFMEMORY           memory;
	PCROSKBLIGHT_CONTEXT devContext = GetDeviceContext(Device);

	UNREFERENCED_PARAMETER(Device);

	CrosKBLightPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL,
		"CrosKBLightGetHidDescriptor Entry\n");

	//
	// This IOCTL is METHOD_NEITHER so WdfRequestRetrieveOutputMemory
	// will correctly retrieve buffer from Irp->UserBuffer. 
	// Remember that HIDCLASS provides the buffer in the Irp->UserBuffer
	// field irrespective of the ioctl buffer type. However, framework is very
	// strict about type checking. You cannot get Irp->UserBuffer by using
	// WdfRequestRetrieveOutputMemory if the ioctl is not a METHOD_NEITHER
	// internal ioctl.
	//
	status = WdfRequestRetrieveOutputMemory(Request, &memory);

	if (!NT_SUCCESS(status))
	{
		CrosKBLightPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL,
			"WdfRequestRetrieveOutputMemory failed 0x%x\n", status);

		return status;
	}

	//
	// Use hardcoded "HID Descriptor" 
	//
	if (devContext->SupportsRGB) {
		bytesToCopy = DefaultHidDescriptorRGB.bLength;
	}
	else {
		bytesToCopy = DefaultHidDescriptorLegacy.bLength;
	}

	if (bytesToCopy == 0)
	{
		status = STATUS_INVALID_DEVICE_STATE;

		CrosKBLightPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL,
			"DefaultHidDescriptor is zero, 0x%x\n", status);

		return status;
	}

	if (devContext->SupportsRGB) {
		status = WdfMemoryCopyFromBuffer(memory,
			0, // Offset
			(PVOID)&DefaultHidDescriptorRGB,
			bytesToCopy);
	}
	else {
		status = WdfMemoryCopyFromBuffer(memory,
			0, // Offset
			(PVOID)&DefaultHidDescriptorLegacy,
			bytesToCopy);
	}

	if (!NT_SUCCESS(status))
	{
		CrosKBLightPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL,
			"WdfMemoryCopyFromBuffer failed 0x%x\n", status);

		return status;
	}

	//
	// Report how many bytes were copied
	//
	WdfRequestSetInformation(Request, bytesToCopy);

	CrosKBLightPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL,
		"CrosKBLightGetHidDescriptor Exit = 0x%x\n", status);

	return status;
}

NTSTATUS
CrosKBLightGetReportDescriptor(
	IN WDFDEVICE Device,
	IN WDFREQUEST Request
	)
{
	NTSTATUS            status = STATUS_SUCCESS;
	ULONG_PTR           bytesToCopy;
	WDFMEMORY           memory;
	PCROSKBLIGHT_CONTEXT devContext = GetDeviceContext(Device);

	UNREFERENCED_PARAMETER(Device);

	CrosKBLightPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL,
		"CrosKBLightGetReportDescriptor Entry\n");

	//
	// This IOCTL is METHOD_NEITHER so WdfRequestRetrieveOutputMemory
	// will correctly retrieve buffer from Irp->UserBuffer. 
	// Remember that HIDCLASS provides the buffer in the Irp->UserBuffer
	// field irrespective of the ioctl buffer type. However, framework is very
	// strict about type checking. You cannot get Irp->UserBuffer by using
	// WdfRequestRetrieveOutputMemory if the ioctl is not a METHOD_NEITHER
	// internal ioctl.
	//
	status = WdfRequestRetrieveOutputMemory(Request, &memory);
	if (!NT_SUCCESS(status))
	{
		CrosKBLightPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL,
			"WdfRequestRetrieveOutputMemory failed 0x%x\n", status);

		return status;
	}

	//
	// Use hardcoded Report descriptor
	//
	if (devContext->SupportsRGB) {
		bytesToCopy = DefaultHidDescriptorRGB.DescriptorList[0].wReportLength;
	}
	else {
		bytesToCopy = DefaultHidDescriptorLegacy.DescriptorList[0].wReportLength;
	}

	if (bytesToCopy == 0)
	{
		status = STATUS_INVALID_DEVICE_STATE;

		CrosKBLightPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL,
			"DefaultHidDescriptor's reportLength is zero, 0x%x\n", status);

		return status;
	}

	if (devContext->SupportsRGB) {
		status = WdfMemoryCopyFromBuffer(memory,
			0,
			(PVOID)DefaultReportDescriptorRGB,
			bytesToCopy);
	}
	else {
		status = WdfMemoryCopyFromBuffer(memory,
			0,
			(PVOID)DefaultReportDescriptorLegacy,
			bytesToCopy);
	}
	if (!NT_SUCCESS(status))
	{
		CrosKBLightPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL,
			"WdfMemoryCopyFromBuffer failed 0x%x\n", status);

		return status;
	}

	//
	// Report how many bytes were copied
	//
	WdfRequestSetInformation(Request, bytesToCopy);

	CrosKBLightPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL,
		"CrosKBLightGetReportDescriptor Exit = 0x%x\n", status);

	return status;
}


NTSTATUS
CrosKBLightGetDeviceAttributes(
	IN WDFREQUEST Request
	)
{
	NTSTATUS                 status = STATUS_SUCCESS;
	PHID_DEVICE_ATTRIBUTES   deviceAttributes = NULL;

	CrosKBLightPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL,
		"CrosKBLightGetDeviceAttributes Entry\n");

	//
	// This IOCTL is METHOD_NEITHER so WdfRequestRetrieveOutputMemory
	// will correctly retrieve buffer from Irp->UserBuffer. 
	// Remember that HIDCLASS provides the buffer in the Irp->UserBuffer
	// field irrespective of the ioctl buffer type. However, framework is very
	// strict about type checking. You cannot get Irp->UserBuffer by using
	// WdfRequestRetrieveOutputMemory if the ioctl is not a METHOD_NEITHER
	// internal ioctl.
	//
	status = WdfRequestRetrieveOutputBuffer(Request,
		sizeof(HID_DEVICE_ATTRIBUTES),
		(PVOID *)&deviceAttributes,
		NULL);
	if (!NT_SUCCESS(status))
	{
		CrosKBLightPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL,
			"WdfRequestRetrieveOutputBuffer failed 0x%x\n", status);

		return status;
	}

	//
	// Set USB device descriptor
	//

	deviceAttributes->Size = sizeof(HID_DEVICE_ATTRIBUTES);
	deviceAttributes->VendorID = CROSKBLIGHT_VID;
	deviceAttributes->ProductID = CROSKBLIGHT_PID;
	deviceAttributes->VersionNumber = CROSKBLIGHT_VERSION;

	//
	// Report how many bytes were copied
	//
	WdfRequestSetInformation(Request, sizeof(HID_DEVICE_ATTRIBUTES));

	CrosKBLightPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL,
		"CrosKBLightGetDeviceAttributes Exit = 0x%x\n", status);

	return status;
}

NTSTATUS
CrosKBLightGetString(
	IN WDFREQUEST Request
	)
{

	NTSTATUS status = STATUS_SUCCESS;
	PWSTR pwstrID;
	size_t lenID;
	WDF_REQUEST_PARAMETERS params;
	void *pStringBuffer = NULL;

	CrosKBLightPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL,
		"CrosKBLightGetString Entry\n");

	WDF_REQUEST_PARAMETERS_INIT(&params);
	WdfRequestGetParameters(Request, &params);

	switch ((ULONG_PTR)params.Parameters.DeviceIoControl.Type3InputBuffer & 0xFFFF)
	{
	case HID_STRING_ID_IMANUFACTURER:
		pwstrID = L"CrosKBLight.\0";
		break;

	case HID_STRING_ID_IPRODUCT:
		pwstrID = L"MaxTouch Touch Screen\0";
		break;

	case HID_STRING_ID_ISERIALNUMBER:
		pwstrID = L"123123123\0";
		break;

	default:
		pwstrID = NULL;
		break;
	}

	lenID = pwstrID ? wcslen(pwstrID)*sizeof(WCHAR) + sizeof(UNICODE_NULL) : 0;

	if (pwstrID == NULL)
	{

		CrosKBLightPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL,
			"CrosKBLightGetString Invalid request type\n");

		status = STATUS_INVALID_PARAMETER;

		return status;
	}

	status = WdfRequestRetrieveOutputBuffer(Request,
		lenID,
		&pStringBuffer,
		&lenID);

	if (!NT_SUCCESS(status))
	{

		CrosKBLightPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL,
			"CrosKBLightGetString WdfRequestRetrieveOutputBuffer failed Status 0x%x\n", status);

		return status;
	}

	RtlCopyMemory(pStringBuffer, pwstrID, lenID);

	WdfRequestSetInformation(Request, lenID);

	CrosKBLightPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL,
		"CrosKBLightGetString Exit = 0x%x\n", status);

	return status;
}

NTSTATUS
CrosKBLightWriteReport(
	IN PCROSKBLIGHT_CONTEXT DevContext,
	IN WDFREQUEST Request
	)
{
	NTSTATUS status = STATUS_SUCCESS;
	WDF_REQUEST_PARAMETERS params;
	PHID_XFER_PACKET transferPacket = NULL;
	size_t bytesWritten = 0;

	CrosKBLightPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL,
		"CrosKBLightWriteReport Entry\n");

	WDF_REQUEST_PARAMETERS_INIT(&params);
	WdfRequestGetParameters(Request, &params);

	if (params.Parameters.DeviceIoControl.InputBufferLength < sizeof(HID_XFER_PACKET))
	{
		CrosKBLightPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL,
			"CrosKBLightWriteReport Xfer packet too small\n");

		status = STATUS_BUFFER_TOO_SMALL;
	}
	else
	{

		transferPacket = (PHID_XFER_PACKET)WdfRequestWdmGetIrp(Request)->UserBuffer;

		if (transferPacket == NULL)
		{
			CrosKBLightPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL,
				"CrosKBLightWriteReport No xfer packet\n");

			status = STATUS_INVALID_DEVICE_REQUEST;
		}
		else
		{
			//
			// switch on the report id
			//

			switch (transferPacket->reportId)
			{
			case REPORTID_KBLIGHT: {
				CrosKBLightSettingsReport* pReport = (CrosKBLightSettingsReport*)transferPacket->reportBuffer;

				int reg = pReport->SetBrightness;
				int val = pReport->Brightness;

				if (reg == 0) {
					int brightness = DevContext->currentBrightness;
					update_brightness(DevContext, brightness);
				}
				else if (reg == 1) {
					DevContext->currentBrightness = val;
#if NOTVM
					CrosKBLightSetBacklight(DevContext, DevContext->currentBrightness);
#endif
				}
				break;
			}
			default:
				CrosKBLightPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL,
					"CrosKBLightWriteReport Unhandled report type %d\n", transferPacket->reportId);

				status = STATUS_INVALID_PARAMETER;

				break;
			}
		}
	}

	CrosKBLightPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL,
		"CrosKBLightWriteReport Exit = 0x%x\n", status);

	return status;

}

NTSTATUS
CrosKBLightProcessVendorReport(
	IN PCROSKBLIGHT_CONTEXT DevContext,
	IN PVOID ReportBuffer,
	IN ULONG ReportBufferLen,
	OUT size_t* BytesWritten
	)
{
	NTSTATUS status = STATUS_SUCCESS;
	WDFREQUEST reqRead;
	PVOID pReadReport = NULL;
	size_t bytesReturned = 0;

	CrosKBLightPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL,
		"CrosKBLightProcessVendorReport Entry\n");

	status = WdfIoQueueRetrieveNextRequest(DevContext->ReportQueue,
		&reqRead);

	if (NT_SUCCESS(status))
	{
		status = WdfRequestRetrieveOutputBuffer(reqRead,
			ReportBufferLen,
			&pReadReport,
			&bytesReturned);

		if (NT_SUCCESS(status))
		{
			//
			// Copy ReportBuffer into read request
			//

			if (bytesReturned > ReportBufferLen)
			{
				bytesReturned = ReportBufferLen;
			}

			RtlCopyMemory(pReadReport,
				ReportBuffer,
				bytesReturned);

			//
			// Complete read with the number of bytes returned as info
			//

			WdfRequestCompleteWithInformation(reqRead,
				status,
				bytesReturned);

			CrosKBLightPrint(DEBUG_LEVEL_INFO, DBG_IOCTL,
				"CrosKBLightProcessVendorReport %d bytes returned\n", bytesReturned);

			//
			// Return the number of bytes written for the write request completion
			//

			*BytesWritten = bytesReturned;

			CrosKBLightPrint(DEBUG_LEVEL_INFO, DBG_IOCTL,
				"%s completed, Queue:0x%p, Request:0x%p\n",
				DbgHidInternalIoctlString(IOCTL_HID_READ_REPORT),
				DevContext->ReportQueue,
				reqRead);
		}
		else
		{
			CrosKBLightPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL,
				"WdfRequestRetrieveOutputBuffer failed Status 0x%x\n", status);
		}
	}
	else
	{
		CrosKBLightPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL,
			"WdfIoQueueRetrieveNextRequest failed Status 0x%x\n", status);
	}

	CrosKBLightPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL,
		"CrosKBLightProcessVendorReport Exit = 0x%x\n", status);

	return status;
}

NTSTATUS
CrosKBLightReadReport(
	IN PCROSKBLIGHT_CONTEXT DevContext,
	IN WDFREQUEST Request,
	OUT BOOLEAN* CompleteRequest
	)
{
	NTSTATUS status = STATUS_SUCCESS;

	CrosKBLightPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL,
		"CrosKBLightReadReport Entry\n");

	//
	// Forward this read request to our manual queue
	// (in other words, we are going to defer this request
	// until we have a corresponding write request to
	// match it with)
	//

	status = WdfRequestForwardToIoQueue(Request, DevContext->ReportQueue);

	if (!NT_SUCCESS(status))
	{
		CrosKBLightPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL,
			"WdfRequestForwardToIoQueue failed Status 0x%x\n", status);
	}
	else
	{
		*CompleteRequest = FALSE;
	}

	CrosKBLightPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL,
		"CrosKBLightReadReport Exit = 0x%x\n", status);

	return status;
}

NTSTATUS
CrosKBLightSetFeature(
	IN PCROSKBLIGHT_CONTEXT DevContext,
	IN WDFREQUEST Request,
	OUT BOOLEAN* CompleteRequest
	)
{
	NTSTATUS status = STATUS_SUCCESS;
	WDF_REQUEST_PARAMETERS params;
	PHID_XFER_PACKET transferPacket = NULL;

	CrosKBLightPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL,
		"CrosKBLightSetFeature Entry\n");

	WDF_REQUEST_PARAMETERS_INIT(&params);
	WdfRequestGetParameters(Request, &params);

	if (params.Parameters.DeviceIoControl.InputBufferLength < sizeof(HID_XFER_PACKET))
	{
		CrosKBLightPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL,
			"CrosKBLightSetFeature Xfer packet too small\n");

		status = STATUS_BUFFER_TOO_SMALL;
	}
	else
	{

		transferPacket = (PHID_XFER_PACKET)WdfRequestWdmGetIrp(Request)->UserBuffer;

		if (transferPacket == NULL)
		{
			CrosKBLightPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL,
				"CrosKBLightWriteReport No xfer packet\n");

			status = STATUS_INVALID_DEVICE_REQUEST;
		}
		else
		{
			//
			// switch on the report id
			//

			switch (transferPacket->reportId)
			{
			case REPORT_ID_LIGHTING_LAMP_ATTRIBUTES_REQUEST: {
				if (transferPacket->reportBufferLen >= sizeof(LampArrayAttributesRequestReport)) {
					LampArrayAttributesRequestReport* requestReport = (LampArrayAttributesRequestReport*)transferPacket->reportBuffer;
					CrosKBLightPrint(DEBUG_LEVEL_INFO, DBG_IOCTL,
						"Update Current Lamp ID %d\n", requestReport->LampID);
					DevContext->CurrentLampID = requestReport->LampID;
					break;
				}
			}
			case REPORT_ID_LIGHTING_LAMP_MULTI_UPDATE: {
				if (transferPacket->reportBufferLen >= sizeof(LampArrayMultiUpdateReport)) {
					LampArrayMultiUpdateReport* multiUpdateReport = (LampArrayMultiUpdateReport*)transferPacket->reportBuffer;
					CrosKBLightPrint(DEBUG_LEVEL_INFO, DBG_IOCTL,
						"Count: %d, Flags: 0x%x\n", multiUpdateReport->LampCount, multiUpdateReport->Flags);
					for (int i = 0; i < multiUpdateReport->LampCount; i++) {
						CrosKBLightPrint(DEBUG_LEVEL_INFO, DBG_IOCTL,
							"ID: %d. Color: %d %d %d %d\n", multiUpdateReport->LampIds[i],
							multiUpdateReport->Colors[i].Red, multiUpdateReport->Colors[i].Green, multiUpdateReport->Colors[i].Blue, multiUpdateReport->Colors[i].Intensity);

						UINT16 LampID = multiUpdateReport->LampIds[i];
						if (LampID < DevContext->RGBLedInfo->LampCount) {
							DevContext->KeyStates[LampID].r = multiUpdateReport->Colors[i].Red;
							DevContext->KeyStates[LampID].g = multiUpdateReport->Colors[i].Green;
							DevContext->KeyStates[LampID].b = multiUpdateReport->Colors[i].Blue;
						}
					}

#if NOTVM
					if (DevContext->SupportsRGB) {
						int keysUpdate = min(EC_RGBKBD_MAX_KEY_COUNT, DevContext->RGBLedInfo->LampCount);

						for (int i = 0; i < keysUpdate; i += KEYSPAGE) {
							int keysPageLen = min(KEYSPAGE, keysUpdate - i);
							size_t outLen = sizeof(ec_params_rgbkbd_set_color) + keysPageLen * sizeof(rgb_s);
							ec_params_rgbkbd_set_color* setColorParams = (ec_params_rgbkbd_set_color*)ExAllocatePoolZero(NonPagedPool, outLen, CROSKBLIGHT_POOL_TAG);
							if (setColorParams) {
								setColorParams->start_key = 1 + i;
								setColorParams->length = min(KEYSPAGE, keysUpdate - i);
								for (int k = 0; k < setColorParams->length; k++) {
									setColorParams->color[k] = DevContext->KeyStates[i + k];
								}

								NTSTATUS cmdSts = send_ec_command(DevContext, EC_CMD_RGBKBD_SET_COLOR, 0, (UINT8*)setColorParams, outLen,
									NULL, 0);
								CrosKBLightPrint(DEBUG_LEVEL_INFO, DBG_IOCTL,
									"Set Multi States: 0x%x\n", cmdSts);
								ExFreePool(setColorParams);
							}
						}
					}
					else {
						if (multiUpdateReport->LampCount > 0) {
							CrosKBLightSetBacklight(DevContext, multiUpdateReport->Colors[0].Intensity);
						}
					}
#endif
					break;
				}
			}
			case REPORT_ID_LIGHTING_LAMP_RANGE_UPDATE: {
				if (transferPacket->reportBufferLen >= sizeof(LampArrayRangeUpdateReport)) {
					LampArrayRangeUpdateReport* rangeUpdateReport = (LampArrayRangeUpdateReport*)transferPacket->reportBuffer;
					CrosKBLightPrint(DEBUG_LEVEL_INFO, DBG_IOCTL,
						"Range Update. Start: %d, End: %d, Color: %d %d %d %d\n",
						rangeUpdateReport->LampIdStart, rangeUpdateReport->LampIdEnd,
						rangeUpdateReport->Color.Red, rangeUpdateReport->Color.Green, rangeUpdateReport->Color.Blue, rangeUpdateReport->Color.Intensity);

					for (int i = rangeUpdateReport->LampIdStart; i <= min(rangeUpdateReport->LampIdEnd, DevContext->RGBLedInfo->LampCount - 1); i++) {
						DevContext->KeyStates[i].r = rangeUpdateReport->Color.Red;
						DevContext->KeyStates[i].g = rangeUpdateReport->Color.Green;
						DevContext->KeyStates[i].b = rangeUpdateReport->Color.Blue;
					}

#if NOTVM
					if (DevContext->SupportsRGB) {
						int keysUpdate = min(EC_RGBKBD_MAX_KEY_COUNT, DevContext->RGBLedInfo->LampCount);

						for (int i = 0; i < keysUpdate; i += KEYSPAGE) {
							int keysPageLen = min(KEYSPAGE, keysUpdate - i);
							size_t outLen = sizeof(ec_params_rgbkbd_set_color) + keysPageLen * sizeof(rgb_s);
							ec_params_rgbkbd_set_color* setColorParams = (ec_params_rgbkbd_set_color*)ExAllocatePoolZero(NonPagedPool, outLen, CROSKBLIGHT_POOL_TAG);
							if (setColorParams) {
								setColorParams->start_key = 1 + i;
								setColorParams->length = min(KEYSPAGE, keysUpdate - i);
								for (int k = 0; k < setColorParams->length; k++) {
									setColorParams->color[k] = DevContext->KeyStates[i + k];
								}

								NTSTATUS cmdSts = send_ec_command(DevContext, EC_CMD_RGBKBD_SET_COLOR, 0, (UINT8*)setColorParams, outLen,
									NULL, 0);
								CrosKBLightPrint(DEBUG_LEVEL_INFO, DBG_IOCTL,
									"Set Multi States: 0x%x\n", cmdSts);
								ExFreePool(setColorParams);
							}
						}
					}
					else {
						CrosKBLightSetBacklight(DevContext, rangeUpdateReport->Color.Intensity);
					}
#endif
					break;
				}
			}case REPORT_ID_LIGHTING_LAMP_ARRAY_CONTROL: {
				if (transferPacket->reportBufferLen >= sizeof(LampArrayControlReport)) {
					LampArrayControlReport* controlReport = (LampArrayControlReport*)transferPacket->reportBuffer;
					CrosKBLightPrint(DEBUG_LEVEL_INFO, DBG_IOCTL,
						"Autonomous Mode? %d\n", controlReport->AutonomousMode);

#if NOTVM
					if (DevContext->SupportsRGB) {
						ec_params_rgbkbd* setRGBParam = (ec_params_rgbkbd*)ExAllocatePoolZero(
							NonPagedPool, sizeof(ec_params_rgbkbd), CROSKBLIGHT_POOL_TAG);
						if (setRGBParam) {
							setRGBParam->subcmd = EC_RGBKBD_SUBCMD_DEMO;
							setRGBParam->demo = controlReport->AutonomousMode ? EC_RGBKBD_DEMO_FLOW : EC_RGBKBD_DEMO_OFF;
							NTSTATUS cmdSts = send_ec_command(DevContext, EC_CMD_RGBKBD, 0, (UINT8*)setRGBParam, sizeof(ec_params_rgbkbd),
								NULL, 0);
							CrosKBLightPrint(DEBUG_LEVEL_INFO, DBG_IOCTL,
								"Set Autonomous: 0x%x\n", cmdSts);
							ExFreePool(setRGBParam);
						}
					}
#endif
					break;
				}
			}
			default:

				CrosKBLightPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL,
					"CrosKBLightSetFeature Unhandled report type %d\n", transferPacket->reportId);

				status = STATUS_INVALID_PARAMETER;

				break;
			}
		}
	}

	CrosKBLightPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL,
		"CrosKBLightSetFeature Exit = 0x%x\n", status);

	return status;
}

NTSTATUS
CrosKBLightGetFeature(
	IN PCROSKBLIGHT_CONTEXT DevContext,
	IN WDFREQUEST Request,
	OUT BOOLEAN* CompleteRequest
	)
{
	NTSTATUS status = STATUS_SUCCESS;
	WDF_REQUEST_PARAMETERS params;
	PHID_XFER_PACKET transferPacket = NULL;

	CrosKBLightPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL,
		"CrosKBLightGetFeature Entry\n");

	WDF_REQUEST_PARAMETERS_INIT(&params);
	WdfRequestGetParameters(Request, &params);

	if (params.Parameters.DeviceIoControl.OutputBufferLength < sizeof(HID_XFER_PACKET))
	{
		CrosKBLightPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL,
			"CrosKBLightGetFeature Xfer packet too small\n");

		status = STATUS_BUFFER_TOO_SMALL;
	}
	else
	{

		transferPacket = (PHID_XFER_PACKET)WdfRequestWdmGetIrp(Request)->UserBuffer;

		if (transferPacket == NULL)
		{
			CrosKBLightPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL,
				"CrosKBLightGetFeature No xfer packet\n");

			status = STATUS_INVALID_DEVICE_REQUEST;
		}
		else
		{
			//
			// switch on the report id
			//

			switch (transferPacket->reportId)
			{
			case REPORT_ID_LIGHTING_LAMP_ARRAY_ATTRIBUTES: {
				if (transferPacket->reportBufferLen >= sizeof(LampArrayAttributesReport)) {
					LampArrayAttributesReport* attributesReport = (LampArrayAttributesReport *)transferPacket->reportBuffer;
					attributesReport->LampCount = DevContext->RGBLedInfo->LampCount;
					attributesReport->Width = DevContext->RGBLedInfo->Width;
					attributesReport->Height = DevContext->RGBLedInfo->Height;
					attributesReport->Depth = DevContext->RGBLedInfo->Depth;
					attributesReport->LampArrayKind = 1; //Keyboard
					attributesReport->MinUpdateInterval = 1000; //10 ms
					break;
				}
			}
			case REPORT_ID_LIGHTING_LAMP_ATTRIBUTES_RESPONSE: {
				if (transferPacket->reportBufferLen >= sizeof(LampArrayAttributesResponseReport)) {
					LampArrayAttributesResponseReport* responseReport = (LampArrayAttributesResponseReport*)transferPacket->reportBuffer;
					UINT8 lampID = DevContext->CurrentLampID;
					if (lampID > DevContext->RGBLedInfo->LampCount) {
						lampID = 0;
					}

					responseReport->LampId = lampID;
					responseReport->LampPosition.x = DevContext->RGBLedInfo->Keys[lampID].Pos_X;
					responseReport->LampPosition.y = DevContext->RGBLedInfo->Keys[lampID].Pos_Y;
					responseReport->LampPosition.z = DevContext->RGBLedInfo->Keys[lampID].Pos_Z;
					responseReport->UpdateLatency = 1000; //10 ms
					responseReport->LampPurposes = 1; //Keyboard
					responseReport->RedLevelCount = DevContext->RGBLedInfo->Keys[lampID].RedCount;
					responseReport->GreenLevelCount = DevContext->RGBLedInfo->Keys[lampID].GreenCount;
					responseReport->BlueLevelCount = DevContext->RGBLedInfo->Keys[lampID].BlueCount;
					responseReport->IntensityLevelCount = DevContext->RGBLedInfo->Keys[lampID].IntensityCount;
					responseReport->IsProgrammable = DevContext->RGBLedInfo->Keys[lampID].IsProgrammable;
					responseReport->InputBinding = DevContext->RGBLedInfo->Keys[lampID].InputBinding;
					break;
				}
			}
			default:

				CrosKBLightPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL,
					"CrosKBLightGetFeature Unhandled report type %d\n", transferPacket->reportId);

				status = STATUS_INVALID_PARAMETER;

				break;
			}
		}
	}

	CrosKBLightPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL,
		"CrosKBLightGetFeature Exit = 0x%x\n", status);

	return status;
}

PCHAR
DbgHidInternalIoctlString(
	IN ULONG IoControlCode
	)
{
	switch (IoControlCode)
	{
	case IOCTL_HID_GET_DEVICE_DESCRIPTOR:
		return "IOCTL_HID_GET_DEVICE_DESCRIPTOR";
	case IOCTL_HID_GET_REPORT_DESCRIPTOR:
		return "IOCTL_HID_GET_REPORT_DESCRIPTOR";
	case IOCTL_HID_READ_REPORT:
		return "IOCTL_HID_READ_REPORT";
	case IOCTL_HID_GET_DEVICE_ATTRIBUTES:
		return "IOCTL_HID_GET_DEVICE_ATTRIBUTES";
	case IOCTL_HID_WRITE_REPORT:
		return "IOCTL_HID_WRITE_REPORT";
	case IOCTL_HID_SET_FEATURE:
		return "IOCTL_HID_SET_FEATURE";
	case IOCTL_HID_GET_FEATURE:
		return "IOCTL_HID_GET_FEATURE";
	case IOCTL_HID_GET_STRING:
		return "IOCTL_HID_GET_STRING";
	case IOCTL_HID_ACTIVATE_DEVICE:
		return "IOCTL_HID_ACTIVATE_DEVICE";
	case IOCTL_HID_DEACTIVATE_DEVICE:
		return "IOCTL_HID_DEACTIVATE_DEVICE";
	case IOCTL_HID_SEND_IDLE_NOTIFICATION_REQUEST:
		return "IOCTL_HID_SEND_IDLE_NOTIFICATION_REQUEST";
	case IOCTL_HID_SET_OUTPUT_REPORT:
		return "IOCTL_HID_SET_OUTPUT_REPORT";
	case IOCTL_HID_GET_INPUT_REPORT:
		return "IOCTL_HID_GET_INPUT_REPORT";
	default:
		return "Unknown IOCTL";
	}
}
