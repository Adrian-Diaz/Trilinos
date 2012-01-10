#ifndef PANZER_STK_NOX_OBSERVER_FACTORY_HPP
#define PANZER_STK_NOX_OBSERVER_FACTORY_HPP

#include "NOX_Abstract_PrePostOperator.H"
#include "Teuchos_RCP.hpp"
#include "Teuchos_Assert.hpp"

#include "Panzer_config.hpp"
#include "Panzer_Traits.hpp"
#include "Panzer_UniqueGlobalIndexer.hpp"
#include "Panzer_LinearObjFactory.hpp"

#include "Panzer_STK_Interface.hpp"
#include "Panzer_STK_Utilities.hpp"

namespace panzer_stk {

  class NOXObserverFactory {

  public:
    
    virtual ~NOXObserverFactory() {}

    virtual Teuchos::RCP<NOX::Abstract::PrePostOperator>
    buildNOXObserver(const Teuchos::RCP<panzer_stk::STK_Interface>& mesh,
		     const Teuchos::RCP<panzer::UniqueGlobalIndexer<int,int> >& dof_manager,
		     const Teuchos::RCP<panzer::LinearObjFactory<panzer::Traits> >& lof) const = 0;
  };

}

#endif
