#ifndef __Panzer_STK_Interface_hpp__
#define __Panzer_STK_Interface_hpp__

#include <Teuchos_RCP.hpp>

#include <stk_mesh/base/Types.hpp>
#include <stk_mesh/base/MetaData.hpp>
#include <stk_mesh/base/BulkData.hpp>
#include <stk_mesh/base/Field.hpp>
#include <stk_mesh/fem/EntityRanks.hpp>
#include <stk_mesh/fem/TopologyHelpers.hpp>
#include <stk_mesh/base/FieldData.hpp>
#include <stk_mesh/fem/DefaultFEM.hpp>
#include <stk_mesh/fem/FEMInterface.hpp>
#include <stk_mesh/fem/CoordinateSystems.hpp>

#include <Shards_CellTopology.hpp>
#include <Shards_CellTopologyData.h>

#include <Panzer_STK_config.hpp>

#ifdef HAVE_IOSS
#include <stk_io/util/UseCase_mesh.hpp>
#endif

namespace panzer_stk {

class PeriodicBC_MatcherBase;

/** Pure virtial base class that builds a basic element. To be
  * overidden with several types of elements.
  */ 
class ElementDescriptor {
public:
   ElementDescriptor(stk::mesh::EntityId gid,const std::vector<stk::mesh::EntityId> & nodes);
   virtual ~ElementDescriptor();

   stk::mesh::EntityId getGID() const { return gid_; }
   const std::vector<stk::mesh::EntityId> & getNodes() const { return nodes_; }
protected:
   stk::mesh::EntityId gid_;
   std::vector<stk::mesh::EntityId> nodes_;

   ElementDescriptor();
};

/** Constructor function for building the element descriptors.
  */ 
Teuchos::RCP<ElementDescriptor> 
buildElementDescriptor(stk::mesh::EntityId elmtId,std::vector<stk::mesh::EntityId> & nodes);

class STK_Interface {
public:
   typedef stk::mesh::Field<double> SolutionFieldType;
   typedef stk::mesh::Field<double,stk::mesh::Cartesian> VectorFieldType;
   typedef stk::mesh::Field<int> ProcIdFieldType;
   typedef stk::mesh::Field<std::size_t> LocalIdFieldType;

   STK_Interface();

   /** Default constructor
     */
   STK_Interface(unsigned dim);

   // functions called before initialize
   //////////////////////////////////////////

   /** Add an element block with a string name
     */
   void addElementBlock(const std::string & name,const CellTopologyData * ctData);

   /** Add a side set with a string name
     */
   void addSideset(const std::string & name,const CellTopologyData * ctData);

   /** Add a solution field
     */ 
   void addSolutionField(const std::string & fieldName,const std::string & blockId);

   //////////////////////////////////////////

   /** Initialize the mesh with the current dimension This also calls
     * commit on the meta data causing it to be frozen. Information
     * about elements blocks has to be commited before this.
     */
   void initialize(stk::ParallelMachine parallelMach,bool setupIO=true);

   // functions to manage and manipulate bulk data
   //////////////////////////////////////////
  
   /** Put the bulk data manager in modification mode.
     */
   void beginModification();

   /** Take the bulk data manager out of modification mode.
     */
   void endModification();

   /** Add a node to the mesh with a specific set of coordinates to the mesh.
     *
     * \pre <code>coord.size()==getDimension()</code> 
     * \pre <code>isModifiable()==true</code>
     */
   void addNode(stk::mesh::EntityId gid, const std::vector<double> & coord);

   void addElement(Teuchos::RCP<ElementDescriptor> & ed,stk::mesh::Part * block);

   /** Addes an entity to a specified side set.
     */
   void addEntityToSideset(stk::mesh::Entity & entity,stk::mesh::Part * sideset);

   // Methods to interrogate the mesh topology and structure
   //////////////////////////////////////////

   /** Grab the coordinates field 
     */
   const VectorFieldType & getCoordinatesField() const
   { return *coordinatesField_; }

   /** Look up a global node and get the coordinate.
     */
   const double * getNodeCoordinates(stk::mesh::EntityId nodeId) const;

   /** Look up a global node and get the coordinate.
     */
   const double * getNodeCoordinates(stk::mesh::Entity * node) const;

   /** Get subcell global IDs
     */
   void getSubcellIndices(unsigned entityRank,stk::mesh::EntityId elementId,
                          std::vector<stk::mesh::EntityId> & subcellIds) const;

   /** Get a vector of elements owned by this processor
     */
   void getMyElements(std::vector<stk::mesh::Entity*> & elements) const;

   /** Get a vector of elements owned by this processor on a particular block ID
     */
   void getMyElements(const std::string & blockID,std::vector<stk::mesh::Entity*> & elements) const;

   /** Get Entities corresponding to the side set requested. 
     * The Entites in the vector should be a dimension
     * lower then <code>getDimension()</code>.
     *
     * \param[in] sideName Name of side set
     * \param[in,out] sides Vector of entities containing the requested sides.
     */
   void getMySides(const std::string & sideName,std::vector<stk::mesh::Entity*> & sides) const;

   /** Get Entities corresponding to the side set requested. This also limits the entities
     * to be in a particular element block. The Entites in the vector should be a dimension
     * lower then <code>getDimension()</code>.
     *
     * \param[in] sideName Name of side set
     * \param[in] blockName Name of block
     * \param[in,out] sides Vector of entities containing the requested sides.
     */
   void getMySides(const std::string & sideName,const std::string & blockName,std::vector<stk::mesh::Entity*> & sides) const;

   // Utility functions
   //////////////////////////////////////////

   /** Write this mesh to exodus. This is a one shot function
     * that will write to a particular exodus output file.
     */
   void writeToExodus(const std::string & filename);

   /** This simply sets up a transient exodus file for writing.
     * No work is performed at this stage. This is used
     * in combination with <code>writeToExodus(double timestep)</code>.
     */
   void setupTransientExodusFile(const std::string & filename);

   /** Write this timestep to the exodus file specified in the
     * <code>setupTransientExodusFile</code>. This uses the
     * current state of the STK fields as the time step.
     */
   void writeToExodus(double timestep);

   // Accessor functions
   //////////////////////////////////////////
 
   Teuchos::RCP<stk::mesh::BulkData> getBulkData() const { return bulkData_; }
   Teuchos::RCP<stk::mesh::MetaData> getMetaData() const { return metaData_; }

   bool isWritable() const;

   bool isModifiable() const
   {  if(bulkData_==Teuchos::null) return false; 
      return bulkData_->synchronized_state()==stk::mesh::BulkData::MODIFIABLE; }

   //! get the dimension
   unsigned getDimension() const
   { return dimension_; }

   //! get the block count
   std::size_t getNumElementBlocks() const
   { return elementBlocks_.size(); }

   /** Get a vector containing the names of the element blocks.
     * This function always returns the current set of element blocks
     * in lexiographic order (uses the sorting built into the std::map).
     * This method can only be called after <code>initialize</code>.
     *
     * \param[in,out] names Vector of names of the element blocks.
     */
   void getElementBlockNames(std::vector<std::string> & names) const;

   /** Get a vector containing the names of the side sets.
     * This function always returns the current set of side sets
     * in lexiographic order (uses the sorting built into the std::map).
     * This method can only be called after <code>initialize</code>.
     *
     * \param[in,out] names Vector of names of the element blocks.
     */
   void getSidesetNames(std::vector<std::string> & name) const;

   //! get the block count
   stk::mesh::Part * getElementBlockPart(const std::string & name) const
   { 
      std::map<std::string, stk::mesh::Part*>::const_iterator itr = elementBlocks_.find(name);   // Element blocks
      if(itr==elementBlocks_.end()) return 0;
      return elementBlocks_.find(name)->second; 
   }

   //! get the side set count
   std::size_t getNumSidesets() const
   { return sidesets_.size(); }

   stk::mesh::Part * getSideset(const std::string & name) const
   { return sidesets_.find(name)->second; }

   //! get the global counts for the entity of specified rank
   std::size_t getEntityCounts(unsigned entityRank) const;

   //! get max entity ID of type entityRank
   stk::mesh::EntityId getMaxEntityId(unsigned entityRank) const;

   // Utilities
   //////////////////////////////////////////

   //! get a set of elements sharing a single node
   void getElementsSharingNode(stk::mesh::EntityId nodeId,std::vector<stk::mesh::Entity *> & elements) const;

   //! get a set of elements sharing multiple nodes
   void getElementsSharingNodes(const std::vector<stk::mesh::EntityId> nodeId,std::vector<stk::mesh::Entity *> & elements) const;

   //! force the mesh to build subcells: edges and faces
   void buildSubcells();

   /** Get an elements local index
     */
   std::size_t elementLocalId(stk::mesh::Entity * elmt) const;

   /**  Get the containing block ID of this element.
     */ 
   std::string containingBlockId(stk::mesh::Entity * elmt);

   /** Get the stk mesh field pointer associated with a particular solution value
     * Assumes there is a field associated with "fieldName,blockId" pair. If none
     * is found an exception (std::runtime_error) is raised.
     */
   stk::mesh::Field<double> * getSolutionField(const std::string & fieldName,
                                               const std::string & blockId) const;

   //! Has <code>initialize</code> been called on this mesh object?
   bool isInitialized() const { return initialized_; }

   /** Get Vector of element entities ordered by their LID, returns an RCP so that
     * it is easily stored by the caller.
     */
   Teuchos::RCP<const std::vector<stk::mesh::Entity*> > getElementsOrderedByLID() const;

   /** Writes a particular field to an array. Notice this is setup to work with
     * the worksets associated with Panzer.
     *
     * \param[in] fieldName Name of field to be filled
     * \param[in] blockId Name of block this set of elements belongs to
     * \param[in] localElementIds Local element IDs for this set of solution values
     * \param[in] solutionValues An two dimensional array object sized by (Cells,Basis Count)
     *
     * \note The block ID is not strictly needed in this context. However forcing the
     *       user to provide it does permit an additional level of safety. The implicit
     *       assumption is that the elements being "set" are part of the specified block.
     *       This prevents the need to perform a null pointer check on the field data, because
     *       the STK_Interface construction of the fields should force it to be nonnull...
     */
   template <typename ArrayT>
   void setSolutionFieldData(const std::string & fieldName,const std::string & blockId,
                             const std::vector<std::size_t> & localElementIds,const ArrayT & solutionValues);

   /** Reads a particular field into an array. Notice this is setup to work with
     * the worksets associated with Panzer.
     *
     * \param[in] fieldName Name of field to be filled
     * \param[in] blockId Name of block this set of elements belongs to
     * \param[in] localElementIds Local element IDs for this set of solution values
     * \param[in] solutionValues An two dimensional array object sized by (Cells,Basis Count)
     *
     * \note The block ID is not strictly needed in this context. However forcing the
     *       user to provide it does permit an additional level of safety. The implicit
     *       assumption is that the elements being retrieved are part of the specified block.
     *       This prevents the need to perform a null pointer check on the field data, because
     *       the STK_Interface construction of the fields should force it to be nonnull...
     */
   template <typename ArrayT>
   void getSolutionFieldData(const std::string & fieldName,const std::string & blockId,
                             const std::vector<std::size_t> & localElementIds,ArrayT & solutionValues) const;

   /** Get vertices associated with a number of elements of the same geometry.
     *
     * \param[in] localIds Element local IDs to construct vertices
     * \param[out] vertices Output array that will be sized (<code>localIds.size()</code>,#Vertices,#Dim)
     *
     * \note If not all elements have the same number of vertices an exception is thrown.
     *       If the size of <code>localIds</code> is 0, the function will silently return
     */
   template <typename ArrayT>
   void getElementVertices(std::vector<std::size_t> & localIds, ArrayT & vertices) const;

   const stk::mesh::fem::FEMInterface & getFEMInterface() const 
   { return *femPtr_; }

   const stk::mesh::EntityRank getElementRank() const { return stk::mesh::fem::element_rank(*femPtr_); }
   const stk::mesh::EntityRank getSideRank() const { return stk::mesh::fem::side_rank(*femPtr_); }
   const stk::mesh::EntityRank getEdgeRank() const { return stk::mesh::fem::edge_rank(*femPtr_); }
   const stk::mesh::EntityRank getNodeRank() const { return stk::mesh::fem::node_rank(*femPtr_); }

   /** Build fields and parts from the meta data
     */
   void initializeFromMetaData();

   /** Setup local element IDs
     */
   void buildLocalElementIDs();

   /** Return a vector containing all the periodic boundary conditions.
     */
   const std::vector<Teuchos::RCP<const PeriodicBC_MatcherBase> > &
   getPeriodicBCVector() const
   { return periodicBCs_; }

   /** Return a vector containing all the periodic boundary conditions.
     */
   std::vector<Teuchos::RCP<const PeriodicBC_MatcherBase> > &
   getPeriodicBCVector()
   { return periodicBCs_; }

   /** Add a periodic boundary condition.
     *
     * \note This does not actually change the underlying mesh.
     *       The object itself only communciates the matched IDs (currently nodes)
     */
   void addPeriodicBC(const Teuchos::RCP<const PeriodicBC_MatcherBase> & bc)
   { periodicBCs_.push_back(bc); }

public: // static operations
   static const std::string coordsString;
   static const std::string nodesString;
   static const std::string edgesString;

protected:

   /** Compute global entity counts.
     */
   void buildEntityCounts();

   /** Compute global entity counts.
     */
   void buildMaxEntityIds();

   std::vector<Teuchos::RCP<const PeriodicBC_MatcherBase> > periodicBCs_;

   Teuchos::RCP<stk::mesh::MetaData> metaData_;
   Teuchos::RCP<stk::mesh::BulkData> bulkData_;

   std::map<std::string, stk::mesh::Part*> elementBlocks_;   // Element blocks
   std::map<std::string, stk::mesh::Part*> sidesets_; // Side sets 
   std::map<std::string, Teuchos::RCP<shards::CellTopology> > elementBlockCT_;

   // for storing/accessing nodes
   stk::mesh::Part * nodesPart_;
   std::vector<stk::mesh::Part*> nodesPartVec_;
   stk::mesh::Part * edgesPart_;
   std::vector<stk::mesh::Part*> edgesPartVec_;

   VectorFieldType * coordinatesField_;
   ProcIdFieldType * processorIdField_;
   LocalIdFieldType * localIdField_;
   
   // maps field names to solution field stk mesh handles
   std::map<std::pair<std::string,std::string>,SolutionFieldType*> fieldNameToSolution_;

   unsigned dimension_;

   bool initialized_;

   // how many elements, faces, edges, and nodes are there globally
   std::vector<std::size_t> entityCounts_;

   // what is maximum entity ID
   std::vector<stk::mesh::EntityId> maxEntityId_;

   int procRank_;
   std::size_t currentLocalId_;

   Teuchos::RCP<stk::mesh::DefaultFEM> femPtr_;

#ifdef HAVE_IOSS
   // I/O support
   Teuchos::RCP<stk::io::util::MeshData> meshData_;
#endif

   // uses lazy evaluation
   mutable Teuchos::RCP<std::vector<stk::mesh::Entity*> > orderedElementVector_;

   // Object describing how to sort a vector of elements using
   // local ID as the key, very short lived object
   class LocalIdCompare {
   public:
     LocalIdCompare(const STK_Interface * mesh) : mesh_(mesh) {}
   
     // Compares two stk mesh entities based on local ID
     bool operator() (stk::mesh::Entity * a,stk::mesh::Entity * b) 
     { return mesh_->elementLocalId(a) < mesh_->elementLocalId(b);}
   
   private:
     const STK_Interface * mesh_;
   };
};

template <typename ArrayT>
void STK_Interface::setSolutionFieldData(const std::string & fieldName,const std::string & blockId,
                                         const std::vector<std::size_t> & localElementIds,const ArrayT & solutionValues)
{
   const std::vector<stk::mesh::Entity*> & elements = *(this->getElementsOrderedByLID());

   // SolutionFieldType * field = metaData_->get_field<SolutionFieldType>(fieldName); // if no blockId is specified you can get the field like this!
   SolutionFieldType * field = this->getSolutionField(fieldName,blockId);

   for(std::size_t cell=0;cell<localElementIds.size();cell++) {
      std::size_t localId = localElementIds[cell];
      stk::mesh::Entity * element = elements[localId];

      // loop over nodes set solution values
      stk::mesh::PairIterRelation relations = element->relations(getNodeRank());
      for(std::size_t i=0;i<relations.size();++i) {
         stk::mesh::Entity * node = relations[i].entity();

         double * solnData = stk::mesh::field_data(*field,*node);
         // TEUCHOS_ASSERT(solnData!=0); // only needed if blockId is not specified
         solnData[0] = solutionValues(cell,i);
      }
   }
}

template <typename ArrayT>
void STK_Interface::getSolutionFieldData(const std::string & fieldName,const std::string & blockId,
                                         const std::vector<std::size_t> & localElementIds,ArrayT & solutionValues) const
{
   const std::vector<stk::mesh::Entity*> & elements = *(this->getElementsOrderedByLID());

   solutionValues.resize(localElementIds.size(),elements[localElementIds[0]]->relations(getNodeRank()).size());

   // SolutionFieldType * field = metaData_->get_field<SolutionFieldType>(fieldName); // if no blockId is specified you can get the field like this!
   SolutionFieldType * field = this->getSolutionField(fieldName,blockId);

   for(std::size_t cell=0;cell<localElementIds.size();cell++) {
      std::size_t localId = localElementIds[cell];
      stk::mesh::Entity * element = elements[localId];

      // loop over nodes set solution values
      stk::mesh::PairIterRelation relations = element->relations(getNodeRank());
      for(std::size_t i=0;i<relations.size();++i) {
         stk::mesh::Entity * node = relations[i].entity();

         double * solnData = stk::mesh::field_data(*field,*node);
         // TEUCHOS_ASSERT(solnData!=0); // only needed if blockId is not specified
         solutionValues(cell,i) = solnData[0]; 
      }
   }
}

template <typename ArrayT>
void STK_Interface::getElementVertices(std::vector<std::size_t> & localElementIds, ArrayT & vertices) const
{
   // nothing to do! silently return
   if(localElementIds.size()==0)
      return;

   const std::vector<stk::mesh::Entity*> & elements = *(this->getElementsOrderedByLID());

   // get *master* cell toplogy...(belongs to first element)
   unsigned masterVertexCount 
      = stk::mesh::fem::get_cell_topology(*elements[localElementIds[0]]).getCellTopologyData()->vertex_count;

   // allocate space
   vertices.resize(localElementIds.size(),masterVertexCount,getDimension());

   // loop over each requested element
   unsigned dim = getDimension();
   for(std::size_t cell=0;cell<localElementIds.size();cell++) {
      stk::mesh::Entity * element = elements[localElementIds[cell]];
      TEUCHOS_ASSERT(element!=0);
 
      unsigned vertexCount 
         = stk::mesh::fem::get_cell_topology(*element).getCellTopologyData()->vertex_count;
      TEST_FOR_EXCEPTION(vertexCount!=masterVertexCount,std::runtime_error,
                         "In call to STK_Interface::getElementVertices all elements "
                         "must have the same vertex count!");

      // loop over all element nodes
      stk::mesh::PairIterRelation nodes = element->relations(stk::mesh::Node);
      TEST_FOR_EXCEPTION(nodes.size()!=masterVertexCount,std::runtime_error,
                         "In call to STK_Interface::getElementVertices cardinality of "
                         "element node relations must be the vertex count!");
      for(std::size_t node=0;node<nodes.size();++node) {
         const double * coord = getNodeCoordinates(nodes[node].entity()->identifier());

         // set each dimension of the coordinate
         for(unsigned d=0;d<dim;d++)
            vertices(cell,node,d) = coord[d];
      }
   }
}
 
}

#endif
