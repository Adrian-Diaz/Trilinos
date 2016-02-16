#ifndef STK_ELEM_ELEM_GRAPH_IMPL_HPP
#define STK_ELEM_ELEM_GRAPH_IMPL_HPP

#include <vector>
#include <map>
#include <stk_topology/topology.hpp>
#include <stk_mesh/base/Entity.hpp>
#include <stk_mesh/base/Types.hpp>
#include <stk_util/parallel/CommSparse.hpp>
#include "../../base/FEMHelpers.hpp"

namespace stk { namespace mesh { class BulkData; } }
namespace stk { namespace mesh { struct sharing_info; } }
namespace stk { namespace mesh { class ElemElemGraph; } }
namespace stk { namespace mesh { class Graph; } }
namespace stk { namespace mesh { class ParallelInfoForGraphEdges; } }
namespace stk { class CommSparse; }

namespace stk { namespace mesh {
namespace impl
{

typedef int64_t LocalId;
static const LocalId INVALID_LOCAL_ID = std::numeric_limits<impl::LocalId>::max();

struct graphEdgeProc
{
    stk::mesh::EntityId m_localElementId;
    stk::mesh::EntityId m_remoteElementId;
    int m_localSide;
    int m_remoteSide;
    int m_proc_id;
    graphEdgeProc(const stk::mesh::EntityId& localElementId, int localSide, const stk::mesh::EntityId &remoteElementId, int remoteSide, int proc_id) :
        m_localElementId(localElementId), m_remoteElementId(remoteElementId), m_localSide(localSide), m_remoteSide(remoteSide), m_proc_id(proc_id) {}
};

struct parallel_info
{
    int m_other_proc;
    int m_permutation;
    stk::topology m_remote_element_toplogy;
    bool m_in_body_to_be_skinned;
    bool m_is_air;

    stk::mesh::EntityId m_chosen_side_id;
    std::vector<PartOrdinal> m_part_ordinals;

    parallel_info(int proc, int perm, stk::mesh::EntityId chosen_face_id, stk::topology other_elem_topology, bool inPart, bool isInAir=false, const std::vector<stk::mesh::PartOrdinal>& part_ords = std::vector<stk::mesh::PartOrdinal>()) :
        m_other_proc(proc), m_permutation(perm), m_remote_element_toplogy(other_elem_topology), m_in_body_to_be_skinned(inPart), m_is_air(isInAir),
        m_chosen_side_id(chosen_face_id), m_part_ordinals(part_ords){}
};

inline
std::ostream& operator<<(std::ostream& out, const parallel_info& info)
{
    out << "(other_proc=" << info.m_other_proc
            << ", perm=" << info.m_permutation
            << ", remote_top=" << info.m_remote_element_toplogy
            << ", in_body=" << (info.m_in_body_to_be_skinned?"true":"false")
            << ", is_air=" << (info.m_is_air?"true":"false")
            << ", chosen_side_id=" << info.m_chosen_side_id
            << ")";
    return out;
}

struct SerialElementData
{
    LocalId m_elementLocalId;
    stk::mesh::EntityId m_elementIdentifier;
    stk::topology m_elementTopology;
    unsigned m_sideIndex;
    stk::mesh::EntityVector m_sideNodes;
    SerialElementData(LocalId elementLocalId, stk::mesh::EntityId elementId, stk::topology elementToplogy, unsigned sideIndex, const stk::mesh::EntityVector& sideNodes) :
        m_elementLocalId(elementLocalId), m_elementIdentifier(elementId), m_elementTopology(elementToplogy), m_sideIndex(sideIndex), m_sideNodes(sideNodes) {}

    SerialElementData()
    : m_elementLocalId(std::numeric_limits<impl::LocalId>::max()),
      m_elementIdentifier(stk::mesh::InvalidEntityId),
      m_elementTopology(stk::topology::INVALID_TOPOLOGY),
      m_sideIndex(std::numeric_limits<unsigned>::max()),
      m_sideNodes(stk::mesh::EntityVector{}) {}

    stk::mesh::EntityId get_element_identifier() const { return m_elementIdentifier; }
    stk::topology get_element_topology() const { return m_elementTopology; }
    const stk::mesh::EntityVector& get_side_nodes() const { return m_sideNodes; }
    LocalId get_element_local_id() const { return m_elementLocalId; }
    unsigned get_element_side_index() const { return m_sideIndex; }

    void clear_side_nodes() { m_sideNodes.clear(); }
    void resize_side_nodes(size_t n) { m_sideNodes.resize(n); }

    void set_element_local_id(LocalId id) { m_elementLocalId = id; }
    void set_element_identifier(stk::mesh::EntityId id) { m_elementIdentifier = id; }
    void set_element_topology(stk::topology topo) { m_elementTopology = topo; }
    void set_element_side_index(unsigned index) { m_sideIndex = index; }

    stk::mesh::EntityVector::iterator side_nodes_begin() { return m_sideNodes.begin(); }

};

struct ParallelElementData
{
    SerialElementData serialElementData;
    int m_procId;
    stk::mesh::EntityId m_suggestedFaceId;
    bool m_isInPart;
    bool m_isAir;
    std::vector<PartOrdinal> m_part_ordinals;

    ParallelElementData()
    : serialElementData(),
      m_procId(-1),
      m_suggestedFaceId(stk::mesh::InvalidEntityId),
      m_isInPart(true),
      m_isAir(false),
      m_part_ordinals()
    {}

    bool is_parallel_edge() const { return m_procId != -1; }

    stk::mesh::EntityId get_element_identifier() const { return serialElementData.get_element_identifier(); }
    stk::topology get_element_topology() const { return serialElementData.get_element_topology(); }
    const stk::mesh::EntityVector& get_side_nodes() const { return serialElementData.get_side_nodes(); }
    LocalId get_element_local_id() const { return serialElementData.get_element_local_id(); }
    unsigned get_element_side_index() const { return serialElementData.get_element_side_index(); }

    void clear_side_nodes() { serialElementData.clear_side_nodes(); }
    void resize_side_nodes(size_t n) { serialElementData.resize_side_nodes(n); }
    void set_element_local_id(LocalId id) { serialElementData.set_element_local_id(id); }
    void set_element_identifier(stk::mesh::EntityId id) { serialElementData.set_element_identifier(id); }
    void set_element_topology(stk::topology topo) { serialElementData.set_element_topology(topo); }
    void set_element_side_index(unsigned index) { serialElementData.set_element_side_index(index); }

    stk::mesh::EntityVector::iterator side_nodes_begin() { return serialElementData.side_nodes_begin(); }
};

struct SharedEdgeInfo
{
    stk::mesh::EntityId m_locaElementlId;
    stk::mesh::EntityId m_remoteElementId;
    int m_procId;
    unsigned m_sideIndex;
    stk::mesh::EntityId m_chosenSideId;
    stk::mesh::EntityVector m_sharedNodes;
    bool m_isInPart;
    bool m_isInAir;
    stk::topology m_remoteElementTopology;
    std::vector<stk::mesh::PartOrdinal> m_partOrdinals;
};

struct ShellConnectivityData
{
    stk::mesh::EntityId m_nearElementId;
    int                 m_nearElementSide;
    int                 m_nearElementProc;
    stk::mesh::EntityId m_shellElementId;
    stk::mesh::EntityId m_farElementId;
    int                 m_farElementSide;
    int                 m_farElementProc;
    bool                m_farElementIsRemote;
};

struct DeletedElementData
{
    impl::LocalId       m_deletedElement;
    stk::mesh::EntityId m_remoteElement;
    int                 m_remoteProc;
};

struct ElementViaSidePair
{
    stk::mesh::Entity element;
    int side;
};

struct IdViaSidePair
{
    stk::mesh::EntityId id;
    int side;
};

}//namespace impl

struct GraphEdge
{
    GraphEdge(impl::LocalId e1, int s1, impl::LocalId e2, int s2) :
        elem1(e1), elem2(e2), side1(s1), side2(s2)
    {}
    GraphEdge() :
        elem1(std::numeric_limits<impl::LocalId>::max()), elem2(std::numeric_limits<impl::LocalId>::max()), side1(-1), side2(-1)
    {}
    impl::LocalId elem1;
    impl::LocalId elem2;
    int side1;
    int side2;
};

typedef GraphEdge CoincidentElementConnection;

struct GraphEdgeLessByElem2 {
    bool operator()(const GraphEdge& a, const GraphEdge& b) const
    {
        if (a.elem2 != b.elem2)
        {
            return a.elem2 < b.elem2;
        }
        else if (a.side2 != b.side2)
        {
            return a.side2 < b.side2;
        }
        else if (a.elem1 != b.elem1)
        {
            return a.elem1 < b.elem1;
        }
        else
        {
            return a.side1 < b.side1;
        }
    }
};

inline
bool operator==(const GraphEdge& a, const GraphEdge& b)
{
    return  a.elem1 == b.elem1 &&
            a.side1 == b.side1 &&
            a.elem2 == b.elem2 &&
            a.side2 == b.side2;
}

inline
std::ostream& operator<<(std::ostream& out, const GraphEdge& graphEdge)
{
    out << "(" << graphEdge.elem1 << "," << graphEdge.side1 << " -> " << graphEdge.elem2 << "," << graphEdge.side2 << ")";
    return out;
}

namespace impl {

typedef std::pair<LocalId,int> ElementSidePair;
typedef std::map<GraphEdge, parallel_info, GraphEdgeLessByElem2> ParallelGraphInfo;
typedef std::vector<std::vector<LocalId> > ElementGraph;
typedef std::vector<std::vector<int> > SidesForElementGraph;
typedef std::vector<ParallelElementData> ParallelElementDataVector;
typedef std::vector<SerialElementData> SerialElementDataVector;

typedef std::vector<GraphEdge> GraphEdgeVector;

NAMED_PAIR( EntitySidePair , stk::mesh::Entity , entity , unsigned , side_id )
NAMED_PAIR( ProcFaceIdPair , int , proc , stk::mesh::EntityId , side_id )
NAMED_PAIR( ProcVecFaceIdPair , std::vector<int> , proc_vec , stk::mesh::EntityId , side_id )

typedef std::multimap<EntitySidePair, ProcFaceIdPair>  ElemSideToProcAndFaceId;

void set_local_ids_and_fill_element_entities_and_topologies(stk::mesh::BulkData& bulkData, stk::mesh::EntityVector& local_id_to_element_entity, std::vector<stk::topology>& element_topologies);
void fill_local_ids_and_fill_element_entities_and_topologies(stk::mesh::BulkData& bulkData, stk::mesh::EntityVector& local_id_to_element_entity, std::vector<LocalId>& entity_to_local_id, std::vector<stk::topology>& element_topologies);

ElemSideToProcAndFaceId get_element_side_ids_to_communicate(const stk::mesh::BulkData& bulkData);
ElemSideToProcAndFaceId get_element_side_ids_to_communicate(const stk::mesh::BulkData& bulkData, const stk::mesh::EntityVector &element_list);

ElemSideToProcAndFaceId build_element_side_ids_to_proc_map(const stk::mesh::BulkData& bulkData, const stk::mesh::EntityVector &elements_to_communicate);

std::vector<graphEdgeProc> get_elements_to_communicate(const stk::mesh::BulkData& bulkData, const stk::mesh::EntityVector &killedElements,
        const ElemElemGraph& elem_graph);

std::vector<graphEdgeProc> communicate_killed_entities(stk::ParallelMachine communicator, const std::vector<graphEdgeProc>& elements_to_comm);

void pack_elements_to_comm(stk::CommSparse &comm, const std::vector<graphEdgeProc>& elements_to_comm);

void add_side_into_exposed_boundary(stk::mesh::BulkData& bulkData, const parallel_info& parallel_edge_info,
        stk::mesh::Entity local_element, int side_id, stk::mesh::EntityId remote_id, const stk::mesh::PartVector& parts_for_creating_side,
        std::vector<stk::mesh::sharing_info> &shared_modified, const stk::mesh::PartVector *boundary_mesh_parts = nullptr);

void remove_side_from_death_boundary(stk::mesh::BulkData& bulkData, stk::mesh::Entity local_element,
        stk::mesh::Part &activePart, stk::mesh::EntityVector &deletedEntities, int side_id);

stk::mesh::PartVector get_stk_parts_for_moving_parts_into_death_boundary(const stk::mesh::PartVector *bc_mesh_parts);

stk::mesh::Entity get_side_for_element(const stk::mesh::BulkData& bulkData, stk::mesh::Entity this_elem_entity, int side_id);

int get_element_side_multiplier();

bool is_id_already_in_use_locally(stk::mesh::BulkData& bulkData, stk::mesh::EntityRank rank, stk::mesh::EntityId id);

bool does_side_exist_with_different_permutation(stk::mesh::BulkData& bulkData, stk::mesh::Entity element,
        stk::mesh::ConnectivityOrdinal side_ordinal, stk::mesh::Permutation perm);

bool does_element_side_exist(stk::mesh::BulkData& bulkData, stk::mesh::Entity element, stk::mesh::ConnectivityOrdinal side_ordinal);

stk::mesh::Entity connect_side_to_element(stk::mesh::BulkData& bulkData, stk::mesh::Entity element,
        stk::mesh::EntityId side_global_id, stk::mesh::ConnectivityOrdinal side_ordinal,
        stk::mesh::Permutation side_permutation, const stk::mesh::PartVector& parts);

void pack_newly_shared_remote_edges(stk::CommSparse &comm, const stk::mesh::BulkData &m_bulk_data, const std::vector<SharedEdgeInfo> &newlySharedEdges);

bool does_element_have_side(const stk::mesh::BulkData& bulkData, stk::mesh::Entity element);

void add_exposed_sides(LocalId elementId, size_t numElemSides, const stk::mesh::Graph& graph, std::vector<int> &element_side_pairs);

void create_sides_created_during_death_part(stk::mesh::MetaData &metaData);
stk::mesh::Part* get_sides_created_during_death_part(const stk::mesh::MetaData &metaData);
void add_parts_from_element(stk::mesh::BulkData& bulkData, stk::mesh::Entity element, stk::mesh::PartVector& side_parts);
stk::mesh::PartVector get_parts_for_creating_side(stk::mesh::BulkData& bulkData, const stk::mesh::PartVector& parts_for_creating_side, stk::mesh::Entity element, int side_ord);
bool side_created_during_death(stk::mesh::BulkData& bulkData, stk::mesh::Entity side);

bool is_local_element(stk::mesh::impl::LocalId elemId);

template <typename T>
void pack_vector_to_proc(stk::CommSparse& comm, const T& data, int otherProc)
{
    comm.send_buffer(otherProc).pack<unsigned>(data.size());
    for(size_t i=0; i<data.size(); ++i)
        comm.send_buffer(otherProc).pack<typename T::value_type>(data[i]);
}

template <typename SideData>
void filter_out_invalid_solid_shell_connections(const stk::mesh::BulkData & mesh,
                                              const stk::mesh::Entity localElement,
                                              const unsigned sideOrdinal,
                                              std::vector<SideData> & connectedElementData)
{
    stk::topology localElemTopology = mesh.bucket(localElement).topology();

    if (localElemTopology.is_shell())
    {
        std::vector<SideData> filteredConnectedElements;
        for (const SideData & connectedElem: connectedElementData)
        {
            if(mesh.identifier(localElement) != connectedElem.get_element_identifier())
            {
                if(connectedElem.get_element_topology().is_shell())
                    add_shell_element_if_coincident(mesh, sideOrdinal, localElement, connectedElem, filteredConnectedElements);
                else
                    add_solid_element_if_normals_oppose_to_shell(mesh, sideOrdinal, localElement, connectedElem, filteredConnectedElements);
            }
        }
        connectedElementData.swap(filteredConnectedElements);
    }
    else
    {
        add_shell_connections_to_this_solid_if_normals_oppose(mesh, localElement, sideOrdinal, connectedElementData);
    }
}

template <typename SideData>
void add_shell_element_if_coincident(const stk::mesh::BulkData& mesh,
                                     const unsigned sideOrdinal,
                                     const stk::mesh::Entity localElement,
                                     const SideData& connectedElem,
                                     std::vector<SideData>& filteredConnectedElements)
{
    const stk::mesh::EntityVector &sideNodesOfReceivedElement = connectedElem.get_side_nodes();
    stk::mesh::OrdinalAndPermutation localElemOrdAndPerm =
            stk::mesh::get_ordinal_and_permutation(mesh, localElement, mesh.mesh_meta_data().side_rank(), sideNodesOfReceivedElement);
    // for shell element, want the nodes of the solid to be in opposite order. So getting non-matching side ordinals
    // means the normals oppose
    bool does_local_shell_side_normal_oppose_other_element_side_normal = (localElemOrdAndPerm.first == sideOrdinal);

    if (does_local_shell_side_normal_oppose_other_element_side_normal)
    {
        filteredConnectedElements.push_back(connectedElem);
    }
}

template <typename SideData>
void add_solid_element_if_normals_oppose_to_shell(const stk::mesh::BulkData& mesh,
                                                  const unsigned sideOrdinal,
                                                  const stk::mesh::Entity localElement,
                                                  const SideData& connectedElem,
                                                  std::vector<SideData>& filteredConnectedElements)
{
    const stk::mesh::EntityVector &sideNodesOfReceivedElement = connectedElem.get_side_nodes();
    stk::mesh::OrdinalAndPermutation localElemOrdAndPerm =
            stk::mesh::get_ordinal_and_positive_permutation(mesh, localElement, mesh.mesh_meta_data().side_rank(), sideNodesOfReceivedElement);
    // for shell element, want the nodes of the solid to be in opposite order. So getting non-matching side ordinals
    // means the normals oppose
    bool does_local_shell_side_normal_oppose_other_element_side_normal = (localElemOrdAndPerm.first != sideOrdinal);

    if (does_local_shell_side_normal_oppose_other_element_side_normal)
    {
        filteredConnectedElements.push_back(connectedElem);
    }
}

template <typename SideData>
void add_shell_connections_to_this_solid_if_normals_oppose(const stk::mesh::BulkData& mesh,
                                                           const stk::mesh::Entity localElement,
                                                           const unsigned sideOrdinal,
                                                           std::vector<SideData>& connectedElementData)
{
    std::vector<SideData> filteredConnectedElements;

    stk::topology localElemTopology = mesh.bucket(localElement).topology();
    stk::topology localSideTopology = localElemTopology.side_topology(sideOrdinal);
    bool foundAnySingleElementThatIsEquivalentToLocalElement = false;
    const stk::mesh::Entity* localElemNodes = mesh.begin_nodes(localElement);
    stk::mesh::EntityVector localElemSideNodes;
    localElemSideNodes.resize(localSideTopology.num_nodes());
    localElemTopology.side_nodes(localElemNodes, sideOrdinal, localElemSideNodes.begin());

    for (const SideData & connectedElem: connectedElementData)
    {
        std::pair<bool,unsigned> result = localSideTopology.equivalent(localElemSideNodes, connectedElem.get_side_nodes());
        const bool isEquivalentNodes = result.first;
        foundAnySingleElementThatIsEquivalentToLocalElement = foundAnySingleElementThatIsEquivalentToLocalElement || isEquivalentNodes;

        if (connectedElem.get_element_topology().is_shell() && isEquivalentNodes)
        {
            stk::mesh::OrdinalAndPermutation localElemOrdAndPerm = stk::mesh::get_ordinal_and_permutation(mesh,
                                                                                                          localElement,
                                                                                                          mesh.mesh_meta_data().side_rank(),
                                                                                                          connectedElem.get_side_nodes());
            bool localNegativeRelativeFacePolarity = !localSideTopology.is_positive_polarity(localElemOrdAndPerm.second);

            if (localNegativeRelativeFacePolarity)
            {
                filteredConnectedElements.push_back(connectedElem);
            }
        }
    }

    if (!filteredConnectedElements.empty())
    {
        connectedElementData.swap(filteredConnectedElements);
    }

    if (!foundAnySingleElementThatIsEquivalentToLocalElement)
    {
        connectedElementData.clear();
    }
}


}}} // end namespaces stk mesh

#endif
