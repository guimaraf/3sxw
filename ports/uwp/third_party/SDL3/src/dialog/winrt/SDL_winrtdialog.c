/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "../../core/windows/SDL_windows.h"
#include "../SDL_dialog.h"
#include "../SDL_dialog_utils.h"
#include "SDL_internal.h"

// todo: move to shared space between c & /zw 
typedef struct
{
    SDL_DialogFileCallback callback;
    void *userdata;
} pickerArgs;

// To be implemented inside /ZW cpp file
extern int PickerThread(void *ptr);

int picker_kicker(void* ptr)
{
    int resp = PickerThread(ptr);
    SDL_free(ptr);
    return resp;
}

// hack: missing a lost of functionality but at least allows for single file selection built in
void SDL_SYS_ShowFileDialogWithProperties(SDL_FileDialogType type, SDL_DialogFileCallback callback, void *userdata, SDL_PropertiesID props)
{
    pickerArgs *args;
    SDL_Thread *thread;

    args = (pickerArgs *)SDL_malloc(sizeof(*args));
    if (args == NULL) {
        callback(userdata, NULL, -1);
        return;
    }

    args->callback = callback;
    args->userdata = userdata;

    thread = SDL_CreateThread(picker_kicker, "SDL_WinRT_BasicPicker", (void *)args);
    SDL_DetachThread(thread);
}
