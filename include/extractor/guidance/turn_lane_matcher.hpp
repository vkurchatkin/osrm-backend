#ifndef OSRM_EXTRACTOR_GUIDANCE_TURN_LANE_MATCHER_HPP_
#define OSRM_EXTRACTOR_GUIDANCE_TURN_LANE_MATCHER_HPP_

#include "extractor/guidance/intersection.hpp"
#include "extractor/guidance/turn_analysis.hpp"
#include "extractor/query_node.hpp"
#include "util/guidance/turn_lanes.hpp"
#include "util/name_table.hpp"
#include "util/node_based_graph.hpp"
#include "util/typedefs.hpp"
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace osrm
{
namespace extractor
{
namespace guidance
{

// Given an Intersection, the graph to access the data and  the turn lanes, the turn lane matcher
// assigns appropriate turn tupels to the different turns.

class TurnLaneMatcher
{
    struct TurnLaneData
    {
        std::string tag;
        LaneID from;
        LaneID to;

        bool operator<(const TurnLaneData &other) const;
    };

  public:
    TurnLaneMatcher(const util::NodeBasedDynamicGraph &node_based_graph,
                    const util::NameTable &turn_lane_strings,
                    const std::vector<QueryNode> &node_info_list,
                    const TurnAnalysis &turn_analysis);

    Intersection assignTurnLanes(const EdgeID via_edge, Intersection intersection) const;

  private:
    using LaneTupel = util::guidance::LaneTupel;
    typedef std::vector<TurnLaneData> LaneDataVector;
    typedef std::map<std::string, std::pair<LaneID, LaneID>> LaneMap;

    std::unordered_map<LaneTupel, std::uint16_t> lane_tupels;
    const util::NodeBasedDynamicGraph &node_based_graph;
    const util::NameTable &turn_lane_strings;
    const std::vector<QueryNode> &node_info_list;
    const TurnAnalysis &turn_analysis;

    bool isSimpleIntersection(const LaneDataVector &turn_lane_data,
                              const Intersection &intersection) const;

    LaneDataVector findRelevantLaneData(const NodeID at, const Intersection &intersection);
    LaneDataVector trimToRelevantLaneData(const LaneDataVector &turn_lane_data,
                                          const Intersection &intersection) const;

    LaneDataVector handleNoneValueAtSimpleTurn(const NodeID at,
                                               LaneDataVector lane_data,
                                               const Intersection &intersection) const;

    Intersection simpleMatchTuplesToTurns(Intersection intersection,
                                          const LaneID num_lanes,
                                          const LaneDataVector &lane_data) const;
};

} // namespace guidance
} // namespace extractor
} // namespace osrm

#endif // OSRM_EXTRACTOR_GUIDANCE_TURN_LANE_MATCHER_HPP_
