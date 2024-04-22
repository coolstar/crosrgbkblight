#if !defined(_CROSKBLIGHT_COMMON_H_)
#define _CROSKBLIGHT_COMMON_H_

//
//These are the device attributes returned by vmulti in response
// to IOCTL_HID_GET_DEVICE_ATTRIBUTES.
//

#define CROSKBLIGHT_PID              0x0002
#define CROSKBLIGHT_VID              0x18D1
#define CROSKBLIGHT_VERSION          0x0001

//
// These are the report ids
//

#define REPORTID_RGBKBLIGHT       0x02

enum {
	REPORT_ID_LIGHTING_LAMP_ARRAY_ATTRIBUTES = 2, // 2
	REPORT_ID_LIGHTING_LAMP_ATTRIBUTES_REQUEST, // 3
	REPORT_ID_LIGHTING_LAMP_ATTRIBUTES_RESPONSE, // 4
	REPORT_ID_LIGHTING_LAMP_MULTI_UPDATE, // 5
	REPORT_ID_LIGHTING_LAMP_RANGE_UPDATE, // 6
	REPORT_ID_LIGHTING_LAMP_ARRAY_CONTROL, // 7
	REPORT_ID_COUNT
};

#include <pshpack1.h>

typedef struct _LAMPCOLOR
{
	UINT8 Red;
	UINT8 Green;
	UINT8 Blue;
	UINT8 Intensity;
} LampColor;

typedef struct _POSITION
{
	UINT32 x;
	UINT32 y;
	UINT32 z;
} Position;

typedef struct _LAMPARRAY_ATTRIBUTES_REPORT
{
	BYTE ReportID;
	UINT16 LampCount;
	UINT32 Width;
	UINT32 Height;
	UINT32 Depth;

	UINT32 LampArrayKind;
	UINT32 MinUpdateInterval;
} LampArrayAttributesReport;

typedef struct _LAMPARRAY_ATTRIBUTES_REQUEST_REPORT
{
	BYTE ReportID;
	UINT16 LampID;
} LampArrayAttributesRequestReport;

#define LAMP_PURPOSE_CONTROL        0x01
#define LAMP_PURPOSE_ACCENT         0x02
#define LAMP_PURPOSE_BRANDING       0x04
#define LAMP_PURPOSE_STATUS         0x08
#define LAMP_PURPOSE_ILLUMINATION   0x10
#define LAMP_PURPOSE_PRESENTATION   0x20

typedef struct _LAMPARRAY_ATTRIBUTES_RESPONSE_REPORT
{
	BYTE ReportID;
	UINT16 LampId;

	Position LampPosition;

	UINT32 UpdateLatency;
	UINT32 LampPurposes;

	UINT8 RedLevelCount;
	UINT8 GreenLevelCount;
	UINT8 BlueLevelCount;
	UINT8 IntensityLevelCount;

	UINT8 IsProgrammable;
	UINT8 InputBinding;
} LampArrayAttributesResponseReport;

typedef struct _LAMPARRAY_MULTI_UPDATE_REPORT
{
	BYTE ReportID;
	UINT8 LampCount;
	UINT8 Flags;
	UINT16 LampIds[8];

	LampColor Colors[8];
} LampArrayMultiUpdateReport;

typedef struct _LAMPARRAY_RANGE_UPDATE_REPORT
{
	BYTE ReportID;
	UINT8 Flags;
	UINT16 LampIdStart;
	UINT16 LampIdEnd;

	LampColor Color;
} LampArrayRangeUpdateReport;

typedef struct _LAMPARRAY_CONTROL_REPORT
{
	BYTE ReportID;
	UINT8 AutonomousMode;
} LampArrayControlReport;

#include <poppack.h>

#endif
#pragma once
