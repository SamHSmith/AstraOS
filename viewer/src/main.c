#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <sys/time.h>

#include <unistd.h>
#include <signal.h>
#include <fcntl.h>

int main()
{
	if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
		fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
		return EXIT_FAILURE;
	}
    int size = 256/256;
    SDL_Window *win = SDL_CreateWindow("Hello World!", 0, 0, 1,1,
                                SDL_WINDOW_SHOWN);
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
    assert(SDL_SetRelativeMouseMode(SDL_TRUE) != -1);

    pid_t pid;

    int child_out = open("../pipe.out", O_RDONLY);
    int child_in = open("../pipe.in", O_WRONLY);

    assert(child_out != -1 && child_in != -1);

    int frame_counter = 0;
    double total_secs = 0.0;
    double longest_frame = 0.0;

    int running = 1;
    while(running)
    {
        SDL_Event sevent;
        SDL_KeyboardEvent* kevent = (SDL_KeyboardEvent*)&sevent;
        while(SDL_PollEvent(&sevent))
        {
            if(sevent.type == SDL_QUIT)
            { running = 0; }
            else if(sevent.type == SDL_KEYDOWN && !kevent->repeat)
            {
                unsigned char scode = kevent->keysym.scancode;
                write(child_in, "d", 1);
                write(child_in, &scode, 1);
            }
            else if(sevent.type == SDL_KEYUP && !kevent->repeat)
            {
                unsigned char scode = kevent->keysym.scancode;
                write(child_in, "u", 1);
                write(child_in, &scode, 1);
            }
        }

        SDL_SetRenderDrawColor(ren, 0,0,0,255);
        SDL_RenderClear(ren);

        unsigned char c = 0;
        c = 'a';
        write(child_in, &c, 1);

        int w, h;
        SDL_GetWindowSize(win, &w, &h);
        unsigned int width = w, height = h;

        write(child_in, &width, 4);
        write(child_in, &height, 4);

        int mouse_data[3];
        mouse_data[2] = SDL_GetRelativeMouseState(&mouse_data[0], &mouse_data[1]);

        write(child_in, mouse_data, 3*4);

        int n_count = 0;
        char* frame_sync = "new;;_;;frame";
        char buf[13];
        while(n_count < 13)
        {
            read(child_out, &c, 1);
            if(c != frame_sync[n_count])
            {
                if(n_count > 0) { write(STDOUT_FILENO, buf, n_count); }
                n_count = 0;
                write(STDOUT_FILENO, &c, 1);
                continue;
            }
 
            buf[n_count] = c;
            n_count++;
        }
 
        struct timeval start, stop;
        gettimeofday(&start, NULL);

        int s = 8;
        unsigned char r = 0;
        for(int y = 0; y < height; y++)
        {
            for(int x = 0; x < width; x++)
            {
                if(s >= 8)
                {
                    read(child_out, &r, 1);
                    s = 0;
                }
                
                SDL_SetRenderDrawColor(ren, 255*((r & (1 << s))!=0),
                                            255*((r & (1 << s))!=0),
                                            255*((r & (1 << s))!=0),
                                            255);
                SDL_RenderDrawPoint(ren, x, y);
                s++;
            }
        }

        gettimeofday(&stop, NULL);
        frame_counter += 1;
        double secs = (double)(stop.tv_usec - start.tv_usec) / 1000000 + (double)(stop.tv_sec - start.tv_sec);
        total_secs += secs;
        if(secs > longest_frame) { longest_frame = secs; }

        if(frame_counter >= 60 * 20)
        {
            printf("Viewer: Avg frame time(ms) = %lf\n", 1000.0 * total_secs / (double)frame_counter);
            printf("Viewer:  Longest frame(ms) = %lf\n", 1000.0 * longest_frame);
            frame_counter = 0;
            total_secs = 0.0;
            longest_frame = 0.0;
        }

        SDL_RenderPresent(ren);
	}

	SDL_DestroyRenderer(ren);
	SDL_DestroyWindow(win);
	SDL_Quit();

	return EXIT_SUCCESS;
}
