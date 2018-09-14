/*
 * Raspberry Pi ADVANCE reader writer
 * Copyright (C) 2018 CUBIC STYLE
 */

#include <stdio.h>
#include <stdint.h>
#include <wiringPi.h>
#include "memory.h"
#include "gpio.h"
#include "header.h"
#include "crc16.c"
#include "crc16.h"
#define LOGO_CRC 0x2e03

void draw_progress(uint32_t p);

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
    typstr = (char *)"MASK ROM";
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


// mx29gl256 or m29w128
class MemoryCubicFlash : public MemoryRom{
  public:
    MemoryCubicFlash(uint32_t rom_size){
      mbit = rom_size;
      typstr = (char *)"Cubic Flash Cartridge";
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

#ifdef __DEBUG__
      for(int i=0; i<4; i++) {
        printf("dev code %d: %04x\n", i, dev_code[i]);
      }
#endif
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
      else
        return -1;
    }

    int32_t seqProgram(uint32_t wadd, uint16_t *dat, uint32_t len){
      const uint32_t buffer_size = 32;

      uint32_t sa = wadd & 0xffff0000, a = wadd;
      uint32_t i = 0, wl, rl = len;
      uint16_t status, k;

      // write buffer programming / 32 word
      while(rl > 0){
        wl = rl > buffer_size ?  buffer_size : rl;
        if(sa + 0x10000 < a + wl) // cant write across sectors
          wl = sa + 0x10000 - a;

        write(0x0555, 0x00aa);
        write(0x02aa, 0x0055);
        write(sa    , 0x0025);
        write(sa    , wl - 1);
        for(int j=0; j<wl; j++)
          write(a++, dat[i++]);
        // confirm
        write(sa,     0x29);

        rl-=wl;
        sa=(a & 0xffff0000);

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

        if(a%0x10000==0){
          draw_progress( 100 - (rl * 100 / len)  );
        }
      }
      draw_progress( 100 );      
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

          if(a%0x10000==0){
              //printf("write:%06x\n",a);
              draw_progress( 100 - ((rl / len) * 100)  );
          }
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

      if(dev_code[0]== 0xbf && (dev_code[1] == 0xd7 || dev_code[1] == 0xd6)){
        return 32;
      }
      return -1;
  	}
};

// official cart backup flash
class MemoryBackupFlash : public MemoryBackup{
    private:
        bool bank;

    public:
    MemoryBackupFlash(uint32_t backup_size){
      bank = backup_size > 64 ? true : false;
      kbyte = backup_size;
      typstr = (char *)"Flash";
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
      // Atmel
      else if(dev_code[0]== 0x1f && dev_code[1] == 0x3d){
        return 64;
      }
      // Sanyo
      else if(dev_code[0]== 0x62 && dev_code[1] == 0x13){
        return 128;
      }
      // Macronix
      else if(dev_code[0]== 0xc2 && dev_code[1] == 0x09){
        return 128;
      }

      return -1;
    }
    
};

Memory* Memory::create(MEMORY_SELECT ms)
{
  int32_t size;
  // ROMの種類を調べる
  if(ms == MEM_ROM){
    if((size = MemoryCubicFlash::find()) > 0){
      return (Memory *)new MemoryCubicFlash(size);
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
    return nullptr;
  }
  return nullptr;
}
