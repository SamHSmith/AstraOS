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
    int size = 13;
    SDL_Window *win = SDL_CreateWindow("Hello World!", 0, 0, 16*size, 9*size, SDL_WINDOW_SHOWN);
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

    pid_t pid;

    int child_out = open("../pipe.out", O_RDONLY);
    int child_in = open("../pipe.in", O_WRONLY);

/*    pid = fork();
    if(pid)
    {
    }
    else // child
    {
        chdir("..");
        execl("/bin/make", "make", "run");
        exit(0);
    } */

    int frame_counter = 0;
    double total_secs = 0.0;
    double longest_frame = 0.0;

    int newlines = 0;
    unsigned char _c = 0;
    while(1)
    {
        while(read(child_out, &_c, 1) == -1) { SDL_Delay(50); }
        if(_c == '\n')
        { newlines++; }
        else
        { newlines = 0; }
        if(newlines >= 5)
        { break; }
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

        unsigned char c = 0;
        c = 'a';
        write(child_in, &c, 1);

        int w, h;
        SDL_GetWindowSize(win, &w, &h);
        unsigned int width = w, height = h;

        c = width & 0xFF;
        write(child_in, &c, 1);
        c = (width >> 8) & 0xFF;
        write(child_in, &c, 1);

        c = height & 0xFF;
        write(child_in, &c, 1);
        c = (height >> 8) & 0xFF;
        write(child_in, &c, 1);

        int mouse_data[3];
        mouse_data[2] = SDL_GetMouseState(&mouse_data[0], &mouse_data[1]);

//        printf("x %d, y %d, state %x\n", mouse_data[0], mouse_data[1], mouse_data[2]);

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
        unsigned char r = 0,g = 0,b = 0;
        for(int y = 0; y < height; y++)
        {
            for(int x = 0; x < width; x++)
            {
                if(s >= 8)
                {
                    read(child_out, &r, 1);
                    read(child_out, &g, 1);
                    read(child_out, &b, 1);
                    s = 0;
                }
                
                SDL_SetRenderDrawColor(ren, 255*((r & (1 << s))!=0),
                                            255*((g & (1 << s))!=0),
                                            255*((b & (1 << s))!=0),
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
