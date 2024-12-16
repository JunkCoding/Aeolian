#ifndef __APP_CONFIG_H__
#define __APP_CONFIG_H__

#include <stdint.h>

#define ICACHE_RODATA_ATTR
#define STORE_ATTR __attribute__( ( aligned( 4 ) ) )

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#define SPI_FLASH_SEC_SIZE          4096
#define SPI_FLASH_SIZE              0x400000
#define SPI_FLASH_BASE_ADDR         0x0

#define INITDATAPOS                 ( SPI_FLASH_BASE_ADDR + SPI_FLASH_SIZE )

// .id field
#define ID_MAX                      0xF0
#define ID_EXTRA_DATA               0xF1      // extra data are not stored in the config_list[]
                                              // they are stored in the config_user date section in the spi-flash
#define ID_EXTRA_DATA_TEMP          0xF3      // record can remove whem flash is cleaned up
#define ID_SKIP_DATA                0xFF

// .valid field
#define SPI_FLASH_RECORD            0xFE      // record stored in spi flash
#define DEFAULT_RECORD              0xFF      // record from program memory
#define RECORD_ERASED               0xF0
#define RECORD_VALID                0xF0

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

enum
{
  TEXT = 1,      // texts are stored in readonly 32bit aligned memory ( irom, iram )
  NUMBER,
  IP_ADDR,
  FLAG,
  NUMARRAY,
  STRUCTURE,
  FILLDATA
};

typedef union
{
  struct
  {
    uint8_t id;
    uint8_t type;
    uint8_t len;
    uint8_t valid;  // >= 0xF0 record is valid
  };
  uint32_t mode;
} cfg_mode_t;

typedef struct
{
  union
  {
    struct
    {
      uint8_t id;
      uint8_t type;
      uint8_t len;      // strlen, without terminating null character
      uint8_t valid;
    };
    uint32_t mode;
  };
  union
  {
    char *text;
    uint32_t val;
  };
} settings_t;

typedef struct
{
  union
  {
    struct
    {
      uint8_t id;
      uint8_t type;
      uint8_t len;
      uint8_t valid;
    };
    uint32_t mode;
  };
  const char *text;
} const_settings_t;

typedef struct
{
  struct
  {
    uint8_t id;
    uint8_t type;
    uint8_t dmy1;
    uint8_t dmy2;
  };
   const char STORE_ATTR *token;
} STORE_ATTR Config_Keyword_t;

typedef struct
{
  const  Config_Keyword_t *config_keywords;
  int num_keywords;
  const_settings_t *defaults;
  int ( *get_config )( int id, char **str, int *value );
  int ( *compare_config )( int id, char *str, int value );
  int ( *update_config )( int id, char *str, int value );
  int ( *apply_config )( int id, char *str, int value );
} Configuration_Item_t;

typedef Configuration_Item_t *Configuration_List_t;

#define NUMARRAY_SIZE  16

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

Configuration_List_t * get_configuration_list( void );


bool    str2ip( const char* str, void *ip );
int     str2int_array( char* str, int* array, int num_values );

void    _config_build_list( Configuration_List_t *cfg_list, int num_cfgs );
int     get_num_configurations( void );
int     get_num_keywords( void );

void    _config_print_defaults( const_settings_t *config_defaults, int num_lists );
void    _config_print_settings( void );

int     _config_get( int id, char *buf, int buf_size );
int     _config_get_int( int id, int *val );
int     _config_get_bool( int id, bool *val );
int     _config_get_uint8( int id, uint8_t *val );

char*   _config_save_str( int id, char *str, int strlen, int type );
char*   _config_save_int( int id, int value, int type );
int     _config_save( cfg_mode_t cfg_mode, char *str, int value );

int      user_config_read( uint32_t addr, char *buf, int len );
int      user_config_write( uint32_t addr, char *buf, int len );
int      user_config_scan( int id, int (call_back)(), void *arg );
uint32_t user_config_invalidate( uint32_t addr );
int      user_config_print( void );

#endif //  __CONFIGS_H__

