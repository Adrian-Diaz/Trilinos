#include <Panzer_STK_Interface.hpp>

#include <limits>

#include <stk_mesh/base/FieldData.hpp>
#include <stk_mesh/base/Comm.hpp>
#include <stk_mesh/base/Selector.hpp>
#include <stk_mesh/base/GetEntities.hpp>
#include <stk_mesh/base/GetBuckets.hpp>
#include <stk_mesh/fem/CreateAdjacentEntities.hpp>

#include <stk_util/parallel/ParallelReduce.hpp>

#ifdef HAVE_IOSS
#include <Ionit_Initializer.h>
#include <stk_io/IossBridge.hpp>
#include <stk_io/util/UseCase_mesh.hpp>
#endif

#include <set>

using Teuchos::RCP;
using Teuchos::rcp;

namespace panzer_stk {

ElementDescriptor::ElementDescriptor() {}
ElementDescriptor::ElementDescriptor(stk::mesh::EntityId gid,const std::vector<stk::mesh::EntityId> & nodes)
   : gid_(gid), nodes_(nodes) {}
ElementDescriptor::~ElementDescriptor() {}

/** Constructor function for building the element descriptors.
  */ 
Teuchos::RCP<ElementDescriptor> 
buildElementDescriptor(stk::mesh::EntityId elmtId,std::vector<stk::mesh::EntityId> & nodes)
{
   return Teuchos::rcp(new ElementDescriptor(elmtId,nodes));
}

const std::string STK_Interface::coordsString = "coordinates";
const std::string STK_Interface::nodesString = "nodes";
const std::string STK_Interface::edgesString = "edges";

STK_Interface::STK_Interface()
   : dimension_(0), initialized_(false), currentLocalId_(0)
{
   metaData_ = rcp(new stk::mesh::MetaData());
   femPtr_ = Teuchos::rcp(new stk::mesh::DefaultFEM(*metaData_));
}

STK_Interface::STK_Interface(unsigned dim)
   : dimension_(dim), initialized_(false), currentLocalId_(0)
{
   metaData_ = rcp(new stk::mesh::MetaData(stk::mesh::fem::entity_rank_names(dimension_)));
   femPtr_ = Teuchos::rcp(new stk::mesh::DefaultFEM(*metaData_,dimension_));

   initializeFromMetaData();
}

void STK_Interface::addSideset(const std::string & name,const CellTopologyData * ctData)
{
   TEUCHOS_ASSERT(not initialized_);
   TEUCHOS_ASSERT(dimension_!=0);

   stk::mesh::Part * sideset = &metaData_->declare_part(name,getSideRank()); 
   sidesets_.insert(std::make_pair(name,sideset));
   stk::mesh::fem::set_cell_topology(*sideset,stk::mesh::fem::CellTopology(ctData));
}

void STK_Interface::addSolutionField(const std::string & fieldName,const std::string & blockId) 
{
   std::pair<std::string,std::string> key = std::make_pair(fieldName,blockId);

   // add & declare field if not already added...currently assuming linears
   if(fieldNameToSolution_.find(key)==fieldNameToSolution_.end()) {
      SolutionFieldType * field = metaData_->get_field<SolutionFieldType>(fieldName);
      if(field==0)
         field = &metaData_->declare_field<SolutionFieldType>(fieldName);     
      fieldNameToSolution_[key] = field;
   }
}

void STK_Interface::initialize(stk::ParallelMachine parallelMach,bool setupIO) 
{
   TEUCHOS_ASSERT(not initialized_);
   TEUCHOS_ASSERT(dimension_!=0); // no zero dimensional meshes!

   stk::mesh::EntityRank elementRank = getElementRank();
   stk::mesh::EntityRank nodeRank = getNodeRank();

   procRank_ = stk::parallel_machine_rank(parallelMach);

   // associating the field with a part: universal part!
   stk::mesh::put_field( *coordinatesField_ , nodeRank, metaData_->universal_part(), getDimension());
   stk::mesh::put_field( *processorIdField_ , elementRank, metaData_->universal_part());
   stk::mesh::put_field( *localIdField_ , elementRank, metaData_->universal_part());

   // register fields for output primarily
   std::set<SolutionFieldType*> uniqueFields;
   std::map<std::pair<std::string,std::string>,SolutionFieldType*>::const_iterator fieldIter;
   for(fieldIter=fieldNameToSolution_.begin();fieldIter!=fieldNameToSolution_.end();++fieldIter)
      uniqueFields.insert(fieldIter->second); // this makes setting up IO easier!

   {
      std::set<SolutionFieldType*>::const_iterator uniqueFieldIter;
      for(uniqueFieldIter=uniqueFields.begin();uniqueFieldIter!=uniqueFields.end();++uniqueFieldIter)
         stk::mesh::put_field(*(*uniqueFieldIter), nodeRank,metaData_->universal_part());
   }

#ifdef HAVE_IOSS
   if(setupIO) {
      // setup Exodus file IO
      /////////////////////////////////////////

      // add element blocks
      {
         std::map<std::string, stk::mesh::Part*>::iterator itr;
         for(itr=elementBlocks_.begin();itr!=elementBlocks_.end();++itr) 
            stk::io::put_io_part_attribute(*itr->second);
      }

      // add side sets
      {
         std::map<std::string, stk::mesh::Part*>::iterator itr;
         for(itr=sidesets_.begin();itr!=sidesets_.end();++itr) 
            stk::io::put_io_part_attribute(*itr->second);
      }
   
      // add nodes
      stk::io::put_io_part_attribute(*nodesPart_);

      stk::io::set_field_role(*coordinatesField_, Ioss::Field::ATTRIBUTE);
      stk::io::set_field_role(*processorIdField_, Ioss::Field::TRANSIENT);

      // add solution fields
      std::set<SolutionFieldType*>::const_iterator uniqueFieldIter;
      for(uniqueFieldIter=uniqueFields.begin();uniqueFieldIter!=uniqueFields.end();++uniqueFieldIter)
         stk::io::set_field_role(*(*uniqueFieldIter), Ioss::Field::TRANSIENT);
   }
#endif

   metaData_->commit();
   bulkData_ = rcp(new stk::mesh::BulkData(*metaData_,parallelMach));

   initialized_ = true;
}

void STK_Interface::beginModification()
{
   TEST_FOR_EXCEPTION(bulkData_==Teuchos::null,std::logic_error,
                      "STK_Interface: Must call \"initialized\" before \"beginModification\"");

   bulkData_->modification_begin();
}

void STK_Interface::endModification()
{
   TEST_FOR_EXCEPTION(bulkData_==Teuchos::null,std::logic_error,
                      "STK_Interface: Must call \"initialized\" before \"endModification\"");

   bulkData_->modification_end();

   buildEntityCounts();
   buildMaxEntityIds();
}

void STK_Interface::addNode(stk::mesh::EntityId gid, const std::vector<double> & coord)
{
   TEST_FOR_EXCEPTION(not isModifiable(),std::logic_error,
                      "STK_Interface::addNode: STK_Interface must be modifiable to add a node");
   TEST_FOR_EXCEPTION(not coord.size()==getDimension(),std::logic_error,
                      "STK_Interface::addNode: number of coordinates in vector must mation dimension");
   TEST_FOR_EXCEPTION(gid==0,std::logic_error,
                      "STK_Interface::addNode: STK has STUPID restriction of no zero GIDs, pick something else");
   stk::mesh::EntityRank nodeRank = getNodeRank();

   stk::mesh::Entity & node = bulkData_->declare_entity(nodeRank,gid,nodesPartVec_);

   // set coordinate vector
   double * fieldCoords = stk::mesh::field_data(*coordinatesField_,node);
   for(std::size_t i=0;i<coord.size();++i)
      fieldCoords[i] = coord[i];
}

void STK_Interface::addEntityToSideset(stk::mesh::Entity & entity,stk::mesh::Part * sideset)
{
   std::vector<stk::mesh::Part*> sidesetV;
   sidesetV.push_back(sideset);

   bulkData_->change_entity_parts(entity,sidesetV);
}

void STK_Interface::addElement(Teuchos::RCP<ElementDescriptor> & ed,stk::mesh::Part * block)
{
   std::vector<stk::mesh::Part*> blockVec;
   blockVec.push_back(block);

   stk::mesh::EntityRank elementRank = getElementRank();
   stk::mesh::EntityRank nodeRank = getNodeRank();
   stk::mesh::Entity & element = bulkData_->declare_entity(elementRank,ed->getGID(),blockVec);

   // build relations that give the mesh structure
   const std::vector<stk::mesh::EntityId> & nodes = ed->getNodes();
   for(std::size_t i=0;i<nodes.size();++i) {
      // add element->node relation
      stk::mesh::Entity * node = bulkData_->get_entity(nodeRank,nodes[i]);
      TEUCHOS_ASSERT(node!=0);
      bulkData_->declare_relation(element,*node,i);
   }

   int * procId = stk::mesh::field_data(*processorIdField_,element);
   procId[0] = procRank_;

   std::size_t * localId = stk::mesh::field_data(*localIdField_,element);
   localId[0] = currentLocalId_;
   currentLocalId_++;
}

void STK_Interface::writeToExodus(const std::string & filename)
{
   #ifdef HAVE_IOSS
      stk::ParallelMachine comm = bulkData_->parallel();

      Ioss::Init::Initializer io;
      stk::io::util::MeshData meshData;
      stk::io::util::create_output_mesh(filename, "", "", comm, 
                                                     *bulkData_, *metaData_,meshData);

      stk::io::util::process_output_request(meshData, *bulkData_, 0.0);
   #else 
      TEUCHOS_ASSERT(false);
   #endif
}

void STK_Interface::setupTransientExodusFile(const std::string & filename)
{
   #ifdef HAVE_IOSS
      stk::ParallelMachine comm = bulkData_->parallel();
      meshData_ = Teuchos::rcp(new stk::io::util::MeshData);
      
      Ioss::Init::Initializer io;
      stk::io::util::create_output_mesh(filename, "", "", comm, 
                                                     *bulkData_, *metaData_,*meshData_);
   #else 
      TEUCHOS_ASSERT(false);
   #endif
}

void STK_Interface::writeToExodus(double timestep)
{
   #ifdef HAVE_IOSS
      stk::io::util::process_output_request(*meshData_, *bulkData_, timestep);
   #else 
      TEUCHOS_ASSERT(false);
   #endif
}

bool STK_Interface::isWritable() const
{
   #ifdef HAVE_IOSS
      return true;
   #else
      return false;
   #endif
}

void STK_Interface::getElementsSharingNode(stk::mesh::EntityId nodeId,std::vector<stk::mesh::Entity *> & elements) const
{
   stk::mesh::EntityRank elementRank = getElementRank();
   stk::mesh::EntityRank nodeRank = getNodeRank();

   // get all relations for node
   stk::mesh::Entity * node = bulkData_->get_entity(nodeRank,nodeId);
   stk::mesh::PairIterRelation relations = node->relations(elementRank);

   // extract elements sharing nodes
   stk::mesh::PairIterRelation::iterator itr;
   for(itr=relations.begin();itr!=relations.end();++itr) {
      elements.push_back(itr->entity());
   }
}

void STK_Interface::getElementsSharingNodes(const std::vector<stk::mesh::EntityId> nodeIds,std::vector<stk::mesh::Entity *> & elements) const
{
   std::vector<stk::mesh::Entity*> current;

   getElementsSharingNode(nodeIds[0],current); // fill it with elements touching first node
   std::sort(current.begin(),current.end());   // sort for intersection on the pointer 

   // find intersection with remaining nodes
   for(std::size_t n=1;n<nodeIds.size();++n) {
      // get elements associated with next node
      std::vector<stk::mesh::Entity*> nextNode;
      getElementsSharingNode(nodeIds[n],nextNode); // fill it with elements touching first node
      std::sort(nextNode.begin(),nextNode.end());   // sort for intersection on the pointer ID

      // intersect next node elements with current element list
      std::vector<stk::mesh::Entity*> intersection(std::min(nextNode.size(),current.size())); 
      std::vector<stk::mesh::Entity*>::const_iterator endItr
            = std::set_intersection(current.begin(),current.end(),
                                    nextNode.begin(),nextNode.end(),
                                    intersection.begin());
      std::size_t newLength = endItr-intersection.begin();
      intersection.resize(newLength);

      // store intersection
      current.clear();
      current = intersection;
   }

   // return the elements computed
   elements = current;
}

void STK_Interface::buildEntityCounts()
{
   entityCounts_.clear();
   stk::mesh::comm_mesh_counts(*bulkData_,entityCounts_);
}

void STK_Interface::buildMaxEntityIds()
{
   // developed to mirror "comm_mesh_counts" in stk_mesh/base/Comm.cpp

   const unsigned entityRankCount =  metaData_->entity_rank_count();
   const size_t   commCount        = 10; // entityRankCount

   TEUCHOS_ASSERT(entityRankCount<10);

   stk::ParallelMachine mach = bulkData_->parallel();

   std::vector<stk::mesh::EntityId> local(commCount,0);

   // determine maximum ID for this processor for each entity type
   stk::mesh::Selector ownedPart = metaData_->locally_owned_part();
   for(unsigned i=0;i<entityRankCount;i++) {
      std::vector<stk::mesh::Entity*> entities;

      stk::mesh::get_selected_entities(ownedPart,bulkData_->buckets(i),entities);

      // determine maximum ID for this processor
      std::vector<stk::mesh::Entity*>::const_iterator itr;  
      for(itr=entities.begin();itr!=entities.end();++itr) {
         stk::mesh::EntityId id = (*itr)->identifier();
         if(id>local[i])
            local[i] = id;
      }
   }

   // get largest IDs across processors
   stk::all_reduce(mach,stk::ReduceMax<10>(&local[0]));
   maxEntityId_.assign(local.begin(),local.begin()+entityRankCount+1); 
}

std::size_t STK_Interface::getEntityCounts(unsigned entityRank) const
{
   TEST_FOR_EXCEPTION(entityRank>=entityCounts_.size(),std::logic_error,
                      "STK_Interface::getEntityCounts: Entity counts do not include rank: " << entityRank);
                      
   return entityCounts_[entityRank];
}

stk::mesh::EntityId STK_Interface::getMaxEntityId(unsigned entityRank) const
{
   TEST_FOR_EXCEPTION(entityRank>=maxEntityId_.size(),std::logic_error,
                      "STK_Interface::getMaxEntityId: Max entity ids do not include rank: " << entityRank);
                      
   return maxEntityId_[entityRank];
}

void STK_Interface::buildSubcells()
{
   stk::mesh::PartVector emptyPartVector;
   stk::mesh::create_adjacent_entities(*bulkData_,emptyPartVector);

   buildEntityCounts();
   buildMaxEntityIds();
}

const double * STK_Interface::getNodeCoordinates(stk::mesh::EntityId nodeId) const
{
   stk::mesh::Entity * node = bulkData_->get_entity(getNodeRank(),nodeId);
   return stk::mesh::field_data(*coordinatesField_,*node);
}

const double * STK_Interface::getNodeCoordinates(stk::mesh::Entity * node) const
{
   return stk::mesh::field_data(*coordinatesField_,*node);
}

void STK_Interface::getSubcellIndices(unsigned entityRank,stk::mesh::EntityId elementId,
                                      std::vector<stk::mesh::EntityId> & subcellIds) const                       
{
   stk::mesh::EntityRank elementRank = getElementRank();
   stk::mesh::Entity * cell = bulkData_->get_entity(elementRank,elementId);
   
   TEST_FOR_EXCEPTION(cell==0,std::logic_error,
                      "STK_Interface::getSubcellIndices: could not find element requested (GID = " << elementId << ")");

   stk::mesh::PairIterRelation subcells = cell->relations(entityRank);
   subcellIds.clear();
   subcellIds.resize(subcells.size(),0);

   // loop over relations and fill subcell vector
   stk::mesh::PairIterRelation::iterator iter;
   for(iter=subcells.begin();iter!=subcells.end();++iter) {
      TEUCHOS_ASSERT(iter->identifier()<subcellIds.size());
      subcellIds[iter->identifier()] = iter->entity()->identifier();
   }
}

void STK_Interface::getMyElements(std::vector<stk::mesh::Entity*> & elements) const
{
   // setup local ownership
   stk::mesh::Selector ownedPart = metaData_->locally_owned_part();

   // grab elements
   stk::mesh::EntityRank elementRank = getElementRank();
   stk::mesh::get_selected_entities(ownedPart,bulkData_->buckets(elementRank),elements);
}

void STK_Interface::getMyElements(const std::string & blockID,std::vector<stk::mesh::Entity*> & elements) const
{
   stk::mesh::Part * elementBlock = getElementBlockPart(blockID);

   TEST_FOR_EXCEPTION(elementBlock==0,std::logic_error,"Could not find element block \"" << blockID << "\"");

   // setup local ownership
   // stk::mesh::Selector block = *elementBlock;
   stk::mesh::Selector ownedBlock = metaData_->locally_owned_part() & (*elementBlock);

   // grab elements
   stk::mesh::EntityRank elementRank = getElementRank();
   stk::mesh::get_selected_entities(ownedBlock,bulkData_->buckets(elementRank),elements);
}

void STK_Interface::getMySides(const std::string & sideName,std::vector<stk::mesh::Entity*> & sides) const
{
   stk::mesh::Selector side = *getSideset(sideName);
   stk::mesh::Selector ownedBlock = metaData_->locally_owned_part() & side;

   // grab elements
   stk::mesh::get_selected_entities(ownedBlock,bulkData_->buckets(getSideRank()),sides);
}

void STK_Interface::getMySides(const std::string & sideName,const std::string & blockName,std::vector<stk::mesh::Entity*> & sides) const
{
   stk::mesh::Selector side = *getSideset(sideName);
   stk::mesh::Selector block = *getElementBlockPart(blockName);
   stk::mesh::Selector ownedBlock = metaData_->locally_owned_part() & block & side;

   // grab elements
   stk::mesh::get_selected_entities(ownedBlock,bulkData_->buckets(getSideRank()),sides);
}

void STK_Interface::getElementBlockNames(std::vector<std::string> & names) const
{
   TEUCHOS_ASSERT(initialized_); // all blocks must have been added

   names.clear();

   // fill vector with automagically ordered string values
   std::map<std::string, stk::mesh::Part*>::const_iterator blkItr;   // Element blocks
   for(blkItr=elementBlocks_.begin();blkItr!=elementBlocks_.end();++blkItr) 
      names.push_back(blkItr->first);
}

void STK_Interface::getSidesetNames(std::vector<std::string> & names) const
{
   TEUCHOS_ASSERT(initialized_); // all blocks must have been added

   names.clear();

   // fill vector with automagically ordered string values
   std::map<std::string, stk::mesh::Part*>::const_iterator sideItr;   // Element blocks
   for(sideItr=sidesets_.begin();sideItr!=sidesets_.end();++sideItr) 
      names.push_back(sideItr->first);
}

std::size_t STK_Interface::elementLocalId(stk::mesh::Entity * elmt) const
{
   const std::size_t * fieldCoords = stk::mesh::field_data(*localIdField_,*elmt);
   return fieldCoords[0];
}

std::string STK_Interface::containingBlockId(stk::mesh::Entity * elmt)
{
   std::map<std::string,stk::mesh::Part*>::const_iterator itr;
   for(itr=elementBlocks_.begin();itr!=elementBlocks_.end();++itr)
      if(elmt->bucket().member(*itr->second))
         return itr->first;
   return "";
}

stk::mesh::Field<double> * STK_Interface::getSolutionField(const std::string & fieldName,
                                                           const std::string & blockId) const
{
   // look up field in map
   std::map<std::pair<std::string,std::string>, SolutionFieldType*>::const_iterator 
         iter = fieldNameToSolution_.find(std::make_pair(fieldName,blockId));
 
   // check to make sure field was actually found
   TEST_FOR_EXCEPTION(iter==fieldNameToSolution_.end(),std::runtime_error,
                      "Field name \"" << fieldName << "\" in block ID \"" << blockId << "\" was not found");

   return iter->second;
}

Teuchos::RCP<const std::vector<stk::mesh::Entity*> > STK_Interface::getElementsOrderedByLID() const
{
   using Teuchos::RCP;
   using Teuchos::rcp;

   if(orderedElementVector_==Teuchos::null) { 
      RCP<std::vector<stk::mesh::Entity*> > elements
         = Teuchos::rcp(new std::vector<stk::mesh::Entity*>);

      // defines ordering of blocks
      std::vector<std::string> blockIds;
      this->getElementBlockNames(blockIds);

      std::vector<std::string>::const_iterator idItr;
      for(idItr=blockIds.begin();idItr!=blockIds.end();++idItr) {
         std::string blockId = *idItr;

         // grab elements on this block
         std::vector<stk::mesh::Entity*> blockElmts;
         this->getMyElements(blockId,blockElmts); 

         // concatenate them into element LID lookup table
         elements->insert(elements->end(),blockElmts.begin(),blockElmts.end());
      }

      // this expensive operation gurantees ordering of local IDs
      std::sort(elements->begin(),elements->end(),LocalIdCompare(this));

      orderedElementVector_ = elements;
   }

   return orderedElementVector_.getConst();
}

void STK_Interface::addElementBlock(const std::string & name,const CellTopologyData * ctData)
{
   TEUCHOS_ASSERT(not initialized_);

   stk::mesh::Part * block = metaData_->get_part(name);
   if(block==0) {
      stk::mesh::EntityRank elementRank = getElementRank();
      block = &metaData_->declare_part(name,elementRank);

      stk::mesh::fem::set_cell_topology(*block,stk::mesh::fem::CellTopology(ctData));
   }

   // construct cell topology object for this block
   Teuchos::RCP<shards::CellTopology> ct
         = Teuchos::rcp(new shards::CellTopology(ctData));

   // add element block part and cell topology
   elementBlocks_.insert(std::make_pair(name,block));
   elementBlockCT_.insert(std::make_pair(name,ct));
}

void STK_Interface::initializeFromMetaData()
{
   dimension_ = femPtr_->get_spatial_dimension();   

   // declare coordinates and node parts
   coordinatesField_ = &metaData_->declare_field<VectorFieldType>(coordsString);
   processorIdField_ = &metaData_->declare_field<ProcIdFieldType>("PROC_ID");
   localIdField_     = &metaData_->declare_field<LocalIdFieldType>("LOCAL_ID");

   nodesPart_        = &metaData_->declare_part(nodesString,getNodeRank());
   nodesPartVec_.push_back(nodesPart_);
}

void STK_Interface::buildLocalElementIDs()
{
   currentLocalId_ = 0;
   
   orderedElementVector_ = Teuchos::null; // forces rebuild of ordered lists

   // might be better (faster) to do this by buckets
   std::vector<stk::mesh::Entity*> elements;
   getMyElements(elements);
 
   for(std::size_t index=0;index<elements.size();++index) {
      stk::mesh::Entity & element = *elements[index];

      // set processor rank
      int * procId = stk::mesh::field_data(*processorIdField_,element);
      procId[0] = procRank_;

      // set local element ID
      std::size_t * localId = stk::mesh::field_data(*localIdField_,element);
      localId[0] = currentLocalId_;
      currentLocalId_++;
   }
}

}
