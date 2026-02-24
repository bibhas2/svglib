#include "svglib.h"
#include "gradient.h"

CComPtr<ID2D1GradientStopCollection> create_gradient_stop_collection(const SVGDevice& device, const SVGGraphicsElement& element) {
	std::vector<D2D1_GRADIENT_STOP> stops;

	for (const auto& child : element.children) {
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

CComPtr<ID2D1LinearGradientBrush> create_linear_gradient_brush(const SVGDevice& device, const SVGLinearGradientElement& linear_gradient, const SVGGraphicsElement& element) {
	CComPtr<ID2D1GradientStopCollection> gradient_stop_collection = create_gradient_stop_collection(device, linear_gradient);

	if (!gradient_stop_collection) {
		return nullptr;
	}

	CComPtr<ID2D1LinearGradientBrush> linear_gradient_brush;
	float x1 = linear_gradient.points[0];
	float y1 = linear_gradient.points[1];
	float x2 = linear_gradient.points[2];
	float y2 = linear_gradient.points[3];
	float width = element.bbox.right - element.bbox.left;
	float height = element.bbox.bottom - element.bbox.top;

	//These calculations are for objectBoundingBox gradint unit.
	auto start_point = D2D1::Point2F(element.bbox.left + x1 * width, element.bbox.top + y1 * height);
	auto end_point = D2D1::Point2F(element.bbox.left + x2 * width, element.bbox.top + y2 * height);

	if (linear_gradient.gradient_units == L"userSpaceOnUse") {
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

	return linear_gradient_brush;
}

CComPtr<ID2D1RadialGradientBrush> create_radial_gradient_brush(const SVGDevice& device, const SVGRadialGradientElement& radial_gradient, const SVGGraphicsElement& element) {
	CComPtr<ID2D1GradientStopCollection> gradient_stop_collection = create_gradient_stop_collection(device, radial_gradient);

	if (!gradient_stop_collection) {
		return nullptr;
	}

	CComPtr<ID2D1RadialGradientBrush> radial_gradient_brush;
	float cx = radial_gradient.points[0];
	float cy = radial_gradient.points[1];
	float r = radial_gradient.points[2];
	float fx = radial_gradient.points[3];
	float fy = radial_gradient.points[4];
	float fr = radial_gradient.points[5]; //Not sure how to use this with Direct2D!!!
	float width = element.bbox.right - element.bbox.left;
	float height = element.bbox.bottom - element.bbox.top;

	//These calculations are for objectBoundingBox gradint unit.
	auto center = D2D1::Point2F(element.bbox.left + cx * width, element.bbox.top + cy * height);
	//Note: Offset origin is the delta from the center and not the actual position of the focal point.
	auto gradient_origin_offset = D2D1::Point2F((fx - cx) * width, (fy - cy) * height);
	auto radius = r * (width < height ? width : height);

	if (radial_gradient.gradient_units == L"userSpaceOnUse") {
		center = D2D1::Point2F(cx, cy);
		gradient_origin_offset = D2D1::Point2F(fx - cx, fy - cy);
		radius = r;
	}

	HRESULT hr = device.device_context->CreateRadialGradientBrush(
		D2D1::RadialGradientBrushProperties(center, gradient_origin_offset, radius, radius),
		gradient_stop_collection,
		&radial_gradient_brush
	);

	if (!SUCCEEDED(hr)) {
		return nullptr;
	}

	return radial_gradient_brush;
}