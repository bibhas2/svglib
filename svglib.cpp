#include <sstream>
#include <string_view>
#include <stack>
#include <xmllite.h>
#include "svglib.h"
#include "defs.h"
#include "ellipse.h"
#include "g.h"
#include "line.h"
#include "path.h"
#include "rect.h"
#include "circle.h"
#include "text.h"
#include "use.h"
#include "gradient.h"
#include "utils.h"

#ifdef DEBUG
#define DEBUG_OUT(x) {std::wstringstream ws; ws << x << std::endl; OutputDebugStringW(ws.str().c_str());}
#else
#define DEBUG_OUT(x)
#endif

void SVGImage::clear() {
	root_element = nullptr;
}

SVGGraphicsElement::SVGGraphicsElement(const SVGGraphicsElement& that) 
    : tag_name(that.tag_name),
	points(that.points),
	children(that.children),
	stroke_width(that.stroke_width),
	fill_brush(that.fill_brush),
	stroke_brush(that.stroke_brush),
	stroke_style(that.stroke_style),
	combined_transform(that.combined_transform),
	styles(that.styles),
	bbox(that.bbox) {

}

std::shared_ptr<SVGGraphicsElement> SVGGraphicsElement::clone() const { 
	return std::make_shared<SVGGraphicsElement>(*this); 
}

void SVGGraphicsElement::render_tree(const SVGDevice& device) const {
	DEBUG_OUT(L"Rendering element: " << tag_name);

	//Save the old transform
	D2D1_MATRIX_3X2_F oldTransform;

	if (combined_transform) {
		DEBUG_OUT(L"Applying transform");
		device.device_context->GetTransform(&oldTransform);

		auto totalTransform = combined_transform.value() * oldTransform;

		device.device_context->SetTransform(totalTransform);
	}

	render(device);

	//Render all child elements
	for (const auto& child : children) {
		child->render_tree(device);
	}

	if (combined_transform) {
		DEBUG_OUT(L"Restoring transform");
		device.device_context->SetTransform(oldTransform);
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

bool get_element_name(IXmlReader *pReader, std::wstring_view& name) {
	const wchar_t* local_name = nullptr;
	UINT len;

	HRESULT hr = pReader->GetLocalName(&local_name, &len);

	if (!SUCCEEDED(hr) || local_name == nullptr) {
		return false;
	}

	name = std::wstring_view(local_name, len);

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
void collect_styles(IXmlReader* pReader, SVGImage& image, std::shared_ptr<SVGGraphicsElement>& new_element) {
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

bool apply_viewbox(ID2D1DeviceContext* device_context, std::shared_ptr<SVGGraphicsElement> e, IXmlReader* pReader) {
	//Default viewport width and height
	float width = device_context->GetSize().width, height = device_context->GetSize().height;
	float vb_x = 0.0f, vb_y = 0.0f, vb_width = width, vb_height = height;

	//Read width and height attributes
	get_size_attribute(pReader, device_context, L"width", width);
	get_size_attribute(pReader, device_context, L"height", height);

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

void SVGGraphicsElement::create_presentation_assets(const std::vector<std::shared_ptr<SVGGraphicsElement>>& parent_stack, const std::map<std::wstring, std::shared_ptr<SVGGraphicsElement>>& id_map, const SVGDevice& device) {
	std::wstring style_value;
	HRESULT hr = S_OK;

	//Set brushes
	float stroke_opacity = 1.0f;

	if (get_style_computed(parent_stack, L"stroke-opacity", style_value) &&
		get_size_value(device.device_context, style_value, stroke_opacity)) {
	}

	get_style_computed(parent_stack, L"stroke", style_value, L"none");

	if (style_value == L"none") {
		this->stroke_brush = nullptr;
	}
	else {
		std::wstring_view gradient_ref_id;
		float r, g, b, a;

		if (get_css_color(style_value, r, g, b, a)) {
			CComPtr<ID2D1SolidColorBrush> brush;

			hr = device.device_context->CreateSolidColorBrush(
				D2D1::ColorF(r, g, b, a * stroke_opacity),
				&brush
			);

			if (SUCCEEDED(hr)) {
				this->stroke_brush = brush;
			}
		}
		else if (get_href_id(style_value, gradient_ref_id)) {
			auto it = id_map.find(std::wstring(gradient_ref_id));

			if (it != id_map.end()) {
				auto linear_gradient = std::dynamic_pointer_cast<SVGLinearGradientElement>(it->second);
				auto radial_gradient = std::dynamic_pointer_cast<SVGRadialGradientElement>(it->second);

				if (linear_gradient) {
					this->stroke_brush = create_linear_gradient_brush(device, *linear_gradient, *this);
				} 
				else if (radial_gradient) {
					this->stroke_brush = create_radial_gradient_brush(device, *radial_gradient, *this);
				}
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
			get_size_value(device.device_context, style_value, miter_limit)) {
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

		hr = device.d2d_factory->CreateStrokeStyle(
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
		get_size_value(device.device_context, style_value, fill_opacity)) {
	}

	//Get fill
	get_style_computed(parent_stack, L"fill", style_value, L"black");
	std::wstring_view gradient_ref_id;
	float r, g, b, a;

	//href and color can both start with #.
	//Try to parse as color first, and if it fails, try to parse as reference.

	if (style_value == L"none") {
		this->fill_brush = nullptr;
	} 
	else if (get_css_color(style_value, r, g, b, a)) {
		CComPtr<ID2D1SolidColorBrush> brush;

		hr = device.device_context->CreateSolidColorBrush(
			D2D1::ColorF(r, g, b, a * fill_opacity),
			&brush
		);
		if (SUCCEEDED(hr)) {
			this->fill_brush = brush;
		}
	}
	else if (get_href_id(style_value, gradient_ref_id)) {
		auto it = id_map.find(std::wstring(gradient_ref_id));

		if (it != id_map.end()) {
			auto linear_gradient = std::dynamic_pointer_cast<SVGLinearGradientElement>(it->second);
			auto radial_gradient = std::dynamic_pointer_cast<SVGRadialGradientElement>(it->second);

			if (linear_gradient) {
				this->fill_brush = create_linear_gradient_brush(device, *linear_gradient, *this);
			} else if (radial_gradient) {
				this->fill_brush = create_radial_gradient_brush(device, *radial_gradient, *this);
			}
		}
	}

	//Get stroke width
	float w;

	if (get_style_computed(parent_stack, L"stroke-width", style_value) &&
		get_size_value(device.device_context, style_value, w)) {
		this->stroke_width = w;
	}
}

void resolve_href(const std::shared_ptr<SVGGraphicsElement>& element, 
	std::vector<std::shared_ptr<SVGGraphicsElement>>& parent_stack,
	const std::map<std::wstring, std::shared_ptr<SVGGraphicsElement>>& id_map, 
	const std::map<std::wstring, std::shared_ptr<SVGGraphicsElement>>& defs_map,
	const SVGDevice& device) {

	if (!element) {
		return; //Branch is skipped
	}

	element->create_presentation_assets(parent_stack, id_map, device);

	parent_stack.push_back(element);

	for (size_t i = 0; i < element->children.size(); ++i) {
		auto& child = element->children[i];

		if (child->tag_name == L"use") {
			//Cast to use element to get the reference
			auto use_element = std::dynamic_pointer_cast<SVGUseElement>(child);
			
			if (use_element) {
				std::shared_ptr<SVGGraphicsElement> referenced_element;
				auto id_it = id_map.find(use_element->href_id);
				
				if (id_it != id_map.end()) {
					referenced_element = id_it->second;
				}

				if (referenced_element) {
					//Clone the referenced element and replace the <use> element with it
					element->children[i] = referenced_element->clone();
				}
				else {
					continue; //skip branch if reference is not found
				}
			}
		}
		else if (child->tag_name == L"linearGradient") {
			//Cast to linear gradient element to get the id
			auto gradient_element = std::dynamic_pointer_cast<SVGLinearGradientElement>(child);

			if (gradient_element && !gradient_element->href_id.empty()) {
				std::shared_ptr<SVGGraphicsElement> referenced_element;
				auto id_it = id_map.find(gradient_element->href_id);

				if (id_it != id_map.end()) {
					referenced_element = std::dynamic_pointer_cast<SVGLinearGradientElement>(id_it->second);

					if (referenced_element) {
						//TBD: We should only copy from template attributes not defined on the gradient element itself. 
						gradient_element->points = referenced_element->points;
					}
				}
			}
		}
		else if (child->tag_name == L"radialGradient") {
			//Cast to radial gradient element to get the id
			auto gradient_element = std::dynamic_pointer_cast<SVGRadialGradientElement>(child);

			if (gradient_element && !gradient_element->href_id.empty()) {
				std::shared_ptr<SVGGraphicsElement> referenced_element;
				auto id_it = id_map.find(gradient_element->href_id);

				if (id_it != id_map.end()) {
					referenced_element = std::dynamic_pointer_cast<SVGRadialGradientElement>(id_it->second);

					if (referenced_element) {
						//TBD: We should only copy from template attributes not defined on the gradient element itself. 
						gradient_element->points = referenced_element->points;
					}
				}
			}
		}


		resolve_href(element->children[i], parent_stack, id_map, defs_map, device);
	}

	parent_stack.pop_back();
}

bool SVG::parse(const wchar_t* file_name, const SVGDevice& device, SVGImage& image) {
	CComPtr<IXmlReader> xml_reader;

	HRESULT hr = ::CreateXmlReader(__uuidof(IXmlReader), (void**)&xml_reader, NULL);

	if (!SUCCEEDED(hr)) {
		return false;
	}

	CComPtr<IStream> file_stream;

	hr = ::SHCreateStreamOnFileEx(file_name, 
		STGM_READ | STGM_SHARE_DENY_WRITE, FILE_ATTRIBUTE_NORMAL, FALSE, NULL, 
		&file_stream);
	
	if (!SUCCEEDED(hr)) {
		return false;
	}

	hr = xml_reader->SetInput(file_stream);

	if (!SUCCEEDED(hr)) {
		return false;
	}

	//Clear previous image
	image.clear();

	std::vector<std::shared_ptr<SVGGraphicsElement>> parent_stack;
	std::map<std::wstring, std::shared_ptr<SVGGraphicsElement>> id_map;
	std::map<std::wstring, std::shared_ptr<SVGGraphicsElement>> defs_map;

	while (true) {
		XmlNodeType node_type;

		hr = xml_reader->Read(&node_type);

		if (hr == S_FALSE) {
			break; //End of file
		}

		if (!SUCCEEDED(hr)) {
			return false;
		}

		if (node_type == XmlNodeType_Element) {
			//We must call IsEmptyElement before reading
			//any attributes!!!
			bool is_self_closing = xml_reader->IsEmptyElement();

			std::wstring_view element_name, attr_value;

			if (!get_element_name(xml_reader, element_name)) {
				return false;
			}

			std::shared_ptr<SVGGraphicsElement> parent_element;
			std::shared_ptr<SVGGraphicsElement> new_element;

			if (!parent_stack.empty()) {
				parent_element = parent_stack.back();
			}

			if (element_name == L"svg") {
				new_element = std::make_shared<SVGGraphicsElement>();

				if (!image.root_element)
				{
					//This is the root <svg> element
					image.root_element = new_element;
				}
				else {
					//Inner svg elements have some special treatment
					float x = 0.0f, y = 0.0f, width = 100.0f, height = 100.0f;

					if (get_size_attribute(xml_reader, device.device_context, L"x", x) &&
						get_size_attribute(xml_reader, device.device_context, L"y", y)) {
						//Position the inner SVG element
						new_element->combined_transform = D2D1::Matrix3x2F::Translation(x, y);
					}
				}

				apply_viewbox(device.device_context, new_element, xml_reader);
			}
			else if (element_name == L"rect") {
				float x = 0.0f, y = 0.0f, width = 0.0f, height = 0.0f, rx = 0.0f, ry = 0.0f;

				get_size_attribute(xml_reader, device.device_context, L"x", x);
				get_size_attribute(xml_reader, device.device_context, L"y", y);
				get_size_attribute(xml_reader, device.device_context, L"width", width);
				get_size_attribute(xml_reader, device.device_context, L"height", height);

				new_element = std::make_shared<SVGRectElement>();

				new_element->points.push_back(x);
				new_element->points.push_back(y);
				new_element->points.push_back(width);
				new_element->points.push_back(height);

				bool has_rx = get_size_attribute(xml_reader, device.device_context, L"rx", rx);
				bool has_ry = get_size_attribute(xml_reader, device.device_context, L"ry", ry);

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
				float cx = 0.0f, cy = 0.0f, r = 0.0f;

				get_size_attribute(xml_reader, device.device_context, L"cx", cx);
				get_size_attribute(xml_reader, device.device_context, L"cy", cy);
				get_size_attribute(xml_reader, device.device_context, L"r", r);

				auto circle_element = std::make_shared<SVGCircleElement>();

				circle_element->points.push_back(cx);
				circle_element->points.push_back(cy);
				circle_element->points.push_back(r);

				new_element = circle_element;
			}
			else if (element_name == L"ellipse") {
				float cx, cy, rx, ry;

				if (!get_size_attribute(xml_reader, device.device_context, L"cx", cx)) {
					cx = 0.0f;
				}

				if (!get_size_attribute(xml_reader, device.device_context, L"cy", cy)) {
					cy = 0.0f;
				}

				if (get_size_attribute(xml_reader, device.device_context, L"rx", rx) &&
					get_size_attribute(xml_reader, device.device_context, L"ry", ry)) {
					
					auto ellipse_element = std::make_shared<SVGEllipseElement>();

					ellipse_element->points.push_back(cx);
					ellipse_element->points.push_back(cy);
					ellipse_element->points.push_back(rx);
					ellipse_element->points.push_back(ry);

					new_element = ellipse_element;
				}
			}
			else if (element_name == L"path") {
				if (get_attribute(xml_reader, L"d", attr_value)) {
					auto path_element = std::make_shared<SVGPathElement>();

					path_element->build_path(device.d2d_factory, attr_value);
					new_element = path_element;
				}
			}
			else if (element_name == L"polyline") {
				if (get_attribute(xml_reader, L"points", attr_value)) {
					auto polyline_element = std::make_shared<SVGPathElement>();
					std::wstring path_data(L"M");
					
					path_data += attr_value;
					
					polyline_element->build_path(device.d2d_factory, path_data);

					new_element = polyline_element;
				}
			}
			else if (element_name == L"polygon") {
				if (get_attribute(xml_reader, L"points", attr_value)) {
					auto polygon_element = std::make_shared<SVGPathElement>();
					std::wstring path_data(L"M");

					path_data.append(attr_value);
					path_data.append(L"Z");

					polygon_element->build_path(device.d2d_factory, path_data);

					new_element = polygon_element;
				}
			}

			else if (element_name == L"group" || element_name == L"g") {
				new_element = std::make_shared<SVGGElement>();
			} else if (element_name == L"line") {
				float x1, y1, x2, y2;

				if (get_size_attribute(xml_reader, device.device_context, L"x1", x1) &&
					get_size_attribute(xml_reader, device.device_context, L"y1", y1) &&
					get_size_attribute(xml_reader, device.device_context, L"x2", x2) &&
					get_size_attribute(xml_reader, device.device_context, L"y2", y2)) {
					
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

				get_size_attribute(xml_reader, device.device_context, L"x", x);
				get_size_attribute(xml_reader, device.device_context, L"y", y);

				text_element->points.push_back(x);
				text_element->points.push_back(y);

				text_element->dwrite_factory = device.dwrite_factory;

				new_element = text_element;
			}
			else if (element_name == L"defs") {
				new_element = std::make_shared<SVGDefsElement>();
			}
			else if (element_name == L"use") {
				auto use_element = std::make_shared<SVGUseElement>();

				if (get_href_id(xml_reader, attr_value)) {
					use_element->href_id = attr_value;
					new_element = use_element;
				}
			}
			else if (element_name == L"linearGradient") {
				auto linear_gradient = std::make_shared<SVGLinearGradientElement>();

				float x1 = 0, y1 = 0, x2 = 1.0, y2 = 0;

				get_size_attribute(xml_reader, device.device_context, L"x1", x1);
				get_size_attribute(xml_reader, device.device_context, L"y1", y1);
				get_size_attribute(xml_reader, device.device_context, L"x2", x2);
				get_size_attribute(xml_reader, device.device_context, L"y2", y2);

				linear_gradient->points.push_back(x1);
				linear_gradient->points.push_back(y1);
				linear_gradient->points.push_back(x2);
				linear_gradient->points.push_back(y2);

				if (get_attribute(xml_reader, L"gradientUnits", attr_value)) {
					linear_gradient->gradient_units = attr_value;
				}

				if (get_href_id(xml_reader, attr_value)) {
					linear_gradient->href_id = attr_value;
				}

				new_element = linear_gradient;
			}
			else if (element_name == L"radialGradient") {
				auto radial_gradient = std::make_shared<SVGRadialGradientElement>();
				float cx = 0.5f, cy = 0.5f, r = 0.5f, fx = 0.0, fy = 0.0, fr = 0.0;

				get_size_attribute(xml_reader, device.device_context, L"cx", cx);
				get_size_attribute(xml_reader, device.device_context, L"cy", cy);
				get_size_attribute(xml_reader, device.device_context, L"r", r);

				if (!get_size_attribute(xml_reader, device.device_context, L"fx", fx)) {
					fx = cx;
				}
				if (!get_size_attribute(xml_reader, device.device_context, L"fy", fy)) {
					fy = cy;
				}
				if (!get_size_attribute(xml_reader, device.device_context, L"fr", fr)) {
					fr = 0.0;
				}

				radial_gradient->points.push_back(cx);
				radial_gradient->points.push_back(cy);
				radial_gradient->points.push_back(r);
				radial_gradient->points.push_back(fx);
				radial_gradient->points.push_back(fy);
				radial_gradient->points.push_back(fr);

				if (get_attribute(xml_reader, L"gradientUnits", attr_value)) {
					radial_gradient->gradient_units = attr_value;
				}

				if (get_href_id(xml_reader, attr_value)) {
					radial_gradient->href_id = attr_value;
				}

				new_element = radial_gradient;
			}
			else if (element_name == L"stop") {
				auto stop_element = std::make_shared<SVGStopElement>();

				float offset = 0;
				D2D1::ColorF stop_color = D2D1::ColorF(D2D1::ColorF::Black);
				float stop_opacity = 1.0f;

				get_size_attribute(xml_reader, device.device_context, L"offset", offset);

				if (get_attribute(xml_reader, L"stop-color", attr_value)) {
					get_css_color(attr_value, stop_color.r, stop_color.g, stop_color.b, stop_color.a);
				}

				if (get_size_attribute(xml_reader, device.device_context, L"stop-opacity", stop_opacity)) {
					stop_color.a = 255 * stop_opacity;
				}

				stop_element->offset = offset;
				stop_element->stop_color = stop_color;
				stop_element->stop_opacity = stop_opacity;

				new_element = stop_element;
			}
			else {
				//Unknown element
				new_element = std::make_shared<SVGGraphicsElement>();
			}

			if (new_element) {
				new_element->tag_name = element_name;

				if (get_attribute(xml_reader, L"id", attr_value)) {
					std::wstring id(attr_value);

					id_map[id] = new_element;

					if (parent_element && parent_element->tag_name == L"defs") {
						defs_map[id] = new_element;
					}
				}

				//Transform is not inherited
				if (get_attribute(xml_reader, L"transform", attr_value)) {
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

				collect_styles(xml_reader, image, new_element);

				if (parent_element) {
					//Add the new element to its parent
					DEBUG_OUT(L"Parent::Child: " << parent_element->tag_name << L"::" << element_name);

					parent_element->children.push_back(new_element);
				}
			}

			if (is_self_closing) {
				//This is the end of the element

				if (new_element) {
					new_element->compute_bbox();
				}
			}
			else {
				//Push the new element onto the stack
				//This may be null if the element is not supported
				parent_stack.push_back(new_element);
			}
		}
		else if (node_type == XmlNodeType_Text) {
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

			hr = xml_reader->GetValue(&pwszValue, &len);

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
		}
		else if (node_type == XmlNodeType_EndElement) {
			std::wstring_view element_name;

			if (!get_element_name(xml_reader, element_name)) {
				return false;
			}

			DEBUG_OUT(L"End Element: " << element_name);

			if (!parent_stack.empty()) {
				//Now that all children are added
				//we can calculate bbox.
				auto element = parent_stack.back();

				parent_stack.pop_back();

				if (element) {
					element->compute_bbox();
				}
			}
		}

	}

	//Do a second pass to resolve references in styles (like fill="url(#gradient1)")
	parent_stack.clear();
	resolve_href(image.root_element, parent_stack, id_map, defs_map, device);

	return true;
}

// Render the loaded bitmap onto the window
void SVG::render(const SVGDevice& device, const SVGImage& image)
{
	device.device_context->BeginDraw();
	device.device_context->Clear(D2D1::ColorF(D2D1::ColorF::White));

	if (image.root_element) {
		//Render the SVG element tree
		image.root_element->render_tree(device);
	}

	device.device_context->EndDraw();
}

void SVGDevice::redraw()
{
	InvalidateRect(wnd, NULL, FALSE);
}

bool SVGDevice::init(HWND _wnd)
{
	wnd = _wnd;

	HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2d_factory);

	if (!SUCCEEDED(hr)) {
		return false;
	}

	hr = DWriteCreateFactory(
		DWRITE_FACTORY_TYPE_SHARED,
		__uuidof(IDWriteFactory),
		reinterpret_cast<IUnknown**>(&dwrite_factory)
	);

	if (!SUCCEEDED(hr)) {
		return false;
	}

	// Create Render Target
	RECT rc;

	GetClientRect(_wnd, &rc);

	hr = d2d_factory->CreateHwndRenderTarget(
		D2D1::RenderTargetProperties(),
		D2D1::HwndRenderTargetProperties(
			_wnd,
			D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top)
		),
		&render_target
	);

	if (!SUCCEEDED(hr)) {
		return false;
	}

	//Get the device context from the render target
	hr = render_target->QueryInterface(IID_PPV_ARGS(&device_context));

	if (!SUCCEEDED(hr)) {
		return false;
	}

	return true;
}

// Resize the render target when the window size changes
void SVGDevice::resize()
{
	RECT rc;

	GetClientRect(wnd, &rc);
	render_target->Resize(D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top));
}
