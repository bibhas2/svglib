#pragma once

struct SVGLineElement : public SVGGraphicsElement {
	void compute_bbox() override;
	void render(ID2D1DeviceContext* pContext) override;
};
