#include <Teuchos_ConfigDefs.hpp>
#include <Teuchos_UnitTestHarness.hpp>
#include <Teuchos_RCP.hpp>
#include <Teuchos_TimeMonitor.hpp>

#include "Panzer_CellData.hpp"
#include "Panzer_InputEquationSet.hpp"
#include "user_app_EquationSetFactory.hpp"

namespace panzer {

  TEUCHOS_UNIT_TEST(equation_set, steady_state)
  {
    panzer::InputEquationSet ies;
    ies.name = "Energy";
    ies.basis = "Q2";
    ies.integration_order = 1;
    ies.model_id = "solid";
    ies.prefix = "ION_";
    
    int num_cells = 20;
    int cell_dim = 2;
    panzer::CellData cell_data(num_cells, cell_dim,
                        Teuchos::rcp(new shards::CellTopology(shards::getCellTopologyData< shards::Quadrilateral<4> >())));

    Teuchos::RCP<panzer::EquationSet_TemplateManager<panzer::Traits> > eq_set;
  
    user_app::MyFactory my_factory;
    eq_set = my_factory.buildEquationSet(ies, cell_data, false);
  }

  TEUCHOS_UNIT_TEST(equation_set, transient)
  {
    panzer::InputEquationSet ies;
    ies.name = "Energy";
    ies.basis = "Q2";
    ies.integration_order = 1;
    ies.model_id = "solid";
    ies.prefix = "ION_";
    
    int num_cells = 20;
    int cell_dim = 2;
    panzer::CellData cell_data(num_cells, cell_dim,
                        Teuchos::rcp(new shards::CellTopology(shards::getCellTopologyData< shards::Quadrilateral<4> >())));

    Teuchos::RCP<panzer::EquationSet_TemplateManager<panzer::Traits> > eq_set;
  
    user_app::MyFactory my_factory;
    eq_set = my_factory.buildEquationSet(ies, cell_data, true);
  }

}
