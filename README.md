## 2D Sorting Visualizer

This is sorting visualizer that sorts a billion numbers. It visualizes this by figuring out
what position a given value would be at if the array were sorted, and maps the x and y to
the red and blue of the image. It does this with two worker threads, one to shuffle and sort
the array, and one to map the values to colored pixels on the screen.

It uses SDL for platform abstraction and graphics, and the STL (`std::shuffle`/`std::sort`)
for shuffling and sorting the list.

It requires about 8 gigabytes of memory, 4 for the array and 4 for the framebuffer. I tried
using the framebuffer directly as the array, but there was no pleasant way to visualize the
values and still have a billion individual ones.

I have an Intel Core i7-13700K and 32GB of RAM, and it takes ~249s in debug mode and 89.44s
in release to complete the sorting, using MSVC's `std::sort` on Windows 11 10.0.26200.5641

### Building

Just run xmake:
```shell
xmake build
```

