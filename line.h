#pragma once

struct SVGLineElement : public SVGGraphicsElement {
	void compute_bbox() override;
	void render(const SVGDevice& device) const override;
};
