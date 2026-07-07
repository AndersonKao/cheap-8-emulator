#include <SDL3/SDL_events.h>
#include <SDL3/SDL_oldnames.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_video.h>
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>

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
uint16_t pc = 0; // program counter
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

// SDL primitives
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
const int SCALE_X = 4;
const int SCALE_Y = 4;



void init_graphics(){
	if( !SDL_Init( SDL_INIT_VIDEO) )
	{
		printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
		abort();
	}

	if (!SDL_CreateWindowAndRenderer("Chip-8 emulator by Anderson Kao", 64 * SCALE_X, 32 * SCALE_Y, SDL_WINDOW_RESIZABLE, &window, &renderer)){
			abort();
	}
	SDL_SetRenderLogicalPresentation(renderer, 640, 480, SDL_LOGICAL_PRESENTATION_LETTERBOX);

}


void render(){
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
	SDL_RenderClear(renderer);

	SDL_FRect rect;
	rect.w = SCALE_X;
	rect.h = SCALE_Y;
	SDL_SetRenderDrawColor(renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);

	for(uint8_t r = 0; r < 64; r++){
		for(uint8_t c = 0; c < 32; c++){
			if(screen[r][c]){
				rect.x = c * SCALE_X;
				rect.y = r * SCALE_Y;
				SDL_RenderRect(renderer, &rect);
			}
		}
	}
	SDL_RenderPresent(renderer);
}

void load_rom(char const *filename){
	FILE *fptr = fopen(filename, "rb");
	if(fptr != NULL){
		unsigned int offset = 0;
#ifdef DEBUG
		printf("read instruction: \n");
#endif
		while(feof(fptr) == 0){
			size_t numread = fread(ram + PROGRAM_ST + offset, sizeof(uint8_t), 2, fptr);
#ifdef DEBUG
			printf("%u bytes: ", numread);
			printf("%02X%02X\n", *(ram + PROGRAM_ST + offset), *(ram + PROGRAM_ST + offset + 1));
			// printf("%c", ((offset + 1) % 8 ? ' ' : '\n'));
#endif
			offset += 2;
		}
		fclose(fptr);
	}
}

uint8_t getnibble1(uint16_t instruction){
	return ((instruction & 0xF000) >> 12);
}

uint8_t getnibble2(uint16_t instruction){
	return (instruction & 0x0F00) >> 8;
}

uint8_t getnibble3(uint16_t instruction){
	return (instruction & 0x00F0) >> 4;
}

uint8_t getnibble4(uint16_t instruction){
	return (instruction & 0x000F);
}

uint16_t getimme_addr(uint16_t instruction){
	return (instruction & 0x0FFF);
}

uint8_t getimme_num(uint16_t instruction){
	return (instruction & 0x00FF);
}

void log_registers(){
	printf("pc: %x, sp: %x\n", pc, sp);
	for(int i = 0; i < 16; i++)
		printf("V%x: %x\n", i, regs[i]);
	printf("index register: %x\n", idx_reg);
	printf("sound timer: %x\n", sound_timer);
	printf("delay timer: %x\n", delay_timer);

uint16_t stack[16]; // stack for subroutine, Chip-8 allows for up to 16 levels of subroutines.
uint8_t screen[64][32] = {0};

}

void log_ram(){
	for(int i = 0; i < 1024; i++)
		printf("RAM[%d]: %x\n", i, ram[i]);
}

void log_stack(){
	for(int i = 0; i < 16; i++)
		printf("ST[%d]: %x\n", i, stack[i]);
}

void decode(uint16_t instruction){
	uint16_t addr;
	uint8_t reg_id;
	uint8_t val;
	uint8_t reg_idx, reg_idy;
	switch(getnibble1(instruction)){
		case 0x1:
			addr = getimme_addr(instruction);
			pc = addr;
			break;
		case 0x2:
			addr = getimme_addr(instruction);
			stack[sp++] = pc;
			pc = addr;
		case 0x3:
			reg_id = getnibble2(instruction);
			val = getimme_num(instruction);	
			if(regs[reg_id] == val){
				pc += 2;
			}
			break;
		case 0x4:
			reg_id = getnibble2(instruction);
			val = getimme_num(instruction);	
			if(regs[reg_id] != val){
				pc += 2;
			}
			break;
		case 0x5:
			reg_idx = getnibble2(instruction);
			reg_idy = getnibble3(instruction);
			if(regs[reg_idx] == regs[reg_idy]){
				pc += 2;
			}
			break;
		case 0x6:
			reg_id = getnibble2(instruction);
			val = getimme_num(instruction);	
			regs[reg_id] = val;
			break;
		case 0x7:
			reg_id = getnibble2(instruction);
			val = getimme_num(instruction);	
			regs[reg_id] += val;
			break;
		case 0x8:
			reg_idx = getnibble2(instruction);
			reg_idy = getnibble3(instruction);
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
				default:
					printf("unsupported instruction\n");
					abort();
					break;
			}
			break;
		case 0x9:
			reg_idx = getnibble2(instruction);
			reg_idy = getnibble3(instruction);
			if(regs[reg_idx] != regs[reg_idy]){
				pc += 2;
			}
			break;
		case 0xA:
			addr = getimme_addr(instruction);
			idx_reg = addr;
			break;
		case 0xD:
			reg_idx = getnibble2(instruction);
			reg_idy = getnibble3(instruction);
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
		default:
			printf("unsupported instruction\n");
			abort();
			break;
	}

}

uint16_t load_instruction(){
	uint16_t left = ram[PROGRAM_ST + pc];
	uint16_t right = ram[PROGRAM_ST + pc + 1];
		printf("instruction %02X\n", left);
		printf("instruction %02X\n", right);
	uint16_t res = (((uint16_t)(left)) << 8) | right;
	return res;
}

SDL_Event event;

void emulate(){
	uint16_t instruction;
	bool running = true;
	while(running){
		while (SDL_PollEvent(&event)){
			if(event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
				running = false;
		}
		log_registers();
		instruction = load_instruction();
		printf("instruction %04X\n", instruction);
		pc+=2;
		if(instruction == 0x00E0){
			// clear screen
			memset(screen, 0, sizeof(uint8_t) * 64 * 32);
		}
		else if(instruction == 0x0000){
			pc = 0;
		}
		else if(instruction == 0x00EE){
			pc = stack[sp--];
		}
		else{
			decode(instruction);
		}
		render();
	}
}

void destroy(){
	SDL_DestroyWindow(window);
	SDL_DestroyRenderer(renderer);
}

int main(int argc, char *argv[]){
	if(argc < 2){
		printf("please provide rom\n");
		return -1;
	}

	load_rom(argv[1]);
	init_graphics();

	emulate();


	destroy();
	return 0;
}
