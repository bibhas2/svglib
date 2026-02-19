#pragma once
struct SVGDefsElement : public SVGGraphicsElement {
	void render_tree(ID2D1DeviceContext* pContext) const override;
};