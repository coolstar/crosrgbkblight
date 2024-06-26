#if !defined(_CROSKEYBOARD_H_)
#define _CROSKEYBOARD_H_

#pragma warning(disable:4200)  // suppress nameless struct/union warning
#pragma warning(disable:4201)  // suppress nameless struct/union warning
#pragma warning(disable:4214)  // suppress bit field types other than int warning
#include <initguid.h>
#include <wdm.h>

#pragma warning(default:4200)
#pragma warning(default:4201)
#pragma warning(default:4214)
#include <wdf.h>

#include <wdmguid.h>

#pragma warning(disable:4201)  // suppress nameless struct/union warning
#pragma warning(disable:4214)  // suppress bit field types other than int warning
#include <hidport.h>

#include "hiddefs.h"
#include "hidcommon.h"
#include "eccmds.h"

#include "kblightsettings.h"
#include "firmware.h"

extern "C"

NTSTATUS
DriverEntry(
	_In_  PDRIVER_OBJECT   pDriverObject,
	_In_  PUNICODE_STRING  pRegistryPath
	);

EVT_WDF_DRIVER_DEVICE_ADD       OnDeviceAdd;
EVT_WDF_OBJECT_CONTEXT_CLEANUP  OnDriverCleanup;

//
// String definitions
//

#define DRIVERNAME                 "croskblight.sys: "

#define CROSKBLIGHT_POOL_TAG            (ULONG) 'lbkC'
#define CROSKBLIGHT_HARDWARE_IDS        L"CoolStar\\CrosKBLight\0\0"
#define CROSKBLIGHT_HARDWARE_IDS_LENGTH sizeof(CROSKBLIGHT_HARDWARE_IDS)

#define NTDEVICE_NAME_STRING       L"\\Device\\CrosKBLight"
#define SYMBOLIC_NAME_STRING       L"\\DosDevices\\CrosKBLight"
//
// This is the default report descriptor for the Hid device provided
// by the mini driver in response to IOCTL_HID_GET_REPORT_DESCRIPTOR.
// 

typedef UCHAR HID_REPORT_DESCRIPTOR, *PHID_REPORT_DESCRIPTOR;

#ifdef DESCRIPTOR_DEF
HID_REPORT_DESCRIPTOR DefaultReportDescriptorRGB[] = {
    TUD_HID_REPORT_DESC_LIGHTING(REPORTID_RGBKBLIGHT)
};

HID_REPORT_DESCRIPTOR DefaultReportDescriptorLegacy[] = {
	0x06, 0x00, 0xff,                    // USAGE_PAGE (Vendor Defined Page 1)
	0x09, 0x01,                          // USAGE (Vendor Usage 1)
	0xa1, 0x01,                          // COLLECTION (Application)
	0x85, REPORTID_KBLIGHT,              //   REPORT_ID (Keyboard Backlight)
	0x15, 0x00,                          //   LOGICAL_MINIMUM (0)
	0x26, 0xff, 0x00,                    //   LOGICAL_MAXIMUM (256)
	0x75, 0x08,                          //   REPORT_SIZE  (8)   - bits
	0x95, 0x01,                          //   REPORT_COUNT (1)  - Bytes
	0x09, 0x02,                          //   USAGE (Vendor Usage 1)
	0x91, 0x02,                          //   OUTPUT (Data,Var,Abs)
	0x09, 0x03,                          //   USAGE (Vendor Usage 2)
	0x91, 0x02,                          //   OUTPUT (Data,Var,Abs)
	0x09, 0x02,                          //   USAGE (Vendor Usage 1)
	0x81, 0x02,                          //   INPUT (Data,Var,Abs)
	0xc0,                                // END_COLLECTION
};


//
// This is the default HID descriptor returned by the mini driver
// in response to IOCTL_HID_GET_DEVICE_DESCRIPTOR. The size
// of report descriptor is currently the size of DefaultReportDescriptor.
//

CONST HID_DESCRIPTOR DefaultHidDescriptorRGB = {
	0x09,   // length of HID descriptor
	0x21,   // descriptor type == HID  0x21
	0x0100, // hid spec release
	0x00,   // country code == Not Specified
	0x01,   // number of HID class descriptors
	{ 0x22,   // descriptor type 
	sizeof(DefaultReportDescriptorRGB) }  // total length of report descriptor
};

CONST HID_DESCRIPTOR DefaultHidDescriptorLegacy = {
	0x09,   // length of HID descriptor
	0x21,   // descriptor type == HID  0x21
	0x0100, // hid spec release
	0x00,   // country code == Not Specified
	0x01,   // number of HID class descriptors
	{ 0x22,   // descriptor type 
	sizeof(DefaultReportDescriptorLegacy) }  // total length of report descriptor
};
#endif

#define true 1
#define false 0

typedef struct _CROSEC_COMMAND {
	UINT32 Version;
	UINT32 Command;
	UINT32 OutSize;
	UINT32 InSize;
	UINT32 Result;
	UINT8 Data[];
} CROSEC_COMMAND, * PCROSEC_COMMAND;

typedef
NTSTATUS
(*PCROSEC_CMD_XFER_STATUS)(
	IN      PVOID Context,
	OUT     PCROSEC_COMMAND Msg
	);

typedef
BOOLEAN
(*PCROSEC_CHECK_FEATURES)(
	IN PVOID Context,
	IN INT Feature
	);

DEFINE_GUID(GUID_CROSEC_INTERFACE_STANDARD,
	0xd7062676, 0xe3a4, 0x11ec, 0xa6, 0xc4, 0x24, 0x4b, 0xfe, 0x99, 0x46, 0xd0);

/*DEFINE_GUID(GUID_DEVICE_PROPERTIES,
	0xdaffd814, 0x6eba, 0x4d8c, 0x8a, 0x91, 0xbc, 0x9b, 0xbf, 0x4a, 0xa3, 0x01);*/ //Windows defender false positive

	//
	// Interface for getting and setting power level etc.,
	//
typedef struct _CROSEC_INTERFACE_STANDARD {
	INTERFACE                        InterfaceHeader;
	PCROSEC_CMD_XFER_STATUS          CmdXferStatus;
	PCROSEC_CHECK_FEATURES           CheckFeatures;
} CROSEC_INTERFACE_STANDARD, * PCROSEC_INTERFACE_STANDARD;

typedef struct _CROSKBLIGHT_CONTEXT
{
	WDFDEVICE FxDevice;

	WDFQUEUE ReportQueue;

	WDFQUEUE IdleQueue;

	PVOID CrosEcBusContext;

	PCROSEC_CMD_XFER_STATUS CrosEcCmdXferStatus;

	WDFIOTARGET busIoTarget;

	UINT8 currentBrightness;

	BOOLEAN SupportsRGB;

	UINT16 CurrentLampID;

	CROSKBLIGHT_INFO* RGBLedInfo;

	rgb_s KeyStates[EC_RGBKBD_MAX_KEY_COUNT];

} CROSKBLIGHT_CONTEXT, *PCROSKBLIGHT_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(CROSKBLIGHT_CONTEXT, GetDeviceContext)

//
// Power Idle Workitem context
// 
typedef struct _IDLE_WORKITEM_CONTEXT
{
	// Handle to a WDF device object
	WDFDEVICE FxDevice;

	// Handle to a WDF request object
	WDFREQUEST FxRequest;

} IDLE_WORKITEM_CONTEXT, * PIDLE_WORKITEM_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(IDLE_WORKITEM_CONTEXT, GetIdleWorkItemContext)

//
// Function definitions
//

DRIVER_INITIALIZE DriverEntry;

EVT_WDF_DRIVER_UNLOAD CrosKBLightDriverUnload;

EVT_WDF_DRIVER_DEVICE_ADD CrosKBLightEvtDeviceAdd;

EVT_WDFDEVICE_WDM_IRP_PREPROCESS CrosKBLightEvtWdmPreprocessMnQueryId;

EVT_WDF_IO_QUEUE_IO_INTERNAL_DEVICE_CONTROL CrosKBLightEvtInternalDeviceControl;

NTSTATUS
CrosKBLightGetHidDescriptor(
	IN WDFDEVICE Device,
	IN WDFREQUEST Request
	);

NTSTATUS
CrosKBLightGetReportDescriptor(
	IN WDFDEVICE Device,
	IN WDFREQUEST Request
	);

NTSTATUS
CrosKBLightGetDeviceAttributes(
	IN WDFREQUEST Request
	);

NTSTATUS
CrosKBLightGetString(
	IN WDFREQUEST Request
	);

NTSTATUS
CrosKBLightWriteReport(
	IN PCROSKBLIGHT_CONTEXT DevContext,
	IN WDFREQUEST Request
	);

NTSTATUS
CrosKBLightProcessVendorReport(
	IN PCROSKBLIGHT_CONTEXT DevContext,
	IN PVOID ReportBuffer,
	IN ULONG ReportBufferLen,
	OUT size_t* BytesWritten
	);

NTSTATUS
CrosKBLightReadReport(
	IN PCROSKBLIGHT_CONTEXT DevContext,
	IN WDFREQUEST Request,
	OUT BOOLEAN* CompleteRequest
	);

NTSTATUS
CrosKBLightSetFeature(
	IN PCROSKBLIGHT_CONTEXT DevContext,
	IN WDFREQUEST Request,
	OUT BOOLEAN* CompleteRequest
	);

NTSTATUS
CrosKBLightGetFeature(
	IN PCROSKBLIGHT_CONTEXT DevContext,
	IN WDFREQUEST Request,
	OUT BOOLEAN* CompleteRequest
	);

PCHAR
DbgHidInternalIoctlString(
	IN ULONG        IoControlCode
	);

VOID
CrosKBLightCompleteIdleIrp(
	IN PCROSKBLIGHT_CONTEXT FxDeviceContext
);

#define KEYSPAGE 40

//
// Helper macros
//

#define DEBUG_LEVEL_ERROR   1
#define DEBUG_LEVEL_INFO    2
#define DEBUG_LEVEL_VERBOSE 3

#define DBG_INIT  1
#define DBG_PNP   2
#define DBG_IOCTL 4

#if DBG
#define CrosKBLightPrint(dbglevel, dbgcatagory, fmt, ...) {          \
    if (CrosKBLightDebugLevel >= dbglevel &&                         \
        (CrosKBLightDebugCatagories && dbgcatagory))                 \
	    {                                                           \
        DbgPrint(DRIVERNAME);                                   \
        DbgPrint(fmt, __VA_ARGS__);                             \
	    }                                                           \
}
#else
#define CrosKBLightPrint(dbglevel, fmt, ...) {                       \
}
#endif

#endif
#pragma once
