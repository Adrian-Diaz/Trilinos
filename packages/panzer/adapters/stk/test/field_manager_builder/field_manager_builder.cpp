#include <Teuchos_ConfigDefs.hpp>
#include <Teuchos_UnitTestHarness.hpp>
#include <Teuchos_RCP.hpp>
#include <Teuchos_TimeMonitor.hpp>

using Teuchos::RCP;
using Teuchos::rcp;

#include "Teuchos_DefaultComm.hpp"
#include "Teuchos_GlobalMPISession.hpp"
#include "Panzer_STK_Version.hpp"
#include "Panzer_STK_config.hpp"
#include "Panzer_STK_Interface.hpp"
#include "Panzer_STK_SquareQuadMeshFactory.hpp"
#include "Panzer_STK_SetupUtilities.hpp"
#include "Panzer_Workset_Builder.hpp"
#include "Panzer_FieldManagerBuilder.hpp"
#include "Panzer_STKConnManager.hpp"
#include "user_app_EquationSetFactory.hpp"
#include "user_app_ModelFactory_TemplateBuilder.hpp"
#include "user_app_BCStrategy_Factory.hpp"

namespace panzer {

  void testInitialzation(panzer::InputPhysicsBlock& ipb,
			 std::vector<panzer::BC>& bcs);

  TEUCHOS_UNIT_TEST(field_manager_builder, basic)
  {
    using Teuchos::RCP;

    RCP<Teuchos::ParameterList> pl = rcp(new Teuchos::ParameterList);
    pl->set("X Blocks",2);
    pl->set("Y Blocks",1);
    pl->set("X Elements",6);
    pl->set("Y Elements",4);
    
    panzer_stk::SquareQuadMeshFactory factory;
    factory.setParameterList(pl);
    RCP<panzer_stk::STK_Interface> mesh = factory.buildMesh(MPI_COMM_WORLD);

    panzer::InputPhysicsBlock ipb;
    std::vector<panzer::BC> bcs;
    testInitialzation(ipb, bcs);


    const std::size_t workset_size = 20;
    std::map<std::string,Teuchos::RCP<std::vector<panzer::Workset> > > 
      volume_worksets = panzer_stk::buildWorksets(*mesh,ipb, workset_size);
    
    const std::map<panzer::BC,Teuchos::RCP<std::map<unsigned,panzer::Workset> >,panzer::LessBC> bc_worksets 
          = panzer_stk::buildBCWorksets(*mesh,ipb,bcs);

    std::map<std::string,std::string> block_ids_to_physics_ids;
    block_ids_to_physics_ids["eblock-0_0"] = "test physics";
    block_ids_to_physics_ids["eblock-1_0"] = "test physics";
    
    std::map<std::string,panzer::InputPhysicsBlock> 
      physics_id_to_input_physics_blocks;
    physics_id_to_input_physics_blocks["test physics"] = ipb;

    const Teuchos::RCP<panzer::ConnManager<int,int> > 
      conn_manager = Teuchos::rcp(new panzer_stk::STKConnManager(mesh));

    
    user_app::MyFactory eqset_factory;
				  
    panzer::FieldManagerBuilder<int,int> fmb;

    user_app::BCFactory bc_factory;

    const bool write_graphviz_files = true;

    fmb.setup(conn_manager,
	      MPI_COMM_WORLD,
	      block_ids_to_physics_ids,
	      physics_id_to_input_physics_blocks,
	      volume_worksets,
	      bc_worksets,
	      Teuchos::as<int>(mesh->getDimension()),
	      eqset_factory,
	      bc_factory,
	      workset_size,
	      write_graphviz_files);
 
    const std::vector< Teuchos::RCP< PHX::FieldManager<panzer::Traits> > >& fmb_vol_fm = 
      fmb.getVolumeFieldManagers();
    
    const std::vector< Teuchos::RCP<std::vector<panzer::Workset> > >& fmb_vol_worksets = 
      fmb.getWorksets();
    
    TEST_EQUALITY(fmb_vol_fm.size(), 2);
    TEST_EQUALITY(fmb_vol_fm.size(), fmb_vol_worksets.size());

    const std::map<panzer::BC, 
      std::map<unsigned,PHX::FieldManager<panzer::Traits> >,
      panzer::LessBC>& fmb_bc_fm = fmb.getBCFieldManagers();
      
    const std::map<panzer::BC,
		   Teuchos::RCP<std::map<unsigned,panzer::Workset> >,
		   panzer::LessBC>& fmb_bc_worksets = fmb.getBCWorksets();

    TEST_EQUALITY(fmb_bc_fm.size(), 3);
    TEST_EQUALITY(fmb_bc_fm.size(), fmb_bc_worksets.size());
  }

  TEUCHOS_UNIT_TEST(field_manager_builder, incremental_setup_interface)
  {
    using Teuchos::RCP;

    panzer_stk::SquareQuadMeshFactory mesh_factory;
    user_app::MyFactory eqset_factory;
    user_app::BCFactory bc_factory;
    const std::size_t workset_size = 20;

    panzer::FieldManagerBuilder<int,int> fmb;

    // setup physic blocks
    /////////////////////////////////////////////
    panzer::InputPhysicsBlock ipb;
    std::vector<panzer::BC> bcs;
    std::map<std::string,Teuchos::RCP<panzer::PhysicsBlock> > physics_blocks;
    {
       std::map<std::string,panzer::InputPhysicsBlock> 
             physics_id_to_input_physics_blocks;

       testInitialzation(ipb, bcs);

       std::map<std::string,std::string> block_ids_to_physics_ids;
       block_ids_to_physics_ids["eblock-0_0"] = "test physics";
       block_ids_to_physics_ids["eblock-1_0"] = "test physics";
    
       physics_id_to_input_physics_blocks["test physics"] = ipb;

       fmb.buildPhysicsBlocks(block_ids_to_physics_ids,
                              physics_id_to_input_physics_blocks,
                              2,workset_size,
                              eqset_factory,
                              physics_blocks);
    }

    // setup mesh
    /////////////////////////////////////////////
    RCP<panzer_stk::STK_Interface> mesh;
    {
       RCP<Teuchos::ParameterList> pl = rcp(new Teuchos::ParameterList);
       pl->set("X Blocks",2);
       pl->set("Y Blocks",1);
       pl->set("X Elements",6);
       pl->set("Y Elements",4);
       mesh_factory.setParameterList(pl);
       mesh = mesh_factory.buildMesh(MPI_COMM_WORLD);
    }

    // setup worksets
    /////////////////////////////////////////////
 
    std::map<std::string,Teuchos::RCP<std::vector<panzer::Workset> > > 
      volume_worksets = panzer_stk::buildWorksets(*mesh,ipb, workset_size);
    
    const std::map<panzer::BC,Teuchos::RCP<std::map<unsigned,panzer::Workset> >,panzer::LessBC> bc_worksets 
          = panzer_stk::buildBCWorksets(*mesh,ipb,bcs);

    // setup DOF manager
    /////////////////////////////////////////////
    const Teuchos::RCP<panzer::ConnManager<int,int> > conn_manager 
           = Teuchos::rcp(new panzer_stk::STKConnManager(mesh));
    const Teuchos::RCP<panzer::UniqueGlobalIndexer<int,int> > dofManager 
          = fmb.buildDOFManager(conn_manager,MPI_COMM_WORLD,physics_blocks);

    // setup field manager builder
    /////////////////////////////////////////////
    fmb.setupVolumeFieldManagers(volume_worksets,physics_blocks,dofManager);
    fmb.setupBCFieldManagers(bc_worksets,physics_blocks,eqset_factory,bc_factory);

    // run tests
    /////////////////////////////////
    const std::vector< Teuchos::RCP< PHX::FieldManager<panzer::Traits> > >& fmb_vol_fm = 
      fmb.getVolumeFieldManagers();
    
    const std::vector< Teuchos::RCP<std::vector<panzer::Workset> > >& fmb_vol_worksets = 
      fmb.getWorksets();
    
    TEST_EQUALITY(fmb_vol_fm.size(), 2);
    TEST_EQUALITY(fmb_vol_fm.size(), fmb_vol_worksets.size());

    const std::map<panzer::BC, 
      std::map<unsigned,PHX::FieldManager<panzer::Traits> >,
      panzer::LessBC>& fmb_bc_fm = fmb.getBCFieldManagers();
      
    const std::map<panzer::BC,
		   Teuchos::RCP<std::map<unsigned,panzer::Workset> >,
		   panzer::LessBC>& fmb_bc_worksets = fmb.getBCWorksets();

    TEST_EQUALITY(fmb_bc_fm.size(), 3);
    TEST_EQUALITY(fmb_bc_fm.size(), fmb_bc_worksets.size());
  }

  void testInitialzation(panzer::InputPhysicsBlock& ipb,
			 std::vector<panzer::BC>& bcs)
  {
    panzer::InputEquationSet ies_1;
    ies_1.name = "Energy";
    ies_1.basis = "Q2";
    ies_1.integration_order = 1;
    ies_1.model_id = 6;
    ies_1.model_factory = "rf";
    ies_1.prefix = "";
    ies_1.params.set<int>("junk", 1);

    panzer::InputEquationSet ies_2;
    ies_2.name = "Energy";
    ies_2.basis = "Q1";
    ies_2.integration_order = 1;
    ies_2.model_id = 6;
    ies_2.model_factory = "rf";
    ies_2.prefix = "ION_";
    ies_2.params.set<int>("junk", 1);

    ipb.physics_block_id = "4";
    ipb.eq_sets.push_back(ies_1);
    ipb.eq_sets.push_back(ies_2);


    {
      std::size_t bc_id = 0;
      panzer::BCType neumann = BCT_Dirichlet;
      std::string sideset_id = "left";
      std::string element_block_id = "eblock-0_0";
      std::string dof_name = "TEMPERATURE";
      std::string strategy = "Constant";
      double value = 5.0;
      Teuchos::ParameterList p;
      p.set("Value",value);
      panzer::BC bc(bc_id, neumann, sideset_id, element_block_id, dof_name, 
		    strategy, p);
      bcs.push_back(bc);
    }    
    {
      std::size_t bc_id = 1;
      panzer::BCType neumann = BCT_Dirichlet;
      std::string sideset_id = "right";
      std::string element_block_id = "eblock-1_0";
      std::string dof_name = "TEMPERATURE";
      std::string strategy = "Constant";
      double value = 5.0;
      Teuchos::ParameterList p;
      p.set("Value",value);
      panzer::BC bc(bc_id, neumann, sideset_id, element_block_id, dof_name, 
		    strategy, p);
      bcs.push_back(bc);
    }   
    {
      std::size_t bc_id = 2;
      panzer::BCType neumann = BCT_Dirichlet;
      std::string sideset_id = "top";
      std::string element_block_id = "eblock-1_0";
      std::string dof_name = "TEMPERATURE";
      std::string strategy = "Constant";
      double value = 5.0;
      Teuchos::ParameterList p;
      p.set("Value",value);
      panzer::BC bc(bc_id, neumann, sideset_id, element_block_id, dof_name, 
		    strategy, p);
      bcs.push_back(bc);
    }
  }

}
