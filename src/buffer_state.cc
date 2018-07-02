#include "buffer_state.h"

#include <cassert>
#include <set>

namespace celerity {
namespace detail {

	buffer_state::buffer_state(cl::sycl::range<3> size, size_t num_nodes) {
		std::unordered_set<node_id> all_nodes(num_nodes);
		for(auto i = 0u; i < num_nodes; ++i)
			all_nodes.insert(i);
		region_nodes.push_back(std::make_pair(GridRegion<3>(sycl_range_to_grid_point(size)), all_nodes));
	}

	std::vector<std::pair<GridBox<3>, std::unordered_set<node_id>>> buffer_state::get_source_nodes(GridRegion<3> request) const {
		std::vector<std::pair<GridBox<3>, std::unordered_set<node_id>>> result;

		// Locate entire region by iteratively removing the largest overlaps
		GridRegion<3> remaining = request;
		while(remaining.area() > 0) {
			size_t largest_overlap = 0;
			size_t largest_overlap_i = -1;
			for(auto i = 0u; i < region_nodes.size(); ++i) {
				auto r = GridRegion<3>::intersect(region_nodes[i].first, remaining);
				const auto area = r.area();
				if(area > largest_overlap) {
					largest_overlap = area;
					largest_overlap_i = i;
				}
			}

			assert(largest_overlap > 0);
			auto r = GridRegion<3>::intersect(region_nodes[largest_overlap_i].first, remaining);
			remaining = GridRegion<3>::difference(remaining, r);
			r.scanByBoxes(
			    [this, &result, largest_overlap_i](const GridBox<3>& b) { result.push_back(std::make_pair(b, region_nodes[largest_overlap_i].second)); });
		}

		return result;
	}

	void buffer_state::update_region(const GridRegion<3>& region, const std::unordered_set<node_id>& nodes) {
		const auto num_regions = region_nodes.size();
		for(auto i = 0u; i < num_regions; ++i) {
			const size_t overlap = GridRegion<3>::intersect(region_nodes[i].first, region).area();
			if(overlap == 0) continue;
			const auto diff = GridRegion<3>::difference(region_nodes[i].first, region);
			if(diff.area() == 0) {
				// New region is larger / equal to stored region - update it
				region_nodes[i].first = region;
				region_nodes[i].second = nodes;
			} else {
				// Stored region needs to be updated as well
				region_nodes[i].first = diff;
				region_nodes.push_back(std::make_pair(region, nodes));
			}
		}

		collapse_regions();
	}

	void buffer_state::collapse_regions() {
		std::set<size_t> erase_indices;
		for(auto i = 0u; i < region_nodes.size(); ++i) {
			const auto& nodes_i = region_nodes[i].second;
			for(auto j = i + 1; j < region_nodes.size(); ++j) {
				const auto& nodes_j = region_nodes[j].second;
				std::vector<node_id> intersection;
				std::set_intersection(nodes_i.cbegin(), nodes_i.cend(), nodes_j.cbegin(), nodes_j.cend(), std::back_inserter(intersection));
				if(intersection.size() == nodes_i.size()) {
					region_nodes[i].first = GridRegion<3>::merge(region_nodes[i].first, region_nodes[j].first);
					erase_indices.insert(j);
				}
			}
		}

		for(auto it = erase_indices.rbegin(); it != erase_indices.rend(); ++it) {
			region_nodes.erase(region_nodes.begin() + *it);
		}
	}

} // namespace detail
} // namespace celerity