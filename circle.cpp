#include "SVGUtil.h"
#include "circle.h"

void SVGCircleElement::compute_bbox() {
	bbox.left = points[0] - points[2];
	bbox.top = points[1] - points[2];
	bbox.right = points[0] + points[2];
	bbox.bottom = points[1] + points[2];
}

void SVGCircleElement::render(ID2D1DeviceContext* pContext) const {
	if (fill_brush) {
		pContext->FillEllipse(
			D2D1::Ellipse(D2D1::Point2F(points[0], points[1]), points[2], points[2]),
			fill_brush
		);
	}
	if (stroke_brush) {
		pContext->DrawEllipse(
			D2D1::Ellipse(D2D1::Point2F(points[0], points[1]), points[2], points[2]),
			stroke_brush,
			stroke_width
		);
	}
}