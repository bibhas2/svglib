#pragma once

struct SVGGElement : public SVGGraphicsElement {
	void create_presentation_assets(const std::vector<std::shared_ptr<SVGGraphicsElement>>& parent_stack, const SVGDevice& device) override;
};
