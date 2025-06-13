#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#ifdef SDL_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#endif

#define COUNT 1000000000

// ceil(sqrt(COUNT))
#define SIZE 31623

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

struct FileMapping {
#ifdef SDL_PLATFORM_WINDOWS
	HANDLE fileHandle;
	HANDLE mappingHandle;
#else
	int32_t file;
#endif
	void* base;
	size_t size;
};

void UnmapFile(FileMapping& map)
{
#ifdef SDL_PLATFORM_WINDOWS
	CloseHandle(map.mappingHandle);
	CloseHandle(map.fileHandle);
#else
	munmap(map.base, map.size);
	close(map.file);
#endif
	memset(&map, 0, sizeof(FileMapping));
}

bool MapFile(const char* name, uint64_t offset, size_t size, FileMapping& map)
{
	map.size = size;
#ifdef SDL_PLATFORM_WINDOWS
	map.fileHandle = CreateFileA(name, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_TEMPORARY | FILE_ACCESS_RANDOM, nullptr);
	if (!map.fileHandle)
	{
		printf("failed to open %s: Win32 error %d\n", name, GetLastError());
		UnmapFile(map);
		return false;
	}

	LARGE_INTEGER largeSize = {};
	largeSize.QuadPart = map.size;
	SetFilePointerEx(map.fileHandle, &largeSize, nullptr, FILE_BEGIN);
	SetEndOfFile(map.fileHandle);
	SetFilePointer(map.fileHandle, 0, 0, FILE_BEGIN);

	map.mappingHandle = CreateFileMappingA(map.fileHandle, nullptr, PAGE_READWRITE,	largeSize.HighPart, largeSize.LowPart, nullptr);
	if (!map.mappingHandle)
	{
		printf("failed to create file mapping: Win32 error %d\n", GetLastError());
		UnmapFile(map);
		return false;
	}

	map.base = MapViewOfFile2(map.mappingHandle, GetCurrentProcess(), 0, nullptr, size, 0, PAGE_READWRITE);
	if (!map.base)
	{
		printf("failed to map %zu bytes from %s: Win32 error %d\n", size, name, GetLastError());
		UnmapFile(map);
		return false;
	}
#else
	map.file = open(name, O_RDWR | O_CREAT | O_TRUNC | O_LARGEFILE, 0644);
	if (map.file < 0)
	{
		printf("failed to open %s: errno %d\n", name, errno);
		UnmapFile(map);
		return false;
	}

	if (ftruncate(map.file, map.size) != 0)
	{
		printf("ftruncate failed: errno %d\n", errno);
		UnmapFile(map);
		return false;
	}

	map.base = mmap(NULL, map.size, PROT_READ | PROT_WRITE, MAP_SHARED, map.file, (off64_t)offset);
	if (!map.base || map.base == MAP_FAILED)
	{
		printf("failed to map %zu bytes from %s: errno %d\n", size, name, errno);
		UnmapFile(map);
		return false;
	}
#endif

	return true;
}

void Reset(uint32_t* array, size_t arraySize)
{
    for (size_t i = 0; i < arraySize; i++)
    {
        array[i] = (uint32_t)i;
    }
}

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
		default:
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

int32_t main(int32_t argc, char* argv[])
{
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

    int width = 1024;
    int height = 576;
    SDL_Window* window = SDL_CreateWindow("Billion Sort", width, height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);

	FileMapping fileMem = {};
	if (!MapFile("billion.bin", 0, COUNT * sizeof(uint32_t) + SIZE * SIZE * 4ull, fileMem))
	{
		return errno;
	}

    size_t arraySize = COUNT;
    uint32_t* array = (uint32_t*)fileMem.base;

	void* surfaceMem = (void*)(array + arraySize);
    SDL_Surface* surface = SDL_CreateSurfaceFrom(SIZE, SIZE, SDL_PIXELFORMAT_XBGR8888, surfaceMem, SIZE * 4);

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
		default:
			break;
        }

        SDL_SetWindowTitle(window, title);

        SDL_BlitSurfaceScaled(surface, nullptr, SDL_GetWindowSurface(window), nullptr, SDL_SCALEMODE_LINEAR);
        SDL_UpdateWindowSurface(window);
    }

    threadData.state = ThreadState::Exiting;
    // don't bother waiting for the
    SDL_WaitThread(renderThread, nullptr);

    SDL_DestroySurface(surface);

	UnmapFile(fileMem);

    SDL_DestroyWindow(window);

    return 0;
}
