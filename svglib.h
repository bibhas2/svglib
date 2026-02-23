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

struct SVGDevice
{
	HWND wnd;
	CComPtr<ID2D1Factory> d2d_factory;
	CComPtr<IDWriteFactory> dwrite_factory;
	CComPtr<ID2D1HwndRenderTarget> render_target;
	CComPtr<ID2D1DeviceContext> device_context;

	bool init(HWND wnd);
	void resize();
	void redraw();
};

struct SVGGraphicsElement {
	std::wstring tag_name;
	float stroke_width = 1.0f;
	CComPtr<ID2D1SolidColorBrush> fill_brush;
	CComPtr<ID2D1SolidColorBrush> stroke_brush;
	CComPtr<ID2D1StrokeStyle> stroke_style;
	std::vector<std::shared_ptr<SVGGraphicsElement>> children;
	std::optional<D2D1_MATRIX_3X2_F> combined_transform;
	std::vector<float> points;
	std::map<std::wstring, std::wstring> styles;
	D2D1_RECT_F bbox{};

	SVGGraphicsElement() = default;
	SVGGraphicsElement(const SVGGraphicsElement& that);

	virtual void render_tree(const SVGDevice& device) const;
	virtual void render(const SVGDevice& device) const {};
	virtual void create_presentation_assets(const std::vector<std::shared_ptr<SVGGraphicsElement>>& parent_stack, const SVGDevice& device);
	virtual void compute_bbox();
	virtual ~SVGGraphicsElement() = default;
	//Creates a deep copy of the element. Used for <use> elements.
	virtual std::shared_ptr<SVGGraphicsElement> clone() const;

	bool get_style_computed(const std::vector<std::shared_ptr<SVGGraphicsElement>>& parent_stack, const std::wstring& style_name, std::wstring& style_value);
	void get_style_computed(const std::vector<std::shared_ptr<SVGGraphicsElement>>& parent_stack, const std::wstring& style_name, std::wstring& style_value, const std::wstring& default_value);
};

struct SVGImage
{
	std::shared_ptr<SVGGraphicsElement> root_element;
	std::map<std::wstring, std::shared_ptr<SVGGraphicsElement>> id_map;
	std::map<std::wstring, std::shared_ptr<SVGGraphicsElement>> defs_map;

	void clear();
};

struct SVG
{
	static bool parse(const wchar_t* file_name, const SVGDevice& device, SVGImage& image);
	static void render(const SVGDevice& device, const SVGImage& image);
};

