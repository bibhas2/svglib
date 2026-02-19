#include "SVGUtil.h"
#include "line.h"

void SVGLineElement::compute_bbox() {
	bbox.left = points[0] < points[2] ? points[0] : points[2];
	bbox.top = points[1] < points[3] ? points[1] : points[3];
	bbox.right = points[0] > points[2] ? points[0] : points[2];
	bbox.bottom = points[1] > points[3] ? points[1] : points[3];
}

void SVGLineElement::render(ID2D1DeviceContext* pContext) const {
	if (stroke_brush) {
		pContext->DrawLine(
			D2D1::Point2F(points[0], points[1]),
			D2D1::Point2F(points[2], points[3]),
			stroke_brush,
			stroke_width,
			stroke_style
		);
	}
}