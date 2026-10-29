#include <winrt/Windows.ApplicationModel.Core.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Gaming.Input.h>
#include <winrt/Windows.Graphics.Display.Core.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.AccessCache.h>
#include <winrt/Windows.Storage.Pickers.h>
#include <winrt/Windows.Storage.Pickers.Provider.h>
#include <winrt/Windows.UI.Composition.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.ViewManagement.h>

#include <windows.h>
#include "SDL3/SDL.h"

using namespace winrt::Windows;
using namespace winrt::Windows::ApplicationModel::Core;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Graphics::Display::Core;
using namespace Storage;
using namespace Storage::AccessCache;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::ViewManagement;

// todo: how to best share this between c/c++?
typedef void (__cdecl *SDL_DialogFileCallback)(void *userdata, const char * const *filelist, int filter);
// todo: move to shared space between c & /zw 
typedef struct
{
    SDL_DialogFileCallback callback;
    void *userdata;
} pickerArgs;

// impl for PickerThread referenced from winrtdialog
// note: this is very minimal and only supports single file picks, a good area to build out if you're bored one day
extern "C" int PickerThread(void* ptr)
{
    bool complete = false;
    pickerArgs* args = (pickerArgs *)ptr;

    auto dispatcher = winrt::Windows::ApplicationModel::Core::CoreApplication::MainView().CoreWindow().Dispatcher();

    dispatcher.RunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::Normal, [args, &complete]() mutable {
        Pickers::FileOpenPicker picker;
        picker.SuggestedStartLocation(Pickers::PickerLocationId::ComputerFolder);
        picker.FileTypeFilter().Append(L"*");

        auto op = picker.PickSingleFileAsync();

        op.Completed([args, &complete](auto &&asyncInfo, auto status) mutable {
            if (status == winrt::Windows::Foundation::AsyncStatus::Completed) {
                auto file = asyncInfo.GetResults();
                if (file) {
                    std::string path = winrt::to_string(file.Path());

                    const char *files[] = { path.c_str(), nullptr };

                    args->callback(args->userdata, files, -1);
                    complete = true;
                    return;
                }
            }
            args->callback(args->userdata, NULL, -1);
            complete = true;
        });
    });

    while (!complete) {
        Sleep(100);
    }

    return 0;
}
