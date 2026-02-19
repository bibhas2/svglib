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

	virtual void render_tree(ID2D1DeviceContext* pContext);
	virtual void render(ID2D1DeviceContext* pContext) {};
	virtual void configure_presentation_style(const std::vector<std::shared_ptr<SVGGraphicsElement>>& parent_stack, ID2D1DeviceContext* pDeviceContext, ID2D1Factory* pD2DFactory);
	virtual void compute_bbox();
	bool get_style_computed(const std::vector<std::shared_ptr<SVGGraphicsElement>>& parent_stack, const std::wstring& style_name, std::wstring& style_value);
	void get_style_computed(const std::vector<std::shared_ptr<SVGGraphicsElement>>& parent_stack, const std::wstring& style_name, std::wstring& style_value, const std::wstring& default_value);
};












struct SVGUtil
{
	HWND wnd;
	CComPtr<ID2D1Factory> pD2DFactory;
	CComPtr<IDWriteFactory> pDWriteFactory;
	CComPtr<ID2D1HwndRenderTarget> pRenderTarget;
	CComPtr<ID2D1DeviceContext> pDeviceContext;
	CComPtr<ID2D1SolidColorBrush> defaultFillBrush;
	CComPtr<ID2D1SolidColorBrush> defaultStrokeBrush;
	CComPtr<IDWriteTextFormat> defaultTextFormat;
	std::shared_ptr<SVGGraphicsElement> root_element;
	std::map<std::wstring, std::shared_ptr<SVGGraphicsElement>> id_map;
	std::map<std::wstring, std::shared_ptr<SVGGraphicsElement>> defs_map;


	bool init(HWND wnd);
	void resize();
	void render();
	void redraw();
	bool parse(const wchar_t* fileName);
};

