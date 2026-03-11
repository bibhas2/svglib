## What is It?
``svglib`` is a SVG file parser and renderer library for Windows. It uses Direct2D for GPU assisted rendering and XMLLite for XML parsing. They are core components of Windows. You don't need to download any external libraries to compile and distribute applications that use ``svglib``.

## Why?

This is meant for Win32 applications and games to easily display SVG images. Just enough of the SVG spec is covered for the library to be useful in most situations.

## Build Instructions

You will need Visual Studio Community Edition with C++ language support.

Clone this repo.

```
git clone git@github.com:bibhas2/svglib.git
```

Clone ``mgui``. This is used by some of the test applications only and not by ``svglib``.

```
git clone git@github.com:bibhas2/mgui.git
```

Open the solution ``svglib/svglib.sln`` in Visual Studio Community Edition and build the solution.

## Using svglib

Include the header file ``svglib.h``. Link with the static library ``svglib.lib``. 

You will also need to link with Direct2D, Direct3D, DirectWrite etc. This is best done by adding these lines to one of the C++ files of your application.

```cpp
#pragma comment(lib, "D3D11.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "xmllite.lib")
#pragma comment(lib, "dwrite.lib")
```

Finally, enable support for C++ 17 for your project.

## Minimal Code Example

For each HWND to be used to display a SVG, you need a ``SVGDevice``.

```cpp
SVGDevice device;
HWND m_wnd = ...;

//Setup the device
bool status = device.init(m_wnd);

//Check status
```

Each SVG image is reprsented by a ``SVGImage``. We can load an image like this.

```cpp
SVGImage image;

if (SVG::load(L"file.svg", device, image)) {
    //Force a WM_PAINT event
	device.redraw();
}
else {
	//Show error message
}
```

Render the image from the ``WM_PAINT`` handler of the window.

```cpp
bool handle_event(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_PAINT:
        PAINTSTRUCT ps;

        //Call BeginPaint and EndPaint, or else we will get continuous
        //WM_PAINT messages.
        BeginPaint(m_wnd, &ps);

        //Render the SVG
        SVG::render(device, image);

        EndPaint(m_wnd, &ps);
        break;
    case WM_SIZE:
        //Resize the device area
        device.resize();
        break;
    case WM_ERASEBKGND:
        //Handle background erase to avoid flickering 
        //during resizing and move
        break;
    default:
        //...
    }

    return true;
}
```

## Technical Notes

Direct2D and DirectWrite pretty much map the SVG spec 1:1. This made writing ``svglib`` fairly trivial. The only exception is ``textPath``. I have no plans to support ``textPath``.

``clipPath`` is also not supported at this time. But, a support is planned in the future.

## Security and Vulnerability
Always make sure that the ``SVGDevice`` was initialized properly. Using a half initialized device can cause unpredicatble results. The library does not validate the device when it's used later to load and render images.

The SVG image files must be trusted. Please note:

- No protection is provided by the library for depth limit for recursive XML structures. 
- No overflow check is done for the coordinate values in the SVG file. 