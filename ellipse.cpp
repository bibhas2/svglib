#include "SVGUtil.h"
#include "ellipse.h"

void SVGEllipseElement::compute_bbox() {
	bbox.left = points[0] - points[2];
	bbox.top = points[1] - points[3];
	bbox.right = points[0] + points[2];
	bbox.bottom = points[1] + points[3];
}

//Render SVGEllipseElement
void SVGEllipseElement::render(ID2D1DeviceContext* pContext) const {
	if (fill_brush) {
		pContext->FillEllipse(
			D2D1::Ellipse(D2D1::Point2F(points[0], points[1]), points[2], points[3]),
			fill_brush
		);
	}
	if (stroke_brush) {
		pContext->DrawEllipse(
			D2D1::Ellipse(D2D1::Point2F(points[0], points[1]), points[2], points[3]),
			stroke_brush,
			stroke_width
		);
	}
}