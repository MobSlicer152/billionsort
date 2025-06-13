## 2D Sorting Visualizer

https://github.com/user-attachments/assets/1f9537b7-5dde-4f53-85da-e397c2542ae6

This is a sorting visualizer that sorts a billion numbers. It visualizes this by figuring out
what position a given value would be at if the array were sorted, and maps the x and y to
the red and blue of the image. It does this with two worker threads, one to shuffle and sort
the array, and one to map the values to colored pixels on the screen.

It uses SDL for platform abstraction and graphics, and the STL (`std::shuffle`/`std::sort`)
for shuffling and sorting the list.

It uses a memory mapped file to store the data, which is 8GB in total. 4GB for the actual numbers,
and another 4 for the framebuffer. If you have enough memory to hold the entire file, it probably
doesn't matter that it's a file.

I have an Intel Core i7-13700K and 32GB of RAM, and it takes ~249s in debug mode and 89.44s
in release to complete the sorting, using MSVC's `std::sort` on Windows 11 10.0.26200.5641.
This was before the memory mapped version.

### Building

Just run xmake:
```shell
xmake build
```

