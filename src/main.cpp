/*
 * Raspberry Pi ADVANCE reader writer
 * Copyright (C) 2018 CUBIC STYLE
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>
#include <wiringPi.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <iostream>
#include <exception> 

#include "gpio.h"
#include "memory.h"

#define VERSION "1.00"

using namespace std;

enum MODE { NONE, READ, WRITE, INFO, BLOCK_ERASE, DUMP,
	CHIP_ERASE, DUPLICATE, VERIFY, BLANK, TEST };

#define LOGO_CRC 0x2e03

void exit_func(int i){
	exit(1);
}

void exit_func2(){
	exit(1);
}

bool nl_flag = false;
void draw_progress(uint32_t p) {	// p:0-100
	if(p > 100)
		printf("draw_progress p > 100 p:%d\n",p);

	uint32_t i=0;
	char buf[128], *b;
	b = buf;
	*b = '\0';
    setbuf(stdout, NULL);

	b += sprintf(b, "[");
	for(; i<p/2; i++)
		b += sprintf(b, "#");
	for(;i<50; i++)
		b += sprintf(b, " ");
	b += sprintf(b, "]");

	printf("%s", buf);
	printf(" %3d%%", p);
    printf("\r");

    if(p==100 && !nl_flag){
    	nl_flag = true;
    	printf("\n");
    }
    else
    	nl_flag = false;
}

int main(int argc,char *argv[]) {
	int32_t i,j, n, len, mode, code, word_count, result, addr, block, block_num, ret;
	unsigned short int dat, dat1, dat2;

	bool backup;
	FILE *fp;
	long file_size;
	struct stat stbuf;
	int fd;
	unsigned char *buffer = 0;

	mode = NONE;
	backup = false;

	len = 0;
	addr = 0;
	block = 0;
	block_num = 0;
	fd = -1;
	result = 1;

	if(argc > 1){
		for(i=1; i<argc; i++){
			if(*argv[i] == '-'){
        		switch( *(argv[i]+1) ){
					case 'w': // ファイル書き込み
						mode = WRITE;
						fd = open(argv[++i], O_RDONLY);
						if (fd == -1) {
							printf("file open error!!\n");
							exit(EXIT_FAILURE);
						}

						fp = fdopen(fd, "rb");
						if (fp == NULL) {
							printf("file fdopen error!!\n");
							exit(EXIT_FAILURE);
						}

						if (fstat(fd, &stbuf) == -1) {
							printf("file fstat error!!\n");
							exit(EXIT_FAILURE);
						}
						if(len==0)
							len = stbuf.st_size;
						break;

					case 'r': // ROM 読み込んでファイルに書き込み
						mode = READ;

						fd = open(argv[++i], O_WRONLY | O_CREAT, 0755);
						if (fd == -1) {
							printf("file open error!!\n");
							exit(EXIT_FAILURE);
						}

						fp = fdopen(fd, "wb");
						if (fp == NULL) {
							printf("file fdopen error!!\n");
							exit(EXIT_FAILURE);
						}

						if (fstat(fd, &stbuf) == -1) {
							printf("file fstat error!!\n");
							exit(EXIT_FAILURE);
						}
						break;

					case 'L':
						len = atoi(argv[++i]) * 1024 * 1024;
						break;

					case 'l':
						len = atoi(argv[++i]);
						break;

					case 'c': // ROM情報取得
						mode = INFO;
						break;

					case 'e': // ROM block erase
						mode = BLOCK_ERASE;
						break;

					case 'V': // ROM VERIFY
						mode = VERIFY;
						fd = open(argv[++i], O_RDONLY);
						if (fd == -1) {
							printf("file open error!!\n");
							exit(EXIT_FAILURE);
						}

						fp = fdopen(fd, "rb");
						if (fp == NULL) {
							printf("file fdopen error!!\n");
							exit(EXIT_FAILURE);
						}

						if (fstat(fd, &stbuf) == -1) {
							printf("file fstat error!!\n");
							exit(EXIT_FAILURE);
						}
						if(len==0)
							len = stbuf.st_size;
						break;

					case 'E': // ROM chip erase
						mode = CHIP_ERASE;
						break;	  

					case 'd': // ROM dump
						mode = DUMP;
						break;

					case 'a': // ROM address
						addr = strtol(argv[++i], NULL, 0);
						break;

					case 'b': // block address
						block = strtol(argv[++i], NULL, 0);
						break;

					case 'n': // erase block num
						block_num = strtol(argv[++i], NULL, 0);
						break;

					case 's': // SRAM(backup rom)
						backup = true;
						break;

					case 'D': // DUPLICATE SET 
						mode = DUPLICATE;
						break;

					case 'B': // FLASH BLANK CHECK
						mode = BLANK;
						break;

					case 'T': // DEBUG TEST 
						mode = TEST;
						break;

					case 'h':
						printf("rpa [-w gbarom] [-r dstfile] [-l size] [-L size(MB)]-c [-a address]\n");
						printf("\t-c: ROM info\n");
					  	printf("\t-r: Read ROM\n");
	  					printf("\t-w: Write ROM\n");
						printf("\t-e: block erase\n");
						printf("\t-E: chip erase\n");	  
						printf("\t-B: BLANK check\n");
						printf("\t-d: dump\n");
						printf("\t-a: rom address\n");
						printf("\t-b: block address\n");
						printf("\t-n: erase block num\n");
						printf("\t-s: SRAM(backup rom)\n");
						printf("\t-h: this help\n");
						exit(1);

					case 'v':
						printf("Raspberry Pi ADVANCE Reader Writer v.%s\n",VERSION);
						printf("Copyright (C) 2018 CUBIC STYLE\n");
						exit(1);

					default:
						printf("option error:%s\n",argv[i]);
						break;
				}
			}
		}
	}

	if ( mode == NONE ) {
		mode = INFO;
	}

	if(len==0){
		if(!backup)
			len = 16 * 1024 * 1024;	// to do
		else 
			len = 32 * 1024;
	}
	if(len%2==1 && backup)
		len++;

	try{
		buffer = new uint8_t[len];		
		memset(buffer, 0, len);
	}
	catch (exception& e)
	{
		cout << e.what() << '\n';
		printf("error : new error \n");
		exit(1);
	}
  
	atexit(exit_func2);
	if (SIG_ERR == signal(SIGHUP, exit_func)) {
		printf("failed to set sighup handler\n");
		exit(1);
	}
	if (SIG_ERR == signal(SIGINT, exit_func)) {
		printf("failed to set sigint handler\n");
		exit(1);
	}

	if(!backup)
	{
		printf("MAIN MEMORY:\n");
		MemoryRom *m = (MemoryRom *)Memory::create();
		if(m == nullptr){
			printf("cartridge can not be detected or not supported.\n");
			exit(1);
		}

		if(mode == INFO){
			printf("Cart: %s\n", m->getTypeStr());
			if(m->getMemoryMbit() > 0)
				printf("Size: %d Mbit\n", m->getMemoryMbit());
		}

		else if(mode == READ){
			printf("READ MODE =>\n");
			m->seqRead(0, (uint16_t *)buffer, len/2);
			write(fd, buffer, len);
			close(fd);
			fclose(fp);
			printf("read finish\n"); 
		}
		else if(mode == DUMP){
			printf("DUMP MODE (0x%x) =>\n", addr);

			try{
				m->seqRead(0, (uint16_t *)buffer, 256);
				printf("secRead:\n");
				for(i=0; i<256; i++){
					printf("%02x ", buffer[i] );
					if(i%16==15)
						printf("\n");
				}
			}
			catch(char *str) {
				printf("%s\n",str);
			}
		}

		else if(mode == BLOCK_ERASE){
			printf(" BLOCK ERASE START =>\n");
			m->secErase(block* 0x10000 , block_num);
			printf("block erase finish\n");    
		}

		else if(mode == CHIP_ERASE){
			printf("CHIP ERASE START =>\n");
			ret = m->chipErase();
			printf("chip erase finish!(%d)\n",ret);
		}

		else if(mode == WRITE)
		{
			len = fread( buffer, sizeof( unsigned char ), len, fp );
			//printf("\nfread : %d\n",len);
			printf("WRITE START =>\n");
			m->seqProgram(0, (uint16_t *)buffer, len/2);
			printf("write finish!\n");
			close(fd);
			fclose(fp);
		}

		else if(mode == DUPLICATE)
		{

		}

		else if(mode == VERIFY)
		{
			printf("VERIFY START =>\n");
			uint8_t * buffer2, b[2];		
			uint32_t error_num = 0;
			try{
				len = fread( buffer, sizeof( unsigned char ), len, fp );
				buffer2 = new uint8_t[len];
				m->seqRead(0, (uint16_t *)buffer2, len/2);
			}
			catch(char *str) {
				printf("%s\n",str);
			}

			for(i=0; i<len; i++){
				if(buffer[i] != buffer2[i]){
				// byteRead で再度チェック
				// todo: f2a カートはsecRead で 0x220000 と 0xfe0000 付近で値がバグることがある
					m->read(i/2, (uint16_t *)b);
					if(( b[i%2] & 0xff) == buffer[i] )
						continue;

					if(error_num < 32)
						printf("%6x: %02x != %02x (rom)\n", i , buffer[i], buffer2[i] );
					error_num++;
				}
			}
			close(fd);
			fclose(fp);
			printf("verify finish!\n");
			printf("error_num:%d\n",error_num);
		}

		else if(mode == BLANK){
			uint8_t b[2];
			uint32_t error_num = 0;
			try{
				len = m->getMemoryMbit() * 1024 * 1024 / 8;
				if(buffer)
					delete[] buffer;
				buffer = new uint8_t[len];
			}
			catch(char *str) {
				printf("%s\n",str);
			}

			printf("BLANK CHECK=>\n");

			m->seqRead(0, (uint16_t *)buffer, len/2);
			for(i=0; i<len; i++){
				if(buffer[i] != 0xff){
				// byteRead で再度チェック
				// todo: f2a カートはsecRead で 0x220000 と 0xfe0000 付近で値がバグることがある
					m->read(i/2, (uint16_t *)b);
					if(( b[i%2] & 0xff) == 0xff ){
						continue;
					}

					if(error_num < 32)
						printf("%06x: %02x != 0xff\n", i , buffer[i]);
					error_num++;
				}
			}
			printf("error_num:%d\n",error_num);
		}

		else if(mode == TEST)
		{


		}


	}
	else if(backup){
		printf("BACKUP MEMORY:\n");
		MemoryBackup *b = (MemoryBackup *)Memory::create(MEM_BACKUP);
		if(b == nullptr){
			printf("cartridge can not be detected or not supported.\n");
			exit(1);
		}

		if(mode == DUMP){
			for(i=0; i<256; i++){
				b->read(i, (uint8_t *)buffer);
				printf("%02x ", buffer[0] );
				if(i%16==15)
					printf("\n");
			}
		}
		else if(mode == READ){
			printf("READ START=>\n");
			uint8_t *buf = (uint8_t *)buffer;
			for(i=0; i<32*1024; i++)
				b->read(i, &buf[i]);
			write(fd, buf, 32*1024);
			close(fd);
			fclose(fp);
			printf("read finish\n"); 
		}
		else if(mode == WRITE){
			printf("WRITE START=>\n");
			uint8_t *buf = (uint8_t *)buffer;
			len = fread( buf, sizeof( unsigned char ), 32*1024, fp );
			//printf("\nfread : %d\n",len);
			printf("WRITE START =>\n");
			for(i=0; i<len; i++)
				b->write(i, buf[i]);
			printf("write finish!\n");
		}
		else if(mode == TEST)
		{

		}
	}

end:
  if(buffer)
    delete[] buffer;
  
  return 0;
}

