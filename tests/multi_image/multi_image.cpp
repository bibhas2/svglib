//An application that tests rendering multiple SVG images in the same window 
// using the same device context.
#include "framework.h"
#include "multi_image.h"
#include "../../../mgui/include/mgui.h"
#include "../../svglib.h"

#pragma comment(lib, "D3D11.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "xmllite.lib")
#pragma comment(lib, "dwrite.lib")
//Needed by mgui
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comdlg32.lib")

class MainWindow : public CFrame {
    SVGDevice device;
    SVGImage images[4];
	const wchar_t* image_files[4] = {L"peacock.svg", L"butterfly.svg", L"bird.svg", L"man.svg"};
    float scales[4] = {0.25f, 0.15f, 0.5f, 0.5f };
public:

    void create() {
        CFrame::create(L"SVG Viewer", 800, 600, IDC_MULTIIMAGE);

        device.init(getWindow());

        for (int i = 0; i < 4; i++) {
            if (!SVG::load(image_files[i], device, images[i])) {
                errorBox((std::wstring(L"Failed to open or parse the SVG file: ") + image_files[i]).c_str());
            }
		}
    }
    void onClose() override {
        CWindow::stop();
    }
    void onCommand(int id, int type, CWindow* source) override {
        if (id == IDM_EXIT) {
            onClose();
        }
    }

    void renderImages() {
        float x = 20.0f;

		SVG::clear(device);

        for (int i = 0; i < 4; i++) {
            SVG::render(device, images[i], x, 20, scales[i]);

            x += 200.0f;
        }
    }

    bool handleEvent(UINT message, WPARAM wParam, LPARAM lParam) {

        switch (message) {
        case WM_PAINT:
            PAINTSTRUCT ps;
            //We must call BeginPaint and EndPaint to validate the
            //invalidated region, or else we will get continuous
            //WM_PAINT messages.
            BeginPaint(m_wnd, &ps);
            renderImages();
            EndPaint(m_wnd, &ps);
            break;
        case WM_SIZE:
            device.resize();
            break;
        case WM_ERASEBKGND:
            //Handle background erase to avoid flickering 
            //during resizing and move
            break;
        default:
            return CWindow::handleEvent(message, wParam, lParam);
        }

        return true;
    }
};

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    CWindow::init(hInstance, IDC_MULTIIMAGE);

    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    if (!SUCCEEDED(hr)) {
        return FALSE;
    }

    MainWindow mainWin;

    mainWin.create();
    mainWin.show();

    CWindow::loop();

    CoUninitialize();

    return 0;
}
