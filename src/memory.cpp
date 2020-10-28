/*
 * Raspberry Pi ADVANCE reader writer
 * Copyright (C) 2018 CUBIC STYLE
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <wiringPi.h>
#include "memory.h"
#include "gpio.h"
#include "header.h"
#include "crc16.c"
#include "crc16.h"
#define LOGO_CRC 0x2e03

void draw_progress(uint32_t p);
#define PROGRESS(d,x)  if(d) draw_progress(x);

Memory::Memory()
{
}


Memory::~Memory()
{
}

// 対応させるメモリを書いていく
//  mask rom
class MemoryMaskRom : public MemoryRom{
  public:
  MemoryMaskRom(){
    type = ROM_MASK;
    typstr = (char *)"MASK ROM";
    memset(&header, 0, sizeof(RomHeader));
  }
    static int32_t find(){
        MemoryRom m;
        if(m.checkGbaHeader() < 0)
          return -1;
        return 1;
    }

    int32_t getDeviceCode(uint16_t *code){
      return -1;
    }

    int32_t write(uint32_t wadd, uint16_t dat){
      return -1;
    }
};


class MemoryCubicFlash : public MemoryRom{
  public:
    MemoryCubicFlash(uint32_t rom_size){
      mbit = rom_size;
      type = ROM_FLASH_CUBIC;
      typstr = (char *)"Cubic Flash Cartridge";
    }

    FlashMemoryInfo info;

    static int32_t getChipId(uint16_t *code){
      MemoryRom m;
      m.write(0x0555, 0x00aa);
      m.write(0x02aa, 0x0055);
      m.write(0x0555, 0x0090);
      // silicon code
      m.read(0x0000, &code[0]);
      // read Device code 1,2,3
      m.read(0x0001, &code[1]);
      m.read(0x000e, &code[2]);
      m.read(0x000f, &code[3]);

      // command reset
      m.write(0x0000, 0x00f0);

      return 4;
    }

    static void setFlashMemoryInfo(FlashMemoryInfo* info, uint16_t *dev_code){
      if(info == NULL)
        return;

      info->manufacturer_id = dev_code[0];
      info->deveice_id[0] = dev_code[1];
      info->deveice_id[1] = dev_code[2];
      info->deveice_id[2] = dev_code[3];


      // mx29gl256
      if((dev_code[0] & 0xff) == 0xC2 && dev_code[1] == 0x227E 
            && dev_code[2] == 0x2222 && dev_code[3] == 0x2201){
        info->mbit =  256;
        info->sector_size = 0x10000;     // 64K Word
        info->sector_mask = 0xFFFF0000;
        strcpy(info->device_name, "MX29GL256F");
      }
      // m29w128
      else if((dev_code[0] & 0xff) == 0x20 && dev_code[1] == 0x227E 
            && dev_code[2] == 0x2221 && dev_code[3] == 0x2200){
        info->mbit =  128;
        info->sector_size = 0x10000;     // 64K Word
        info->sector_mask = 0xFFFF0000;
        strcpy(info->device_name, "M29W128G");
      }
      // S29GL032
      else if((dev_code[0] & 0xff) == 0x01 && dev_code[1] == 0x227E 
            && dev_code[2] == 0x221a && dev_code[3] == 0x2200){
        info->mbit =  32;
        info->sector_size = 0x8000;     // 32K Word
        info->sector_mask = 0xFFFF8000;
        strcpy(info->device_name, "S29GL032");
      }
      // S29GL064
      else if((dev_code[0] & 0xff) == 0x01 && dev_code[1] == 0x227E 
            && dev_code[2] == 0x220c && dev_code[3] == 0x2201){
        info->mbit =  64;
        info->sector_size = 0x8000;     // 32K Word
        info->sector_mask = 0xFFFF8000;
        strcpy(info->device_name, "S29GL064");
      }
      // MT28EW256ABA
      else if((dev_code[0] & 0xff) == 0x89 && dev_code[1] == 0x227E 
            && dev_code[2] == 0x2222 && dev_code[3] == 0x2201){
        info->mbit =  256;
        info->sector_size = 0x10000;     // 64K Word
        info->sector_mask = 0xFFFF0000;
        strcpy(info->device_name, "MT28EW256");
      }
      // MT28EW128ABA
      else if((dev_code[0] & 0xff) == 0x89 && dev_code[1] == 0x227E 
            && dev_code[2] == 0x2221 && dev_code[3] == 0x2201){
        info->mbit =  128;
        info->sector_size = 0x10000;     // 64K Word
        info->sector_mask = 0xFFFF0000;
        strcpy(info->device_name, "MT28EW128");
      }

#ifdef __DEBUG__
      printf("\nFlash Memory Info=>\n");
      printf("  Density : %d MBit\n", info->mbit);
      printf("  manufacturer id: %04x\n", info->manufacturer_id );
      printf("  deveice id 1: %04x\n", info->deveice_id[0] );
      printf("             2: %04x\n", info->deveice_id[1] );
      printf("             3: %04x\n", info->deveice_id[2] );
      printf("\n");
#endif

    }


    static int32_t find(FlashMemoryInfo* info = NULL){
      uint16_t dev_code[4];
      getChipId(dev_code);
#ifdef __DEBUG__
      for(int i=0; i<4; i++) {
        printf("dev code %d: %04x\n", i, dev_code[i]);
      }
#endif
      setFlashMemoryInfo(info, dev_code);

      // mx29gl256
      if((dev_code[0] & 0xff) == 0xC2 && dev_code[1] == 0x227E 
            && dev_code[2] == 0x2222 && dev_code[3] == 0x2201){
        return 256;
      }
      // m29w128
      else if((dev_code[0] & 0xff) == 0x20 && dev_code[1] == 0x227E 
            && dev_code[2] == 0x2221 && dev_code[3] == 0x2200){
        return 128;
      }
      // S29GL032
      else if((dev_code[0] & 0xff) == 0x01 && dev_code[1] == 0x227E 
            && dev_code[2] == 0x221a && dev_code[3] == 0x2200){
        return 32;
      }
      // S29GL064
      else if((dev_code[0] & 0xff) == 0x01 && dev_code[1] == 0x227E 
            && dev_code[2] == 0x220c && dev_code[3] == 0x2201){
        return 64;
      }
      // MT28EW256ABA
      else if((dev_code[0] & 0xff) == 0x89 && dev_code[1] == 0x227E 
            && dev_code[2] == 0x2222 && dev_code[3] == 0x2201){
        return 256;
      }
      // MT28EW128ABA
      else if((dev_code[0] & 0xff) == 0x89 && dev_code[1] == 0x227E 
            && dev_code[2] == 0x2221 && dev_code[3] == 0x2201){
        return 128;
      }
      // Intel
      else if((dev_code[0] & 0xff) == 0x8a && dev_code[1] == 0x8902){ 
        return 128;
      }
      
      else
        return -1;
    }

    char *getDeviceName(){
      return info.device_name;
    }

    int32_t seqProgram(uint32_t wadd, uint16_t *dat, uint32_t len){
      const uint32_t buffer_size = 32;

      uint32_t sa = wadd & info.sector_mask, a = wadd;
      uint32_t i = 0, wl, rl = len;
      uint16_t status, k;

      // write buffer programming / 32 word
      while(rl > 0){
        wl = rl > buffer_size ?  buffer_size : rl;
        if(sa + info.sector_size < a + wl) // cant write across sectors
          wl = sa + info.sector_size - a;

        write(0x0555, 0x00aa);
        write(0x02aa, 0x0055);
        write(sa    , 0x0025);
        write(sa    , wl - 1);
        for(int j=0; j<wl; j++){
          write(a++, dat[i]);
          i++;
        }

        // confirm
        write(sa,     0x29);

        rl-=wl;
        sa=(a & info.sector_mask);

        k=0;
        while(1){
          delayMicroseconds(120); // buffer program typ:120us max:240us
          read(a-1, &status);   
          //printf("%02x %02x\n", dat[i-1] & 0xff, status & 0xff);
          if( (dat[i-1]&0x80) == (status&0x80))
            break;
          if(k++==4)  // timeout
            break;
        }

        PROGRESS( a % info.sector_size==0, 100 - (rl * 100 / len) );
      }
      PROGRESS(true, 100);   
      return 1;
    }

    int32_t seqProgramWriteCommand(uint32_t wadd, uint16_t *dat, uint32_t len){
      const uint32_t buffer_size = 32;

      uint32_t sa = wadd & 0xffff0000, a = wadd;
      uint32_t i = 0, wl, rl = len;
      uint16_t status, k;

      // write buffer programming / 32 word
      for(int j=0; j<len; j++){
        write(0x0555, 0x00aa);
        write(0x02aa, 0x0055);
        write(0x0555, 0x00a0);
        write(a++, dat[i++]);

        k=0;
        while(1){
          delayMicroseconds(120); // buffer program typ:120us max:240us S29GL064 max:1200us
          read(a-1, &status);   
          //printf("%02x %02x\n", dat[i-1] & 0xff, status & 0xff);
          if( (dat[i-1]&0x80) == (status&0x80))
            break;
          if(k++==15)  // timeout
            break;
        }

        PROGRESS( a%0x10000==0, 100 - (a * 100 / len) );
      }
      PROGRESS(true, 100);   
      return 1;
    }


    int32_t secErase(uint32_t wadd, uint32_t num){
        for(int32_t n=0; n<num; n++){
            // sec erase command
            write(0x0555, 0x00aa);
            write(0x02aa, 0x0055);
            write(0x0555, 0x0080);
            write(0x0555, 0x00aa);
            write(0x02aa, 0x0055);
            write((wadd & 0xffff0000) + n*0x10000, 0x0030);

            // check progress
            uint16_t tgl1, tgl2, i=0;
            read(0x0000, &tgl1);

            while(1){
              read(0x0000, &tgl2);
         
              //printf("%02x\n", tgl2 & 0xff);
              //printf(".");

              if( (tgl1&0x40) == (tgl2&0x40))
                break;

              tgl1 = tgl2;

              // mx29gl256 sector erase typ:0.5sec max:3.5sec
              if(i++==7)  // timeout
                break;

              delay(500);
            }
        }
        return 1;
    }


    int32_t chipErase(){
      // chip erase command
      write(0x0555, 0x00aa);
      write(0x02aa, 0x0055);
      write(0x0555, 0x0080);
      write(0x0555, 0x00aa);
      write(0x02aa, 0x0055);
      write(0x0555, 0x0010);

      delay(100);

      // check progress
      uint16_t tgl1, tgl2, i=0;
      read(0x0000, &tgl1);

      while(1){
        read(0x0000, &tgl2);
   
        //printf("%02x\n", tgl2 & 0xff);
        //printf(".\n");
        draw_progress( i * 100 / 250);

        if( (tgl1&0x40) == (tgl2&0x40))
          break;

        tgl1 = tgl2;

        // mx29gl256 chip erase typ:100sec max:250sec
        if(i++==250)  // timeout
          break;

        delay(1000);
      }

      draw_progress(100);

      return 1;
    }
};

/*
// m29w128
class MemoryCubicFlash128Mb : public MemoryRom{
    public:
    MemoryCubicFlash128Mb(){
        mbit = 128;
    }

    static int32_t getChipId(uint16_t *code){
      MemoryRom m;
      m.write(0x0555, 0x00aa);
      m.write(0x02aa, 0x0055);
      m.write(0x0555, 0x0090);
      // silicon code
      m.read(0x0000, &code[0]);
      // read Device code 1,2,3
      m.read(0x0001, &code[1]);
      m.read(0x000e, &code[2]);
      m.read(0x000f, &code[3]);

      // command reset
      m.write(0x0000, 0x00f0);
      return 4;
    }

    static int32_t find(){
      uint16_t dev_code[4];
      getChipId(dev_code);

      for(int i=0; i<4; i++){
        printf("dev code %d: %04x\n", i, dev_code[i]);
      }

      if((dev_code[0] & 0xff) == 0x20 && dev_code[1] == 0x227E 
            && dev_code[2] == 0x2221 && dev_code[3] == 0x2200){
        return true;
      }
      else
        return false;
    }
};
*/

// Flash2Advance
class MemoryF2aFlash : public MemoryRom{
    private:
        bool turbo;

        static void unlockRegister(MemoryRom& m){
          m.write(0x987654, 0x5354);
          for(int i=0; i<500; i++)
            m.write(0x012345, 0x1234);
          m.write(0x007654, 0x5354);
          m.write(0x012345, 0x5354);
          for(int i=0; i<500; i++)
            m.write(0x012345, 0x5678);
          m.write(0x987654, 0x5354);
          m.write(0x012345, 0x5354);
          m.write(0x765400, 0x5678);
          m.write(0x013450, 0x1234);
          for(int i=0; i<500; i++)
            m.write(0x012345, 0xabcd);
          m.write(0x987654, 0x5354);

          m.write(0xF12345, 0x9413);
      }

    public:
        MemoryF2aFlash(uint32_t rom_size){
            turbo = false;
            switch(rom_size >> 8){
                case 0x16 :       // 28F320J3A
                    mbit = 32;
                    break;
                case 0x17 :       // 28F640J3A
                    mbit = 64;
                    break;
                case 0x18 :       // 28F128J3A
                    mbit = 128;
                    break;
                default:
                    turbo = true;
                    // new cart
                    switch(rom_size & 0xff){
                      case 0x16:    // 2 x 28F320J3A
                          mbit = 64;
                          break;
                      case 0x17:    // 2 x 28F640J3A
                          mbit = 128;
                          break;
                      case 0x18:    // 2 x 28F128J3A
                          mbit = 256;
                          break;
                     }
            }
            typstr = (char *)"Flash Advance Cartridge";
        }


      static int32_t getChipId(uint16_t *code){
          MemoryRom m;
          unlockRegister(m);

          // Identifier mode
          m.write(0x000000, 0x0090);

          // Manufacturer ID
          m.read(0x000000, &code[0]);
          // Device ID
          m.read(0x000001, &code[1]);
          m.read(0x000002, &code[2]);

          // command reset
          m.write(0x0000, 0x00ff);
          m.write(0x0000, 0x00ff);

          return 3;
      }

    static int32_t find(){
        uint16_t dev_code[3];
        getChipId(dev_code);

#ifdef __DEBUG__
        for(int i=0; i<3; i++)
            printf("dev code %d: %04x\n", i, dev_code[i]);
#endif

        if(dev_code[0] == 0x0089 && ( (dev_code[1] >= 0x16 && dev_code[1] <= 0x18) 
                                      || (dev_code[2] >= 0x16 && dev_code[2] <= 0x18) ) )
            return (dev_code[1] & 0xff) << 8 | (dev_code[2] & 0xff);
        else
            return -1;
    }

    int32_t waitEraseReady(uint32_t wadd){
        uint16_t a, b, i=0;
        // check progress
        while(1){
            read(wadd, &a);
            a &= 0x80;
            if( turbo ){
                read(wadd + 1, &b);
                b &= 0x80;
            }
            else
                b = 0x80;

            if( (a + b) == 0x100 )
                break;

            //28F128j3a block erase typ:1sec max:5sec
            if(i++>110){  // timeout
              // printf("timeout! %x\n", wadd);
              break;
            }
            delay(50);
        }
    }

    int32_t waitWriteReady(uint32_t wadd){
        uint16_t a, b, i=0;
        // check progress
        while(1){
            read(wadd, &a);
            a &= 0x80;
            if( turbo ){
                read(wadd + 1, &b);
                b &= 0x80;
            }
            else
                b = 0x80;

            //printf("%08x a:%02x b:%02x\n", wadd, a, b);
            //printf(".");

            if( (a + b) == 0x100 )
                break;

            //28F128j3a write buffer typ:218us max:654us
            if(i++>5){  // timeout
              //printf("timeout! %x\n", wadd);
              break;
            }
            delayMicroseconds(220);
        }
    }


    int32_t secErase(uint32_t wadd, uint32_t num){  // turbo : block size: 2 * 0x10000 WB
        uint32_t a;
        for(int32_t i=0; i<num; i++){
            draw_progress( i * 100 / num  );

            // printf("sec erase : %2d\n", i);
            a = wadd + i * (turbo ? 0x20000 : 0x10000);

            write(0x0000 , 0x0070);
            if( turbo )
                write( 0x0001 , 0x0070);

            waitEraseReady(0x0000);

            // block erase command
            write(a , 0x0020);
            if( turbo )
                write( a+1 , 0x0020);

            waitEraseReady(0x0000);

            // confirm command
            write(a , 0x00d0);
            if( turbo )
                write( a+1, 0x00d0);

            waitEraseReady(0x0000);
        }
        draw_progress(100);

        // command reset
        write(0x0000, 0x00ff);
        if( turbo )
            write(0x0001, 0x00ff);
        write(0x0000, 0x00ff);
        if( turbo )
            write(0x0001, 0x00ff);
      return 1;
    }


    int32_t chipErase(){
        return secErase(0, turbo ? mbit / 2 : mbit);
    }

    int32_t seqProgram(uint32_t wadd, uint16_t *dat, uint32_t len){
      // write buffer programming
      // turbo : 16 word * 2
      // non turbo : 16 word
      uint32_t buffer_size = turbo ? 32 : 16;

      uint32_t sa = wadd & 0xffff0000, a = wadd;
      uint32_t i = 0, wl, rl = len;
      uint16_t status, k;

      while(rl > 0){
          wl = rl > buffer_size ?  buffer_size : rl;
          if(sa + 0x10000 < a + wl) // cant write across sectors
              wl = sa + 0x10000 - a;

          if(turbo){
              write(a, 0x00E8);
              write(a, wl/2 + wl%2 - 1);
              if(wl > 1){
                  write(a+1, 0x00E8);
                  write(a+1, wl/2 - 1);
              }
          }
          else{
              write(a, 0x00E8);
              write(a, wl - 1);
          }

          waitWriteReady(a);

          for(int j=0; j<wl; j++)
              write(a++, dat[i++]);

          // confirm
          write(a, 0xD0);
          if(turbo)
              write(a+1,     0xD0);

          rl-=wl;
          sa=(a & 0xffff0000);

          waitWriteReady(a);

        PROGRESS( a%0x10000==0, 100 - (rl * 100 / len) );
      }
      draw_progress( 100 );

      // command reset
      write(0x0000, 0x00ff);
      if( turbo )
          write(0x0001, 0x00ff);
      write(0x0000, 0x00ff);
      if( turbo )
          write(0x0001, 0x00ff);
      return 1;
    }

};


int32_t MemoryRom::read(uint32_t wadd, uint16_t *dat)
{
  gpio.setAdd(wadd);
  gpio.csLow();
  gpio.rdLow();
  *dat = gpio.getData();
  gpio.rdHigh();
  gpio.csHigh();
  return 1;
}

int32_t MemoryRom::seqRead(uint32_t wadd, uint16_t *dat, uint32_t len)
{
  for(uint32_t i=0; i<len; i++){
    if( (i+wadd) % 0x10000 == 0 || i == 0){
      gpio.csHigh();
      gpio.setAdd(i+wadd);
      gpio.csLow();
      draw_progress( ( i * 100 / len));
    }
    gpio.rdLow();
    dat[i] = gpio.getData();
    gpio.rdHigh();
  }
  draw_progress(100);
  gpio.csHigh();
  return len;
}

int32_t MemoryRom::write(uint32_t wadd, uint16_t dat)
{
  gpio.setAdd(wadd);
  gpio.csLow();
  gpio.setData(dat);
  gpio.wrLow();
  gpio.wrHigh();
  gpio.csHigh();
  return 1;
}

int32_t MemoryRom::getGbaHeader()
{
    uint16_t *buffer = (uint16_t *)&header;
    // READ GBA HEADER
    for(int i=0; i<192/2; i++)
      read(i, &buffer[i]);
    return 1;
}

int32_t MemoryRom::checkGbaHeader()
{
    uint16_t *buffer = new uint16_t[192/2];
    // READ GBA HEADER
    for(int i=0; i<192/2; i++)
      read(i, &buffer[i]);

    // seqRead(0x0000, (uint16_t *)buffer, 192/2);
    Header *header = (Header *)buffer;
    
    if(gen_crc16(header->logo, sizeof(header->logo)) != LOGO_CRC){
#ifdef __DEBUG__
      printf(" Header n logo not match!\n");
#endif
      return -1;
    }
    return 1;
}

// Backup
int32_t MemoryBackup::read(uint32_t badd, uint8_t *dat)
{
  gpio.setAdd(badd);
  gpio.csLow();
  gpio.rdLow();
  *dat = (uint8_t)gpio.getData();
  gpio.rdHigh();
  gpio.csHigh();
  return 1;
}

int32_t MemoryBackup::write(uint32_t badd, uint8_t dat)
{
  gpio.setAdd(badd);
  gpio.csLow();
  gpio.setData(dat);
  gpio.wrLow();
  gpio.wrHigh();
  gpio.csHigh();
  return 1;
}

int32_t MemoryBackup::load(uint32_t badd, uint8_t *dat, uint32_t len){
  for(int i=0; i<len; i++){
    read(badd + i, &dat[i]);
    PROGRESS( i%0x100==0, i * 100 / len );
  }
  draw_progress(100);
  return len;
}

int32_t MemoryBackup::csf()
{
  gpio.csFalling();
  return 1;
}

class MemoryBackupSram : public MemoryBackup{
	public:
    MemoryBackupSram(){
      kbyte = 32;
      typstr = (char *)"Sram(Fram)";
      type = BACKUP_SRAM;
    }
	static int32_t find(){
    MemoryBackup m;
    uint8_t t,t2;
    m.read(0x7FFF, &t);
    m.write(0x7FFF, ~t);
    m.read(0x7FFF, &t2);
    if((~t & 0xff) == t2){
      m.write(0x7FFF, t);
      return 1;
    }
    else
      return -1;
	}

  int32_t getDeviceCode(uint16_t *code){
      return -1;
  }

  int32_t save(uint32_t badd, uint8_t *dat, uint32_t len){
      for(int i=0; i<len; i++){
        write(badd + i, dat[i]);
        PROGRESS( i%0x100==0, i * 100 / len );
      }
      draw_progress( 100 );
      return len;
  }

  int32_t chipErase(){
    uint32_t len = kbyte * 1024;
    uint8_t *tmp = new uint8_t[len];
    memset(tmp, 0xff, len);
    save(0, tmp, len);
  }
};


class MemoryBackupEEPRom : public MemoryBackup{
	public:
	static int32_t find(){
	}

  int32_t getDeviceCode(uint16_t *code){
      return -1;
  }
};



// sst39vf0x0
class MemoryBackupCubic : public MemoryBackup{
	public:
    MemoryBackupCubic(uint32_t backup_size){
      kbyte = backup_size;
      typstr = (char *)"Cubic Flash";
      type = BACKUP_FLASH_CUBIC;
    }

    static int32_t getChipId(uint8_t *code){
      MemoryBackup m;
      m.write(0x5555, 0xaa);
      m.write(0x2aaa, 0x55);
      m.write(0x5555, 0x90);

      // need CS ridging
      m.csf();

      // silicon code
      m.read(0x0000, &code[0]);
      // read Device code 1,2,3
      m.read(0x0001, &code[1]);

      // command reset
      m.write(0x0000, 0x00f0);
      return 4;
    }

  	static int32_t find(){
        uint8_t dev_code[2];
        getChipId(dev_code);

#ifdef __DEBUG__
        for(int i=0; i<2; i++)
            printf("dev code %d: %02x\n", i, dev_code[i]);
#endif

      if(dev_code[0]== 0xbf && (dev_code[1] == 0xd7 || dev_code[1] == 0xd6 || dev_code[1] == 0xd5)){
        return 64;
      }
      return -1;
  	}
  int32_t save(uint32_t badd, uint8_t *dat, uint32_t len){
      uint8_t tmp;
      int32_t time_over = 5;
      for(int i=0; i<len; i++){
        write(0x5555, 0xaa);
        write(0x2aaa, 0x55);
        write(0x5555, 0xa0);
        write(badd + i, dat[i]);

        while(time_over){
          delayMicroseconds(20); // Byte-Program Time max:20us
          read(badd + i, &tmp);
          if( dat[i] ==  tmp)
            break;
          time_over--;
        }
        PROGRESS( i%0x100==0, i * 100 / len );
      }
      draw_progress( 100 );
      // command reset
      write(0x0000, 0x00f0);
      return 1;
  }

  int32_t chipErase(){
      uint8_t tmp;
      int32_t time_over = 5;
      write(0x5555, 0xaa);
      write(0x2aaa, 0x55);
      write(0x5555, 0x80);
      write(0x5555, 0xaa);
      write(0x2aaa, 0x55);
      write(0x5555, 0x10);

      while(time_over){
          delay(100);
          read(0x00, &tmp);
          if( 0xff ==  tmp)
            break;
          time_over--;
      }

      return 1;
  }
};

// official cart backup flash
class MemoryBackupFlash : public MemoryBackup{
    public:
    MemoryBackupFlash(uint32_t backup_size){
      kbyte = backup_size;
      typstr = (char *)"Flash";
      type = BACKUP_FLASH;
    }

    static int32_t getChipId(uint8_t *code){
      MemoryBackup m;
      m.write(0x5555, 0xaa);
      m.write(0x2aaa, 0x55);
      m.write(0x5555, 0x90);

      // need CS ridging
      m.csf();

      // silicon code
      m.read(0x0000, &code[0]);
      // read Device code 1,2,3
      m.read(0x0001, &code[1]);

      // command reset
      m.write(0x0000, 0x00f0);
      return 4;
    }

    static int32_t find(){
        uint8_t dev_code[2];
        getChipId(dev_code);

#ifdef __DEBUG__
        for(int i=0; i<2; i++)
            printf("dev code %d: %02x\n", i, dev_code[i]);
#endif

      // SST
      if(dev_code[0]== 0xbf && dev_code[1] == 0xd4){
        return 64;
      }
      // Macronix
      else if(dev_code[0]== 0xc2 && dev_code[1] == 0x1c){
        return 64;
      }
      // Panasonic
      else if(dev_code[0]== 0x32 && dev_code[1] == 0x1b){
        return 64;
      }

      return -1;
    }

  int32_t save(uint32_t badd, uint8_t *dat, uint32_t len){
      uint8_t tmp;
      int32_t time_over = 10;
      for(int i=0; i<len; i++){
        write(0x5555, 0xaa);
        write(0x2aaa, 0x55);
        write(0x5555, 0xa0);
        write(badd + i, dat[i]);

        while(time_over){
          delayMicroseconds(20);
          read(badd + i, &tmp);
          if( dat[i] == tmp)
            break;
          time_over--;
        }
        PROGRESS( i%0x100==0, i * 100 / len );
      }
      draw_progress(100);
      // command reset
      write(0x0000, 0x00f0);
  }

  int32_t chipErase(){
      uint8_t tmp;
      int32_t time_over = 5;
      write(0x5555, 0xaa);
      write(0x2aaa, 0x55);
      write(0x5555, 0x80);
      write(0x5555, 0xaa);
      write(0x2aaa, 0x55);
      write(0x5555, 0x10);

      while(time_over){
          delay(200);
          read(0x00, &tmp);
          if( 0xff ==  tmp)
            break;
          time_over--;
      }

      return 1;
  }
};


class MemoryBackupFlashAtmel : public MemoryBackupFlash{
    public:
    MemoryBackupFlashAtmel(uint32_t backup_size) : MemoryBackupFlash (backup_size){
      typstr = (char *)"Flash(Atmel)";
    }

    static int32_t find(){
        uint8_t dev_code[2];
        getChipId(dev_code);

#ifdef __DEBUG__
        for(int i=0; i<2; i++)
            printf("dev code %d: %02x\n", i, dev_code[i]);
#endif

      // Atmel
      if(dev_code[0]== 0x1f && dev_code[1] == 0x3d){
        return 64;
      }

      return -1;
    }

  int32_t save(uint32_t badd, uint8_t *dat, uint32_t len){
      uint8_t tmp;
      int32_t time_over = 10;
      for(int i=0; i<len; i++){
        write(0x5555, 0xaa);
        write(0x2aaa, 0x55);
        write(0x5555, 0xa0);
        for(int j=0; i<len && j<128; j++, i++)
          write(badd + i , dat[i]);
    
        while(time_over){
          delayMicroseconds(20);
          read(badd + (i-1), &tmp);
          if( dat[i-1] == tmp)
            break;
          time_over--;
        }
        PROGRESS( i%0x80==0, i * 100 / len );
      }
      draw_progress(100);
      // command reset
      write(0x0000, 0x00f0);
  }

};

class MemoryBackupFlashLarge : public MemoryBackupFlash{
    public:
    MemoryBackupFlashLarge(uint32_t backup_size) : MemoryBackupFlash (backup_size){
    }

    int32_t bank_num = -1;

    static int32_t find(){
        uint8_t dev_code[2];
        getChipId(dev_code);

#ifdef __DEBUG__
        for(int i=0; i<2; i++)
            printf("dev code %d: %02x\n", i, dev_code[i]);
#endif

      // Sanyo
      if(dev_code[0]== 0x62 && dev_code[1] == 0x13){
        return 128;
      }
      // Macronix
      else if(dev_code[0]== 0xc2 && dev_code[1] == 0x09){
        return 128;
      }
      return -1;
    }

    // to do
  int32_t load(uint32_t badd, uint8_t *dat, uint32_t len){
    for(int i=0; i<len; i++){

      // バンク切り替え
      if( ((badd + i) / 0x10000) != bank_num)
        switchBank((badd + i) / 0x10000);

      read(badd + i, &dat[i]);
      PROGRESS( i%0x100==0, i * 100 / len );
    }
    draw_progress(100);
    return len;
  }

  // to do
  int32_t save(uint32_t badd, uint8_t *dat, uint32_t len){
      uint8_t tmp;
      int32_t time_over = 10;     
      for(int i=0; i<len; i++){

        // バンク切り替え
        if( ((badd + i) / 0x10000) != bank_num){
          switchBank((badd + i) / 0x10000);
          delayMicroseconds(20);
        }

        write(0x5555, 0xaa);
        write(0x2aaa, 0x55);
        write(0x5555, 0xa0);
        write(badd + i, dat[i]);

        while(time_over){
          delayMicroseconds(20);
          read(badd + i, &tmp);
          if( dat[i] == tmp)
            break;
          time_over--;
        }
        PROGRESS( i%0x100==0, i * 100 / len );
      }
      draw_progress(100);
      // command reset
      write(0x0000, 0x00f0);
  }


  void switchBank(uint32_t num){
    write(0x5555, 0xaa);
    write(0x2aaa, 0x55);
    write(0x5555, 0xB0);
    write(0x0000, num);
    bank_num = num;
  }

};

Memory* Memory::create(MEMORY_SELECT ms)
{
  int32_t size;
  // ROMの種類を調べる
  if(ms == MEM_ROM){
    FlashMemoryInfo info;
    if((size = MemoryCubicFlash::find(&info)) > 0){
      MemoryCubicFlash *m = new MemoryCubicFlash(size);
      m->info = info;
      return (Memory *)m;
    }
    if((size = MemoryF2aFlash::find()) > 0 ){
      return (Memory *)new MemoryF2aFlash(size);
    }
    if(MemoryMaskRom::find() < 0)
      return nullptr;

    return (Memory *)new MemoryMaskRom();
  }
  else if(ms == MEM_BACKUP){
    if(MemoryBackupSram::find() > 0){
      return (Memory *)new MemoryBackupSram();
    }
    else if((size = MemoryBackupCubic::find()) > 0){
      return (Memory *)new MemoryBackupCubic(size);
    }
    else if((size = MemoryBackupFlash::find()) > 0){
      return (Memory *)new MemoryBackupFlash(size);
    }
    else if((size = MemoryBackupFlashLarge::find()) > 0){
      return (Memory *)new MemoryBackupFlashLarge(size);
    }

    return nullptr;
  }
  return nullptr;
}
