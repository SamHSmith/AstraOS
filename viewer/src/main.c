#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <unistd.h>

int main()
{
	if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
		fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
		return EXIT_FAILURE;
	}

	SDL_Window *win = SDL_CreateWindow("Hello World!", 0, 0, 267, 150, SDL_WINDOW_SHOWN);
	if (win == NULL) {
		fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
		return EXIT_FAILURE;
	}

	SDL_Renderer *ren = SDL_CreateRenderer(win, -1,SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (ren == NULL) {
		fprintf(stderr, "SDL_CreateRenderer Error: %s\n", SDL_GetError());
		if (win != NULL) {
			SDL_DestroyWindow(win);
		}
		SDL_Quit();
		return EXIT_FAILURE;
	}

    int pipefds[2];
    pipe(pipefds);
    pid_t pid;

    pid = fork();
    if(pid)
    {
        close(pipefds[1]);
    }
    else // child
    {
        close(pipefds[0]);
        dup2(pipefds[1], STDOUT_FILENO);
        chdir("..");
        execlp("make", "make", "run");
//        execlp("find", "find", "/");
        exit(0);
    }

    int running = 1;
    while(running)
    {
        SDL_Event sevent;
        while(SDL_PollEvent(&sevent))
        {
            if(sevent.type == SDL_QUIT)
            { running = 0; }
        }

        SDL_SetRenderDrawColor(ren, 0,0,0,255);
        SDL_RenderClear(ren);

        int width, height;
        SDL_GetWindowSize(win, &width, &height);

        unsigned char c = 0;
        int s = 0;
        for(int y = 0; y < height; y++)
        {
            for(int x = 0; x < width; x++)
            {
                if(s)
                { c = c >> 4; s = 0; }
                else
                {
                    read(pipefds[0], &c, 1);
                    s = 1;
                }
                
                SDL_SetRenderDrawColor(ren, 255*((c & 8)!=0), 255*((c & 4)!=0),
                                            255*((c & 2)!=0), 255*((c & 1)!=0));
                SDL_RenderDrawPoint(ren, x, y);
            }
        }

        SDL_RenderPresent(ren);
	}

	SDL_DestroyRenderer(ren);
	SDL_DestroyWindow(win);
	SDL_Quit();

	return EXIT_SUCCESS;
}
