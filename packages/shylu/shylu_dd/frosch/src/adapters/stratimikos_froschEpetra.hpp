

#include "Stratimikos_DefaultLinearSolverBuilder.hpp"



#include <Thyra_FROSchEpetraPreconFactory_decl.hpp>



#include "Teuchos_RCP.hpp"
#include "Teuchos_ParameterList.hpp"
#include "Teuchos_TestForException.hpp"
#include "Teuchos_AbstractFactoryStd.hpp"

#include <string>
#include "Kokkos_DefaultNode.hpp"


namespace Stratimikos {
    
    template <typename LocalOrdinal = int, typename GlobalOrdinal = int, typename Node = KokkosClassic::DefaultNode::DefaultNodeType>
    void enableFROSch(DefaultLinearSolverBuilder& builder, const std::string& stratName = "FROSch"){
        const Teuchos::RCP<const Teuchos::ParameterList> precValidParams = Teuchos::sublist(builder.getValidParameters(), "Preconditioner Types");
        
        TEUCHOS_TEST_FOR_EXCEPTION(precValidParams->isParameter(stratName), std::logic_error,
                                   "Stratimikos::enableFROSch cannot add \"" + stratName +"\" because it is already included in builder!");
        
        typedef Thyra::PreconditionerFactoryBase<double> Base;
        typedef Thyra::FROSch_EpetraPreconFactory<double, LocalOrdinal, GlobalOrdinal, Node> Impl;
        
        builder.setPreconditioningStrategyFactory(Teuchos::abstractFactoryStd<Base, Impl>(), stratName);
    }
    
}

