#ifndef MEMORIES_GL_HEADERS_H
#define MEMORIES_GL_HEADERS_H
/* SDL_opengl.h declares nothing on iOS ("No OpenGL on iOS"), but the game's
 * OpenGL ES 3 path uses the same GL names, types and function-pointer types
 * as on Android, where that header declares them. On iOS the header is
 * therefore included as though the platform were not iOS: it only declares
 * (the OpenGL ES calls themselves come from the OpenGLES framework, the
 * desktop-only ones from gl_desktop_none.c). */
#include <SDL3/SDL_platform.h>
#ifdef SDL_PLATFORM_IOS
#undef SDL_PLATFORM_IOS
#include <SDL3/SDL_opengl.h>
#define SDL_PLATFORM_IOS 1
#else
#include <SDL3/SDL_opengl.h>
#endif
#endif
