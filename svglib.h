#pragma once

#include <d2d1_2.h>
#include <wincodec.h>
#include <atlbase.h>
#include <vector>
#include <string>
#include <memory>
#include <optional>
#include <map>
#include <dwrite.h>

//Represents the rendering device and associated Direct2D and DirectWrite objects.
//At this time only Win32 HWND based device is supported.
struct SVGDevice
{
	HWND wnd;
	CComPtr<ID2D1Factory> d2d_factory;
	CComPtr<IDWriteFactory> dwrite_factory;
	CComPtr<ID2D1HwndRenderTarget> render_target;
	CComPtr<ID2D1DeviceContext> device_context;

	//Initializes the SVGDevice with the given window handle. 
	//Various Direct2D and DirectWrite objects are created at this point. 
	//Returns true on success, false on failure.
	bool init(HWND wnd);

	//Resizes the display surface to match the current size of the window. 
	//This should be called in response to WM_SIZE messages.
	void resize();

	//Redraws the window. This should be called after any changes to the 
	//SVGImage that require a redraw. Such as after loading an image or changing 
	//the zoom level.
	void redraw();
};

//Represents an XML element in the SVG file such as <g>, <rect>, <circle>, etc.
struct SVGGraphicsElement {
	std::wstring tag_name;
	float stroke_width = 1.0f;
	CComPtr<ID2D1Brush> fill_brush;
	CComPtr<ID2D1Brush> stroke_brush;
	CComPtr<ID2D1StrokeStyle> stroke_style;
	std::vector<std::shared_ptr<SVGGraphicsElement>> children;
	std::optional<D2D1_MATRIX_3X2_F> combined_transform;
	std::vector<float> points;
	std::map<std::wstring, std::wstring> styles;
	std::map<std::wstring, std::wstring> attributes;
	D2D1_RECT_F bbox{};

	SVGGraphicsElement() = default;
	SVGGraphicsElement(const SVGGraphicsElement& that);

	virtual void render_tree(const SVGDevice& device) const;
	virtual void render(const SVGDevice& device) const {};
	virtual void create_presentation_assets(const std::vector<std::shared_ptr<SVGGraphicsElement>>& parent_stack, const std::map<std::wstring, std::shared_ptr<SVGGraphicsElement>>& id_map, const SVGDevice& device);
	virtual void compute_bbox();
	virtual ~SVGGraphicsElement() = default;
	//Creates a deep copy of the element. Used for <use> elements.
	virtual std::shared_ptr<SVGGraphicsElement> clone() const;

	bool get_style_computed(const std::vector<std::shared_ptr<SVGGraphicsElement>>& parent_stack, const std::wstring& style_name, std::wstring& style_value);
	void get_style_computed(const std::vector<std::shared_ptr<SVGGraphicsElement>>& parent_stack, const std::wstring& style_name, std::wstring& style_value, const std::wstring& default_value);
	bool get_attribute_in_references(const std::vector<std::shared_ptr<SVGGraphicsElement>>& chain, const std::wstring& attr_name, std::wstring& attr_value) const;
};

//Represents a loaded SVG image. 
//Contains the root element of the SVG file and any presentation assets created during 
//loading such as brushes and stroke styles.
struct SVGImage
{
	std::shared_ptr<SVGGraphicsElement> root_element;

	void clear();
};

struct SVG
{
	//Loads an SVG file and populates the SVGImage structure. Returns true on success, false on failure.
	//The must later be rendered using the same device that was used to load it. This is because some presentation assets 
	//like brushes are created for that device during loading and are stored in the SVGImage structure.
	//The device must be initialized before calling this function.
	//If an image was already loaded in the SVGImage structure, it will be cleared before loading the new one.
	static bool load(const wchar_t* file_name, const SVGDevice& device, SVGImage& image);

	//Renders the SVGImage on the given device. The image must have been loaded using the same device.
	static void render(const SVGDevice& device, const SVGImage& image);

	//Renders the SVGImage on the given device with the specified position and scale. The image must be loaded using the same device.
	static void render(const SVGDevice& device, const SVGImage& image, float x, float y, float scale);
};

