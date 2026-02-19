#pragma once

struct SVGCircleElement : public SVGGraphicsElement {
	void compute_bbox() override;
	void render(const SVGDevice& device) const override;
};

