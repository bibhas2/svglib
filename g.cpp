#include "svglib.h"
#include "g.h"

void SVGGElement::create_presentation_assets(const std::vector<std::shared_ptr<SVGGraphicsElement>>& parent_stack, const std::map<std::wstring, std::shared_ptr<SVGGraphicsElement>>& id_map, const SVGDevice& device) {
	//Group element doesn't need to create any brushes.
}
