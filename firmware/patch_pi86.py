#!/usr/bin/env python3
"""
Apply SDL2 compatibility fixes to pi86.cpp for modern Raspberry Pi OS.

The upstream pi86.cpp was written for older Pi OS (Buster/Bullseye) where SDL2
allowed rendering from worker threads. On Bookworm/Trixie, SDL2 requires all
rendering and event calls to originate from the main thread. Three changes are
needed:

  1. SDL_WINDOW_OPENGL  -> SDL_WINDOW_SHOWN   (no OpenGL window context)
  2. SDL_RENDERER_ACCELERATED -> SDL_RENDERER_SOFTWARE  (software renderer)
  3. Move rendering from the screen_loop worker thread into the main loop.
     SDL_PumpEvents() is also added so the keyboard thread's SDL_PollEvent
     continues to work correctly.
"""

import re
import sys

def apply(path):
    with open(path, 'r') as f:
        src = f.read()

    # Fix 1 & 2: window flag and renderer type
    src = src.replace('SDL_WINDOW_OPENGL',        'SDL_WINDOW_SHOWN')
    src = src.replace('SDL_RENDERER_ACCELERATED', 'SDL_RENDERER_SOFTWARE')

    # Fix 3: replace the main loop section
    # Match from "thread keyboard_loop" through "screen_loop.join();"
    pattern = (
        r'(thread\s+keyboard_loop\s*\(keyboard\)\s*;[^\n]*\n)'   # keep keyboard thread
        r'.*?'                                                      # skip screen_loop thread + old loop
        r'keyboard_loop\s*\.\s*join\s*\(\s*\)\s*;[^\n]*\n'
        r'\s*screen_loop\s*\.\s*join\s*\(\s*\)\s*;'
    )

    replacement = (
        r'\1'
        '\n'
        '\tchar _vm40[2000], _vm80[4000], _vm320[0x4000], _cur[2];\n'
        '\twhile (Stop_Flag != true)\n'
        '\t{\n'
        '\t\tSDL_PumpEvents();\n'
        '\t\tchar _vm = Read_Memory_Byte(0x00449);\n'
        '\t\tif (_vm == 0x00 || _vm == 0x01) {\n'
        '\t\t\tRead_Memory_Array(0xB8000, _vm40, 2000);\n'
        '\t\t\tRead_Memory_Array(0x00450, _cur, 2);\n'
        '\t\t\tMode_0_40x25(renderer, _vm40, _cur);\n'
        '\t\t} else if (_vm == 0x02 || _vm == 0x03) {\n'
        '\t\t\tRead_Memory_Array(0xB8000, _vm80, 4000);\n'
        '\t\t\tRead_Memory_Array(0x00450, _cur, 2);\n'
        '\t\t\tMode_2_80x25(renderer, _vm80, _cur);\n'
        '\t\t} else if (_vm == 0x04) {\n'
        '\t\t\tRead_Memory_Array(0xB8000, _vm320, 0x4000);\n'
        '\t\t\tif (Read_Memory_Byte(0x00466) == 0x20)\n'
        '\t\t\t\tGraphics_Mode_320_200_Palette_1(renderer, _vm320);\n'
        '\t\t\telse\n'
        '\t\t\t\tGraphics_Mode_320_200_Palette_0(renderer, _vm320);\n'
        '\t\t}\n'
        '\t\tif (Read_IO_Byte(0xF0FF) == 0x00) { Stop_Flag = true; break; }\n'
        '\t}\n'
        '\n'
        '\tkeyboard_loop.join();'
    )

    result, n = re.subn(pattern, replacement, src, flags=re.DOTALL)

    if n == 0:
        print("ERROR: Could not find the main loop pattern in pi86.cpp.")
        print("       The upstream file may be a different version than expected.")
        print("       Apply the changes manually — see INSTRUCTOR_SETUP.md Step 5.")
        sys.exit(1)

    with open(path, 'w') as f:
        f.write(result)

    print(f"All three SDL2 fixes applied to {path}")


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} path/to/pi86.cpp")
        sys.exit(1)
    apply(sys.argv[1])
