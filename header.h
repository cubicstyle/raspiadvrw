#include <stdint.h>

typedef struct
{
  uint32_t start_code;
  uint8_t  logo[156];
  uint8_t  game_title[12];
  uint32_t game_code;
  uint16_t maker_code;
  uint8_t  fixed_value;  // 0x96
  uint8_t  unit_code;  // 0x00
  uint8_t  device_type;
  uint8_t  unused[7];
  uint8_t  version;  
  uint8_t  complement_check; 
  uint16_t reserve;  // 0x0000
} Header;

