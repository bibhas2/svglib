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

struct SVGImage
{
	std::shared_ptr<SVGGraphicsElement> root_element;

	void clear();
};

struct SVG
{
	static bool parse(const wchar_t* file_name, const SVGDevice& device, SVGImage& image);
	static void render(const SVGDevice& device, const SVGImage& image);
};

