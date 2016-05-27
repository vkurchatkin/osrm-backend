#include "extractor/guidance/constants.hpp"
#include "extractor/guidance/motorway_handler.hpp"
#include "extractor/guidance/toolkit.hpp"

#include "util/guidance/toolkit.hpp"
#include "util/simple_logger.hpp"

#include <limits>
#include <utility>

#include <boost/assert.hpp>

using osrm::util::guidance::angularDeviation;
using osrm::util::guidance::getTurnDirection;

namespace osrm
{
namespace extractor
{
namespace guidance
{
namespace detail
{
inline bool isMotorwayClass(const FunctionalRoadClass road_class)
{
    return road_class == FunctionalRoadClass::MOTORWAY || road_class == FunctionalRoadClass::TRUNK;
}

inline bool isMotorwayClass(EdgeID eid, const util::NodeBasedDynamicGraph &node_based_graph)
{
    return isMotorwayClass(node_based_graph.GetEdgeData(eid).road_classification.road_class);
}
inline FunctionalRoadClass roadClass(const ConnectedRoad &road,
                                     const util::NodeBasedDynamicGraph &graph)
{
    return graph.GetEdgeData(road.turn.eid).road_classification.road_class;
}

inline bool isRampClass(EdgeID eid, const util::NodeBasedDynamicGraph &node_based_graph)
{
    return isRampClass(node_based_graph.GetEdgeData(eid).road_classification.road_class);
}
}

MotorwayHandler::MotorwayHandler(const util::NodeBasedDynamicGraph &node_based_graph,
                                 const std::vector<QueryNode> &node_info_list,
                                 const util::NameTable &name_table,
                                 const SuffixTable &street_name_suffix_table)
    : IntersectionHandler(node_based_graph, node_info_list, name_table, street_name_suffix_table)
{
}

MotorwayHandler::~MotorwayHandler() {}

bool MotorwayHandler::canProcess(const NodeID,
                                 const EdgeID via_eid,
                                 const Intersection &intersection) const
{
    bool has_motorway = false;
    bool has_normal_roads = false;

    for (const auto &road : intersection)
    {
        const auto &out_data = node_based_graph.GetEdgeData(road.turn.eid);
        // not merging or forking?
        if (road.entry_allowed && angularDeviation(road.turn.angle, STRAIGHT_ANGLE) > 60)
            return false;
        else if (out_data.road_classification.road_class == FunctionalRoadClass::MOTORWAY ||
                 out_data.road_classification.road_class == FunctionalRoadClass::TRUNK)
        {
            if (road.entry_allowed)
                has_motorway = true;
        }
        else if (!isRampClass(out_data.road_classification.road_class))
            has_normal_roads = true;
    }

    if (has_normal_roads)
        return false;

    const auto &in_data = node_based_graph.GetEdgeData(via_eid);
    return has_motorway ||
           in_data.road_classification.road_class == FunctionalRoadClass::MOTORWAY ||
           in_data.road_classification.road_class == FunctionalRoadClass::TRUNK;
}

Intersection MotorwayHandler::
operator()(const NodeID, const EdgeID via_eid, Intersection intersection) const
{
    const auto &in_data = node_based_graph.GetEdgeData(via_eid);

    // coming from motorway
    if (detail::isMotorwayClass(in_data.road_classification.road_class))
    {
        intersection = fromMotorway(via_eid, std::move(intersection));
        std::for_each(intersection.begin(), intersection.end(), [](ConnectedRoad &road) {
            if (road.turn.instruction.type == TurnType::OnRamp)
                road.turn.instruction.type = TurnType::OffRamp;
        });
        return intersection;
    }
    else // coming from a ramp
    {
        return fromRamp(via_eid, std::move(intersection));
        // ramp merging straight onto motorway
    }
}

Intersection MotorwayHandler::fromMotorway(const EdgeID via_eid, Intersection intersection) const
{
    const auto &in_data = node_based_graph.GetEdgeData(via_eid);
    BOOST_ASSERT(detail::isMotorwayClass(in_data.road_classification.road_class));

    const auto countExitingMotorways = [this](const Intersection &intersection) {
        unsigned count = 0;
        for (const auto &road : intersection)
        {
            if (road.entry_allowed && detail::isMotorwayClass(road.turn.eid, node_based_graph))
                ++count;
        }
        return count;
    };

    // find the angle that continues on our current highway
    const auto getContinueAngle = [this, in_data](const Intersection &intersection) {
        for (const auto &road : intersection)
        {
            const auto &out_data = node_based_graph.GetEdgeData(road.turn.eid);
            if (road.turn.angle != 0 && in_data.name_id == out_data.name_id &&
                in_data.name_id != INVALID_NAME_ID &&
                detail::isMotorwayClass(out_data.road_classification.road_class))
                return road.turn.angle;
        }
        return intersection[0].turn.angle;
    };

    const auto getMostLikelyContinue = [this, in_data](const Intersection &intersection) {
        double angle = intersection[0].turn.angle;
        double best = 180;
        for (const auto &road : intersection)
        {
            const auto &out_data = node_based_graph.GetEdgeData(road.turn.eid);
            if (detail::isMotorwayClass(out_data.road_classification.road_class) &&
                angularDeviation(road.turn.angle, STRAIGHT_ANGLE) < best)
            {
                best = angularDeviation(road.turn.angle, STRAIGHT_ANGLE);
                angle = road.turn.angle;
            }
        }
        return angle;
    };

    const auto findBestContinue = [&]() {
        const double continue_angle = getContinueAngle(intersection);
        if (continue_angle != intersection[0].turn.angle)
            return continue_angle;
        else
            return getMostLikelyContinue(intersection);
    };

    // find continue angle
    const double continue_angle = findBestContinue();
    // highway does not continue and has no obvious choice
    if (continue_angle == intersection[0].turn.angle)
    {
        if (intersection.size() == 2)
        {
            // do not announce ramps at the end of a highway
            intersection[1].turn.instruction = {TurnType::NoTurn,
                                                getTurnDirection(intersection[1].turn.angle)};
        }
        else if (intersection.size() == 3)
        {
            // splitting ramp at the end of a highway
            if (intersection[1].entry_allowed && intersection[2].entry_allowed)
            {
                assignFork(via_eid, intersection[2], intersection[1]);
            }
            else
            {
                // ending in a passing ramp
                if (intersection[1].entry_allowed)
                    intersection[1].turn.instruction = {
                        TurnType::NoTurn, getTurnDirection(intersection[1].turn.angle)};
                else
                    intersection[2].turn.instruction = {
                        TurnType::NoTurn, getTurnDirection(intersection[2].turn.angle)};
            }
        }
        else if (intersection.size() == 4 &&
                 detail::roadClass(intersection[1], node_based_graph) ==
                     detail::roadClass(intersection[2], node_based_graph) &&
                 detail::roadClass(intersection[2], node_based_graph) ==
                     detail::roadClass(intersection[3], node_based_graph))
        {
            // tripple fork at the end
            assignFork(via_eid, intersection[3], intersection[2], intersection[1]);
        }

        else if (countValid(intersection) > 0) // check whether turns exist at all
        {
            // FALLBACK, this should hopefully never be reached
            util::SimpleLogger().Write(logDEBUG)
                << "Fallback reached from motorway, no continue angle, " << intersection.size()
                << " roads, " << countValid(intersection) << " valid ones.";
            return fallback(std::move(intersection));
        }
    }
    else
    {
        const unsigned exiting_motorways = countExitingMotorways(intersection);

        if (exiting_motorways == 0)
        {
            // Ending in Ramp
            for (auto &road : intersection)
            {
                if (road.entry_allowed)
                {
                    BOOST_ASSERT(detail::isRampClass(road.turn.eid, node_based_graph));
                    road.turn.instruction =
                        TurnInstruction::SUPPRESSED(getTurnDirection(road.turn.angle));
                }
            }
        }
        else if (exiting_motorways == 1)
        {
            // normal motorway passing some ramps or mering onto another motorway
            if (intersection.size() == 2)
            {
                BOOST_ASSERT(!detail::isRampClass(intersection[1].turn.eid, node_based_graph));

                intersection[1].turn.instruction =
                    getInstructionForObvious(intersection.size(), via_eid,
                                             isThroughStreet(1, intersection), intersection[1]);
            }
            else
            {
                // Normal Highway exit or merge
                for (auto &road : intersection)
                {
                    // ignore invalid uturns/other
                    if (!road.entry_allowed)
                        continue;

                    if (road.turn.angle == continue_angle)
                    {
                        road.turn.instruction = getInstructionForObvious(
                            intersection.size(), via_eid, isThroughStreet(1, intersection), road);
                    }
                    else if (road.turn.angle < continue_angle)
                    {
                        road.turn.instruction = {
                            detail::isRampClass(road.turn.eid, node_based_graph) ? TurnType::OffRamp
                                                                                 : TurnType::Turn,
                            (road.turn.angle < 145) ? DirectionModifier::Right
                                                    : DirectionModifier::SlightRight};
                    }
                    else if (road.turn.angle > continue_angle)
                    {
                        road.turn.instruction = {
                            detail::isRampClass(road.turn.eid, node_based_graph) ? TurnType::OffRamp
                                                                                 : TurnType::Turn,
                            (road.turn.angle > 215) ? DirectionModifier::Left
                                                    : DirectionModifier::SlightLeft};
                    }
                }
            }
        }
        // handle motorway forks
        else if (exiting_motorways > 1)
        {
            if (exiting_motorways == 2 && intersection.size() == 2)
            {
                intersection[1].turn.instruction =
                    getInstructionForObvious(intersection.size(), via_eid,
                                             isThroughStreet(1, intersection), intersection[1]);
                util::SimpleLogger().Write(logDEBUG) << "Disabled U-Turn on a freeway";
                intersection[0].entry_allowed = false; // UTURN on the freeway
            }
            else if (exiting_motorways == 2)
            {
                // standard fork
                std::size_t first_valid = std::numeric_limits<std::size_t>::max(),
                            second_valid = std::numeric_limits<std::size_t>::max();
                for (std::size_t i = 0; i < intersection.size(); ++i)
                {
                    if (intersection[i].entry_allowed &&
                        detail::isMotorwayClass(intersection[i].turn.eid, node_based_graph))
                    {
                        if (first_valid < intersection.size())
                        {
                            second_valid = i;
                            break;
                        }
                        else
                        {
                            first_valid = i;
                        }
                    }
                }
                assignFork(via_eid, intersection[second_valid], intersection[first_valid]);
            }
            else if (exiting_motorways == 3)
            {
                // triple fork
                std::size_t first_valid = std::numeric_limits<std::size_t>::max(),
                            second_valid = std::numeric_limits<std::size_t>::max(),
                            third_valid = std::numeric_limits<std::size_t>::max();
                for (std::size_t i = 0; i < intersection.size(); ++i)
                {
                    if (intersection[i].entry_allowed &&
                        detail::isMotorwayClass(intersection[i].turn.eid, node_based_graph))
                    {
                        if (second_valid < intersection.size())
                        {
                            third_valid = i;
                            break;
                        }
                        else if (first_valid < intersection.size())
                        {
                            second_valid = i;
                        }
                        else
                        {
                            first_valid = i;
                        }
                    }
                }
                assignFork(via_eid, intersection[third_valid], intersection[second_valid],
                           intersection[first_valid]);
            }
            else
            {
                util::SimpleLogger().Write(logDEBUG) << "Found motorway junction with more than "
                                                        "2 exiting motorways or additional ramps";
                return fallback(std::move(intersection));
            }
        } // done for more than one highway exit
    }
    return intersection;
}

Intersection MotorwayHandler::fromRamp(const EdgeID via_eid, Intersection intersection) const
{
    auto num_valid_turns = countValid(intersection);
    // ramp straight into a motorway/ramp
    if (intersection.size() == 2 && num_valid_turns == 1)
    {
        BOOST_ASSERT(!intersection[0].entry_allowed);
        BOOST_ASSERT(detail::isMotorwayClass(intersection[1].turn.eid, node_based_graph));

        intersection[1].turn.instruction = getInstructionForObvious(
            intersection.size(), via_eid, isThroughStreet(1, intersection), intersection[1]);
    }
    else if (intersection.size() == 3)
    {
        // merging onto a passing highway / or two ramps merging onto the same highway
        if (num_valid_turns == 1)
        {
            BOOST_ASSERT(!intersection[0].entry_allowed);
            // check order of highways
            //          4
            //     5         3
            //
            //   6              2
            //
            //     7         1
            //          0
            if (intersection[1].entry_allowed)
            {
                if (detail::isMotorwayClass(intersection[1].turn.eid, node_based_graph) &&
                    node_based_graph.GetEdgeData(intersection[2].turn.eid).name_id !=
                        INVALID_NAME_ID &&
                    node_based_graph.GetEdgeData(intersection[2].turn.eid).name_id ==
                        node_based_graph.GetEdgeData(intersection[1].turn.eid).name_id)
                {
                    // circular order indicates a merge to the left (0-3 onto 4
                    if (angularDeviation(intersection[1].turn.angle, STRAIGHT_ANGLE) <
                        NARROW_TURN_ANGLE)
                        intersection[1].turn.instruction = {TurnType::Merge,
                                                            DirectionModifier::SlightLeft};
                    else // fallback
                        intersection[1].turn.instruction = {
                            TurnType::Merge, getTurnDirection(intersection[1].turn.angle)};
                }
                else // passing by the end of a motorway
                {
                    intersection[1].turn.instruction =
                        getInstructionForObvious(intersection.size(), via_eid,
                                                 isThroughStreet(1, intersection), intersection[1]);
                }
            }
            else
            {
                BOOST_ASSERT(intersection[2].entry_allowed);
                if (detail::isMotorwayClass(intersection[2].turn.eid, node_based_graph) &&
                    node_based_graph.GetEdgeData(intersection[1].turn.eid).name_id !=
                        INVALID_NAME_ID &&
                    node_based_graph.GetEdgeData(intersection[1].turn.eid).name_id ==
                        node_based_graph.GetEdgeData(intersection[0].turn.eid).name_id)
                {
                    // circular order (5-0) onto 4
                    if (angularDeviation(intersection[2].turn.angle, STRAIGHT_ANGLE) <
                        NARROW_TURN_ANGLE)
                        intersection[2].turn.instruction = {TurnType::Merge,
                                                            DirectionModifier::SlightRight};
                    else // fallback
                        intersection[2].turn.instruction = {
                            TurnType::Merge, getTurnDirection(intersection[2].turn.angle)};
                }
                else // passing the end of a highway
                {
                    intersection[2].turn.instruction =
                        getInstructionForObvious(intersection.size(), via_eid,
                                                 isThroughStreet(2, intersection), intersection[2]);
                }
            }
        }
        else
        {
            BOOST_ASSERT(num_valid_turns == 2);
            // UTurn on ramps is not possible
            BOOST_ASSERT(!intersection[0].entry_allowed);
            BOOST_ASSERT(intersection[1].entry_allowed);
            BOOST_ASSERT(intersection[2].entry_allowed);
            // two motorways starting at end of ramp (fork)
            //  M       M
            //    \   /
            //      |
            //      R
            if (detail::isMotorwayClass(intersection[1].turn.eid, node_based_graph) &&
                detail::isMotorwayClass(intersection[2].turn.eid, node_based_graph))
            {
                assignFork(via_eid, intersection[2], intersection[1]);
            }
            else
            {
                // continued ramp passing motorway entry
                //      M  R
                //      M  R
                //      | /
                //      R
                if (detail::isMotorwayClass(node_based_graph.GetEdgeData(intersection[1].turn.eid)
                                                .road_classification.road_class))
                {
                    intersection[1].turn.instruction = {TurnType::Turn,
                                                        DirectionModifier::SlightRight};
                    intersection[2].turn.instruction = {TurnType::Continue,
                                                        DirectionModifier::SlightLeft};
                }
                else
                {
                    assignFork(via_eid, intersection[2], intersection[1]);
                }
            }
        }
    }
    // On - Off Ramp on passing Motorway, Ramp onto Fork(?)
    else if (intersection.size() == 4)
    {
        bool passed_highway_entry = false;
        for (auto &road : intersection)
        {
            const auto &edge_data = node_based_graph.GetEdgeData(road.turn.eid);
            if (!road.entry_allowed &&
                detail::isMotorwayClass(edge_data.road_classification.road_class))
            {
                passed_highway_entry = true;
            }
            else if (detail::isMotorwayClass(edge_data.road_classification.road_class))
            {
                road.turn.instruction = {TurnType::Merge, passed_highway_entry
                                                              ? DirectionModifier::SlightRight
                                                              : DirectionModifier::SlightLeft};
            }
            else
            {
                BOOST_ASSERT(isRampClass(edge_data.road_classification.road_class));
                road.turn.instruction = {TurnType::OffRamp, getTurnDirection(road.turn.angle)};
            }
        }
    }
    else
    { // FALLBACK, hopefully this should never been reached
        util::SimpleLogger().Write(logDEBUG) << "Reached fallback on motorway ramp with "
                                             << intersection.size() << " roads and "
                                             << countValid(intersection) << " valid turns.";
        return fallback(std::move(intersection));
    }
    return intersection;
}

Intersection MotorwayHandler::fallback(Intersection intersection) const
{
    for (auto &road : intersection)
    {
        const auto &out_data = node_based_graph.GetEdgeData(road.turn.eid);

        util::SimpleLogger().Write(logDEBUG)
            << "road: " << toString(road) << " Name: " << out_data.name_id
            << " Road Class: " << (int)out_data.road_classification.road_class;

        if (!road.entry_allowed)
            continue;

        const auto type = detail::isMotorwayClass(out_data.road_classification.road_class)
                              ? TurnType::Merge
                              : TurnType::Turn;

        if (type == TurnType::Turn)
        {
            if (angularDeviation(road.turn.angle, STRAIGHT_ANGLE) < FUZZY_ANGLE_DIFFERENCE)
                road.turn.instruction = {type, DirectionModifier::Straight};
            else
            {
                road.turn.instruction = {type, road.turn.angle > STRAIGHT_ANGLE
                                                   ? DirectionModifier::SlightLeft
                                                   : DirectionModifier::SlightRight};
            }
        }
        else
        {
            road.turn.instruction = {type, road.turn.angle < STRAIGHT_ANGLE
                                               ? DirectionModifier::SlightLeft
                                               : DirectionModifier::SlightRight};
        }
    }
    return intersection;
}

} // namespace guidance
} // namespace extractor
} // namespace osrm
