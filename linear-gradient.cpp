#include "svglib.h"
#include "linear-gradient.h"

CComPtr<ID2D1LinearGradientBrush> create_linear_gradient_brush(const SVGDevice& device, const SVGLinearGradientElement& linear_gradient, const SVGGraphicsElement& element) {
	std::vector<D2D1_GRADIENT_STOP> stops;

	for (const auto& child : linear_gradient.children) {
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

	CComPtr<ID2D1LinearGradientBrush> linear_gradient_brush;
	float x1 = linear_gradient.points[0];
	float y1 = linear_gradient.points[1];
	float x2 = linear_gradient.points[2];
	float y2 = linear_gradient.points[3];
	float width = element.bbox.right - element.bbox.left;
	float height = element.bbox.bottom - element.bbox.top;

	auto start_point = D2D1::Point2F(element.bbox.left + x1 * width, element.bbox.top + y1 * height);
	auto end_point = D2D1::Point2F(element.bbox.left + x2 * width, element.bbox.top + y2 * height);

	hr = device.device_context->CreateLinearGradientBrush(
		D2D1::LinearGradientBrushProperties(start_point, end_point),
		gradient_stop_collection,
		&linear_gradient_brush
	);

	if (!SUCCEEDED(hr)) {
		return nullptr;
	}

	return linear_gradient_brush;
}