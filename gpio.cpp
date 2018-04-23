#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <wiringPi.h>
#include <mcp23s08.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "gpio.h"

#define CS        26
#define CS2       2
#define RD        20
#define WR        3
#define RST       23 

#define MCP_BASE  123
#define MCP_WAIT 50
#define CS_WAIT 3   //2
#define RD_WAIT 4
#define WR_WAIT 5   //3
#define WR_WAIT_B 1

union CART_PIN{
    uint16_t io[24];
    struct ROM_BUS{
        uint16_t low[16];
        uint16_t high[8];
    } rom;
    struct BACKUP_BUS{
        uint16_t add[16];
        uint16_t dat[8];
    } backup;
} cart = {16, 19, 13, 12, 6, 5, 25, 24, 22, 27, 18, 17, 15, 14, 4, 21};

Gpio* Gpio::instance = NULL;

Gpio::Gpio(){

    wiringPiSetupGpio();

    pinMode(CS, OUTPUT);
    digitalWrite(CS, 1);
    pinMode(CS2, OUTPUT);
    digitalWrite(CS2, 1);
    pinMode(RD, OUTPUT);
    digitalWrite(RD, 1);
    pinMode(WR, OUTPUT);
    digitalWrite(WR, 1);
    pinMode(RST, OUTPUT);
    digitalWrite(RST, 1);

    mcp23s08Setup (MCP_BASE, 1, 0);

    for(int i=0; i<8; i++)
        cart.io[i + 16] = MCP_BASE + i;
}

Gpio::~Gpio(){
    digitalWrite(CS, 1);
    digitalWrite(CS2, 1);
    digitalWrite(RD, 1);
    digitalWrite(WR, 1);
    digitalWrite(RST, 1);
}

void Gpio::wait(int32_t n)
{
  delayMicroseconds(n);
}

// mcpのIOと設定を分ける場合を考えて別に定義する
void Gpio::setPinMode(int32_t pin, int32_t d){
    if(pin >= MCP_BASE){
        pinMode(pin, d);
    }
    else{
        pinMode(pin, d);
    }
}

void Gpio::setDigitalWrite(int32_t pin, int32_t w){
    if(pin >= MCP_BASE){
        digitalWrite(pin, w);
        //delay(5);
    }
    else{
        digitalWrite(pin, w);
    }
}

int32_t Gpio::getDigitalRead(int32_t pin){
    int32_t d;
    if(pin >= MCP_BASE){
        d = digitalRead(pin);
        //delay(1);
    }
    else{
        d = digitalRead(pin);
    }
    return d;
}

void Gpio::setDiretion(DIRECTION_RW d){
    int i;
    if(mem == MEM_ROM){
        if(d == DIR_READ)
            for(i=0; i<16; i++)
                  pinMode(cart.rom.low[i], INPUT);
        else if(d==DIR_WRITE)
            for(i=0; i<16; i++)
                  pinMode(cart.rom.low[i], OUTPUT);
    }
    else if(mem == MEM_BACKUP){
        if(d == DIR_READ){
            for(i=0; i<8; i++){
                  pinMode(cart.backup.dat[i], INPUT);
                  //pullUpDnControl (cart.backup.dat[i], PUD_UP) ;
            }
            //wait(MCP_WAIT);
        }
        else if(d==DIR_WRITE)
            for(i=0; i<8; i++)
                  pinMode(cart.backup.dat[i], OUTPUT);
    }
    dir = d;
}

void Gpio::init(MEMORY_SELECT m)
{

    addrbuff = -1;
    mem = m;

    // 念のため mcp をリセット
    digitalWrite(RST, 0);
    wait(20);
    digitalWrite(RST, 1);
    wait(20);

    // ずっと向きが変わらないもの
    if(mem == MEM_ROM){
        for(int i=0; i<8; i++)
            setPinMode(cart.rom.high[i], OUTPUT);
    }
    else if(mem == MEM_BACKUP){
        for(int i=0; i<16; i++)
            setPinMode(cart.backup.add[i], OUTPUT);
    }
    setDiretion(DIR_READ);
}

void Gpio::setAdd(uint32_t addr){
  if(mem == MEM_ROM){
      if(dir != DIR_WRITE)
          setDiretion(DIR_WRITE);

      for(int i=0; i<16; i++){
          setDigitalWrite(cart.rom.low[i], (addr >> i) & 1 );
      }

      uint32_t hadd = ((addr >> 16) & 0xff);
      // mcp への digialWrite は時間がかかるので、変えるときだけしかやらない
      for(int i=0; i<8; i++){
          if( addrbuff < 0 || ( ( (hadd >> i) & 1) != ( (addrbuff >> i) & 1) ) ) 
              setDigitalWrite(cart.rom.high[i], (hadd >> i) & 1 );
      }
      addrbuff = hadd;
  }
  else if(mem == MEM_BACKUP){  
      for(int i=0; i<16; i++)
          setDigitalWrite(cart.backup.add[i], (addr >> i) & 1 );
  }
}

void Gpio::setData(uint16_t data){
    if(mem == MEM_ROM){
        if(dir != DIR_WRITE)
            setDiretion(DIR_WRITE);

        for(int i=0; i<16; i++)
            setDigitalWrite(cart.rom.low[i], (data >> i) & 1 );
    }
    else if(mem == MEM_BACKUP){
        if(dir != DIR_WRITE)
            setDiretion(DIR_WRITE);

        for(int i=0; i<8; i++)
            setDigitalWrite(cart.backup.dat[i], (data >> i) & 1 );
    }
}

uint16_t Gpio::getData(){
    uint16_t data = 0;
    if(mem == MEM_ROM){
        if(dir != DIR_READ)
            setDiretion(DIR_READ);

        for(int i=0; i<16; i++){
            data |= (getDigitalRead(cart.rom.low[i]) << i);
        }
    }
    else if(mem == MEM_BACKUP){
        if(dir != DIR_READ)
            setDiretion(DIR_READ);

        for(int i=0; i<8; i++)
            data |= (getDigitalRead(cart.backup.dat[i]) << i);
    }
    return data;
}

void Gpio::csHigh(){
    if(mem == MEM_ROM)
        digitalWrite(CS, 1);
    else if(mem == MEM_BACKUP){
        digitalWrite(CS2, 1);
    }
    wait(CS_WAIT);
}

void Gpio::csLow(){
    if(mem == MEM_ROM)
        digitalWrite(CS, 0);
    else if(mem == MEM_BACKUP){
        digitalWrite(CS2, 0);
    }
    wait(CS_WAIT);
}

void Gpio::csFalling(){
    csHigh();
    csLow();
}

void Gpio::rdHigh(){
    digitalWrite(RD, 1);
    wait(RD_WAIT);  
}

void Gpio::rdLow(){
    digitalWrite(RD, 0);
    wait(RD_WAIT);  
}

void Gpio::wrHigh(){
    wait(WR_WAIT_B);  
    digitalWrite(WR, 1);
    wait(WR_WAIT);  
}

void Gpio::wrLow(){
    wait(WR_WAIT_B);  
    digitalWrite(WR, 0);
    wait(WR_WAIT);  
}

Gpio& Gpio::getInstance() {
    return *instance;
}

Gpio& Gpio::create() {
    if ( !instance ) {
        instance = new Gpio;
    }
    return *instance;
}


