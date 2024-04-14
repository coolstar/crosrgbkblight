#pragma once

#include <pshpack1.h>

typedef struct _CROSKBLIGHT_KEY_INFO {
	UINT32 Pos_X;
	UINT32 Pos_Y;
	UINT32 Pos_Z;
	UINT8 RedCount;
	UINT8 GreenCount;
	UINT8 BlueCount;
	UINT8 IntensityCount;
	UINT8 IsProgrammable;
	UINT8 InputBinding;
} CROSKBLIGHT_KEY_INFO;

typedef struct _CROSKBLIGHT_INFO {
	//Board Attributes
	UINT32 LampCount;
	UINT32 Width;
	UINT32 Height;
	UINT32 Depth;

	//Keys
	CROSKBLIGHT_KEY_INFO Keys[];
} CROSKBLIGHT_INFO;

#include <poppack.h>