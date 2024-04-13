#pragma once

//--------------------------------------------------------------------+
// Macros Helper
//--------------------------------------------------------------------+
#define TU_ARRAY_SIZE(_arr)   ( sizeof(_arr) / sizeof(_arr[0]) )
#define TU_MIN(_x, _y)        ( ( (_x) < (_y) ) ? (_x) : (_y) )
#define TU_MAX(_x, _y)        ( ( (_x) > (_y) ) ? (_x) : (_y) )

#define TU_U16(_high, _low)   ((UINT16) (((_high) << 8) | (_low)))
#define TU_U16_HIGH(_u16)     ((UINT8) (((_u16) >> 8) & 0x00ff))
#define TU_U16_LOW(_u16)      ((UINT8) ((_u16)       & 0x00ff))
#define U16_TO_U8S_BE(_u16)   TU_U16_HIGH(_u16), TU_U16_LOW(_u16)
#define U16_TO_U8S_LE(_u16)   TU_U16_LOW(_u16), TU_U16_HIGH(_u16)

#define TU_U32_BYTE3(_u32)    ((UINT8) ((((UINT32) _u32) >> 24) & 0x000000ff)) // MSB
#define TU_U32_BYTE2(_u32)    ((UINT8) ((((UINT32) _u32) >> 16) & 0x000000ff))
#define TU_U32_BYTE1(_u32)    ((UINT8) ((((UINT32) _u32) >>  8) & 0x000000ff))
#define TU_U32_BYTE0(_u32)    ((UINT8) (((UINT32)  _u32)        & 0x000000ff)) // LSB

#define U32_TO_U8S_BE(_u32)   TU_U32_BYTE3(_u32), TU_U32_BYTE2(_u32), TU_U32_BYTE1(_u32), TU_U32_BYTE0(_u32)
#define U32_TO_U8S_LE(_u32)   TU_U32_BYTE0(_u32), TU_U32_BYTE1(_u32), TU_U32_BYTE2(_u32), TU_U32_BYTE3(_u32)

#define TU_BIT(n)             (1UL << (n))

// Generate a mask with bit from high (31) to low (0) set, e.g TU_GENMASK(3, 0) = 0b1111
#define TU_GENMASK(h, l)      ( (UINT32_MAX << (l)) & (UINT32_MAX >> (31 - (h))) )

//--------------------------------------------------------------------+
// REPORT DESCRIPTOR
//--------------------------------------------------------------------+

//------------- ITEM & TAG -------------//
#define HID_REPORT_DATA_0(data)
#define HID_REPORT_DATA_1(data) , data
#define HID_REPORT_DATA_2(data) , U16_TO_U8S_LE(data)
#define HID_REPORT_DATA_3(data) , U32_TO_U8S_LE(data)

#define HID_REPORT_ITEM(data, tag, type, size) \
  (((tag) << 4) | ((type) << 2) | (size)) HID_REPORT_DATA_##size(data)

// Report Item Types
enum {
    RI_TYPE_MAIN = 0,
    RI_TYPE_GLOBAL = 1,
    RI_TYPE_LOCAL = 2
};

//------------- Main Items - HID 1.11 section 6.2.2.4 -------------//

// Report Item Main group
enum {
    RI_MAIN_INPUT = 8,
    RI_MAIN_OUTPUT = 9,
    RI_MAIN_COLLECTION = 10,
    RI_MAIN_FEATURE = 11,
    RI_MAIN_COLLECTION_END = 12
};

//------------- Input, Output, Feature - HID 1.11 section 6.2.2.5 -------------//
#define HID_DATA             (0<<0)
#define HID_CONSTANT         (1<<0)

#define HID_ARRAY            (0<<1)
#define HID_VARIABLE         (1<<1)

#define HID_ABSOLUTE         (0<<2)
#define HID_RELATIVE         (1<<2)

#define HID_WRAP_NO          (0<<3)
#define HID_WRAP             (1<<3)

#define HID_LINEAR           (0<<4)
#define HID_NONLINEAR        (1<<4)

#define HID_PREFERRED_STATE  (0<<5)
#define HID_PREFERRED_NO     (1<<5)

#define HID_NO_NULL_POSITION (0<<6)
#define HID_NULL_STATE       (1<<6)

#define HID_NON_VOLATILE     (0<<7)
#define HID_VOLATILE         (1<<7)

#define HID_BITFIELD         (0<<8)
#define HID_BUFFERED_BYTES   (1<<8)

//------------- Collection Item - HID 1.11 section 6.2.2.6 -------------//
enum {
    HID_COLLECTION_PHYSICAL = 0,
    HID_COLLECTION_APPLICATION,
    HID_COLLECTION_LOGICAL,
    HID_COLLECTION_REPORT,
    HID_COLLECTION_NAMED_ARRAY,
    HID_COLLECTION_USAGE_SWITCH,
    HID_COLLECTION_USAGE_MODIFIER
};

#define HID_INPUT(x)           HID_REPORT_ITEM(x, RI_MAIN_INPUT         , RI_TYPE_MAIN, 1)
#define HID_OUTPUT(x)          HID_REPORT_ITEM(x, RI_MAIN_OUTPUT        , RI_TYPE_MAIN, 1)
#define HID_COLLECTION(x)      HID_REPORT_ITEM(x, RI_MAIN_COLLECTION    , RI_TYPE_MAIN, 1)
#define HID_FEATURE(x)         HID_REPORT_ITEM(x, RI_MAIN_FEATURE       , RI_TYPE_MAIN, 1)
#define HID_COLLECTION_END     HID_REPORT_ITEM(x, RI_MAIN_COLLECTION_END, RI_TYPE_MAIN, 0)

//------------- Global Items - HID 1.11 section 6.2.2.7 -------------//

// Report Item Global group
enum {
    RI_GLOBAL_USAGE_PAGE = 0,
    RI_GLOBAL_LOGICAL_MIN = 1,
    RI_GLOBAL_LOGICAL_MAX = 2,
    RI_GLOBAL_PHYSICAL_MIN = 3,
    RI_GLOBAL_PHYSICAL_MAX = 4,
    RI_GLOBAL_UNIT_EXPONENT = 5,
    RI_GLOBAL_UNIT = 6,
    RI_GLOBAL_REPORT_SIZE = 7,
    RI_GLOBAL_REPORT_ID = 8,
    RI_GLOBAL_REPORT_COUNT = 9,
    RI_GLOBAL_PUSH = 10,
    RI_GLOBAL_POP = 11
};

#define HID_USAGE_PAGE(x)         HID_REPORT_ITEM(x, RI_GLOBAL_USAGE_PAGE, RI_TYPE_GLOBAL, 1)
#define HID_USAGE_PAGE_N(x, n)    HID_REPORT_ITEM(x, RI_GLOBAL_USAGE_PAGE, RI_TYPE_GLOBAL, n)

#define HID_LOGICAL_MIN(x)        HID_REPORT_ITEM(x, RI_GLOBAL_LOGICAL_MIN, RI_TYPE_GLOBAL, 1)
#define HID_LOGICAL_MIN_N(x, n)   HID_REPORT_ITEM(x, RI_GLOBAL_LOGICAL_MIN, RI_TYPE_GLOBAL, n)

#define HID_LOGICAL_MAX(x)        HID_REPORT_ITEM(x, RI_GLOBAL_LOGICAL_MAX, RI_TYPE_GLOBAL, 1)
#define HID_LOGICAL_MAX_N(x, n)   HID_REPORT_ITEM(x, RI_GLOBAL_LOGICAL_MAX, RI_TYPE_GLOBAL, n)

#define HID_PHYSICAL_MIN(x)       HID_REPORT_ITEM(x, RI_GLOBAL_PHYSICAL_MIN, RI_TYPE_GLOBAL, 1)
#define HID_PHYSICAL_MIN_N(x, n)  HID_REPORT_ITEM(x, RI_GLOBAL_PHYSICAL_MIN, RI_TYPE_GLOBAL, n)

#define HID_PHYSICAL_MAX(x)       HID_REPORT_ITEM(x, RI_GLOBAL_PHYSICAL_MAX, RI_TYPE_GLOBAL, 1)
#define HID_PHYSICAL_MAX_N(x, n)  HID_REPORT_ITEM(x, RI_GLOBAL_PHYSICAL_MAX, RI_TYPE_GLOBAL, n)

#define HID_UNIT_EXPONENT(x)      HID_REPORT_ITEM(x, RI_GLOBAL_UNIT_EXPONENT, RI_TYPE_GLOBAL, 1)
#define HID_UNIT_EXPONENT_N(x, n) HID_REPORT_ITEM(x, RI_GLOBAL_UNIT_EXPONENT, RI_TYPE_GLOBAL, n)

#define HID_UNIT(x)               HID_REPORT_ITEM(x, RI_GLOBAL_UNIT, RI_TYPE_GLOBAL, 1)
#define HID_UNIT_N(x, n)          HID_REPORT_ITEM(x, RI_GLOBAL_UNIT, RI_TYPE_GLOBAL, n)

#define HID_REPORT_SIZE(x)        HID_REPORT_ITEM(x, RI_GLOBAL_REPORT_SIZE, RI_TYPE_GLOBAL, 1)
#define HID_REPORT_SIZE_N(x, n)   HID_REPORT_ITEM(x, RI_GLOBAL_REPORT_SIZE, RI_TYPE_GLOBAL, n)

#define HID_REPORT_ID(x)          HID_REPORT_ITEM(x, RI_GLOBAL_REPORT_ID, RI_TYPE_GLOBAL, 1),
#define HID_REPORT_ID_N(x, n)     HID_REPORT_ITEM(x, RI_GLOBAL_REPORT_ID, RI_TYPE_GLOBAL, n),

#define HID_REPORT_COUNT(x)       HID_REPORT_ITEM(x, RI_GLOBAL_REPORT_COUNT, RI_TYPE_GLOBAL, 1)
#define HID_REPORT_COUNT_N(x, n)  HID_REPORT_ITEM(x, RI_GLOBAL_REPORT_COUNT, RI_TYPE_GLOBAL, n)

#define HID_PUSH                  HID_REPORT_ITEM(x, RI_GLOBAL_PUSH, RI_TYPE_GLOBAL, 0)
#define HID_POP                   HID_REPORT_ITEM(x, RI_GLOBAL_POP, RI_TYPE_GLOBAL, 0)

//------------- LOCAL ITEMS 6.2.2.8 -------------//

enum {
    RI_LOCAL_USAGE = 0,
    RI_LOCAL_USAGE_MIN = 1,
    RI_LOCAL_USAGE_MAX = 2,
    RI_LOCAL_DESIGNATOR_INDEX = 3,
    RI_LOCAL_DESIGNATOR_MIN = 4,
    RI_LOCAL_DESIGNATOR_MAX = 5,
    // 6 is reserved
    RI_LOCAL_STRING_INDEX = 7,
    RI_LOCAL_STRING_MIN = 8,
    RI_LOCAL_STRING_MAX = 9,
    RI_LOCAL_DELIMITER = 10,
};

#define HID_USAGE(x)              HID_REPORT_ITEM(x, RI_LOCAL_USAGE, RI_TYPE_LOCAL, 1)
#define HID_USAGE_N(x, n)         HID_REPORT_ITEM(x, RI_LOCAL_USAGE, RI_TYPE_LOCAL, n)

#define HID_USAGE_MIN(x)          HID_REPORT_ITEM(x, RI_LOCAL_USAGE_MIN, RI_TYPE_LOCAL, 1)
#define HID_USAGE_MIN_N(x, n)     HID_REPORT_ITEM(x, RI_LOCAL_USAGE_MIN, RI_TYPE_LOCAL, n)

#define HID_USAGE_MAX(x)          HID_REPORT_ITEM(x, RI_LOCAL_USAGE_MAX, RI_TYPE_LOCAL, 1)
#define HID_USAGE_MAX_N(x, n)     HID_REPORT_ITEM(x, RI_LOCAL_USAGE_MAX, RI_TYPE_LOCAL, n)

#define HID_USAGE_PAGE_LIGHTING_AND_ILLUMINATION 0x59

/// HID Usage Table - Lighting And Illumination Page (0x59)
enum {
	HID_USAGE_LIGHTING_LAMP_ARRAY = 0x01,
	HID_USAGE_LIGHTING_LAMP_ARRAY_ATTRIBUTES_REPORT = 0x02,
	HID_USAGE_LIGHTING_LAMP_COUNT = 0x03,
	HID_USAGE_LIGHTING_BOUNDING_BOX_WIDTH_IN_MICROMETERS = 0x04,
	HID_USAGE_LIGHTING_BOUNDING_BOX_HEIGHT_IN_MICROMETERS = 0x05,
	HID_USAGE_LIGHTING_BOUNDING_BOX_DEPTH_IN_MICROMETERS = 0x06,
	HID_USAGE_LIGHTING_LAMP_ARRAY_KIND = 0x07,
	HID_USAGE_LIGHTING_MIN_UPDATE_INTERVAL_IN_MICROSECONDS = 0x08,
	HID_USAGE_LIGHTING_LAMP_ATTRIBUTES_REQUEST_REPORT = 0x20,
	HID_USAGE_LIGHTING_LAMP_ID = 0x21,
	HID_USAGE_LIGHTING_LAMP_ATTRIBUTES_RESPONSE_REPORT = 0x22,
	HID_USAGE_LIGHTING_POSITION_X_IN_MICROMETERS = 0x23,
	HID_USAGE_LIGHTING_POSITION_Y_IN_MICROMETERS = 0x24,
	HID_USAGE_LIGHTING_POSITION_Z_IN_MICROMETERS = 0x25,
	HID_USAGE_LIGHTING_LAMP_PURPOSES = 0x26,
	HID_USAGE_LIGHTING_UPDATE_LATENCY_IN_MICROSECONDS = 0x27,
	HID_USAGE_LIGHTING_RED_LEVEL_COUNT = 0x28,
	HID_USAGE_LIGHTING_GREEN_LEVEL_COUNT = 0x29,
	HID_USAGE_LIGHTING_BLUE_LEVEL_COUNT = 0x2A,
	HID_USAGE_LIGHTING_INTENSITY_LEVEL_COUNT = 0x2B,
	HID_USAGE_LIGHTING_IS_PROGRAMMABLE = 0x2C,
	HID_USAGE_LIGHTING_INPUT_BINDING = 0x2D,
	HID_USAGE_LIGHTING_LAMP_MULTI_UPDATE_REPORT = 0x50,
	HID_USAGE_LIGHTING_RED_UPDATE_CHANNEL = 0x51,
	HID_USAGE_LIGHTING_GREEN_UPDATE_CHANNEL = 0x52,
	HID_USAGE_LIGHTING_BLUE_UPDATE_CHANNEL = 0x53,
	HID_USAGE_LIGHTING_INTENSITY_UPDATE_CHANNEL = 0x54,
	HID_USAGE_LIGHTING_LAMP_UPDATE_FLAGS = 0x55,
	HID_USAGE_LIGHTING_LAMP_RANGE_UPDATE_REPORT = 0x60,
	HID_USAGE_LIGHTING_LAMP_ID_START = 0x61,
	HID_USAGE_LIGHTING_LAMP_ID_END = 0x62,
	HID_USAGE_LIGHTING_LAMP_ARRAY_CONTROL_REPORT = 0x70,
	HID_USAGE_LIGHTING_AUTONOMOUS_MODE = 0x71,
};

// HID Lighting and Illumination Report Descriptor Template
// - 1st parameter is report id HID_REPORT_ID(n) (optional)
#define TUD_HID_REPORT_DESC_LIGHTING(report_id) \
  HID_USAGE_PAGE ( HID_USAGE_PAGE_LIGHTING_AND_ILLUMINATION ),\
  HID_USAGE      ( HID_USAGE_LIGHTING_LAMP_ARRAY            ),\
  HID_COLLECTION ( HID_COLLECTION_APPLICATION               ),\
    /* Lamp Array Attributes Report */ \
    HID_REPORT_ID (report_id                                    ) \
    HID_USAGE ( HID_USAGE_LIGHTING_LAMP_ARRAY_ATTRIBUTES_REPORT ),\
    HID_COLLECTION ( HID_COLLECTION_LOGICAL                     ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_LAMP_COUNT                          ),\
      HID_LOGICAL_MIN   ( 0                                                      ),\
      HID_LOGICAL_MAX_N ( 65535, 3                                               ),\
      HID_REPORT_SIZE   ( 16                                                     ),\
      HID_REPORT_COUNT  ( 1                                                      ),\
      HID_FEATURE       ( HID_CONSTANT | HID_VARIABLE | HID_ABSOLUTE             ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_BOUNDING_BOX_WIDTH_IN_MICROMETERS   ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_BOUNDING_BOX_HEIGHT_IN_MICROMETERS  ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_BOUNDING_BOX_DEPTH_IN_MICROMETERS   ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_LAMP_ARRAY_KIND                     ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_MIN_UPDATE_INTERVAL_IN_MICROSECONDS ),\
      HID_LOGICAL_MIN   ( 0                                                      ),\
      HID_LOGICAL_MAX_N ( 2147483647, 3                                          ),\
      HID_REPORT_SIZE   ( 32                                                     ),\
      HID_REPORT_COUNT  ( 5                                                      ),\
      HID_FEATURE       ( HID_CONSTANT | HID_VARIABLE | HID_ABSOLUTE             ),\
    HID_COLLECTION_END ,\
    /* Lamp Attributes Request Report */ \
    HID_REPORT_ID       ( report_id + 1                                     ) \
    HID_USAGE           ( HID_USAGE_LIGHTING_LAMP_ATTRIBUTES_REQUEST_REPORT ),\
    HID_COLLECTION      ( HID_COLLECTION_LOGICAL                            ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_LAMP_ID             ),\
      HID_LOGICAL_MIN   ( 0                                      ),\
      HID_LOGICAL_MAX_N ( 65535, 3                               ),\
      HID_REPORT_SIZE   ( 16                                     ),\
      HID_REPORT_COUNT  ( 1                                      ),\
      HID_FEATURE       ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),\
    HID_COLLECTION_END ,\
    /* Lamp Attributes Response Report */ \
    HID_REPORT_ID       ( report_id + 2                                      ) \
    HID_USAGE           ( HID_USAGE_LIGHTING_LAMP_ATTRIBUTES_RESPONSE_REPORT ),\
    HID_COLLECTION      ( HID_COLLECTION_LOGICAL                             ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_LAMP_ID                        ),\
      HID_LOGICAL_MIN   ( 0                                                 ),\
      HID_LOGICAL_MAX_N ( 65535, 3                                          ),\
      HID_REPORT_SIZE   ( 16                                                ),\
      HID_REPORT_COUNT  ( 1                                                 ),\
      HID_FEATURE       ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE            ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_POSITION_X_IN_MICROMETERS      ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_POSITION_Y_IN_MICROMETERS      ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_POSITION_Z_IN_MICROMETERS      ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_UPDATE_LATENCY_IN_MICROSECONDS ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_LAMP_PURPOSES                  ),\
      HID_LOGICAL_MIN   ( 0                                                 ),\
      HID_LOGICAL_MAX_N ( 2147483647, 3                                     ),\
      HID_REPORT_SIZE   ( 32                                                ),\
      HID_REPORT_COUNT  ( 5                                                 ),\
      HID_FEATURE       ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE            ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_RED_LEVEL_COUNT                ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_GREEN_LEVEL_COUNT              ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_BLUE_LEVEL_COUNT               ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_INTENSITY_LEVEL_COUNT          ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_IS_PROGRAMMABLE                ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_INPUT_BINDING                  ),\
      HID_LOGICAL_MIN   ( 0                                                 ),\
      HID_LOGICAL_MAX_N ( 255, 2                                            ),\
      HID_REPORT_SIZE   ( 8                                                 ),\
      HID_REPORT_COUNT  ( 6                                                 ),\
      HID_FEATURE       ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE            ),\
    HID_COLLECTION_END ,\
    /* Lamp Multi-Update Report */ \
    HID_REPORT_ID       ( report_id + 3                               ) \
    HID_USAGE           ( HID_USAGE_LIGHTING_LAMP_MULTI_UPDATE_REPORT ),\
    HID_COLLECTION      ( HID_COLLECTION_LOGICAL                      ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_LAMP_COUNT               ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_LAMP_UPDATE_FLAGS        ),\
      HID_LOGICAL_MIN   ( 0                                           ),\
      HID_LOGICAL_MAX   ( 8                                           ),\
      HID_REPORT_SIZE   ( 8                                           ),\
      HID_REPORT_COUNT  ( 2                                           ),\
      HID_FEATURE       ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE      ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_LAMP_ID                  ),\
      HID_LOGICAL_MIN   ( 0                                           ),\
      HID_LOGICAL_MAX_N ( 65535, 3                                    ),\
      HID_REPORT_SIZE   ( 16                                          ),\
      HID_REPORT_COUNT  ( 8                                           ),\
      HID_FEATURE       ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE      ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_RED_UPDATE_CHANNEL       ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_GREEN_UPDATE_CHANNEL     ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_BLUE_UPDATE_CHANNEL      ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_INTENSITY_UPDATE_CHANNEL ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_RED_UPDATE_CHANNEL       ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_GREEN_UPDATE_CHANNEL     ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_BLUE_UPDATE_CHANNEL      ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_INTENSITY_UPDATE_CHANNEL ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_RED_UPDATE_CHANNEL       ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_GREEN_UPDATE_CHANNEL     ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_BLUE_UPDATE_CHANNEL      ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_INTENSITY_UPDATE_CHANNEL ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_RED_UPDATE_CHANNEL       ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_GREEN_UPDATE_CHANNEL     ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_BLUE_UPDATE_CHANNEL      ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_INTENSITY_UPDATE_CHANNEL ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_RED_UPDATE_CHANNEL       ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_GREEN_UPDATE_CHANNEL     ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_BLUE_UPDATE_CHANNEL      ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_INTENSITY_UPDATE_CHANNEL ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_RED_UPDATE_CHANNEL       ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_GREEN_UPDATE_CHANNEL     ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_BLUE_UPDATE_CHANNEL      ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_INTENSITY_UPDATE_CHANNEL ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_RED_UPDATE_CHANNEL       ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_GREEN_UPDATE_CHANNEL     ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_BLUE_UPDATE_CHANNEL      ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_INTENSITY_UPDATE_CHANNEL ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_RED_UPDATE_CHANNEL       ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_GREEN_UPDATE_CHANNEL     ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_BLUE_UPDATE_CHANNEL      ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_INTENSITY_UPDATE_CHANNEL ),\
      HID_LOGICAL_MIN   ( 0                                           ),\
      HID_LOGICAL_MAX_N ( 255, 2                                      ),\
      HID_REPORT_SIZE   ( 8                                           ),\
      HID_REPORT_COUNT  ( 32                                          ),\
      HID_FEATURE       ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE      ),\
    HID_COLLECTION_END ,\
    /* Lamp Range Update Report */ \
    HID_REPORT_ID       ( report_id + 4 ) \
    HID_USAGE           ( HID_USAGE_LIGHTING_LAMP_RANGE_UPDATE_REPORT ),\
    HID_COLLECTION      ( HID_COLLECTION_LOGICAL                      ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_LAMP_UPDATE_FLAGS        ),\
      HID_LOGICAL_MIN   ( 0                                           ),\
      HID_LOGICAL_MAX   ( 8                                           ),\
      HID_REPORT_SIZE   ( 8                                           ),\
      HID_REPORT_COUNT  ( 1                                           ),\
      HID_FEATURE       ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE      ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_LAMP_ID_START            ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_LAMP_ID_END              ),\
      HID_LOGICAL_MIN   ( 0                                           ),\
      HID_LOGICAL_MAX_N ( 65535, 3                                    ),\
      HID_REPORT_SIZE   ( 16                                          ),\
      HID_REPORT_COUNT  ( 2                                           ),\
      HID_FEATURE       ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE      ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_RED_UPDATE_CHANNEL       ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_GREEN_UPDATE_CHANNEL     ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_BLUE_UPDATE_CHANNEL      ),\
      HID_USAGE         ( HID_USAGE_LIGHTING_INTENSITY_UPDATE_CHANNEL ),\
      HID_LOGICAL_MIN   ( 0                                           ),\
      HID_LOGICAL_MAX_N ( 255, 2                                      ),\
      HID_REPORT_SIZE   ( 8                                           ),\
      HID_REPORT_COUNT  ( 4                                           ),\
      HID_FEATURE       ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE      ),\
    HID_COLLECTION_END ,\
    /* Lamp Array Control Report */ \
    HID_REPORT_ID      ( report_id + 5                                ) \
    HID_USAGE          ( HID_USAGE_LIGHTING_LAMP_ARRAY_CONTROL_REPORT ),\
    HID_COLLECTION     ( HID_COLLECTION_LOGICAL                       ),\
      HID_USAGE        ( HID_USAGE_LIGHTING_AUTONOMOUS_MODE     ),\
      HID_LOGICAL_MIN  ( 0                                      ),\
      HID_LOGICAL_MAX  ( 1                                      ),\
      HID_REPORT_SIZE  ( 8                                      ),\
      HID_REPORT_COUNT ( 1                                      ),\
      HID_FEATURE      ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),\
    HID_COLLECTION_END ,\
  HID_COLLECTION_END \