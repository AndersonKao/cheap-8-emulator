#include "stdio.h"
#include "stdlib.h"
#include "stdint.h"
#define PROGRAM_ST 0x200
#define PROGRAM_ST_ETI660 0x660
#define VF 15
uint8_t ram[4096]; // 4096 bytes RAM
uint16_t stack[16]; // stack for subroutine, Chip-8 allows for up to 16 levels of subroutines.
uint8_t regs[16]; // 16 Vx registers, V0-VF.
uint16_t idx_reg;
uint8_t delay_timer;
uint8_t sound_timer;
uint16_t pc; // program counter
uint8_t sp; // stack pointer
uint8_t screen[64][32] = {0};


/*
 * Memory Map:
+---------------+= 0xFFF (4095) End of Chip-8 RAM
|               |
|               |
|               |
|               |
|               |
| 0x200 to 0xFFF|
|     Chip-8    |
| Program / Data|
|     Space     |
|               |
|               |
|               |
+- - - - - - - -+= 0x600 (1536) Start of ETI 660 Chip-8 programs
|               |
|               |
|               |
+---------------+= 0x200 (512) Start of most Chip-8 programs
| 0x000 to 0x1FF|
| Reserved for  |
|  interpreter  |
+---------------+= 0x000 (0) Start of Chip-8 RAM

*/

void load_rom(char const *filename){
	FILE *fptr = fopen(filename, "r");
	if(fptr != NULL){
		unsigned int offset = 0;
#ifdef DEBUG
			printf("read instruction: \n");
#endif
		while(feof(fptr) == 0){
			fread(ram + PROGRAM_ST + offset, 1, 2, fptr);
#ifdef DEBUG
			printf("%02X%02X", *(ram + PROGRAM_ST + offset), *(ram + PROGRAM_ST + offset + 1));
			printf("%c", ((offset + 1) % 8 ? ' ' : '\n'));
#endif
			offset += 2;
		}
		fclose(fptr);
	}
}

inline uint8_t getnibble1(uint16_t instruction){
	return ((instruction & 0xF000) >> 12);
}

inline uint8_t getnibble2(uint16_t instruction){
	return (instruction & 0x0F00) >> 8;
}

inline uint8_t getnibble3(uint16_t instruction){
	return (instruction & 0x00F0) >> 4;
}

inline uint8_t getnibble4(uint16_t instruction){
	return (instruction & 0x000F);
}

inline uint16_t getimme_addr(uint16_t instruction){
	return (instruction & 0x0FFF);
}

inline uint8_t getimme_num(uint16_t instruction){
	return (instruction & 0x00FF);
}

void emulate(){

	while(true){
		uint16_t instruction;
		instruction = ram[PROGRAM_ST + pc];
		pc+=2;
		if(instruction == 0x00E0){
			// clear screen
			memset(screen, 0, sizeof(uint8_t) * 64 * 32);
		}
		else if(instruction == 0x00EE){
			pc = stack[sp--];
		}
		else{
			switch(getnibble1(instruction)){
				case 0x1:
					uint16_t addr = getimme_addr(instruction);
					pc = addr;
					break;
				case 0x2:
					uint16_t addr = getimme_addr(instruction);
					stack[sp++] = pc;
					pc = addr;
				case 0x3:
					uint8_t reg_id = getnibble2(instruction);
					uint8_t val = getimme_num(instruction);	
					if(regs[reg_id] == val){
						pc += 2;
					}
					break;
				case 0x4:
					uint8_t reg_id = getnibble2(instruction);
					uint8_t val = getimme_num(instruction);	
					if(regs[reg_id] != val){
						pc += 2;
					}
					break;
				case 0x5:
					uint8_t reg_idx = getnibble2(instruction);
					uint8_t reg_idy = getnibble3(instruction);
					if(regs[reg_idx] == regs[reg_idy]){
						pc += 2;
					}
					break;
				case 0x6:
					uint8_t reg_id = getnibble2(instruction);
					uint8_t val = getimme_num(instruction);	
					regs[reg_id] = val;
					break;
				case 0x7:
					uint8_t reg_id = getnibble2(instruction);
					uint8_t val = getimme_num(instruction);	
					regs[reg_id] += val;
					break;
				case 0x8:
					uint8_t reg_idx = getnibble2(instruction);
					uint8_t reg_idy = getnibble3(instruction);
					switch(getnibble4(instruction)){
					case 0x0:
						regs[reg_idx] = regs[reg_idy];
						break;
					case 0x1:
						regs[reg_idx] |= regs[reg_idy];
						break;
					case 0x2:
						regs[reg_idx] &= regs[reg_idy];
						break;
					case 0x3:
						regs[reg_idx] ^= regs[reg_idy];
						break;
					case 0x4:
						if(regs[reg_idx] + regs[reg_idy] < regs[reg_idx]){
							regs[VF] = 1;
						}
						else{
							regs[VF] = 0;
						}
						regs[reg_idx] += regs[reg_idy];
						break;
					case 0x5:
						if(regs[reg_idx] >= regs[reg_idy]){
							regs[VF] = 1;
						}
						else{
							regs[VF] = 0;
						}
						regs[reg_idx] = regs[reg_idx] - regs[reg_idy];
						break;
					case 0x7:
						if(regs[reg_idy] >= regs[reg_idx]){
							regs[VF] = 1;
						}
						else{
							regs[VF] = 0;
						}
						regs[reg_idx] = regs[reg_idy] - regs[reg_idx];
						break;

					}
					break;
				case 0x9:
					uint8_t reg_idx = getnibble2(instruction);
					uint8_t reg_idy = getnibble3(instruction);
					if(regs[reg_idx] != regs[reg_idy]){
						pc += 2;
					}
					break;
				case 0xA:
					uint16_t addr = getimme_addr(instruction);
					idx_reg = addr;
					break;
				case 0xD:
					uint8_t reg_idx = getnibble2(instruction);
					uint8_t reg_idy = getnibble3(instruction);
					uint8_t coord_x = regs[reg_idx] & (0xBF); // mod 64
					uint8_t coord_y = regs[reg_idy] & (0x1F); // mod 32
					regs[VF] = 0;
					uint8_t row_count = getnibble4(instruction);
					for(int r = 0; r < row_count; r++){
						uint8_t render_byte = ram[idx_reg + r];
						for(int offset = 0; offset < 8; offset++){
							if(render_byte & (1 << offset)){
								uint8_t x = coord_x + offset;
								uint8_t y = coord_y + r;
								if(x < 64 && y < 32){
									if(screen[x][y] == 1){
										regs[VF] = 1;
									}
									screen[x][y] ^= 1;	
								}
							}
						}
					}
					break;
			}
			break;
		}

		render();
		//decode(instruction, k);

	}
	
}

int main(int argc, char *argv[]){
	if(argc < 2){
		printf("please provide rom\n");
		return -1;
	}

	load_rom(argv[1]);

	emulate();


	return 0;
}
