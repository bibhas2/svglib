#include <sstream>
#include <string_view>
#include <stack>
#include <xmllite.h>
#include "SVGUtil.h"
#include "defs.h"
#include "ellipse.h"
#include "g.h"
#include "line.h"
#include "path.h"
#include "rect.h"
#include "circle.h"
#include "text.h"
#include "utils.h"

void SVGGraphicsElement::render_tree(ID2D1DeviceContext* pContext) {
	OutputDebugStringW(L"Rendering element: ");
	OutputDebugStringW(tag_name.c_str());
	OutputDebugStringW(L"\n");

	//Save the old transform
	D2D1_MATRIX_3X2_F oldTransform;

	if (combined_transform) {
		OutputDebugStringW(L"Applying transform\n");
		pContext->GetTransform(&oldTransform);

		auto totalTransform = combined_transform.value() * oldTransform;

		pContext->SetTransform(totalTransform);
	}

	render(pContext);

	//Render all child elements
	for (const auto& child : children) {
		child->render_tree(pContext);
	}

	if (combined_transform) {
		OutputDebugStringW(L"Restoring transform\n");
		pContext->SetTransform(oldTransform);
	}
}

void SVGGraphicsElement::compute_bbox() {
	if (children.empty()) {
		return;
	}

	bbox = children[0]->bbox;

	for (const auto& child : children) {
		bbox.left = child->bbox.left < bbox.left ? child->bbox.left : bbox.left;
		bbox.top = child->bbox.top < bbox.top ? child->bbox.top : bbox.top;
		bbox.right = child->bbox.right > bbox.right ? child->bbox.right : bbox.right;
		bbox.bottom = child->bbox.bottom > bbox.bottom ? child->bbox.bottom : bbox.bottom;
	}
}

bool SVGUtil::init(HWND _wnd)
{
	wnd = _wnd;

	HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &pD2DFactory);
	
	if (!SUCCEEDED(hr)) {
		return false;
	}

	hr = DWriteCreateFactory(
		DWRITE_FACTORY_TYPE_SHARED,
		__uuidof(IDWriteFactory),
		reinterpret_cast<IUnknown**>(&pDWriteFactory)
	);

	if (!SUCCEEDED(hr)) {
		return false;
	}

	// Create Render Target
	RECT rc;

	GetClientRect(_wnd, &rc);

	hr = pD2DFactory->CreateHwndRenderTarget(
		D2D1::RenderTargetProperties(),
		D2D1::HwndRenderTargetProperties(
			_wnd,
			D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top)
		),
		&pRenderTarget
	);

	if (!SUCCEEDED(hr)) {
		return false;
	}

	//Get the device context from the render target
	hr = pRenderTarget->QueryInterface(IID_PPV_ARGS(&pDeviceContext));

	if (!SUCCEEDED(hr)) {
		return false;
	}

	hr = pDeviceContext->CreateSolidColorBrush(
		D2D1::ColorF(D2D1::ColorF::Black),
		&defaultStrokeBrush
	);

	if (!SUCCEEDED(hr)) {
		return false;
	}

	hr = pDeviceContext->CreateSolidColorBrush(
		D2D1::ColorF(D2D1::ColorF::Black),
		&defaultFillBrush
	);

	if (!SUCCEEDED(hr)) {
		return false;
	}

	return true;
}

// Resize the render target when the window size changes
void SVGUtil::resize()
{
	RECT rc;

	GetClientRect(wnd, &rc);
	pRenderTarget->Resize(D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top));
}

// Render the loaded bitmap onto the window
void SVGUtil::render()
{
	pDeviceContext->BeginDraw();
	pDeviceContext->Clear(D2D1::ColorF(D2D1::ColorF::White));

	if (root_element) {
		//Render the SVG element tree
		root_element->render_tree(pDeviceContext);
	}

	pDeviceContext->EndDraw();
}

bool get_element_name(IXmlReader *pReader, std::wstring_view& name) {
	const wchar_t* pwszLocalName = NULL;
	UINT len;

	HRESULT hr = pReader->GetLocalName(&pwszLocalName, &len);

	if (!SUCCEEDED(hr)) {
		return false;
	}

	name = std::wstring_view(pwszLocalName, len);

	return true;
}


//A simple parser for inline CSS styles.
void parse_css_style_string(std::wstring_view styleStr, std::map<std::wstring, std::wstring>& styles) {
	std::vector<std::wstring_view> declarations = split_string(styleStr, L";");

	for (const auto& decl : declarations) {
		size_t colonPos = decl.find(L':');
		if (colonPos != std::wstring_view::npos) {
			std::wstring_view property = decl.substr(0, colonPos);
			std::wstring_view value = decl.substr(colonPos + 1);
			
			ltrim_str(property);
			ltrim_str(value);

			if (!property.empty() && !value.empty()) {
				styles[std::wstring(property)] = std::wstring(value);
			}
		}
	}
}

//Normalizes style from both the "style" attribute and presentation attributes like "fill", "stroke", etc.
void collect_styles(IXmlReader* pReader, std::shared_ptr<SVGGraphicsElement>& new_element) {
	std::wstring_view style_str;

	if (get_attribute(pReader, L"style", style_str)) {
		parse_css_style_string(style_str, new_element->styles);
	}

	const wchar_t* presentation_attributes[] = {
		L"fill", 
		L"fill-opacity", 
		L"stroke-opacity",
		L"stroke-linecap",
		L"stroke-linejoin",
		L"stroke-miterlimit",
		L"stroke", 
		L"stroke-width", 
		L"font-family", 
		L"font-size", 
		L"font-weight", 
		L"font-style"
	};

	for (const wchar_t* attr_name : presentation_attributes) {
		std::wstring_view attr_value;

		if (get_attribute(pReader, attr_name, attr_value)) {
			new_element->styles[std::wstring(attr_name)] = std::wstring(attr_value);
		}
	}
}

bool SVGGraphicsElement::get_style_computed(const std::vector<std::shared_ptr<SVGGraphicsElement>>& parent_stack, const std::wstring& style_name, std::wstring& style_value) {
	auto it = styles.find(style_name);

	if (it != styles.end()) {
		style_value = it->second;

		return true;
	}

	//If the style is not defined on the current element, check the parent elements
	//Loop through parent stack from top to bottom (for a vector: back to front)
	for (auto it = parent_stack.rbegin(); it != parent_stack.rend(); ++it) {
		const auto& parent = *it;
		auto styleIt = parent->styles.find(style_name);

		if (styleIt != parent->styles.end()) {
			style_value = styleIt->second;

			return true;
		}
	}

	return false;
}

void SVGGraphicsElement::get_style_computed(const std::vector<std::shared_ptr<SVGGraphicsElement>>& parent_stack, const std::wstring& style_name, std::wstring& style_value, const std::wstring& default_value) {
	if (!get_style_computed(parent_stack, style_name, style_value)) {
		style_value = default_value;
	}
}

bool apply_viewbox(ID2D1DeviceContext* pContext, std::shared_ptr<SVGGraphicsElement> e, IXmlReader* pReader) {
	//Default viewport width and height
	float width = 300.0f, height = 150.0f;
	float vb_x = 0.0f, vb_y = 0.0f, vb_width = width, vb_height = height;

	//Read width and height attributes
	get_size_attribute(pReader, pContext, L"width", width);
	get_size_attribute(pReader, pContext, L"height", height);

	std::wstring_view viewBoxStr;
	std::wstringstream ws;

	if (get_attribute(pReader, L"viewBox", viewBoxStr)) {
		//Replace comma with spaces
		for (wchar_t ch : viewBoxStr) {
			if (ch == L',') {
				ch = L' ';
			}

			ws << ch;
		}

		//Parse viewBox attribute.
		//For now expect all four values to be present.
		if (!(ws >> vb_x >> vb_y >> vb_width >> vb_height)) {
			return false;
		}

		if (vb_width <= 0.0f || vb_height <= 0.0f) {
			return false;
		}

		//Calculate scale factors
		float scale_x = width / vb_width;
		float scale_y = height / vb_height;
		float scale = scale_x < scale_y ? scale_x : scale_y;

		//Create transform matrix
		D2D1_MATRIX_3X2_F viewboxTransform = D2D1::Matrix3x2F::Translation(-vb_x, -vb_y) *
			D2D1::Matrix3x2F::Scale(scale, scale);
		e->combined_transform = viewboxTransform;

		return true;
	}
	else {
		return false;
	}
}

void SVGGraphicsElement::configure_presentation_style(const std::vector<std::shared_ptr<SVGGraphicsElement>>& parent_stack, ID2D1DeviceContext* pDeviceContext, ID2D1Factory* pD2DFactory) {
	std::wstring style_value;
	HRESULT hr = S_OK;

	//Set brushes
	float stroke_opacity = 1.0f;

	if (get_style_computed(parent_stack, L"stroke-opacity", style_value) &&
		get_size_value(pDeviceContext, style_value, stroke_opacity)) {
	}

	get_style_computed(parent_stack, L"stroke", style_value, L"none");

	if (style_value == L"none") {
		this->stroke_brush = nullptr;
	}
	else {
		float r, g, b, a;

		if (get_rgba(style_value, r, g, b, a)) {
			CComPtr<ID2D1SolidColorBrush> brush;

			hr = pDeviceContext->CreateSolidColorBrush(
				D2D1::ColorF(r, g, b, a * stroke_opacity),
				&brush
			);

			if (SUCCEEDED(hr)) {
				this->stroke_brush = brush;
			}
		}

		D2D1_CAP_STYLE cap_style = D2D1_CAP_STYLE_FLAT;

		if (get_style_computed(parent_stack, L"stroke-linecap", style_value)) {
			if (style_value == L"round") {
				cap_style = D2D1_CAP_STYLE_ROUND;
			}
			else if (style_value == L"square") {
				cap_style = D2D1_CAP_STYLE_SQUARE;
			}
		}

		D2D1_LINE_JOIN line_join = D2D1_LINE_JOIN_MITER;

		if (get_style_computed(parent_stack, L"stroke-linejoin", style_value)) {
			if (style_value == L"bevel") {
				line_join = D2D1_LINE_JOIN_BEVEL;
			}
			else if (style_value == L"round") {
				line_join = D2D1_LINE_JOIN_ROUND;
			}
		}

		float miter_limit = 4.0f;

		if (get_style_computed(parent_stack, L"stroke-miterlimit", style_value) &&
			get_size_value(pDeviceContext, style_value, miter_limit)) {
		}

		D2D1_STROKE_STYLE_PROPERTIES stroke_properties = D2D1::StrokeStyleProperties(
			cap_style,     // Start cap
			cap_style,     // End cap
			D2D1_CAP_STYLE_ROUND,    // Dash cap
			line_join,    // Line join
			miter_limit//,                   // Miter limit
			//D2D1_DASH_STYLE_CUSTOM,  // Dash style
			//0.0f                     // Dash offset
		);

		CComPtr<ID2D1StrokeStyle> ss;

		hr = pD2DFactory->CreateStrokeStyle(
			&stroke_properties,
			nullptr,
			0,
			&ss
		);

		if (SUCCEEDED(hr)) {
			this->stroke_style = ss;
		}
	}

	//Get fill opacity
	//TBD: We read this as a size, even though only % and plain numbers are allowed.
	float fill_opacity = 1.0f;

	if (get_style_computed(parent_stack, L"fill-opacity", style_value) &&
		get_size_value(pDeviceContext, style_value, fill_opacity)) {
	}

	//Get fill
	get_style_computed(parent_stack, L"fill", style_value, L"black");

	if (style_value == L"none") {
		this->fill_brush = nullptr;
	}
	else {
		float r, g, b, a;

		if (get_rgba(style_value, r, g, b, a)) {
			CComPtr<ID2D1SolidColorBrush> brush;

			hr = pDeviceContext->CreateSolidColorBrush(
				D2D1::ColorF(r, g, b, a * fill_opacity),
				&brush
			);
			if (SUCCEEDED(hr)) {
				this->fill_brush = brush;
			}
		}
	}

	//Get stroke width
	float w;

	if (get_style_computed(parent_stack, L"stroke-width", style_value) &&
		get_size_value(pDeviceContext, style_value, w)) {
		this->stroke_width = w;
	}
}

bool SVGUtil::parse(const wchar_t* fileName) {
	CComPtr<IXmlReader> pReader;

	HRESULT hr = ::CreateXmlReader(__uuidof(IXmlReader), (void**)&pReader, NULL);

	if (!SUCCEEDED(hr)) {
		return false;
	}

	CComPtr<IStream> pFileStream;

	hr = SHCreateStreamOnFileEx(fileName, STGM_READ | STGM_SHARE_DENY_WRITE, FILE_ATTRIBUTE_NORMAL, FALSE, NULL, &pFileStream);
	
	if (!SUCCEEDED(hr)) {
		return false;
	}

	hr = pReader->SetInput(pFileStream);

	if (!SUCCEEDED(hr)) {
		return false;
	}

	//Clear any existing root element
	root_element = nullptr;

	std::vector<std::shared_ptr<SVGGraphicsElement>> parent_stack;

	while (true) {
		XmlNodeType nodeType;

		hr = pReader->Read(&nodeType);

		if (hr == S_FALSE) {
			break; //End of file
		}

		if (!SUCCEEDED(hr)) {
			return false;
		}

		if (nodeType == XmlNodeType_Element) {
			//We must call IsEmptyElement before reading
			//any attributes!!!
			bool is_self_closing = pReader->IsEmptyElement();

			std::wstring_view element_name, attr_value;

			if (!get_element_name(pReader, element_name)) {
				return false;
			}

			std::shared_ptr<SVGGraphicsElement> parent_element;
			std::shared_ptr<SVGGraphicsElement> new_element;

			if (!parent_stack.empty()) {
				parent_element = parent_stack.back();
			}

			if (element_name == L"svg") {
				new_element = std::make_shared<SVGGraphicsElement>();

				//Set up default brushes
				new_element->fill_brush = defaultFillBrush;
				new_element->stroke_brush = nullptr;

				if (!root_element)
				{
					//This is the root <svg> element
					root_element = new_element;
				}
				else {
					//Inner svg elements have some special treatment
					float x = 0.0f, y = 0.0f, width = 100.0f, height = 100.0f;

					if (get_size_attribute(pReader, pDeviceContext, L"x", x) &&
						get_size_attribute(pReader, pDeviceContext, L"y", y)) {
						//Position the inner SVG element
						new_element->combined_transform = D2D1::Matrix3x2F::Translation(x, y);
					}
				}

				apply_viewbox(pDeviceContext, new_element, pReader);
			}
			else if (element_name == L"rect") {
				float x, y, width, height, rx, ry;
				if (get_size_attribute(pReader, pDeviceContext, L"x", x) &&
					get_size_attribute(pReader, pDeviceContext, L"y", y) &&
					get_size_attribute(pReader, pDeviceContext, L"width", width) &&
					get_size_attribute(pReader, pDeviceContext, L"height", height)) {
					new_element = std::make_shared<SVGRectElement>();

					new_element->points.push_back(x);
					new_element->points.push_back(y);
					new_element->points.push_back(width);
					new_element->points.push_back(height);
				}

				bool has_rx = get_size_attribute(pReader, pDeviceContext, L"rx", rx);
				bool has_ry = get_size_attribute(pReader, pDeviceContext, L"ry", ry);

				if (has_rx || has_ry) {
					if (!has_rx) {
						rx = ry;
					}
					if (!has_ry) {
						ry = rx;
					}

					new_element->points.push_back(rx);
					new_element->points.push_back(ry);
				}
			}
			else if (element_name == L"circle") {
				float cx, cy, r;

				if (get_size_attribute(pReader, pDeviceContext, L"cx", cx) &&
					get_size_attribute(pReader, pDeviceContext, L"cy", cy) &&
					get_size_attribute(pReader, pDeviceContext, L"r", r)) {
					
					auto circle_element = std::make_shared<SVGCircleElement>();

					circle_element->points.push_back(cx);
					circle_element->points.push_back(cy);
					circle_element->points.push_back(r);

					new_element = circle_element;
				}
			}
			else if (element_name == L"ellipse") {
				float cx, cy, rx, ry;

				if (!get_size_attribute(pReader, pDeviceContext, L"cx", cx)) {
					cx = 0.0f;
				}

				if (!get_size_attribute(pReader, pDeviceContext, L"cy", cy)) {
					cy = 0.0f;
				}

				if (get_size_attribute(pReader, pDeviceContext, L"rx", rx) &&
					get_size_attribute(pReader, pDeviceContext, L"ry", ry)) {
					
					auto ellipse_element = std::make_shared<SVGEllipseElement>();

					ellipse_element->points.push_back(cx);
					ellipse_element->points.push_back(cy);
					ellipse_element->points.push_back(rx);
					ellipse_element->points.push_back(ry);

					new_element = ellipse_element;
				}
			}
			else if (element_name == L"path") {
				if (get_attribute(pReader, L"d", attr_value)) {
					auto path_element = std::make_shared<SVGPathElement>();

					path_element->build_path(pD2DFactory, attr_value);
					new_element = path_element;
				}
			}
			else if (element_name == L"polyline") {
				if (get_attribute(pReader, L"points", attr_value)) {
					auto polyline_element = std::make_shared<SVGPathElement>();
					std::wstring path_data(L"M");
					
					path_data += attr_value;
					
					polyline_element->build_path(pD2DFactory, path_data);

					new_element = polyline_element;
				}
			}
			else if (element_name == L"polygon") {
				if (get_attribute(pReader, L"points", attr_value)) {
					auto polygon_element = std::make_shared<SVGPathElement>();
					std::wstring path_data(L"M");

					path_data.append(attr_value);
					path_data.append(L"Z");

					polygon_element->build_path(pD2DFactory, path_data);

					new_element = polygon_element;
				}
			}

			else if (element_name == L"group" || element_name == L"g") {
				new_element = std::make_shared<SVGGElement>();
			} else if (element_name == L"line") {
				float x1, y1, x2, y2;

				if (get_size_attribute(pReader, pDeviceContext, L"x1", x1) &&
					get_size_attribute(pReader, pDeviceContext, L"y1", y1) &&
					get_size_attribute(pReader, pDeviceContext, L"x2", x2) &&
					get_size_attribute(pReader, pDeviceContext, L"y2", y2)) {
					
					auto line_element = std::make_shared<SVGLineElement>();

					line_element->points.push_back(x1);
					line_element->points.push_back(y1);
					line_element->points.push_back(x2);
					line_element->points.push_back(y2);

					new_element = line_element;
				}
			}
			else if (element_name == L"text") {
				auto text_element = std::make_shared<SVGTextElement>();
				float x = 0, y = 0;

				get_size_attribute(pReader, pDeviceContext, L"x", x);
				get_size_attribute(pReader, pDeviceContext, L"y", y);

				text_element->points.push_back(x);
				text_element->points.push_back(y);

				text_element->pDWriteFactory = pDWriteFactory;

				new_element = text_element;
			}
			else if (element_name == L"defs") {
				new_element = std::make_shared<SVGDefsElement>();
			}
			else if (element_name == L"use") {
				if (get_href_id(pReader, attr_value)) {
					auto it = defs_map.find(std::wstring(attr_value));

					if (it != defs_map.end()) {
						//TBD: Clone the referred element
						new_element = it->second;
					}
				}
			}
			else {
				//Unknown element
				new_element = std::make_shared<SVGGraphicsElement>();
			}

			if (new_element) {
				new_element->tag_name = element_name;

				if (get_attribute(pReader, L"id", attr_value)) {
					std::wstring id(attr_value);

					id_map[id] = new_element;

					if (parent_element && parent_element->tag_name == L"defs") {
						defs_map[id] = new_element;
					}
				}

				//Transform is not inherited
				if (get_attribute(pReader, L"transform", attr_value)) {
					D2D1_MATRIX_3X2_F trans = D2D1::Matrix3x2F::Identity();

					//If the element already has a transform (like inner <svg>), combine them
					if (new_element->combined_transform)
					{
						trans = new_element->combined_transform.value();
					}

					if (build_transform_matrix(attr_value, trans)) {
						new_element->combined_transform = trans;
					}
				}

				collect_styles(pReader, new_element);

				new_element->configure_presentation_style(parent_stack, pDeviceContext, pD2DFactory);

				if (parent_element) {
					//Add the new element to its parent
					OutputDebugStringW(L"Parent::Child: ");
					OutputDebugStringW(parent_element->tag_name.c_str());
					OutputDebugStringW(L"::");
					OutputDebugStringW(element_name.data());
					OutputDebugStringW(L"\n");

					parent_element->children.push_back(new_element);
				}
			}

			//Do not add self closing elements like <circle .../> to the parent stack
			if (!is_self_closing)
			{
				//Push the new element onto the stack
				//This may be null if the element is not supported
				parent_stack.push_back(new_element);
			}
			else {
				//This is the end of the element
				new_element->compute_bbox();
			}
		}
		else if (nodeType == XmlNodeType_Text) {
			if (parent_stack.empty()) {
				return false;
			}

			std::shared_ptr<SVGGraphicsElement> parent_element = parent_stack.back();

			//If the parent is a text then cast it to SVGTextElement
			if (!parent_element || parent_element->tag_name != L"text") {
				continue; //Text nodes are only valid inside <text> elements
			}

			auto text_element = std::dynamic_pointer_cast<SVGTextElement>(parent_element);

			if (!text_element) {
				return false;
			}

			const wchar_t* pwszValue = NULL;
			UINT32 len;

			hr = pReader->GetValue(&pwszValue, &len);

			if (!SUCCEEDED(hr) || pwszValue == nullptr) {
				return false;
			}

			//Collapse white space if needed.
			std::wstring style_value;

			text_element->get_style_computed(parent_stack, L"white-space", style_value, L"normal");

			if (style_value == L"normal") {
				std::wstring_view source(pwszValue, len);

				collapse_whitespace(source, text_element->text_content);
			}
			else {
				text_element->text_content.assign(pwszValue, len);
			}

			hr = pDWriteFactory->CreateTextLayout(
				text_element->text_content.c_str(),           // The string to be laid out
				text_element->text_content.size(),     // The length of the string
				text_element->text_format,    // The initial format (font, size, etc.)
				pDeviceContext->GetSize().width,       // Maximum width of the layout box
				pDeviceContext->GetSize().height,      // Maximum height of the layout box
				&text_element->text_layout    // Output: the resulting IDWriteTextLayout
			);

			if (!SUCCEEDED(hr)) {
				return false;
			}

			// To prevent wrapping and force it to stay on one line:
			text_element->text_layout->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

			//Get the font baseline
			UINT32 lineCount = 0;

			//First get the line count
			hr = text_element->text_layout->GetLineMetrics(nullptr, 0, &lineCount);

			if (!SUCCEEDED(hr) && hr != HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER)) {
				return false;
			}

			if (lineCount == 0) {
				//Nothing there
				return false;
			}

			//Allocate memory for metrics
			std::vector<DWRITE_LINE_METRICS> lineMetrics(lineCount);

			hr = text_element->text_layout->GetLineMetrics(lineMetrics.data(), lineMetrics.size(), &lineCount);

			if (!SUCCEEDED(hr)) {
				return false;
			}

			text_element->baseline = lineMetrics[0].baseline;

			text_element->bbox.left = text_element->points[0];
			text_element->bbox.top = text_element->points[1];
			text_element->bbox.right = text_element->bbox.left + pDeviceContext->GetSize().width;
			text_element->bbox.bottom = text_element->bbox.top + pDeviceContext->GetSize().height;
		}
		else if (nodeType == XmlNodeType_EndElement) {
			std::wstring_view element_name;

			if (!get_element_name(pReader, element_name)) {
				return false;
			}

			OutputDebugStringW(L"End Element: ");
			OutputDebugStringW(element_name.data());
			OutputDebugStringW(L"\n");

			if (!parent_stack.empty()) {
				//Now that all children are added
				//we can calculate bbox.
				parent_stack.back()->compute_bbox();

				parent_stack.pop_back();
			}
		}
	}

	return true;
}

void SVGUtil::redraw()
{
	InvalidateRect(wnd, NULL, FALSE);
}