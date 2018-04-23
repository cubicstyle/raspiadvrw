#ifndef __GPIO_H__
#define __GPIO_H__

#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>

enum MEMORY_SELECT {
    MEM_ROM,
    MEM_BACKUP
};

enum DIRECTION_RW{
    DIR_WRITE,
    DIR_READ
};

class Gpio{
    private:
        static Gpio* instance;
        MEMORY_SELECT mem;
        DIRECTION_RW dir;
        int32_t addrbuff;

        void wait(int32_t n);
        void setPinMode(int32_t pin, int32_t d);
        void setDigitalWrite(int32_t pin, int32_t w);
        int32_t getDigitalRead(int32_t pin);
        void setDiretion(DIRECTION_RW d);

    public:
        Gpio();
        ~Gpio();
        void init(MEMORY_SELECT m);
        void setAdd(uint32_t addr);
        void setData(uint16_t data);
        uint16_t getData();
        void csHigh();
        void csLow();
        void csFalling();
        void rdHigh();
        void rdLow();
        void wrHigh();
        void wrLow();

        static Gpio& getInstance();
        static Gpio& create();
};

#endif
