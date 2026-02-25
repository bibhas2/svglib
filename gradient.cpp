#include "svglib.h"
#include "gradient.h"
#include "utils.h"

void SVGStopElement::create_presentation_assets(const std::vector<std::shared_ptr<SVGGraphicsElement>>& parent_stack, const std::map<std::wstring, std::shared_ptr<SVGGraphicsElement>>& id_map, const SVGDevice& device) {
	SVGGraphicsElement::create_presentation_assets(parent_stack, id_map, device);

	std::wstring stop_color_str;

	if (get_style_computed(parent_stack, L"stop-color", stop_color_str)) {
		get_css_color(stop_color_str, stop_color.r, stop_color.g, stop_color.b, stop_color.a);
	}

	std::wstring stop_opacity_str = L"1.0";

	if (get_style_computed(parent_stack, L"stop-opacity", stop_opacity_str)) {
		float stop_opacity = 1.0f;

		if (get_size_value(device.device_context, stop_opacity_str, stop_opacity)) {
			stop_color.a = 255 * stop_opacity;
		}
	}
}

CComPtr<ID2D1GradientStopCollection> create_gradient_stop_collection(const SVGDevice& device, const std::vector<std::shared_ptr<SVGGraphicsElement>>& stop_elements) {
	std::vector<D2D1_GRADIENT_STOP> stops;

	for (const auto& child : stop_elements) {
		if (child->tag_name == L"stop") {
			auto stop = std::dynamic_pointer_cast<SVGStopElement>(child);
			if (stop) {
				stops.push_back(D2D1::GradientStop(stop->offset, stop->stop_color));
			}
		}
	}

	if (stops.empty()) {
		return nullptr;
	}

	CComPtr<ID2D1GradientStopCollection> gradient_stop_collection;
	HRESULT hr = device.device_context->CreateGradientStopCollection(
		stops.data(),
		static_cast<UINT32>(stops.size()),
		D2D1_GAMMA_2_2,
		D2D1_EXTEND_MODE_CLAMP,
		&gradient_stop_collection
	);

	if (!SUCCEEDED(hr)) {
		return nullptr;
	}

	return gradient_stop_collection;
}

CComPtr<ID2D1GradientStopCollection> create_gradient_stop_collection(const SVGDevice& device, const std::vector<std::shared_ptr<SVGGraphicsElement>>& chain, const SVGGraphicsElement& gradient_element) {
	if (!gradient_element.children.empty()) {
		return create_gradient_stop_collection(device, gradient_element.children);
	}
	
	//Walk up the reference chain looking for stops
	for (const auto& ref : chain) {
		if (!ref->children.empty()) {
			return create_gradient_stop_collection(device, ref->children);
		}
	}

	return nullptr;
}

CComPtr<ID2D1LinearGradientBrush> create_linear_gradient_brush(const SVGDevice& device, const std::map<std::wstring, std::shared_ptr<SVGGraphicsElement>>& id_map, const SVGLinearGradientElement& linear_gradient, const SVGGraphicsElement& element) {
	std::vector<std::shared_ptr<SVGGraphicsElement>> chain;

	build_reference_chain(linear_gradient, id_map, chain);

	CComPtr<ID2D1GradientStopCollection> gradient_stop_collection = create_gradient_stop_collection(device, chain, linear_gradient);

	if (!gradient_stop_collection) {
		return nullptr;
	}

	float x1 = 0, y1 = 0, x2 = 1.0, y2 = 0;
	std::wstring attr_value;

	if (linear_gradient.get_attribute_in_references(chain, L"x1", attr_value)) {
		get_size_value(device.device_context, attr_value, x1);
	}
	if (linear_gradient.get_attribute_in_references(chain, L"y1", attr_value)) {
		get_size_value(device.device_context, attr_value, y1);
	}
	if (linear_gradient.get_attribute_in_references(chain, L"x2", attr_value)) {
		get_size_value(device.device_context, attr_value, x2);
	}
	if (linear_gradient.get_attribute_in_references(chain, L"y2", attr_value)) {
		get_size_value(device.device_context, attr_value, y2);
	}

	//Get the gradient units from the chain
	std::wstring gradient_units;

	if (linear_gradient.get_attribute_in_references(chain, L"gradientUnits", attr_value)) {
		gradient_units = attr_value;
	}
	else {
		gradient_units = L"objectBoundingBox";
	}

	CComPtr<ID2D1LinearGradientBrush> linear_gradient_brush;
	float width = element.bbox.right - element.bbox.left;
	float height = element.bbox.bottom - element.bbox.top;

	//These calculations are for objectBoundingBox gradint unit.
	auto start_point = D2D1::Point2F(element.bbox.left + x1 * width, element.bbox.top + y1 * height);
	auto end_point = D2D1::Point2F(element.bbox.left + x2 * width, element.bbox.top + y2 * height);

	if (gradient_units == L"userSpaceOnUse") {
		start_point = D2D1::Point2F(x1, y1);
		end_point = D2D1::Point2F(x2, y2);
	}

	HRESULT hr = device.device_context->CreateLinearGradientBrush(
		D2D1::LinearGradientBrushProperties(start_point, end_point),
		gradient_stop_collection,
		&linear_gradient_brush
	);

	if (!SUCCEEDED(hr)) {
		return nullptr;
	}

	if (linear_gradient.combined_transform) {
		auto trans = linear_gradient.combined_transform.value();

		if (gradient_units == L"objectBoundingBox") {
			trans = D2D1::Matrix3x2F::Translation(-element.bbox.left, -element.bbox.top) * 
				trans * 
				D2D1::Matrix3x2F::Translation(element.bbox.left, element.bbox.top);
		}

		linear_gradient_brush->SetTransform(trans);
	}

	return linear_gradient_brush;
}

CComPtr<ID2D1RadialGradientBrush> create_radial_gradient_brush(const SVGDevice& device, const std::map<std::wstring, std::shared_ptr<SVGGraphicsElement>>& id_map, const SVGRadialGradientElement& radial_gradient, const SVGGraphicsElement& element) {
	std::vector<std::shared_ptr<SVGGraphicsElement>> chain;

	build_reference_chain(radial_gradient, id_map, chain);

	CComPtr<ID2D1GradientStopCollection> gradient_stop_collection = create_gradient_stop_collection(device, chain, radial_gradient);

	if (!gradient_stop_collection) {
		return nullptr;
	}

	float cx = 0.5f, cy = 0.5f, r = 0.5f, fx = 0.0, fy = 0.0, fr = 0.0;
	std::wstring attr_value;

	if (radial_gradient.get_attribute_in_references(chain, L"cx", attr_value)) {
		get_size_value(device.device_context, attr_value, cx);
	}
	if (radial_gradient.get_attribute_in_references(chain, L"cy", attr_value)) {
		get_size_value(device.device_context, attr_value, cy);
	}
	if (radial_gradient.get_attribute_in_references(chain, L"r", attr_value)) {
		get_size_value(device.device_context, attr_value, r);
	}
	if (radial_gradient.get_attribute_in_references(chain, L"fx", attr_value)) {
		get_size_value(device.device_context, attr_value, fx);
	} else {
		fx = cx;
	}
	if (radial_gradient.get_attribute_in_references(chain, L"fy", attr_value)) {
		get_size_value(device.device_context, attr_value, fy);
	} else {
		fy = cy;
	}
	if (radial_gradient.get_attribute_in_references(chain, L"fr", attr_value)) {
		get_size_value(device.device_context, attr_value, fr);
	} else {
		fr = 0.0;
	}

	//Get the gradient units from the chain
	std::wstring gradient_units;

	if (radial_gradient.get_attribute_in_references(chain, L"gradientUnits", attr_value)) {
		gradient_units = attr_value;
	} else {
		gradient_units = L"objectBoundingBox";
	}

	CComPtr<ID2D1RadialGradientBrush> radial_gradient_brush;
	float width = element.bbox.right - element.bbox.left;
	float height = element.bbox.bottom - element.bbox.top;

	//These calculations are for objectBoundingBox gradint unit.
	auto center = D2D1::Point2F(element.bbox.left + cx * width, element.bbox.top + cy * height);
	//Note: Offset origin is the delta from the center and not the actual position of the focal point.
	auto gradient_origin_offset = D2D1::Point2F((fx - cx) * width, (fy - cy) * height);
	auto radius_x = r * width;
	auto radius_y = r * height;

	if (gradient_units == L"userSpaceOnUse") {
		center = D2D1::Point2F(cx, cy);
		gradient_origin_offset = D2D1::Point2F(fx - cx, fy - cy);
		radius_x = radius_y = r;
	}

	HRESULT hr = device.device_context->CreateRadialGradientBrush(
		D2D1::RadialGradientBrushProperties(center, gradient_origin_offset, radius_x, radius_y),
		gradient_stop_collection,
		&radial_gradient_brush
	);

	if (!SUCCEEDED(hr)) {
		return nullptr;
	}

	if (radial_gradient.combined_transform) {
		auto trans = radial_gradient.combined_transform.value();

		if (gradient_units == L"objectBoundingBox") {
			trans = D2D1::Matrix3x2F::Translation(-element.bbox.left, -element.bbox.top) *
				trans *
				D2D1::Matrix3x2F::Translation(element.bbox.left, element.bbox.top);
		}

		radial_gradient_brush->SetTransform(trans);
	}

	return radial_gradient_brush;
}