//Compiler
//g++ pi86.cpp x86.cpp buslog.cpp cga.cpp vga.cpp font.h timer.cpp drives.cpp keycodes.h -o pi86 `sdl2-config --cflags --libs` -pthread -lwiringPi -lX11 -Wl,--allow-multiple-definition

// SDL2 compatibility note (Raspberry Pi OS Bookworm/Trixie):
// SDL2 on modern Pi OS requires all rendering and event-pump calls to originate
// from the main thread.  Three changes were made relative to the upstream file:
//   1. SDL_WINDOW_OPENGL        -> SDL_WINDOW_SHOWN
//   2. SDL_RENDERER_ACCELERATED -> SDL_RENDERER_SOFTWARE
//   3. Rendering moved from the screen_loop worker thread into the main loop;
//      SDL_PumpEvents() added so the keyboard thread's SDL_PollEvent still works.

// Pi 3B display fix:
// SDL2's software renderer uses the SDL window surface as its render target
// directly.  On Pi 3B (non-default X11 visual), SDL's internal XPutImage
// corrupts the X window every frame, fighting any correction we apply.
// Fix: create the renderer targeting an OFFSCREEN SDL_Surface instead.
// SDL_RenderPresent on a surface-backed renderer is a no-op (no window to
// update), so SDL never touches the X window.  x11_flush() then has sole
// ownership of the X window and pushes frames via raw XPutImage.
// This works on Pi 4B too — raw XPutImage works on both platforms.

#include "SDL.h"
#include "SDL_syswm.h"
#include <X11/Xlib.h>
#include <stdio.h>
#include <fstream>
#include <unistd.h>
#include <thread>
#include "x86.h"
#include "font.h"
#include "vga.h"
#include "timer.h"
#include "drives.h"
#include "keycodes.h"


using namespace std;

void keyboard();

// ---------------------------------------------------------------------------
// Manual X11 flush — sole owner of the X window drawable.
// SDL renders into an offscreen surface; x11_flush() pushes it to the screen.
// Rate-capped at ~30 fps: sufficient for BIOS/DOS and keeps CPU load low.
// ---------------------------------------------------------------------------
static Display     *x11_dpy  = NULL;
static Window       x11_win  = 0;
static GC           x11_gc   = None;
static Visual      *x11_vis  = NULL;
static int          x11_dep  = 0;
static XImage      *x11_img  = NULL;
static SDL_Surface *x11_surf = NULL;   // offscreen render target

static void x11_flush()
{
	if (!x11_dpy || !x11_surf || !x11_img) return;

	static Uint32 last = 0;
	Uint32 now = SDL_GetTicks();
	if (now - last < 33) return;   // ~30 fps cap
	last = now;

	x11_img->data           = (char *)x11_surf->pixels;
	x11_img->bytes_per_line = x11_surf->pitch;
	XPutImage(x11_dpy, x11_win, x11_gc,
	          x11_img, 0, 0, 0, 0, x11_surf->w, x11_surf->h);
	XSync(x11_dpy, False);
	x11_img->data = NULL;   // pixels owned by SDL; never let XImage free them
}

int main(int argc, char* argv[]) {

	SDL_Window *window;
	SDL_Renderer *renderer = NULL;
	SDL_Init(SDL_INIT_VIDEO);

	window = SDL_CreateWindow(
		"x86",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		720,
		400,
		SDL_WINDOW_SHOWN   // SDL_WINDOW_OPENGL removed: causes cross-thread
		                   // rendering failure on SDL2 >= 2.24
	);

	// Offscreen surface: the renderer draws here instead of the window surface.
	// SDL_RenderPresent on a surface-backed renderer is a no-op, so SDL never
	// calls XPutImage and never corrupts the X window.
	// Pixel format matches the standard X11 RGB visual (R=0xFF0000 etc.).
	x11_surf = SDL_CreateRGBSurface(0, 720, 400, 32,
	                                0x00FF0000, 0x0000FF00, 0x000000FF, 0);
	renderer = SDL_CreateSoftwareRenderer(x11_surf);

	// Initialise X11 flush path — get the display/window/GC from SDL.
	{
		SDL_SysWMinfo wm;
		SDL_VERSION(&wm.version);
		if (SDL_GetWindowWMInfo(window, &wm) &&
		    wm.subsystem == SDL_SYSWM_X11)
		{
			x11_dpy = wm.info.x11.display;
			x11_win = wm.info.x11.window;
			x11_gc  = XCreateGC(x11_dpy, x11_win, 0, NULL);
			XWindowAttributes attrs;
			XGetWindowAttributes(x11_dpy, x11_win, &attrs);
			x11_vis = attrs.visual;
			x11_dep = attrs.depth;
			x11_img = XCreateImage(x11_dpy, x11_vis, x11_dep,
			                       ZPixmap, 0,
			                       NULL,            // data: set per-frame
			                       x11_surf->w, x11_surf->h,
			                       32, x11_surf->pitch);
		}
	}

	//The bios file to load
	Load_Bios("pcxtbios.rom");

	///////////////////////////////////////////////////////////////////
	//Change this Start(V30); 8086 or Start(V20); 8088 to set the processor
	///////////////////////////////////////////////////////////////////
	Start(V20);

	//Drive images a: and C:
	Start_Drives("floppy.img", "hdd.img");
	//Starts the system timer, IRQ0 / INT 0x08
	Start_System_Timer();

	thread keyboard_loop(keyboard);		//Start Keyboard

	// Rendering runs on the main thread (SDL2 requirement on modern Pi OS).
	// SDL_PumpEvents() keeps the X11 event queue drained so keyboard_loop's
	// SDL_PollEvent() continues to receive key events.
	char _vm40[2000], _vm80[4000], _vm320[0x4000], _cur[2];
	while (Stop_Flag != true)
	{
		SDL_PumpEvents();
		char _vm = Read_Memory_Byte(0x00449);
		if (_vm == 0x00 || _vm == 0x01) {
			Read_Memory_Array(0xB8000, _vm40, 2000);
			Read_Memory_Array(0x00450, _cur, 2);
			Mode_0_40x25(renderer, _vm40, _cur);
			x11_flush();
		} else if (_vm == 0x02 || _vm == 0x03) {
			Read_Memory_Array(0xB8000, _vm80, 4000);
			Read_Memory_Array(0x00450, _cur, 2);
			Mode_2_80x25(renderer, _vm80, _cur);
			x11_flush();
		} else if (_vm == 0x04) {
			Read_Memory_Array(0xB8000, _vm320, 0x4000);
			if (Read_Memory_Byte(0x00466) == 0x20)
				Graphics_Mode_320_200_Palette_1(renderer, _vm320);
			else
				Graphics_Mode_320_200_Palette_0(renderer, _vm320);
			x11_flush();
		}
		if (Read_IO_Byte(0xF0FF) == 0x00) { Stop_Flag = true; break; }
	}

	keyboard_loop.join();

	if (x11_img)  { x11_img->data = NULL; XFree(x11_img); }
	if (x11_surf) { SDL_FreeSurface(x11_surf); }
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}


void Insert_Key(char character_code, char scan_code) //Interrupt_9
{
	char Key_Buffer_Tail =  Read_Memory_Byte(0x041C);
	Write_Memory_Byte(0x400 + Key_Buffer_Tail, character_code);
	Write_Memory_Byte(0x401 + Key_Buffer_Tail, scan_code);
	Key_Buffer_Tail = Key_Buffer_Tail + 2;
	if(Key_Buffer_Tail >=  Read_Memory_Byte(0x0482))
	{
		Key_Buffer_Tail = Read_Memory_Byte(0x0480);
	}
	Write_Memory_Byte(0x041C, Key_Buffer_Tail);
}

void keyboard()
{
	SDL_Event e;
	while(Stop_Flag != true)
	{
		if (SDL_PollEvent(&e))
		{
			if (e.type == SDL_QUIT)
			{
				Stop_Flag = true;
				break;
			}
			if(e.type == SDL_KEYDOWN)
	 		{
				Write_IO_Byte(0x0060, scan_codes[e.key.keysym.scancode]);
				IRQ1();
			}
			if(e.type == SDL_KEYUP)
	 		{
				Write_IO_Byte(0x0060, (scan_codes[e.key.keysym.scancode] + 0x80));
				IRQ1();
			}
		}
	}
}
