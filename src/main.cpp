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

#define VERSION "1.12"

using namespace std;

enum MODE { NONE, READ, WRITE, INFO, BLOCK_ERASE, DUMP,
	CHIP_ERASE, DUPLICATE, VERIFY, BLANK, TEST, TEST2 };

#define LOGO_CRC 0x2e03

void exit_func(int i){
	exit(1);
}

void exit_func2(){
	exit(1);
}

void print_version(){
	printf("Raspberry Pi ADVANCE Reader Writer v%s\n",VERSION);
	printf("Copyright (C) 2018 CUBIC STYLE\n\n");
}

void print_help(){
	printf("rpa [-w gbarom] [-r dstfile] [-l size] [-L size(MB)]-c [-a address]\n");
	printf("\t-s \t\t\tBackup memory mode (Sram, Fram, Flash, ...Not yet supported EEPROM)\n\n");

	printf("\t-c \t\t\tROM info\n");
	printf("\t-r <filename> \t\tRead ROM(or backup memory)\n");
	printf("\t-w <filename> \t\tWrite ROM(or backup memory)\n");
	printf("\t-e \t\t\tblock erase\n");
	printf("\t-E \t\t\tchip erase\n");	  
	printf("\t-B \t\t\tblank check\n");
	printf("\t-d \t\t\tdump\n");

	printf("\t-a \t\t\trom address\n");
	printf("\t-b \t\t\tblock address\n");
	printf("\t-n \t\t\terase block num\n");
	printf("\t-h \t\t\tthis help\n\n");
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
	int32_t i,j, n, len, mode, code, word_count, result, addr, block, block_num, ret, filesize;
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
					case 'V': // ROM VERIFY
					case 'D': // Duplicate
						if(*(argv[i]+1) == 'w')
							mode = WRITE;
						else if(*(argv[i]+1) == 'V')
							mode = VERIFY;
						else if(*(argv[i]+1) == 'D')
							mode = DUPLICATE;

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
						filesize = stbuf.st_size;
						if(len==0)
							len = filesize;
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
					case 'B': // FLASH BLANK CHECK
						mode = BLANK;
						break;

					case 'T': // TEST 
						mode = TEST;
						break;

					case '@': // TEST2
						mode = TEST2;
						break;

					case 'h':
						print_help();
						exit(1);

					case 'v':
						print_version();
						exit(1);

					default:
						printf("option error:%s\n",argv[i]);
						break;
				}
			}
		}
	}

	print_version();

	if ( mode == NONE ) {
		print_help();

		mode = INFO;
	}

	if(len==0){
		if(!backup)
			len = 16 * 1024 * 1024;	// to do
		else 
			len = 32 * 1024;
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
		printf("Main ROM mode =>\n");
		MemoryRom *m = (MemoryRom *)Memory::create();
		if(m == nullptr){
			printf("cartridge can not be detected or not supported.\n");
			exit(1);
		}

		if(len%2==1)
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

		if(mode == INFO){
			printf("  Cartridge type:  %s\n", m->getTypeStr());
			if(m->getMemoryMbit() > 0)
				printf("  Size:            %d Mbit\n", m->getMemoryMbit());
			if(m->checkGbaHeader() > 0){
				m->getGbaHeader();
				uint8_t title[13];
				memset(title, 0, 13);
				strncpy((char *)title, (char *)m->header.game_title, 12);
				printf("  Game title:      %s\n",title);
			}
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
			// ERASE
			printf("CHIP ERASE START =>\n");
			ret = m->chipErase();
			printf("chip erase finish!(%d)\n",ret);

			// WRITE
			len = fread( buffer, sizeof( unsigned char ), len, fp );
			//printf("\nfread : %d\n",len);
			printf("WRITE START =>\n");
			m->seqProgram(0, (uint16_t *)buffer, len/2);
			printf("write finish!\n");

			// VERIFY
			printf("VERIFY START =>\n");
			uint8_t * buffer2, b[2];		
			uint32_t error_num = 0;
			try{
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
		printf("Backup memory mode =>\n");
		MemoryBackup *b = (MemoryBackup *)Memory::create(MEM_BACKUP);
		if(b == nullptr){
			printf("cartridge can not be detected or not supported.\n");
			exit(1);
		}
		printf("  Memory type:  %s\n", b->getTypeStr());
		printf("  Size:         %d KB\n", b->getMemoryKb());

		try{
			len = b->getMemoryKb() * 1024;
			buffer = new uint8_t[len];
			memset(buffer, 0, len);
		}
		catch (exception& e)
		{
			cout << e.what() << '\n';
			printf("error : new error \n");
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
			printf("READ START =>\n");
			uint8_t *buf = (uint8_t *)buffer;
			b->load(0, buf, len);
			write(fd, buf, len);
			close(fd);
			fclose(fp);
			printf("read finish\n"); 
		}
		else if(mode == WRITE){
			printf("WRITE START =>\n");
			uint8_t *buf = (uint8_t *)buffer;
			if(filesize > len)
				printf("WARNING: file size is over %d ( > %d )\n", filesize, len);

			EBackupType backup_type = b->getType();
			if( backup_type >= BACKUP_FLASH && backup_type <= BACKUP_FLASH_CUBIC){
				printf("Blank check...\n");
				b->load(0, buf, len);
				for(i=0; i<len; i++){
					if(buf[i] != 0xff){
						printf("WARNING: backup flash is not blank! Execute memory erase.\n");
						b->chipErase();
						break;
					}
				}
			}

			len = fread( buf, sizeof( unsigned char ), len, fp );
			b->save(0, buf, len);
			printf("write finish!\n");
		}
		else if(mode == CHIP_ERASE){
			printf("CHIP ERASE START =>\n");
			ret = b->chipErase();
			printf("chip erase finish!(%d)\n",ret);
		}
		else if(mode == BLANK){
			printf("BLANK CHECK=>\n");
			int32_t error=0;
			uint8_t *buf = (uint8_t *)buffer;
			b->load(0, buf, len);
			for(i=0; i<len; i++){
				if(buf[i] != 0xff){
					if(error++ < 32)
						printf("%04x: %02x != 0xff\n", i , buf[i]);
				}
			}
			printf("BLANK error:%d\n", error);
		}
		else if(mode == TEST)
		{
			printf("BACKUP MEMORY TEST START=>\n");
			int32_t error=0;
			uint8_t *buf = (uint8_t *)buffer;
			// ERASE TEST
			b->chipErase();
			b->load(0, buf, len);
			for(i=0; i<len; i++){
				if(buf[i] != 0xff){
					if(error++ < 32)
						printf("%04x: %02x != 0xff\n", i , buf[i]);
				}
			}
			printf("BLANK TEST error:%d\n", error);
			if(error > 0)
				goto end;

			// WRITE TEST 0xAA
			memset(buf, 0xaa, len);
			b->save(0, buf, len);

			for(i=0; i<len; i++){
				if(buf[i] != 0xaa){
					if(error++ < 32)
						printf("%04x: %02x != 0xff\n", i , buf[i]);
				}
			}
			printf("WRITE 0xAA TEST error:%d\n", error);
			if(error > 0)
				goto end;


			// WRITE TEST 0x55
			b->chipErase();
			memset(buf, 0x55, len);
			b->save(0, buf, len);

			for(i=0; i<len; i++){
				if(buf[i] != 0x55){
					if(error++ < 32)
						printf("%04x: %02x != 0xff\n", i , buf[i]);
				}
			}
			printf("WRITE 0x55 TEST error:%d\n", error);
			if(error > 0)
				goto end;

			b->chipErase();

			printf("BACKUP MEMOEY TEST is Sucess\n");
		}
		else if(mode == TEST2)
		{
			printf("BACKUP MEMORY TEST2 START=>\n");
			int32_t error=0;
			uint8_t *buf = (uint8_t *)buffer;
			// ERASE TEST
			b->chipErase();
			b->load(0, buf, len);
			for(i=0; i<len; i++){
				if(buf[i] != 0xff){
					if(error++ < 32)
						printf("%04x: %02x != 0xff\n", i , buf[i]);
				}
			}
			printf("BLANK TEST error:%d\n", error);
			if(error > 0)
				goto end;

			// WRITE TEST
			memset(buf, 0x31, len);
			b->save(0, buf, len/2);
			memset(buf, 0x32, len);
			b->save(len/2, buf, len/2);
		}
	}

end:
  if(buffer)
    delete[] buffer;
  
  return 0;
}


