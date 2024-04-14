#ifndef __CROS_EC_REGS_H__
#define __CROS_EC_REGS_H__

#define BIT(nr) (1UL << (nr))

/* Command version mask */
#define EC_VER_MASK(version) (1UL << (version))

/* Get keyboard backlight */
/* OBSOLETE - Use EC_CMD_PWM_SET_DUTY */
#define EC_CMD_PWM_GET_KEYBOARD_BACKLIGHT 0x0022

#include <pshpack1.h>

struct ec_response_pwm_get_keyboard_backlight {
	UINT8 percent;
	UINT8 enabled;
};

#include <poppack.h>

/* Set keyboard backlight */
/* OBSOLETE - Use EC_CMD_PWM_SET_DUTY */
#define EC_CMD_PWM_SET_KEYBOARD_BACKLIGHT 0x0023

#include <pshpack1.h>

struct ec_params_pwm_set_keyboard_backlight {
	UINT8 percent;
};

#include <poppack.h>

/* Set target fan PWM duty cycle */
#define EC_CMD_PWM_SET_FAN_DUTY 0x0024

/* Version 0 of input params */
#include <pshpack4.h>

struct ec_params_pwm_set_fan_duty_v0 {
	UINT32 percent;
};

#include <poppack.h>

/* Version 1 of input params */
#include <pshpack1.h>

struct ec_params_pwm_set_fan_duty_v1 {
	UINT32 percent;
	UINT8 fan_idx;
};

#include <poppack.h>

#define EC_CMD_PWM_SET_DUTY 0x0025
/* 16 bit duty cycle, 0xffff = 100% */
#define EC_PWM_MAX_DUTY 0xffff

enum ec_pwm_type {
	/* All types, indexed by board-specific enum pwm_channel */
	EC_PWM_TYPE_GENERIC = 0,
	/* Keyboard backlight */
	EC_PWM_TYPE_KB_LIGHT,
	/* Display backlight */
	EC_PWM_TYPE_DISPLAY_LIGHT,
	EC_PWM_TYPE_COUNT,
};

#include <pshpack4.h>

struct ec_params_pwm_set_duty {
	UINT16 duty;     /* Duty cycle, EC_PWM_MAX_DUTY = 100% */
	UINT8 pwm_type;  /* ec_pwm_type */
	UINT8 index;     /* Type-specific index, or 0 if unique */
};

#include <poppack.h>

#define EC_CMD_PWM_GET_DUTY 0x0026

#include <pshpack1.h>

struct ec_params_pwm_get_duty {
	UINT8 pwm_type;  /* ec_pwm_type */
	UINT8 index;     /* Type-specific index, or 0 if unique */
};

#include <poppack.h>

#include <pshpack2.h>

struct ec_response_pwm_get_duty {
	UINT16 duty;     /* Duty cycle, EC_PWM_MAX_DUTY = 100% */
};

#include <poppack.h>

#define EC_CMD_RGBKBD_SET_COLOR 0x013A
#define EC_CMD_RGBKBD 0x013B

#define EC_RGBKBD_MAX_KEY_COUNT 128
#define EC_RGBKBD_MAX_RGB_COLOR 0xFFFFFF
#define EC_RGBKBD_MAX_SCALE 0xFF

enum rgbkbd_state {
	/* RGB keyboard is reset and not initialized. */
	RGBKBD_STATE_RESET = 0,
	/* RGB keyboard is initialized but not enabled. */
	RGBKBD_STATE_INITIALIZED,
	/* RGB keyboard is disabled. */
	RGBKBD_STATE_DISABLED,
	/* RGB keyboard is enabled and ready to receive a command. */
	RGBKBD_STATE_ENABLED,

	/* Put no more entry below */
	RGBKBD_STATE_COUNT,
};

enum ec_rgbkbd_subcmd {
	EC_RGBKBD_SUBCMD_CLEAR = 1,
	EC_RGBKBD_SUBCMD_DEMO = 2,
	EC_RGBKBD_SUBCMD_SET_SCALE = 3,
	EC_RGBKBD_SUBCMD_GET_CONFIG = 4,
	EC_RGBKBD_SUBCMD_COUNT
};

enum ec_rgbkbd_demo {
	EC_RGBKBD_DEMO_OFF = 0,
	EC_RGBKBD_DEMO_FLOW = 1,
	EC_RGBKBD_DEMO_DOT = 2,
	EC_RGBKBD_DEMO_COUNT,
};

enum ec_rgbkbd_type {
	EC_RGBKBD_TYPE_UNKNOWN = 0,
	EC_RGBKBD_TYPE_PER_KEY = 1, /* e.g. Vell */
	EC_RGBKBD_TYPE_FOUR_ZONES_40_LEDS = 2, /* e.g. Taniks */
	EC_RGBKBD_TYPE_FOUR_ZONES_12_LEDS = 3, /* e.g. Osiris */
	EC_RGBKBD_TYPE_FOUR_ZONES_4_LEDS = 4, /* e.g. Mithrax */
	EC_RGBKBD_TYPE_COUNT,
};

struct rgb_s {
	UINT8 r, g, b;
};

struct ec_rgbkbd_set_scale {
	UINT8 key;
	struct rgb_s scale;
};

#include <pshpack1.h>
struct ec_params_rgbkbd {
	UINT8 subcmd; /* Sub-command (enum ec_rgbkbd_subcmd) */
	union {
		struct rgb_s color; /* EC_RGBKBD_SUBCMD_CLEAR */
		UINT8 demo; /* EC_RGBKBD_SUBCMD_DEMO */
		struct ec_rgbkbd_set_scale set_scale;
	};
};

struct ec_response_rgbkbd {
	/*
	 * RGBKBD type supported by the device.
	 */

	UINT8 rgbkbd_type; /* enum ec_rgbkbd_type */
};

struct ec_params_rgbkbd_set_color {
	/* Specifies the starting key ID whose color is being changed. */
	UINT8 start_key;
	/* Specifies # of elements in <color>. */
	UINT8 length;
	/* RGB color data array of length up to MAX_KEY_COUNT. */
	struct rgb_s color[];
};
#include <poppack.h>

/* Read versions supported for a command */
#define EC_CMD_GET_CMD_VERSIONS 0x0008

/**
 * struct ec_params_get_cmd_versions - Parameters for the get command versions.
 * @cmd: Command to check.
 */
#include <pshpack1.h>
struct ec_params_get_cmd_versions {
	UINT8 cmd;
};
#include <poppack.h>

/**
 * struct ec_params_get_cmd_versions_v1 - Parameters for the get command
 *         versions (v1)
 * @cmd: Command to check.
 */
#include <pshpack2.h>
struct ec_params_get_cmd_versions_v1 {
	UINT16 cmd;
};
#include <poppack.h>

/**
 * struct ec_response_get_cmd_version - Response to the get command versions.
 * @version_mask: Mask of supported versions; use EC_VER_MASK() to compare with
 *                a desired version.
 */
#include <pshpack4.h>
struct ec_response_get_cmd_versions {
	UINT32 version_mask;
};
#include <poppack.h>


/* Get protocol information */
#define EC_CMD_GET_PROTOCOL_INFO	0x0b

/* Flags for ec_response_get_protocol_info.flags */
/* EC_RES_IN_PROGRESS may be returned if a command is slow */
#define EC_PROTOCOL_INFO_IN_PROGRESS_SUPPORTED (1 << 0)

#include <pshpack4.h>

struct ec_response_get_protocol_info {
	/* Fields which exist if at least protocol version 3 supported */

	/* Bitmask of protocol versions supported (1 << n means version n)*/
	UINT32 protocol_versions;

	/* Maximum request packet size, in bytes */
	UINT16 max_request_packet_size;

	/* Maximum response packet size, in bytes */
	UINT16 max_response_packet_size;

	/* Flags; see EC_PROTOCOL_INFO_* */
	UINT32 flags;
};

#include <poppack.h>

#endif /* __CROS_EC_REGS_H__ */