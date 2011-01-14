#include <vector>
#include <string>
#include <sstream>
#include "Phalanx_DataLayout_MDALayout.hpp"
#include "Phalanx_FieldManager.hpp"

#include "Panzer_DOFManager.hpp"
#include "Panzer_ConnManager.hpp"
#include "Panzer_Traits.hpp"
#include "Panzer_Workset.hpp"
#include "Panzer_Workset_Builder.hpp"
#include "Panzer_PhysicsBlock.hpp"
#include "Panzer_Shards_Utilities.hpp"
#include "Panzer_BCStrategy_Factory.hpp"
#include "Panzer_BCStrategy_TemplateManager.hpp"
#include "Panzer_CellData.hpp"
#include "Shards_CellTopology.hpp"
#include "Panzer_InputPhysicsBlock.hpp"
#include "Teuchos_FancyOStream.hpp"
#include "Panzer_StlMap_Utilities.hpp"
#include "Panzer_IntrepidFieldPattern.hpp"

//#include "EpetraExt_BlockMapOut.h"

//=======================================================================
//=======================================================================

namespace panzer {
namespace _hide {
typedef std::pair<std::string,Teuchos::RCP<panzer::Basis> > StrBasisPair;
struct StrBasisComp {
   bool operator() (const StrBasisPair & lhs, const StrBasisPair & rhs) const
   {return lhs.first<rhs.first;}
};
}
}

//=======================================================================
//=======================================================================
template<typename LO, typename GO>
void panzer::FieldManagerBuilder<LO,GO>::
setup(const Teuchos::RCP<panzer::ConnManager<LO,GO> >& conn_manager,
      MPI_Comm comm,
      const std::map<std::string,std::string>& block_ids_to_physics_ids,
      const std::map<std::string,panzer::InputPhysicsBlock>& physics_id_to_input_physics_blocks,
      const std::map<std::string,Teuchos::RCP<std::vector<panzer::Workset> > >& volume_worksets,
      const std::map<panzer::BC,Teuchos::RCP<BCFaceWorksetMap>,panzer::LessBC>& bc_worksets,
      int base_cell_dimension,
      const panzer::EquationSetFactory& eqset_factory,
      const panzer::BCStrategyFactory& bc_factory,
      std::size_t workset_size,
      bool write_graphviz_files)
{
  Teuchos::RCP<Teuchos::FancyOStream> pout = Teuchos::getFancyOStream(Teuchos::rcpFromRef(std::cout));
  pout->setShowProcRank(true);
  pout->setOutputToRootOnly(0);

  // Build the physics objects and register all variable providers
  std::vector<Teuchos::RCP<panzer::PhysicsBlock> > phxPhysicsBlocks;

  {
    using Teuchos::RCP;
    using Teuchos::rcp;
    
    this->buildPhysicsBlocks(block_ids_to_physics_ids, 
			     physics_id_to_input_physics_blocks,
			     base_cell_dimension, 
			     workset_size,
			     eqset_factory,
                             phxPhysicsBlocks);
    
    this->buildDOFManager(conn_manager, comm, phxPhysicsBlocks);
    
    // Order worksets so they are associated with physics blocks correctly
    std::vector<Teuchos::RCP<panzer::PhysicsBlock> >::const_iterator itr = phxPhysicsBlocks.begin();
    for (;itr!=phxPhysicsBlocks.end();++itr) 
      worksets_.push_back(volume_worksets.find((*itr)->elementBlockID())->second);

    this->buildFieldManagers(comm, phxPhysicsBlocks, 
			     phx_volume_field_managers_, write_graphviz_files);

  }

  // ***************************
  // BCs
  // ***************************
  std::map<panzer::BC,Teuchos::RCP<BCFaceWorksetMap>,panzer::LessBC>::const_iterator bc = 
    bc_worksets.begin();
  for (; bc != bc_worksets.end(); ++bc) {
    
    std::map<std::string,std::string>::const_iterator physics_id_iterator = 
      block_ids_to_physics_ids.find(bc->first.elementBlockID());
    
    TEST_FOR_EXCEPTION(physics_id_iterator == block_ids_to_physics_ids.end(), 
		       std::runtime_error,
		       "Error: the following boundary condition has invalid "
		       << "element block:\n" << bc->first << endl);

    std::string physics_id = physics_id_iterator->second;

    const panzer::InputPhysicsBlock& ipb = 
      panzer::getEntry(physics_id_to_input_physics_blocks, physics_id);

    const panzer::CellData volume_cell_data(workset_size, base_cell_dimension);

    Teuchos::RCP<panzer::PhysicsBlock> bc_pb = 
      Teuchos::rcp(new panzer::PhysicsBlock(ipb, bc->first.elementBlockID(),
					    volume_cell_data, eqset_factory));

    const shards::CellTopology volume_cell_topology = 
      bc_pb->getBaseCellTopology();
    
    bc_worksets_[bc->first] = bc->second; 

    // Build one FieldManager for each local side workset for each dirichlet bc
    std::map<unsigned,PHX::FieldManager<panzer::Traits> >& field_managers = 
      bc_field_managers_[bc->first];

    // Loop over local face indices and setup each field manager
    for (std::map<unsigned,panzer::Workset>::const_iterator wkst = 
	   bc_worksets_[bc->first]->begin(); wkst != bc_worksets_[bc->first]->end();
	 ++wkst) {

      PHX::FieldManager<panzer::Traits>& fm = field_managers[wkst->first];
      
      // register evaluators from strategy
      Teuchos::RCP<panzer::BCStrategy_TemplateManager<panzer::Traits> > bcs = 
	bc_factory.buildBCStrategy(bc->first);
      
      const panzer::CellData side_cell_data(wkst->second.num_cells,
					    base_cell_dimension,
					    wkst->first);      

      panzer::PhysicsBlock side_pb(ipb, bc->first.elementBlockID(),
				   side_cell_data, eqset_factory);
      
      // Iterate over evaluation types
      for (panzer::BCStrategy_TemplateManager<panzer::Traits>::iterator 
	     bcs_type = bcs->begin(); bcs_type != bcs->end(); ++bcs_type) {
	bcs_type->buildAndRegisterEvaluators(fm,side_pb);
      }

      // Setup the fieldmanager
      Traits::SetupData setupData;
      setupData.globalIndexer_ = dofMngr_;
      Teuchos::RCP<std::vector<panzer::Workset> > worksets = 
	Teuchos::rcp(new(std::vector<panzer::Workset>));
      worksets->push_back(wkst->second);
      setupData.worksets_ = worksets;
      fm.postRegistrationSetup(setupData);
    }
    
  }

  // BCs and Worksets only exist in workset map if elements are on this processor. 
  // For now only write bcs if being run in serial
  int num_procs = 0;
  MPI_Comm_size(comm, &num_procs);
  if (write_graphviz_files && (num_procs == 1)) {
    for (bc = bc_worksets.begin(); bc != bc_worksets.end(); ++bc) {
      std::stringstream filename;
      filename << "panzer_dependency_graph_bc_" << bc->first.bcID();  
      bc_field_managers_[bc->first].begin()->second.writeGraphvizFile(filename.str(), ".dot");
    }
  }

}

//=======================================================================
//=======================================================================
template<typename LO, typename GO>
void panzer::FieldManagerBuilder<LO,GO>::print(std::ostream& os) const
{
  os << "panzer::FieldManagerBuilder<LO,GO> output:  Not implemented yet!";
}

//=======================================================================
//=======================================================================
template<typename LO, typename GO>
void panzer::FieldManagerBuilder<LO,GO>::buildPhysicsBlocks(const std::map<std::string,std::string>& block_ids_to_physics_ids,
                                                            const std::map<std::string,panzer::InputPhysicsBlock>& physics_id_to_input_physics_blocks,
                                                            int base_cell_dimension, std::size_t workset_size,
	                                                    const panzer::EquationSetFactory & eqset_factory,
                                                            std::vector<Teuchos::RCP<panzer::PhysicsBlock> > & physicsBlocks
                                                            ) const 
{
   using Teuchos::RCP;
   using Teuchos::rcp;

   // loop over all block id physics id pairs
   std::map<std::string,std::string>::const_iterator itr;
   for (itr = block_ids_to_physics_ids.begin(); itr!=block_ids_to_physics_ids.end();++itr) {
      std::string element_block_id = itr->first;
      std::string physics_block_id = itr->second;
 
      const panzer::CellData volume_cell_data(workset_size, base_cell_dimension);
      
      // find InputPhysicsBlock that corresponds to a paricular block ID
      std::map<std::string,panzer::InputPhysicsBlock>::const_iterator ipb_it = 
            physics_id_to_input_physics_blocks.find(physics_block_id);

      // sanity check: passes only if there is a paricular physics ID
      TEST_FOR_EXCEPTION(ipb_it == physics_id_to_input_physics_blocks.end(),
			 std::runtime_error,
			 "Falied to find InputPhysicsBlock for physics id: "
			 << physics_block_id << "!");

      const panzer::InputPhysicsBlock& ipb = ipb_it->second;
      RCP<panzer::PhysicsBlock> pb = 
	rcp(new panzer::PhysicsBlock(ipb, element_block_id, volume_cell_data, eqset_factory));
      physicsBlocks.push_back(pb);
   }
}

//=======================================================================
//=======================================================================
template<typename LO, typename GO>
void panzer::FieldManagerBuilder<LO,GO>::buildDOFManager(const Teuchos::RCP<panzer::ConnManager<LO,GO> > & conn_manager, MPI_Comm comm,
                                                         const std::vector<Teuchos::RCP<panzer::PhysicsBlock> > & physicsBlocks)
{
   using namespace panzer::_hide;

   Teuchos::RCP<Teuchos::FancyOStream> pout = Teuchos::getFancyOStream(Teuchos::rcpFromRef(std::cout));
   pout->setShowProcRank(true);
   pout->setOutputToRootOnly(0);

   // build the DOF manager for the problem
   Teuchos::RCP<panzer::DOFManager<LO,GO> > dofManager 
         = Teuchos::rcp(new panzer::DOFManager<LO,GO>(conn_manager,comm));

   std::vector<Teuchos::RCP<panzer::PhysicsBlock> >::const_iterator physIter;
   for(physIter=physicsBlocks.begin();physIter!=physicsBlocks.end();++physIter) {
      Teuchos::RCP<const panzer::PhysicsBlock> pb = *physIter;
       
      const std::vector<StrBasisPair> & blockFields = pb->getProvidedDOFs();

      // insert all fields into a set
      std::set<StrBasisPair,StrBasisComp> fieldNames;
      fieldNames.insert(blockFields.begin(),blockFields.end()); 

      // add basis to DOF manager: block specific
      std::set<StrBasisPair>::const_iterator fieldItr; 
      for (fieldItr=fieldNames.begin();fieldItr!=fieldNames.end();++fieldItr) {
         Teuchos::RCP< Intrepid::Basis<double,Intrepid::FieldContainer<double> > > intrepidBasis 
               = fieldItr->second->getIntrepidBasis();
         Teuchos::RCP<IntrepidFieldPattern> fp = Teuchos::rcp(new IntrepidFieldPattern(intrepidBasis));
         dofManager->addField(pb->elementBlockID(),fieldItr->first,fp);

         *pout << "\"" << fieldItr->first << "\" Field Pattern = \n";
         fp->print(*pout);
      }
   } 

   dofManager->buildGlobalUnknowns();
   dofManager->printFieldInformation(*pout);
   dofMngr_ = dofManager;
}
//=======================================================================
//=======================================================================
template<typename LO, typename GO>
void panzer::FieldManagerBuilder<LO,GO>::buildFieldManagers(MPI_Comm comm,
							    const std::vector<Teuchos::RCP<panzer::PhysicsBlock> >& phxPhysicsBlocks, 
							    std::vector< Teuchos::RCP< PHX::FieldManager<panzer::Traits> > >& phx_volume_field_managers,
							    bool write_graphviz_files) const
{
  using Teuchos::RCP;
  using Teuchos::rcp;

  phx_volume_field_managers.resize(0);
  
  for (std::size_t block=0; block < phxPhysicsBlocks.size(); ++block) {

    phx_volume_field_managers.push_back(Teuchos::rcp(new PHX::FieldManager<panzer::Traits>));

    RCP<panzer::PhysicsBlock> pb = phxPhysicsBlocks[block];
    
    // register the evaluators
    PHX::FieldManager<panzer::Traits>& fm = 
      *(phx_volume_field_managers[block]);
    
    pb->buildAndRegisterEquationSetEvaluators(fm);
    pb->buildAndRegisterGatherScatterEvaluators(fm);
    //pb->buildAndRegisterModelEvaluators(fm, pb->getProvidedDOFs());
    
    Traits::SetupData setupData;
    setupData.globalIndexer_ = dofMngr_;
    setupData.worksets_ = worksets_[block];
    fm.postRegistrationSetup(setupData);
    
    if (write_graphviz_files) {
      int my_rank;
      MPI_Comm_rank(comm, &my_rank);
      if (my_rank == 0) {
	std::stringstream filename;
	filename << "panzer_dependency_graph_" << block;  
	fm.writeGraphvizFile(filename.str(), ".dot");
      }
    }

  }
  
}

//=======================================================================
//=======================================================================
template<typename LO, typename GO>
void panzer::FieldManagerBuilder<LO,GO>::buildPhysicsBlocks(const std::map<std::string,std::string>& block_ids_to_physics_ids,
                                                            const std::map<std::string,panzer::InputPhysicsBlock>& physics_id_to_input_physics_blocks,
                                                            int base_cell_dimension, std::size_t workset_size,
	                                                    const panzer::EquationSetFactory & eqset_factory,
                                                            std::map<std::string,Teuchos::RCP<panzer::PhysicsBlock> > & physicsBlocks) const
{
   using Teuchos::RCP;
   using Teuchos::rcp;

   // loop over all block id physics id pairs
   std::map<std::string,std::string>::const_iterator itr;
   for (itr = block_ids_to_physics_ids.begin(); itr!=block_ids_to_physics_ids.end();++itr) {
      std::string element_block_id = itr->first;
      std::string physics_block_id = itr->second;

      const panzer::CellData volume_cell_data(workset_size, base_cell_dimension);
      
      // find InputPhysicsBlock that corresponds to a paricular block ID
      std::map<std::string,panzer::InputPhysicsBlock>::const_iterator ipb_it = 
            physics_id_to_input_physics_blocks.find(physics_block_id);

      // sanity check: passes only if there is a paricular physics ID
      TEST_FOR_EXCEPTION(ipb_it == physics_id_to_input_physics_blocks.end(),
			 std::runtime_error,
			 "Falied to find InputPhysicsBlock for physics id: "
			 << physics_block_id << "!");

      const panzer::InputPhysicsBlock& ipb = ipb_it->second;
      RCP<panzer::PhysicsBlock> pb = 
	rcp(new panzer::PhysicsBlock(ipb, element_block_id, volume_cell_data, eqset_factory));
      physicsBlocks.insert(std::make_pair(element_block_id,pb));
   }
}
//=======================================================================
//=======================================================================
template<typename LO, typename GO>
Teuchos::RCP<panzer::DOFManager<LO,GO> > panzer::FieldManagerBuilder<LO,GO>::
                              buildDOFManager(const Teuchos::RCP<panzer::ConnManager<LO,GO> > & conn_manager, MPI_Comm comm,
                                              const std::map<std::string,Teuchos::RCP<panzer::PhysicsBlock> > & physicsBlocks) const
{
   using namespace panzer::_hide;

   Teuchos::RCP<Teuchos::FancyOStream> pout = Teuchos::getFancyOStream(Teuchos::rcpFromRef(std::cout));
   pout->setShowProcRank(true);
   pout->setOutputToRootOnly(0);

   // build the DOF manager for the problem
   Teuchos::RCP<panzer::DOFManager<LO,GO> > dofMngr 
         = Teuchos::rcp(new panzer::DOFManager<LO,GO>(conn_manager,comm));

   std::map<std::string,Teuchos::RCP<panzer::PhysicsBlock> >::const_iterator physIter;
   for(physIter=physicsBlocks.begin();physIter!=physicsBlocks.end();++physIter) {
      Teuchos::RCP<const panzer::PhysicsBlock> pb = physIter->second;
       
      const std::vector<StrBasisPair> & blockFields = pb->getProvidedDOFs();

      // insert all fields into a set
      std::set<StrBasisPair,StrBasisComp> fieldNames;
      fieldNames.insert(blockFields.begin(),blockFields.end()); 

      // add basis to DOF manager: block specific
      std::set<StrBasisPair>::const_iterator fieldItr; 
      for (fieldItr=fieldNames.begin();fieldItr!=fieldNames.end();++fieldItr) {
         Teuchos::RCP< Intrepid::Basis<double,Intrepid::FieldContainer<double> > > intrepidBasis 
               = fieldItr->second->getIntrepidBasis();
         Teuchos::RCP<IntrepidFieldPattern> fp = Teuchos::rcp(new IntrepidFieldPattern(intrepidBasis));
         dofMngr->addField(pb->elementBlockID(),fieldItr->first,fp);

         *pout << "\"" << fieldItr->first << "\" Field Pattern = \n";
         fp->print(*pout);
      }
   } 

   dofMngr->buildGlobalUnknowns();
   dofMngr->printFieldInformation(*pout);

   return dofMngr;
}

//=======================================================================
//=======================================================================
template<typename LO, typename GO>
void panzer::FieldManagerBuilder<LO,GO>::setupVolumeFieldManagers( 
                                            const std::map<std::string,Teuchos::RCP<std::vector<panzer::Workset> > >& volume_worksets, 
                                                                       // element block -> vector of worksets
                                            const std::map<std::string,Teuchos::RCP<panzer::PhysicsBlock> >& physicsBlocks, 
                                            const Teuchos::RCP<panzer::UniqueGlobalIndexer<LO,GO> > & dofManager)
{
  using Teuchos::RCP;
  using Teuchos::rcp;

  dofMngr_ = dofManager; // assign DOF manager for now

  worksets_.clear();
  phx_volume_field_managers_.clear();
  
  std::map<std::string,Teuchos::RCP<panzer::PhysicsBlock> >::const_iterator blkItr;
  for (blkItr=physicsBlocks.begin();blkItr!=physicsBlocks.end();++blkItr) {
    std::string blockId = blkItr->first; // to look up worksets
    RCP<panzer::PhysicsBlock> pb = blkItr->second;

    // build a field manager object
    Teuchos::RCP<PHX::FieldManager<panzer::Traits> > fm 
          = Teuchos::rcp(new PHX::FieldManager<panzer::Traits>);
    
    // use the physics block to register evaluators
    pb->buildAndRegisterEquationSetEvaluators(*fm);
    pb->buildAndRegisterGatherScatterEvaluators(*fm);
    //pb->buildAndRegisterModelEvaluators(fm, pb->getProvidedDOFs());
     
    // build the setup data using passed in information
    Traits::SetupData setupData;
    setupData.globalIndexer_ = dofMngr_;
    setupData.worksets_ = volume_worksets.find(blockId)->second;
    fm->postRegistrationSetup(setupData);

    // make sure to add the field manager & workset to the list 
    worksets_.push_back(setupData.worksets_);
    phx_volume_field_managers_.push_back(fm); 
  }
}
//=======================================================================
//=======================================================================
template<typename LO, typename GO>
void panzer::FieldManagerBuilder<LO,GO>::setupBCFieldManagers(
                           const std::map<panzer::BC,Teuchos::RCP<std::map<unsigned,panzer::Workset> >,panzer::LessBC>& bc_worksets,
                                                       // boundary condition -> map of (side_id,worksets)
                           const std::map<std::string,Teuchos::RCP<panzer::PhysicsBlock> >& physicsBlocks,
	                   const panzer::EquationSetFactory & eqset_factory,
                           const panzer::BCStrategyFactory& bc_factory)
{
  // ***************************
  // BCs
  // ***************************
  std::map<panzer::BC,Teuchos::RCP<BCFaceWorksetMap>,panzer::LessBC>::const_iterator bc = 
    bc_worksets.begin();
  for (; bc != bc_worksets.end(); ++bc) {
    std::string element_block_id = bc->first.elementBlockID(); 
    Teuchos::RCP<const panzer::PhysicsBlock> volume_pb = physicsBlocks.find(element_block_id)->second;
    const shards::CellTopology volume_cell_topology = volume_pb->getBaseCellTopology();
    int base_cell_dimension = volume_pb->cellData().baseCellDimension();
    
    bc_worksets_[bc->first] = bc->second; 

    // Build one FieldManager for each local side workset for each dirichlet bc
    std::map<unsigned,PHX::FieldManager<panzer::Traits> >& field_managers = 
      bc_field_managers_[bc->first];

    // Loop over local face indices and setup each field manager
    for (std::map<unsigned,panzer::Workset>::const_iterator wkst = 
	   bc_worksets_[bc->first]->begin(); wkst != bc_worksets_[bc->first]->end();
	 ++wkst) {

      PHX::FieldManager<panzer::Traits>& fm = field_managers[wkst->first];
      
      // register evaluators from strategy
      Teuchos::RCP<panzer::BCStrategy_TemplateManager<panzer::Traits> > bcs = 
	bc_factory.buildBCStrategy(bc->first);
      
      const panzer::CellData side_cell_data(wkst->second.num_cells,
					    base_cell_dimension,
					    wkst->first);      

      Teuchos::RCP<panzer::PhysicsBlock> side_pb 
            = volume_pb->copyWithCellData(side_cell_data, eqset_factory);
      
      // Iterate over evaluation types
      for (panzer::BCStrategy_TemplateManager<panzer::Traits>::iterator 
	     bcs_type = bcs->begin(); bcs_type != bcs->end(); ++bcs_type) {
	bcs_type->buildAndRegisterEvaluators(fm,*side_pb);
      }

      // Setup the fieldmanager
      Traits::SetupData setupData;
      setupData.globalIndexer_ = dofMngr_;
      Teuchos::RCP<std::vector<panzer::Workset> > worksets = 
	Teuchos::rcp(new(std::vector<panzer::Workset>));
      worksets->push_back(wkst->second);
      setupData.worksets_ = worksets;
      fm.postRegistrationSetup(setupData);
    }
    
  }
}
//=======================================================================
//=======================================================================
template<typename LO, typename GO>
std::ostream& panzer::operator<<(std::ostream& os, const panzer::FieldManagerBuilder<LO,GO>& rfd)
{
  rfd.print(os);
  return os;
}
