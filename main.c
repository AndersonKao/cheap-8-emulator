#include <SDL3/SDL_asyncio.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_oldnames.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_video.h>
#include <stdint.h>
#include <time.h>
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>

#include "stdio.h"
#include "stdlib.h"
#include "stdint.h"
#define PROGRAM_ST 0x200
#define PROGRAM_ST_ETI660 0x660
#define DIGITS_ADDR 0x000
#define V0 0 
#define VF 15
// 4096 bytes RAM
uint8_t ram[4096] = {
	0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
	0x20, 0x60, 0x20, 0x20, 0x70, // 1
	0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
	0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
	0x90, 0x90, 0xF0, 0x10, 0x10, // 4
	0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
	0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
	0xF0, 0x10, 0x20, 0x40, 0x40, // 7
	0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
	0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9 
	0xF0, 0x90, 0xF0, 0x90, 0x90, // A
	0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
	0xF0, 0x80, 0x80, 0x80, 0xF0, // C
	0xE0, 0x90, 0x90, 0x90, 0xE0, // D
	0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
	0xF0, 0x80, 0xF0, 0x80, 0x80 // F
}; 
uint16_t stack[16]; // stack for subroutine, Chip-8 allows for up to 16 levels of subroutines.
uint8_t regs[16]; // 16 Vx registers, V0-VF.
uint8_t keypad[16]; // 16 keys;
uint16_t idx_reg;
uint8_t delay_timer;
uint8_t sound_timer;
uint16_t pc = PROGRAM_ST; // program counter
uint8_t sp; // stack pointer
uint8_t screen[32][64] = {0};

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
const int SCALE_X = 16;
const int SCALE_Y = 16;

void init_graphics(){
	if( !SDL_Init( SDL_INIT_VIDEO) )
	{
		printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
		abort();
	}

	if (!SDL_CreateWindowAndRenderer("Chip-8 emulator by Anderson Kao", 64 * SCALE_X, 32 * SCALE_Y, SDL_WINDOW_RESIZABLE, &window, &renderer)){
			abort();
	}
	// SDL_SetRenderLogicalPresentation(renderer, 640, 480, SDL_LOGICAL_PRESENTATION_LETTERBOX);

}


void render(){
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
	SDL_RenderClear(renderer);

	SDL_FRect rect;
	rect.w = SCALE_X;
	rect.h = SCALE_Y;
	SDL_SetRenderDrawColor(renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);

	for(uint8_t r = 0; r < 32; r++){
		for(uint8_t c = 0; c < 64; c++){
			if(screen[r][c]){
				rect.x = c * SCALE_X;
				rect.y = r * SCALE_Y;
				SDL_RenderFillRect(renderer, &rect);
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
	uint8_t pred;
	uint8_t key;
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
				case 0x6:
#ifndef SUPER_OR_48
					regs[reg_idx] = regs[reg_idy];
#endif
					regs[VF] = regs[reg_idx] & 1;
					regs[reg_idx] >>= 1;
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
				case 0xE:
#ifndef SUPER_OR_48
					regs[reg_idx] = regs[reg_idy];
#endif
					regs[VF] = regs[reg_idx] & (0x80);
					regs[reg_idx] <<= 1;
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
		case 0xB:
			addr = getimme_addr(instruction);
#ifdef SUPER_OR_48
			pc = addr + regs[V0];
#else
			reg_idx = getnibble2(instruction);	
			pc = addr + regs[reg_idx];
#endif
			break;
		case 0xA:
			addr = getimme_addr(instruction);
			idx_reg = addr;
			break;
		case 0xC:
			reg_idx = getnibble2(instruction);
			val = getimme_num(instruction);
			regs[reg_idx] = (rand() % UINT8_MAX) & val;
			break;
		case 0xD:
			reg_idx = getnibble2(instruction);
			reg_idy = getnibble3(instruction);
			uint8_t coord_x = regs[reg_idx] & (0xBF); // mod 64
			uint8_t coord_y = regs[reg_idy] & (0x1F); // mod 32
			regs[VF] = 0;
			uint8_t row_count = getnibble4(instruction);
			for(uint8_t r = 0; r < row_count; r++){
				uint8_t render_byte = ram[idx_reg + r];
				for(uint8_t offset = 0; offset < 8; offset++){
					if(render_byte & (1 << (7 - offset))){
						uint8_t x = coord_x + offset;
						uint8_t y = coord_y + r;
						if(x < 64 && y < 32){
							if(screen[y][x] == 1){
								regs[VF] = 1;
							}
							screen[y][x] ^= 1;	
						}
					}
				}
			}
			break;
		case 0xE:
			pred = getimme_num(instruction);
			reg_idx = getnibble2(instruction);
			key = regs[reg_idx];
			if(key > 0xF){
				printf("keypad range in 0 - F. Error.\n");
				abort();
			}
			if(pred == 0x9E){
				if(keypad[key])
					pc+=2;
			}
			else if(pred == 0xA1){
				if(!keypad[key])
					pc+=2;
			}
			else{
				printf("unsupported instruction\n");
				abort();
			}
			break;
		case 0xF:
			pred = getimme_num(instruction);
			reg_idx = getnibble2(instruction);
			if(pred == 0x07){
				regs[reg_idx] = delay_timer;
			}
			else if(pred == 0x15){
				delay_timer = regs[reg_idx];
			}
			else if(pred == 0x18){
				sound_timer = regs[reg_idx];
			}
			else if(pred == 0x1E){
#ifdef INDEX_OVERFLOW
				if(idx_reg + regs[reg_idx] < idx_reg)				
					regs[VF] = 1;
#endif
				idx_reg += regs[reg_idx];
			}
			else if(pred == 0x0A){
				bool pressed = false;
				for(uint8_t i = 0; i < 16; i++){
					if(keypad[i] == 1){
						regs[reg_idx] = i;
						pressed = true;
						break;
					}
				}
				if(pressed == false)
					pc -= 2;
			}
			else if(pred == 0x29){
				idx_reg = DIGITS_ADDR + ((uint16_t)regs[reg_idx] &0x0F ) * 5;
			}
			else if(pred == 0x33){
				val = regs[reg_idx];
				ram[idx_reg] = val / 100;
				ram[idx_reg +1] = (val / 10) % 10;
				ram[idx_reg +2] = (val % 10);
			}
			else if(pred == 0x55){
				for(uint8_t i = 0; i <= reg_idx; i++){
					ram[idx_reg + i] = regs[i];
				}
#ifdef COSMAC_VIP
				idx_reg += reg_idx;
#endif
			}
			else if(pred == 0x65){
				for(uint8_t i = 0; i <= reg_idx; i++){
					regs[i] = ram[idx_reg + i];
				}
#ifdef COSMAC_VIP
				idx_reg += reg_idx;
#endif
			}
			else{
				printf("unsupported instruction\n");
				abort();
			}
			break;

		default:
			printf("unsupported instruction\n");
			abort();
			break;
	}

}

uint16_t load_instruction(){
	uint16_t left = ram[pc];
	uint16_t right = ram[pc + 1];
	uint16_t res = (((uint16_t)(left)) << 8) | right;
	return res;
}

void SLEEP(int msec){
	struct timespec ts;
	ts.tv_sec = msec/1000;
	ts.tv_nsec = msec%1000*1000*1000;
	nanosleep(&ts, NULL);
}

const bool *key_states;
void update_keyboard(){
	/*
	 * 1 2 3 C
	 * 4 5 6 D
	 * 7 8 9 E
	 * A 0 B F
	 * */
	keypad[0x1] = key_states[SDL_SCANCODE_1];
	keypad[0x2] = key_states[SDL_SCANCODE_2];
	keypad[0x3] = key_states[SDL_SCANCODE_3];
	keypad[0xC] = key_states[SDL_SCANCODE_4];
	keypad[0x4] = key_states[SDL_SCANCODE_Q];
	keypad[0x5] = key_states[SDL_SCANCODE_W];
	keypad[0x6] = key_states[SDL_SCANCODE_E];
	keypad[0xD] = key_states[SDL_SCANCODE_R];
	keypad[0x7] = key_states[SDL_SCANCODE_A];
	keypad[0x8] = key_states[SDL_SCANCODE_S];
	keypad[0x9] = key_states[SDL_SCANCODE_D];
	keypad[0xE] = key_states[SDL_SCANCODE_F];
	keypad[0xA] = key_states[SDL_SCANCODE_Z];
	keypad[0x0] = key_states[SDL_SCANCODE_X];
	keypad[0xB] = key_states[SDL_SCANCODE_C];
	keypad[0xF] = key_states[SDL_SCANCODE_V];
}

void decrease_timer(){
	static clock_t pre_time = 0;

	clock_t now_time = clock();
	if(now_time - pre_time >= 1000){
		if(delay_timer > 0)
			delay_timer--;
		if(sound_timer > 0)
			sound_timer--;
	}
	pre_time = now_time;
}

SDL_Event event;

void emulate(){
	key_states = SDL_GetKeyboardState(NULL);
	uint16_t instruction;
	bool running = true;
	while(running){
		if (SDL_PollEvent(&event)){
			if(event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
				running = false;
		}
		update_keyboard();
		decrease_timer();
		instruction = load_instruction();
#ifdef DEBUG
		log_registers();
		printf("instruction %04X\n", instruction);
#endif
		pc+=2;
		if(instruction == 0x00E0){
			// clear screen
			memset(screen, 0, sizeof(uint8_t) * 32 * 64);
		}
		else if(instruction == 0x0000){
//			pc = 0;
		}
		else if(instruction == 0x00EE){
			pc = stack[--sp];
		}
		else{
			decode(instruction);
		}
		render();
#ifdef DEBUG
		// getc(stdin);
#endif
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

	srand(time(NULL));
	load_rom(argv[1]);
	init_graphics();

	emulate();


	destroy();
	return 0;
}
