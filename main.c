
int main(int argc, char* argv[])
{
	CpuState currentState;
	VgaState vga = { .x = 0, .y = 0, .left = 0, .fbp = 0, .cpu = 0, };
	DTime stats;

	vga.stats = &stats;

	CpuThread cpuThread = {
		.cpuState = &currentState,
		.vga = &vga
	};

	srand(time(NULL));
	garble((void*)ROM, sizeof(ROM));
	garble((void*)RAM, sizeof(RAM));
	garble((void*)&currentState, sizeof(currentState));

	FILE* fp = fopen(argv[1], "rb");

	if(!fp)
		fp = fopen("../clone/gigatron-rom/ROMv6.rom", "rb");

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

	stats.t = -2;

	stats_init(&stats);

	_stats_frame_start(&stats);

	if(pthread_create(&cpuThread.threadID, 0, &CpuCycleThread, &cpuThread))
		goto cleanup;

	while (!quitRequest)
	{
		if (stats.t < 0) currentState.PC = 0;

		CpuCycleStep(&currentState, &vga);
		VgaCycle(&vga, &currentState);
	}

	putchar('\n');

cleanup:
	SDL_DestroyRenderer(vga.rend);
	SDL_DestroyWindow(vga.win);
	SDL_Quit();
}
