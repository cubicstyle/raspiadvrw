#pragma once
#include <stdint.h>
#include "gpio.h"

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
} RomHeader;


typedef struct 
{
    uint32_t mbit;  //memory size Mbit
    uint16_t manufacturer_id;
    uint16_t deveice_id[3];
    char device_name[16];
    uint32_t sector_size; // word size
    uint32_t sector_mask;
    uint32_t write_buffer; // word size
    uint32_t buff_prg_wait;
} FlashMemoryInfo;

// Factory Class
class Memory{
    private:
        Memory* m;

    protected:
        Gpio &gpio = Gpio::create();
        Memory();
        ~Memory();
    public:
        static Memory* create(MEMORY_SELECT ms = MEM_ROM);
/*
        virtual bool find();
        virtual int32_t getDeviceCode(uint16_t *code);
        */
};

enum ERomType{
    ROM_NONE, ROM_MASK, ROM_FLASH, ROM_FLASH_CUBIC
};

// MainROMの基底クラス
class MemoryRom : public Memory{
    protected:
        uint32_t mbit;  //memory size Mbit
        char* typstr;
        ERomType type;

    public:
        RomHeader header;
        //virtual bool getHeader();
        virtual int32_t write(uint32_t wadd, uint16_t dat);
        virtual int32_t read(uint32_t wadd, uint16_t *dat);
        virtual int32_t seqRead(uint32_t wadd, uint16_t *dat, uint32_t len);

        virtual int32_t program(uint32_t wadd, uint16_t dat){
            return -1;
        };
        virtual int32_t seqProgram(uint32_t wadd, uint16_t *dat, uint32_t len){
            return -1;
        };
        virtual int32_t seqProgramWriteCommand(uint32_t wadd, uint16_t *dat, uint32_t len){
            return -1;
        };
        virtual int32_t secErase(uint32_t wadd, uint32_t num){
            return -1;
        };
        virtual int32_t chipErase(){
            return -1;
        };
        virtual int32_t checkGbaHeader();
        virtual int32_t getGbaHeader();

        virtual char * getDeviceName(){
            return (char *)"\0";
        };

        char * getTypeStr(){
            return typstr;
        };

        ERomType getType(){
            return type;
        };

        uint32_t getMemoryMbit(){
            return mbit;
        };
/*
        virtual int32_t verfy(uint32_t address, uint16_t *dat, uint32_t len);
        virtual int32_t duplicate();  
        virtual int32_t reset();
*/
        MemoryRom(){
            mbit = 0;
            typstr = (char *)"";
            gpio.init(MEM_ROM);
        }

        MemoryRom(uint32_t rom_size){
            mbit = rom_size;
            typstr = (char *)"";
            gpio.init(MEM_ROM);
        }
};

enum EBackupType{
    BACKUP_NONE, BACKUP_SRAM, BACKUP_FLASH, BACKUP_FLASH_LARGE, BACKUP_FLASH_CUBIC, BACKUP_EEPROM
};

// BackupROMの基底クラス
class MemoryBackup : public Memory{
    protected:
        uint32_t kbyte; //memory size Kbit
        char* typstr;
        EBackupType type;

    public:
        virtual int32_t write(uint32_t badd, uint8_t dat);
        virtual int32_t read(uint32_t badd, uint8_t *dat);
        virtual int32_t csf();
        virtual int32_t save(uint32_t badd, uint8_t *dat, uint32_t len){
            return -1;  
        }
        virtual int32_t load(uint32_t badd, uint8_t *dat, uint32_t len);

        virtual int32_t chipErase(){
            return -1;
        };
        char * getTypeStr(){
            return typstr;
        };

        EBackupType getType(){
            return type;
        };

        uint32_t getMemoryKb(){
            return kbyte;
        };

        MemoryBackup(){
            kbyte = 0;
            gpio.init(MEM_BACKUP);
            typstr = (char *)"NONE";
            type = BACKUP_NONE;
        }
};

