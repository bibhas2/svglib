#include "SVGUtil.h"
#include "rect.h"

void SVGRectElement::compute_bbox() {
	bbox.left = points[0];
	bbox.top = points[1];
	bbox.right = points[0] + points[2];
	bbox.bottom = points[1] + points[3];
}

void SVGRectElement::render(ID2D1DeviceContext* pContext) const {
	if (fill_brush) {
		if (points.size() == 4) {
			pContext->FillRectangle(
				D2D1::RectF(points[0], points[1], points[0] + points[2], points[1] + points[3]),
				fill_brush
			);
		}
		else if (points.size() == 6) {
			pContext->FillRoundedRectangle(
				D2D1::RoundedRect(
					D2D1::RectF(points[0], points[1], points[0] + points[2], points[1] + points[3]),
					points[4], points[5]),
				fill_brush
			);
		}
	}
	if (stroke_brush) {
		if (points.size() == 4) {
			pContext->DrawRectangle(
				D2D1::RectF(points[0], points[1], points[0] + points[2], points[1] + points[3]),
				stroke_brush,
				stroke_width,
				stroke_style
			);
		}
		else if (points.size() == 6) {
			pContext->DrawRoundedRectangle(
				D2D1::RoundedRect(
					D2D1::RectF(points[0], points[1], points[0] + points[2], points[1] + points[3]),
					points[4], points[5]),
				stroke_brush,
				stroke_width,
				stroke_style
			);
		}
	}
}
