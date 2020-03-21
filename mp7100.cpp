/*
 * Multicomp MP7100  / OWON SP3051 / 3101 / 6031 / 5051
 *
 * V0.1 - March 20, 2020, Peak COVID19
 * 
 *
 * Written by Paul L Daniels (pldaniels@gmail.com)
 *
 */

#include <SDL.h>
#include <SDL_ttf.h>

#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define FL __FILE__,__LINE__

/*
 * Should be defined in the Makefile to pass to the compiler from
 * the github build revision
 *
 */
#ifndef BUILD_VER 
#define BUILD_VER 000
#endif

#ifndef BUILD_DATE
#define BUILD_DATE " "
#endif

#define SSIZE 1024

#define INTERFRAME_SLEEP	200000 // 0.2 seconds

#define DATA_FRAME_SIZE 12
#define ee ""
#define uu "\u00B5"
#define kk "k"
#define MM "M"
#define mm "m"
#define nn "n"
#define pp "p"
#define dd "\u00B0"
#define oo "\u03A9"

#define MMFLAG_AUTORANGE	0b01000000

char SEPARATOR_DP[] = ".";

struct glb {
	uint8_t debug;
	uint8_t quiet;
	uint16_t flags;
	char *output_file;
	char *device;

	int interval;
	int font_size;
	int window_width, window_height;
	int wx_forced, wy_forced;
	SDL_Color font_color_volts, font_color_amps, background_color;
};

/*
 * A whole bunch of globals, because I need
 * them accessible in the Windows handler
 *
 * So many of these I'd like to try get away from being
 * a global.
 *
 */
struct glb *glbs;

/*
 * Test to see if a file exists
 *
 * Does not test anything else such as read/write status
 *
 */
bool fileExists(const char *filename) {
	struct stat buf;
	return (stat(filename, &buf) == 0);
}


char digit( unsigned char dg ) {

	int d;
	char g;

	switch (dg) {
		case 0x30: g = '0'; d = 0; break;
		case 0x31: g = '1'; d = 1; break;
		case 0x32: g = '2'; d = 2; break;
		case 0x33: g = '3'; d = 3; break;
		case 0x34: g = '4'; d = 4; break;
		case 0x35: g = '5'; d = 5; break;
		case 0x36: g = '6'; d = 6; break;
		case 0x37: g = '7'; d = 7; break;
		case 0x38: g = '8'; d = 8; break;
		case 0x39: g = '9'; d = 9; break;
		case 0x3E: g = 'L'; d = 0; break;
		case 0x3F: g = ' '; d = 0; break;
		default: g = ' ';
	}

	return g;
}

/*-----------------------------------------------------------------\
  Date Code:	: 20180127-220248
  Function Name	: init
  Returns Type	: int
  ----Parameter List
  1. struct glb *g ,
  ------------------
  Exit Codes	:
  Side Effects	:
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int init(struct glb *g) {
	g->debug = 0;
	g->quiet = 0;
	g->flags = 0;
	g->output_file = NULL;
	g->interval = 100000;
	g->device = NULL;

	g->font_size = 60;
	g->window_width = 400;
	g->window_height = 100;
	g->wx_forced = 0;
	g->wy_forced = 0;

	g->font_color_volts =  { 10, 200, 10 };
	g->font_color_amps =  { 200, 200, 10 };
	g->background_color = { 0, 0, 0 };

	return 0;
}

void show_help(void) {
	fprintf(stdout,"MP7100 Power supply display\r\n"
			"By Paul L Daniels / pldaniels@gmail.com\r\n"
			"Build %d / %s\r\n"
			"\r\n"
			" [-p <usbtmc path, ie /dev/usbtmc2>] \r\n"
			"\r\n"
			"\t-h: This help\r\n"
			"\t-d: debug enabled\r\n"
			"\t-q: quiet output\r\n"
			"\t-v: show version\r\n"
			"\t-z <font size in pt>\r\n"
			"\t-cv <volts colour, a0a0ff>\r\n"
			"\t-ca <amps colour, ffffa0>\r\n"
			"\t-cb <background colour, 101010>\r\n"
			"\t-t <interval> (sleep delay between samples, default 100,000us)\r\n"
			"\r\n"
			"\texample: MP7100 -p /dev/usbtmc2\r\n"
			, BUILD_VER
			, BUILD_DATE 
			);
} 


/*-----------------------------------------------------------------\
  Date Code:	: 20180127-220258
  Function Name	: parse_parameters
  Returns Type	: int
  ----Parameter List
  1. struct glb *g,
  2.  int argc,
  3.  char **argv ,
  ------------------
  Exit Codes	:
  Side Effects	:
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int parse_parameters(struct glb *g, int argc, char **argv ) {
	int i;

	if (argc == 1) {
		show_help();
		exit(1);
	}

	for (i = 0; i < argc; i++) {
		if (argv[i][0] == '-') {
			/* parameter */
			switch (argv[i][1]) {

				case 'h':
					show_help();
					exit(1);
					break;

				case 'z':
					i++;
					if (i < argc) {
						g->font_size = atoi(argv[i]);
					} else {
						fprintf(stdout,"Insufficient parameters; -z <font size pts>\n");
						exit(1);
					}
					break;

				case 'p':
					/*
					 * com port can be multiple things in linux
					 * such as /dev/ttySx or /dev/ttyUSBxx
					 */
					i++;
					if (i < argc) {
						g->device = argv[i];
					} else {
						fprintf(stdout,"Insufficient parameters; -p <usb TMC port ie, /dev/usbtmc2>\n");
						exit(1);
					}
					break;

				case 'o':
					/* 
					 * output file where this program will put the text
					 * line containing the information FlexBV will want 
					 *
					 */
					i++;
					if (i < argc) {
						g->output_file = argv[i];
					} else {
						fprintf(stdout,"Insufficient parameters; -o <output file>\n");
						exit(1);
					}
					break;

				case 'd': g->debug = 1; break;

				case 'q': g->quiet = 1; break;

				case 'v':
							 fprintf(stdout,"Build %d\r\n", BUILD_VER);
							 exit(0);
							 break;

				case 't':
							 i++;
							 g->interval = atoi(argv[i]);
							 break;

				case 'c':
							 int a,b,c;
							 if (argv[i][2] == 'v') {
								 i++;
								 sscanf(argv[i], "%2hhx%2hhx%2hhx"
										 , &g->font_color_volts.r
										 , &g->font_color_volts.g
										 , &g->font_color_volts.b
										 );

							 } else if (argv[i][2] == 'a') {
								 i++;
								 sscanf(argv[i], "%2hhx%2hhx%2hhx"
										 , &g->font_color_amps.r
										 , &g->font_color_amps.g
										 , &g->font_color_amps.b
										 );

							 } else if (argv[i][2] == 'b') {
								 i++;
								 sscanf(argv[i], "%2hhx%2hhx%2hhx"
										 , &(g->background_color.r)
										 , &(g->background_color.g)
										 , &(g->background_color.b)
										 );

							 }
							 break;

				case 'w':
							 if (argv[i][2] == 'x') {
								 i++;
								 g->wx_forced = atoi(argv[i]);
							 } else if (argv[i][2] == 'y') {
								 i++;
								 g->wy_forced = atoi(argv[i]);
							 }
							 break;

				default: break;
			} // switch
		}
	}

	return 0;
}



/*-----------------------------------------------------------------\
  Date Code:	: 20180127-220307
  Function Name	: main
  Returns Type	: int
  ----Parameter List
  1. int argc,
  2.  char **argv ,
  ------------------
  Exit Codes	:
  Side Effects	:
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int main ( int argc, char **argv ) {

	SDL_Event event;
	SDL_Surface *surface, *surface_2;
	SDL_Texture *texture, *texture_2;

	char linetmp[SSIZE]; // temporary string for building main line of text

//	uint8_t d[SSIZE];
//	uint8_t dt[SSIZE];      // Serial data packet
//	int dt_loaded = 0;	// set when we have our first valid data
//	uint8_t dps = 0;     // Number of decimal places
	struct glb g;        // Global structure for passing variables around
	int i = 0;           // Generic counter
	char temp_char;        // Temporary character
	char tfn[4096];
	bool quit = false;
	bool error_flag = false;
	int f;

	glbs = &g;

	/*
	 * Initialise the global structure
	 */
	init(&g);

	/*
	 * Parse our command line parameters
	 */
	parse_parameters(&g, argc, argv);
	if (g.device == NULL) {
		fprintf(stdout,"Require valid device (ie, -p /dev/usbtmc2 )\nExiting\n");
		exit(1);
	}

	/* 
	 * check paramters
	 *
	 */
	if (g.font_size < 10) g.font_size = 10;
	if (g.font_size > 200) g.font_size = 200;

	if (g.output_file) snprintf(tfn,sizeof(tfn),"%s.tmp",g.output_file);

	/*
	 * Handle the USB port
	 */

	f = open( g.device, O_RDWR );
	if (f == -1) {
		fprintf(stdout, "Error opening device [%s] : %s\n", g.device, strerror(errno));
		exit (1);
	}

	/*
	 * Setup SDL2 and fonts
	 *
	 */

	SDL_Init(SDL_INIT_VIDEO);
	TTF_Init();
	TTF_Font *font = TTF_OpenFont("RobotoMono-Regular.ttf", g.font_size);
	TTF_Font *font_small = TTF_OpenFont("RobotoMono-Regular.ttf", g.font_size/4);

	/*
	 * Get the required window size.
	 *
	 * Parameters passed can override the font self-detect sizing
	 *
	 */
	TTF_SizeText(font, " 00.000V ", &g.window_width, &g.window_height);
	g.window_height *= 1.85;

	if (g.wx_forced) g.window_width = g.wx_forced;
	if (g.wy_forced) g.window_height = g.wy_forced;

	SDL_Window *window = SDL_CreateWindow("MP7100", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, g.window_width, g.window_height, 0);
	SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);
	if (!font) {
		fprintf(stderr,"Error trying to open font :( \r\n");
		exit(1);
	}

	/* Select the color for drawing. It is set to red here. */
	SDL_SetRenderDrawColor(renderer, g.background_color.r, g.background_color.g, g.background_color.b, 255 );

	/* Clear the entire screen to our selected color. */
	SDL_RenderClear(renderer);

	/*
	 *
	 * Parent will terminate us... else we'll become a zombie
	 * and hope that the almighty PID 1 will reap us
	 *
	 */
	while (!quit) {
		char line1[1024];
		char line2[1024];
		char buf_volt[100];
		char buf_curr[100];

		char *p, *q;
		double v = 0.0;
		int end_of_frame_received = 0;
		uint8_t range;
		uint8_t dpp = 0;
		ssize_t bytes_read = 0;
		ssize_t sz;


		while (SDL_PollEvent(&event)) {
			switch (event.type)
			{
				case SDL_KEYDOWN:
					if (event.key.keysym.sym == SDLK_q) quit = true;
					break;
				case SDL_QUIT:
					quit = true;
					break;
			}
		}

		linetmp[0] = '\0';


		if (error_flag) {
			f = open( g.device, O_RDWR );
			if (f == -1) {
				fprintf(stdout, "Error opening device [%s] : %s\n", g.device, strerror(errno));
				sleep(1);
			} else {
				error_flag = false;
			}

		}

		/*
		 * VOLTAGE
		 *
		 */
		sz = write(f, "MEAS:VOLT?", sizeof("MEAS:VOLT?"));
		if (sz < 0) {
			error_flag = true;
			fprintf(stdout,"Error sending VOLT query: %s\n", strerror(errno));
			snprintf(buf_volt,sizeof(buf_volt),"NODATA");
			//exit(1);
		}
		usleep(20000);

		int bp = 0;
		do {
			sz = read(f, buf_volt +bp, sizeof(buf_volt) -1 -bp);
			buf_volt[bp+sz] = '\0';
//			fprintf(stdout,"%s|", buf_volt+bp);

			if (sz == -1) {
			error_flag = true;
				fprintf(stdout,"Error reading data from PSU: %s\n", strerror(errno));
				snprintf(buf_volt,sizeof(buf_volt),"NODATA");
				break;
				//exit(1);
			}

			bp += sz;
			if (sz == 0) break;
			if (bp >= sizeof(buf_volt)) break;
			usleep(1000);
		} while (sz);
		buf_volt[bp] = '\0';
		if ((bp > 0) && buf_volt[bp-1] == '\n') buf_volt[bp -1] = '\0';


		/*
		 * CURRENT
		 *
		 */
		bp = 0;
		sz = write(f, "MEAS:CURR?", sizeof("MEAS:CURR?"));
		if (sz < 0) {
			error_flag = true;
			fprintf(stdout,"Error sending CURRENT query: %s\n", strerror(errno));
			snprintf(buf_curr,sizeof(buf_curr),"NODATA");
			//exit(1);
		}
		usleep(20000);
		do {
			sz = read(f, buf_curr+bp, sizeof(buf_curr) -1 -bp);
			if (sz == -1) {
				error_flag = true;
				fprintf(stdout,"Error reading data from PSU: %s\n", strerror(errno));
				snprintf(buf_curr,sizeof(buf_curr),"NODATA");
				break;
				//exit(1);
			}

			buf_curr[bp+sz] = '\0';
//			fprintf(stdout,"%s|", buf_curr+bp);
			bp += sz;

			if (sz == 0) break;
			if (bp >= sizeof(buf_curr)) break;
			usleep(1000);
		} while (sz);
		buf_curr[bp] = '\0';
		if ((bp > 0) && buf_curr[bp-1] == '\n') buf_curr[bp -1] = '\0';

		/*
		 *
		 * END OF DATA ACQUISITION
		 *
		 */


		//snprintf(line1, sizeof(line1)-1, "%s%s", buf_volt, error_flag?"":"V");
		snprintf(line1, sizeof(line1), "%7s%s", buf_volt, error_flag?"":"V");
		snprintf(line2, sizeof(line2), "%7s%s", buf_curr, error_flag?"":"A");
		if (g.debug) fprintf(stdout,"%s\n%s\n", line1, line2);


		{
			int texW = 0;
			int texH = 0;
			int texW2 = 0;
			int texH2 = 0;
			SDL_RenderClear(renderer);
			surface = TTF_RenderUTF8_Solid(font, line1, g.font_color_volts);
			texture = SDL_CreateTextureFromSurface(renderer, surface);
			SDL_QueryTexture(texture, NULL, NULL, &texW, &texH);
			SDL_Rect dstrect = { 0, 0, texW, texH };
			SDL_RenderCopy(renderer, texture, NULL, &dstrect);

			surface_2 = TTF_RenderUTF8_Solid(font, line2, g.font_color_amps);
			texture_2 = SDL_CreateTextureFromSurface(renderer, surface_2);
			SDL_QueryTexture(texture_2, NULL, NULL, &texW2, &texH2);
			dstrect = { 0, texH -(texH /5), texW2, texH2 };
			SDL_RenderCopy(renderer, texture_2, NULL, &dstrect);

			SDL_RenderPresent(renderer);

			SDL_DestroyTexture(texture);
			SDL_FreeSurface(surface);
			if (1) {
				SDL_DestroyTexture(texture_2);
				SDL_FreeSurface(surface_2);
			}

		if (error_flag) {
			sleep(1);
		} else {
			usleep(g.interval);
		}


		}


		if (g.output_file) {
			/*
			 * Only write the file out if it doesn't
			 * exist. 
			 *
			 */
			if (!fileExists(g.output_file)) {
				FILE *f;
				fprintf(stderr,"%s:%d: output filename = %s\r\n", FL, g.output_file);
				f = fopen(tfn,"w");
				if (f) {
					fprintf(f,"%s", linetmp);
					fprintf(stderr,"%s:%d: %s => %s\r\n", FL, linetmp, tfn);
					fclose(f);
					rename(tfn, g.output_file);
				}
			}
		}

	} // while(1)

	close(f);

	TTF_CloseFont(font);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	TTF_Quit();
	SDL_Quit();

	return 0;

}
