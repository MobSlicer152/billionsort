#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#define COUNT 1000000000

// ceil(sqrt(COUNT))
#define SIZE 31623

void Reset(uint32_t* array, size_t arraySize)
{
    for (size_t i = 0; i < arraySize; i++)
    {
        array[i] = (uint32_t)i;
    }
}

enum ThreadState
{
    Resetting,
    Idle,
    Sorting,
    Exiting
};

struct ThreadData
{
    uint64_t lastStart;
    uint64_t lastEnd;
    SDL_Surface* surface;
    uint32_t* array;
    size_t arraySize;
    std::atomic<ThreadState> state;
};

int32_t RenderThread(void* rawData)
{
    auto& data = *(ThreadData*)rawData;
    auto surface = data.surface;
    auto array = data.array;
    auto arraySize = data.arraySize;

    while (data.state != ThreadState::Exiting)
    {
        auto pixels = (uint8_t*)surface->pixels;
        for (size_t i = 0; i < arraySize; i++)
        {
            // figure out the (x, y) for this pixel
            uint32_t x = (uint32_t)(i % surface->w);
            uint32_t y = (uint32_t)(i / surface->w);

            // figure out the "correct" (x, y) for this value
            uint32_t ai = array[i];
            uint32_t ax = (uint32_t)(ai % surface->w);
            uint32_t ay = (uint32_t)(ai / surface->w);

            // make a color for the position
            float r = (float)ax / surface->w;
            float g = 0;
            float b = (float)ay / surface->h;

            size_t p = surface->pitch * y + x * 4;
            pixels[p + 0] = (uint8_t)(r * 255);
            pixels[p + 1] = (uint8_t)(g * 255);
            pixels[p + 2] = (uint8_t)(b * 255);
        }
    }

    return 0;
}

int32_t SortThread(void* rawData)
{
    auto& data = *(ThreadData*)rawData;

    Reset(data.array, data.arraySize);

    auto start = data.array;
    auto end = data.array + data.arraySize - 1;

    while (data.state != ThreadState::Exiting)
    {
        switch (data.state)
        {
        case ThreadState::Idle: {
            SDL_Delay(5);
            break;
        }
        case ThreadState::Resetting: {
            std::shuffle(start, end, std::default_random_engine((uint32_t)SDL_GetTicksNS()));
            data.state = ThreadState::Sorting;
            break;
        }
        case ThreadState::Sorting: {
            data.lastStart = SDL_GetTicks();
            std::sort(start, end);
            data.lastEnd = SDL_GetTicks();
            data.state = ThreadState::Idle;
            break;
        }
        }
    }

    return 0;
}

int32_t SDL_main(int32_t argc, char* argv[])
{
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

    int width = 1024;
    int height = 576;
    SDL_Window* window = SDL_CreateWindow("Billion Sort", width, height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);

    SDL_Surface* surface = SDL_CreateSurface(SIZE, SIZE, SDL_PIXELFORMAT_XBGR8888);

    size_t arraySize = COUNT;
    uint32_t* array = new uint32_t[arraySize];

    ThreadData threadData = {};
    threadData.surface = surface;
    threadData.array = array;
    threadData.arraySize = arraySize;
    threadData.state = ThreadState::Resetting;

    SDL_Thread* sortThread = SDL_CreateThread(SortThread, "sort", (void*)&threadData);
    SDL_DetachThread(sortThread);
    SDL_Thread* renderThread = SDL_CreateThread(RenderThread, "render", (void*)&threadData);
    SDL_DetachThread(renderThread);

    char title[64] = {};
    bool running = true;
    while (running)
    {
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            switch (e.type)
            {
            case SDL_EVENT_QUIT: {
                running = false;
                break;
            }
            case SDL_EVENT_WINDOW_RESIZED: {
                width = e.window.data1;
                height = e.window.data1;
                break;
            }
            case SDL_EVENT_KEY_DOWN: {
                auto key = e.key.key;
                switch (key)
                {
                case SDLK_S: {
                    // tell the sorting thread to start sorting again if it's idle
                    threadData.state = ThreadState::Resetting;
                    break;
                }
                }
            }
            }
        }

        switch (threadData.state)
        {
        case ThreadState::Resetting: {
            snprintf(title, sizeof(title), "Resetting...");
            break;
        }
        case ThreadState::Idle: {
            snprintf(
                title, sizeof(title), "Done after %.2f seconds. Press S to sort again.",
                (threadData.lastEnd - threadData.lastStart) / 1000.0f);
            break;
        }
        case ThreadState::Sorting: {
            snprintf(
                title, sizeof(title), "Sorting... (%.2f seconds elapsed)", (SDL_GetTicks() - threadData.lastStart) / 1000.0f);
            break;
        }
        }

        SDL_SetWindowTitle(window, title);

        SDL_BlitSurfaceScaled(surface, nullptr, SDL_GetWindowSurface(window), nullptr, SDL_SCALEMODE_LINEAR);
        SDL_UpdateWindowSurface(window);
    }

    threadData.state = ThreadState::Exiting;
    // don't bother waiting for the
    SDL_WaitThread(renderThread, nullptr);

    delete[] array;
    SDL_DestroySurface(surface);
    SDL_DestroyWindow(window);

    return 0;
}
