#include "input.h"
#include "sdldraw.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_timer.h>
#include <SDL2/SDL_image.h>
//#include <SDL2/SDL_ttf.h>

#define WINDOW_WIDTH (960)
#define WINDOW_HEIGHT (540)

typedef struct
{
    int16_t x, y, left;
    uint8_t lineBuffer[WINDOW_HEIGHT][WINDOW_WIDTH];
    uint8_t oldOutput;
//
    SDL_Renderer* rend;
    SDL_Window* win;
} VgaState;

typedef struct 
{
    uint16_t PC;
    uint8_t IR, D, AC, X, Y, OUTPUT, undef;
} CpuState;

uint8_t ROM[1<<16][2];  //1 megabit = 128 kilobyte rom data
uint8_t RAM[1<<15];     //32 kilobyte of ram data
uint8_t Input = 0xff;   //input from port, normally high

int quitRequest = 0;
    
CpuState CpuCycle(const CpuState newState)
{
    CpuState oldState = newState;

    oldState.IR = ROM[newState.PC][0];
    oldState.D  = ROM[newState.PC][1];

    int ins = newState.IR >> 5;
    int mod = (newState.IR >> 2) & 7;
    int bus = newState.IR & 3;
    int W = (ins == 6);
    int J = (ins == 7);

    uint8_t lo = newState.D;
    uint8_t hi = 0;
    uint8_t* to = NULL;
    int incX = 0;

    if(!J)
        switch (mod)
        {
            #define E(p) (W?0:p)
            case 0: to = E(&oldState.AC); break;
            case 1: to = E(&oldState.AC); lo = newState.X; break;
            case 2: to = E(&oldState.AC); hi = newState.Y; break;
            case 3: to = E(&oldState.AC); lo = newState.X; hi = newState.Y; break;
            case 4: to = &oldState.X; break;
            case 5: to = &oldState.Y; break;
            case 6: to = E(&oldState.OUTPUT); break;
            case 7: to = E(&oldState.OUTPUT); lo = newState.X; hi = newState.Y; incX = 1; break;
        }
    uint16_t addres = (hi << 8) | lo;

    int B = newState.undef; //databus
    switch (bus)
    {
        case 0: B = newState.D; break;
        case 1: if (!W) B = RAM[addres & 0x7fff]; break;
        case 2: B = newState.AC; break;
        case 3: B = Input; break;
    }

    if (W)
    {
        RAM[addres & 0x7fff] = B;
    }

    uint8_t ALU; //arithic logic unit

    switch (ins)
    {
        case 0: ALU = B; break;
        case 1: ALU = newState.AC & B; break;
        case 2: ALU = newState.AC | B; break;
        case 3: ALU = newState.AC ^ B; break;
        case 4: ALU = newState.AC + B; break;
        case 5: ALU = newState.AC - B; break;
        case 6: ALU = newState.AC; break;
        case 7: ALU = -newState.AC; break;
    }

    if (to) *to = ALU;
    if (incX) oldState.X = newState.X + 1;

    oldState.PC = newState.PC + 1;

    if (J)
    {
        if (mod != 0)
        {
            int cond = (newState.AC >> 7) + 2*(newState.AC == 0);
            if (mod & (1 << cond))
                oldState.PC = (newState.PC & 0xff00) | B;
        } else 
            oldState.PC = (newState.Y << 8) | B;
    }
    return oldState;
}

void garble(uint8_t mem[], unsigned int length)
{
    for (unsigned int i = 0; i < length; i++)
    {
        mem[i] = rand();
    }
}

void VgaCycle(VgaState *const vga, CpuState *const currentState)
{
    const uint8_t output = currentState->OUTPUT;
    const uint8_t oldOutput = vga->oldOutput;
    vga->oldOutput = output;
    
    uint8_t *const linePixel = &vga->lineBuffer[vga->y][vga->x++];
    const uint8_t pixel = output & 63;
    
    int hSync = (output & 0x40) - (oldOutput & 0x40);
    int vSync = (output & 0x80) - (oldOutput & 0x80);

    if (hSync > 0)
    {
        vga->left = vga->x >> 1;

        vga->x = 0;
        vga->y++;

        currentState->undef = rand() & 0xff;
    }

    if (vSync < 0 )
    {
        vga->y = 0;

        quitRequest = GetQuitRequest();
        Input = GetInput(Input);
        DrawGigatronExtendedIO(vga->rend, Input);

        SDL_RenderPresent(vga->rend);
    }

    if ((0 < vga->y) && (WINDOW_HEIGHT > vga->y)
        && ((0 < vga->x) && (WINDOW_WIDTH > vga->x)))
    {
        if(pixel != linePixel[0])
        {
            *linePixel = pixel;

            const int16_t r = (pixel & 3) * 0x55;
            const int16_t g = ((pixel >> 2) & 3) * 0x55;
            const int16_t b = ((pixel >> 4) & 3) * 0x55;
            
            SDL_SetRenderDrawColor(vga->rend, r, g, b, 255);

            const int16_t x = vga->left + (vga->x << 2);
            const int16_t y = vga->y;

            SDL_RenderDrawLine(vga->rend, x, y, x + 4, y);
        }
    }
}

int main(int argc, char* argv[])
{
    CpuState currentState;
    VgaState vga = { .x = 0, .y = 0, .left = 0 };
    
    srand(time(NULL));
    garble((void*)ROM, sizeof(ROM));
    garble((void*)RAM, sizeof(RAM));
    garble((void*)&currentState, sizeof(currentState));
    
    FILE* fp = fopen(argv[1], "rb");

    if (!fp)
    {
        fprintf(stderr, "Failed to open ROM-file\n");
        exit(EXIT_FAILURE);
    }

    fread(ROM, 1, sizeof(ROM), fp);
    fclose(fp);
    
    // attempt to initialize graphics and timer system
    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER) != 0)
    {
        printf("error initializing SDL: %s\n", SDL_GetError());
        return 1;
    }

    vga.win = SDL_CreateWindow("Jalecko's Gigatron Emulator", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT,0);

    if (!vga.win)
    {
        printf("error creating window: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // create a renderer, which sets up the graphics hardware
    Uint32 render_flags = SDL_RENDERER_ACCELERATED;
    vga.rend = SDL_CreateRenderer(vga.win, -1, render_flags);
    if (!vga.rend)
    {
        printf("error creating renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(vga.win);
        SDL_Quit();
        return 1;
    }

    SDL_RenderPresent(vga.rend);

    long long t = -2;
    
    while (!quitRequest)
    {
        if (t < 0) currentState.PC = 0;

        CpuState newState = CpuCycle(currentState);

        currentState = newState;
        t++;

        VgaCycle(&vga, &newState);
    }

    SDL_DestroyRenderer(vga.rend);
    SDL_DestroyWindow(vga.win);
    SDL_Quit();
}
