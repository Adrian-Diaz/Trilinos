#ifndef MUELU_SCHURCOMPLEMENTFACTORY_DEF_HPP_
#define MUELU_SCHURCOMPLEMENTFACTORY_DEF_HPP_

#include <Xpetra_BlockedCrsOperator.hpp>
#include <Xpetra_MultiVectorFactory.hpp>
#include <Xpetra_VectorFactory.hpp>
#include <Xpetra_OperatorFactory.hpp>
#include <Xpetra_Operator.hpp>
#include <Xpetra_CrsOperator.hpp>
#include <Xpetra_BlockedCrsOperator.hpp>
#include <Xpetra_CrsMatrix.hpp>
#include "MueLu_Level.hpp"
#include "MueLu_Monitor.hpp"
#include "MueLu_Utilities.hpp"
#include "MueLu_HierarchyHelpers.hpp"

#include "MueLu_SchurComplementFactory.hpp"

namespace MueLu {

template <class Scalar, class LocalOrdinal, class GlobalOrdinal, class Node, class LocalMatOps>
SchurComplementFactory<Scalar, LocalOrdinal, GlobalOrdinal, Node, LocalMatOps>::SchurComplementFactory(Teuchos::RCP<const FactoryBase> AFact, Scalar omega/**size_t row, size_t col, LocalOrdinal blksize*/)
: AFact_(AFact), omega_(omega)
  { }

template <class Scalar, class LocalOrdinal, class GlobalOrdinal, class Node, class LocalMatOps>
SchurComplementFactory<Scalar, LocalOrdinal, GlobalOrdinal, Node, LocalMatOps>::~SchurComplementFactory() {}

template <class Scalar, class LocalOrdinal, class GlobalOrdinal, class Node, class LocalMatOps>
void SchurComplementFactory<Scalar, LocalOrdinal, GlobalOrdinal, Node, LocalMatOps>::DeclareInput(Level &currentLevel) const {
  currentLevel.DeclareInput("A",AFact_.get(),this);
}

/*template <class Scalar,class LocalOrdinal, class GlobalOrdinal, class Node, class LocalMatOps>
void SchurComplementFactory<Scalar, LocalOrdinal, GlobalOrdinal, Node, LocalMatOps>::AddFactoryManager(RCP<const FactoryManagerBase> FactManager) {
  //FactManager_ = FactManager;
}*/

template <class Scalar, class LocalOrdinal, class GlobalOrdinal, class Node, class LocalMatOps>
void SchurComplementFactory<Scalar, LocalOrdinal, GlobalOrdinal, Node, LocalMatOps>::Build(Level & currentLevel) const
{
  FactoryMonitor  m(*this, "Building Schur Complement", currentLevel);
  Teuchos::RCP<Operator> A = currentLevel.Get<RCP<Operator> >("A", AFact_.get());

  RCP<BlockedCrsOperator> bA = Teuchos::rcp_dynamic_cast<BlockedCrsOperator>(A);
  TEUCHOS_TEST_FOR_EXCEPTION(bA == Teuchos::null, Exceptions::BadCast, "MueLu::BlockedPFactory::Build: input matrix A is not of type BlockedCrsOperator! A generated by AFact_ must be a 2x2 block operator. error.");

  Teuchos::RCP<CrsMatrix> A00 = bA->getMatrix(0,0);
  Teuchos::RCP<CrsMatrix> A01 = bA->getMatrix(0,1);
  Teuchos::RCP<CrsMatrix> A10 = bA->getMatrix(1,0);
  Teuchos::RCP<CrsMatrix> A11 = bA->getMatrix(1,1);

  Teuchos::RCP<CrsOperator> Op00 = Teuchos::rcp(new CrsOperator(A00));
  Teuchos::RCP<CrsOperator> Op01 = Teuchos::rcp(new CrsOperator(A01));
  Teuchos::RCP<CrsOperator> Op10 = Teuchos::rcp(new CrsOperator(A10));
  Teuchos::RCP<CrsOperator> Op11 = Teuchos::rcp(new CrsOperator(A11));

  Teuchos::RCP<Operator> F = Teuchos::rcp_dynamic_cast<Operator>(Op00);
  Teuchos::RCP<Operator> G = Teuchos::rcp_dynamic_cast<Operator>(Op01);
  Teuchos::RCP<Operator> D = Teuchos::rcp_dynamic_cast<Operator>(Op10);
  Teuchos::RCP<Operator> Z = Teuchos::rcp_dynamic_cast<Operator>(Op11);


  // extract diagonal of F. store it in ArrayRCP object
  Teuchos::ArrayRCP<SC> AdiagFinv = Utils::GetMatrixDiagonal(F);

  //copy the value of G so we can do the left scale. To do so, a new Operator is built and them the original values of G are added.
  RCP<Operator> FhatinvG = OperatorFactory::Build(G->getRowMap(), G->getGlobalMaxNumRowEntries());
  Utils2::TwoMatrixAdd(G, false, 1.0, FhatinvG, 0.0);
  FhatinvG->fillComplete(G->getDomainMap(),G->getRowMap()); // complete the matrix. left scaling does not change the pattern of the operator.
  // Here the first boolean is true, it means we multiply by the inverse of the diagonal of F_
  Utils::MyOldScaleMatrix(FhatinvG,AdiagFinv,true,false,false);

  // build D \hat{F}^{-1} G
  RCP<Operator> DFhatinvG = Utils::TwoMatrixMultiply(D,false,FhatinvG,false);

  // build full SchurComplement operator
  // S = - 1/omega D \hat{F}^{-1} G + Z
  RCP<Operator> S;
  Utils2::TwoMatrixAdd(Z,false,1.0,DFhatinvG,false,-1.0/omega_,S);
  S->fillComplete();

  {
    // note: variable "A" generated by this SchurComplement factory is in fact the SchurComplement matrix
    // we have to use the variable name "A" since the Smoother object expects the matrix to be called "A"
    currentLevel.Set("A", S, this);
  }
}

} // namespace MueLu

#endif /* MUELU_SCHURCOMPLEMENTFACTORY_DEF_HPP_ */
