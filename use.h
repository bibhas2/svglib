#pragma once
struct SVGUseElement : public SVGGraphicsElement {
	std::wstring href_id;

	void create_presentation_assets(const std::vector<std::shared_ptr<SVGGraphicsElement>>& parent_stack, const std::map<std::wstring, std::shared_ptr<SVGGraphicsElement>>& id_map, const SVGDevice& device) override;
};

