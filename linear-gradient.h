#pragma once
struct SVGLinearGradientElement : public SVGGraphicsElement
{
	std::wstring gradient_units = L"objectBoundingBox"; // or "userSpaceOnUse"
};

struct SVGStopElement : public SVGGraphicsElement
{
	float offset = 0.0f;
	D2D1::ColorF stop_color = D2D1::ColorF(D2D1::ColorF::Black);
	float stop_opacity = 1.0f;
};

CComPtr<ID2D1LinearGradientBrush> create_linear_gradient_brush(const SVGDevice& device, const SVGLinearGradientElement& linear_gradient, const SVGGraphicsElement& element);
