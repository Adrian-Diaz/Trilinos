// @HEADER
//
// ***********************************************************************
//
//        MueLu: A package for multigrid based preconditioning
//                  Copyright 2012 Sandia Corporation
//
// Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
// the U.S. Government retains certain rights in this software.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the Corporation nor the names of the
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY SANDIA CORPORATION "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SANDIA CORPORATION OR THE
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact
//                    Jonathan Hu       (jhu@sandia.gov)
//                    Andrey Prokopenko (aprokop@sandia.gov)
//                    Ray Tuminaro      (rstumin@sandia.gov)
//
// ***********************************************************************
//
// @HEADER
#include <iostream>
#include <map>

// Teuchos
#include <Teuchos_StandardCatchMacros.hpp>
#include <Teuchos_XMLParameterListHelpers.hpp>
#include <Teuchos_StackedTimer.hpp>

// Xpetra
#include <Xpetra_Map.hpp>
#include <Xpetra_MapFactory.hpp>
#include <Xpetra_MultiVector.hpp>
#include <Xpetra_Vector.hpp>
#include <Xpetra_MultiVectorFactory.hpp>
#include <Xpetra_CrsMatrix.hpp>
#include <Xpetra_CrsMatrixFactory.hpp>
#include <Xpetra_Parameters.hpp>
#include <Xpetra_IO.hpp>

// MueLu
#include <MueLu_RefMaxwell.hpp>
#include <MueLu_TpetraOperator.hpp>
#include <MueLu_TestHelpers_Common.hpp>
#include <MueLu_Exceptions.hpp>

using Teuchos::RCP;
using Teuchos::rcp;
using Teuchos::TimeMonitor;

#ifdef HAVE_MUELU_TPETRA
#include <MueLu_TpetraOperator.hpp>
#endif

// Belos
#ifdef HAVE_MUELU_BELOS
#include <BelosConfigDefs.hpp>
#include <BelosLinearProblem.hpp>
#include <BelosSolverFactory.hpp>
#include <BelosTpetraAdapter.hpp>
#include <BelosXpetraAdapter.hpp>     // => This header defines Belos::XpetraOp
#endif

// Stratimikos
#if defined(HAVE_MUELU_STRATIMIKOS) && defined(HAVE_MUELU_THYRA)
// Thyra includes
#include <Thyra_LinearOpWithSolveBase.hpp>
#include <Thyra_VectorBase.hpp>
#include <Thyra_SolveSupportTypes.hpp>
// Stratimikos includes
#include <Stratimikos_DefaultLinearSolverBuilder.hpp>
#include <Stratimikos_MueLuHelpers.hpp>
#endif

// Support for ML interface
#if defined(HAVE_MUELU_ML) and defined(HAVE_MUELU_EPETRA)
#include <Xpetra_EpetraOperator.hpp>
#include "ml_MultiLevelPreconditioner.h"
#include "ml_MultiLevelOperator.h"
#include "ml_RefMaxwell.h"
#endif

// Helper functions for compilation purposes
#ifdef TPETRA_ENABLE_TEMPLATE_ORDINALS
template<class Scalar, class LocalOrdinal, class GlobalOrdinal, class Node>
#else
template<class Scalar, class Node>
#endif
struct EpetraSolvers_Wrapper{
#ifdef TPETRA_ENABLE_TEMPLATE_ORDINALS
  static void Generate_ML_MaxwellPreconditioner(Teuchos::RCP<Xpetra::Matrix<Scalar,LocalOrdinal,GlobalOrdinal,Node> >& SM,
                                                Teuchos::RCP<Xpetra::Matrix<Scalar,LocalOrdinal,GlobalOrdinal,Node> >& D0,
                                                Teuchos::RCP<Xpetra::Matrix<Scalar,LocalOrdinal,GlobalOrdinal,Node> >& Kn,
                                                Teuchos::RCP<Xpetra::MultiVector<Scalar,LocalOrdinal,GlobalOrdinal,Node> >& nullspace,
                                                Teuchos::RCP<Xpetra::MultiVector<typename Teuchos::ScalarTraits<Scalar>::coordinateType,LocalOrdinal,GlobalOrdinal,Node> >& coords,
#else
  using LocalOrdinal = typename Tpetra::Map<>::local_ordinal_type;
  using GlobalOrdinal = typename Tpetra::Map<>::global_ordinal_type;
  static void Generate_ML_MaxwellPreconditioner(Teuchos::RCP<Xpetra::Matrix<Scalar,Node> >& SM,
                                                Teuchos::RCP<Xpetra::Matrix<Scalar,Node> >& D0,
                                                Teuchos::RCP<Xpetra::Matrix<Scalar,Node> >& Kn,
                                                Teuchos::RCP<Xpetra::MultiVector<Scalar,Node> >& nullspace,
                                                Teuchos::RCP<Xpetra::MultiVector<typename Teuchos::ScalarTraits<Scalar>::coordinateType,Node> >& coords,
#endif
                                                Teuchos::ParameterList & mueluList,
#ifdef TPETRA_ENABLE_TEMPLATE_ORDINALS
                                                Teuchos::RCP<Xpetra::Operator<Scalar,LocalOrdinal,GlobalOrdinal,Node> >& mlopX) {
#else
                                                Teuchos::RCP<Xpetra::Operator<Scalar,Node> >& mlopX) {
#endif
    throw std::runtime_error("Template parameter mismatch");
  }

#ifdef TPETRA_ENABLE_TEMPLATE_ORDINALS
  static void Generate_ML_RefMaxwellPreconditioner(Teuchos::RCP<Xpetra::Matrix<Scalar,LocalOrdinal,GlobalOrdinal,Node> >& SM,
                                                   Teuchos::RCP<Xpetra::Matrix<Scalar,LocalOrdinal,GlobalOrdinal,Node> >& D0,
                                                   Teuchos::RCP<Xpetra::Matrix<Scalar,LocalOrdinal,GlobalOrdinal,Node> >& Ms,
                                                   Teuchos::RCP<Xpetra::Matrix<Scalar,LocalOrdinal,GlobalOrdinal,Node> >& M0inv,
                                                   Teuchos::RCP<Xpetra::Matrix<Scalar,LocalOrdinal,GlobalOrdinal,Node> >& M1,
                                                   Teuchos::RCP<Xpetra::MultiVector<Scalar,LocalOrdinal,GlobalOrdinal,Node> >& nullspace,
                                                   Teuchos::RCP<Xpetra::MultiVector<Scalar,LocalOrdinal,GlobalOrdinal,Node> >& node_material,
                                                   Teuchos::RCP<Xpetra::MultiVector<typename Teuchos::ScalarTraits<Scalar>::coordinateType,LocalOrdinal,GlobalOrdinal,Node> >& coords,
#else
  static void Generate_ML_RefMaxwellPreconditioner(Teuchos::RCP<Xpetra::Matrix<Scalar,Node> >& SM,
                                                   Teuchos::RCP<Xpetra::Matrix<Scalar,Node> >& D0,
                                                   Teuchos::RCP<Xpetra::Matrix<Scalar,Node> >& Ms,
                                                   Teuchos::RCP<Xpetra::Matrix<Scalar,Node> >& M0inv,
                                                   Teuchos::RCP<Xpetra::Matrix<Scalar,Node> >& M1,
                                                   Teuchos::RCP<Xpetra::MultiVector<Scalar,Node> >& nullspace,
                                                   Teuchos::RCP<Xpetra::MultiVector<Scalar,Node> >& node_material,
                                                   Teuchos::RCP<Xpetra::MultiVector<typename Teuchos::ScalarTraits<Scalar>::coordinateType,Node> >& coords,
#endif
                                                   Teuchos::ParameterList & mueluList,
#ifdef TPETRA_ENABLE_TEMPLATE_ORDINALS
                                                   Teuchos::RCP<Xpetra::Operator<Scalar,LocalOrdinal,GlobalOrdinal,Node> > & mlopX) {
#else
                                                   Teuchos::RCP<Xpetra::Operator<Scalar,Node> > & mlopX) {
#endif
    throw std::runtime_error("Template parameter mismatch");
  }
};

#if defined(HAVE_MUELU_EPETRA)
template<class GlobalOrdinal>
#ifdef TPETRA_ENABLE_TEMPLATE_ORDINALS
struct EpetraSolvers_Wrapper<double,int,GlobalOrdinal,Kokkos::Compat::KokkosSerialWrapperNode> {
  static void Generate_ML_MaxwellPreconditioner(Teuchos::RCP<Xpetra::Matrix<double,int,GlobalOrdinal,Kokkos::Compat::KokkosSerialWrapperNode> >& SM,
                                                Teuchos::RCP<Xpetra::Matrix<double,int,GlobalOrdinal,Kokkos::Compat::KokkosSerialWrapperNode> >& D0,
                                                Teuchos::RCP<Xpetra::Matrix<double,int,GlobalOrdinal,Kokkos::Compat::KokkosSerialWrapperNode> >& Kn,
                                                Teuchos::RCP<Xpetra::MultiVector<double,int,GlobalOrdinal,Kokkos::Compat::KokkosSerialWrapperNode> >& nullspace,
#else
struct EpetraSolvers_Wrapper<double,Kokkos::Compat::KokkosSerialWrapperNode> {
  static void Generate_ML_MaxwellPreconditioner(Teuchos::RCP<Xpetra::Matrix<double,Kokkos::Compat::KokkosSerialWrapperNode> >& SM,
                                                Teuchos::RCP<Xpetra::Matrix<double,Kokkos::Compat::KokkosSerialWrapperNode> >& D0,
                                                Teuchos::RCP<Xpetra::Matrix<double,Kokkos::Compat::KokkosSerialWrapperNode> >& Kn,
                                                Teuchos::RCP<Xpetra::MultiVector<double,Kokkos::Compat::KokkosSerialWrapperNode> >& nullspace,
#endif
                                                Teuchos::RCP<Xpetra::MultiVector<typename Teuchos::ScalarTraits<double>::coordinateType,int,GlobalOrdinal,Kokkos::Compat::KokkosSerialWrapperNode> >& coords,
                                                Teuchos::ParameterList & mueluList,
#ifdef TPETRA_ENABLE_TEMPLATE_ORDINALS
                                                Teuchos::RCP<Xpetra::Operator<double,int,GlobalOrdinal,Kokkos::Compat::KokkosSerialWrapperNode> >& mlopX) {
#else
                                                Teuchos::RCP<Xpetra::Operator<double,Kokkos::Compat::KokkosSerialWrapperNode> >& mlopX) {
#endif
#if defined(HAVE_MUELU_ML)
    typedef double SC;
    typedef int LO;
    typedef GlobalOrdinal GO;
    typedef Kokkos::Compat::KokkosSerialWrapperNode NO;
    typedef typename Teuchos::ScalarTraits<SC>::coordinateType coordinate_type;
#ifdef TPETRA_ENABLE_TEMPLATE_ORDINALS
    typedef typename Xpetra::Matrix<SC,LO,GO,NO> Matrix;
#else
    typedef typename Xpetra::Matrix<SC,NO> Matrix;
#endif

#ifdef TPETRA_ENABLE_TEMPLATE_ORDINALS
    RCP<const Epetra_CrsMatrix> epetraSM = Xpetra::Helpers<SC, LO, GO, NO>::Op2EpetraCrs(SM);
    RCP<const Epetra_CrsMatrix> epetraD0 = Xpetra::Helpers<SC, LO, GO, NO>::Op2EpetraCrs(D0);
#else
    RCP<const Epetra_CrsMatrix> epetraSM = Xpetra::Helpers<SC, NO>::Op2EpetraCrs(SM);
    RCP<const Epetra_CrsMatrix> epetraD0 = Xpetra::Helpers<SC, NO>::Op2EpetraCrs(D0);
#endif
    if(!coords.is_null()) {
#ifdef TPETRA_ENABLE_TEMPLATE_ORDINALS
      RCP<const Epetra_MultiVector> epetraCoord =  MueLu::Utilities<coordinate_type,LO,GO,NO>::MV2EpetraMV(coords);
#else
      RCP<const Epetra_MultiVector> epetraCoord =  MueLu::Utilities<coordinate_type,NO>::MV2EpetraMV(coords);
#endif
      if(epetraCoord->NumVectors() > 0)  mueluList.set("x-coordinates",(*epetraCoord)[0]);
      if(epetraCoord->NumVectors() > 1)  mueluList.set("y-coordinates",(*epetraCoord)[1]);
      if(epetraCoord->NumVectors() > 2)  mueluList.set("z-coordinates",(*epetraCoord)[2]);
    }
    if(!nullspace.is_null()) {
#ifdef TPETRA_ENABLE_TEMPLATE_ORDINALS
      RCP<const Epetra_MultiVector> epetraNullspace =  MueLu::Utilities<SC,LO,GO,NO>::MV2EpetraMV(nullspace);
#else
      RCP<const Epetra_MultiVector> epetraNullspace =  MueLu::Utilities<SC,NO>::MV2EpetraMV(nullspace);
#endif
      mueluList.set("null space: dimension",epetraNullspace->NumVectors());
      mueluList.set("null space: vectors",(*epetraNullspace)[0]);
      mueluList.set("null space: type","pre-computed");
    }
    RCP<const Epetra_CrsMatrix> epetraKn;
    if (Kn.is_null()) {
#ifdef TPETRA_ENABLE_TEMPLATE_ORDINALS
      RCP<Matrix> temp = Xpetra::MatrixFactory<SC,LO,GO,NO>::Build(SM->getRangeMap());
      Xpetra::MatrixMatrix<SC,LO,GO,NO>::Multiply(*SM,false,*D0,false,*temp,true,true);
      RCP<Matrix> Kn2 = Xpetra::MatrixFactory<SC,LO,GO,NO>::Build(D0->getDomainMap());
      Xpetra::MatrixMatrix<SC,LO,GO,NO>::Multiply(*D0,true,*temp,false,*Kn2,true,true);
      epetraKn = Xpetra::Helpers<SC, LO, GO, NO>::Op2EpetraCrs(Kn2);
#else
      RCP<Matrix> temp = Xpetra::MatrixFactory<SC,NO>::Build(SM->getRangeMap());
      Xpetra::MatrixMatrix<SC,NO>::Multiply(*SM,false,*D0,false,*temp,true,true);
      RCP<Matrix> Kn2 = Xpetra::MatrixFactory<SC,NO>::Build(D0->getDomainMap());
      Xpetra::MatrixMatrix<SC,NO>::Multiply(*D0,true,*temp,false,*Kn2,true,true);
      epetraKn = Xpetra::Helpers<SC, NO>::Op2EpetraCrs(Kn2);
#endif
    } else
#ifdef TPETRA_ENABLE_TEMPLATE_ORDINALS
      epetraKn = Xpetra::Helpers<SC, LO, GO, NO>::Op2EpetraCrs(Kn);
#else
      epetraKn = Xpetra::Helpers<SC, NO>::Op2EpetraCrs(Kn);
#endif

    RCP<Epetra_Operator> mlop  = rcp<Epetra_Operator>(new ML_Epetra::MultiLevelPreconditioner(*epetraSM,*epetraD0,*epetraKn,mueluList,true));
#if defined(HAVE_MUELU_BELOS)
    // NOTE: Belos needs the Apply() and AppleInverse() routines of ML swapped.  So...
    mlop = rcp<Belos::EpetraPrecOp>(new Belos::EpetraPrecOp(mlop));
#endif

    mlopX = rcp(new Xpetra::EpetraOperator<GO,NO>(mlop));
#else
    TEUCHOS_TEST_FOR_EXCEPTION(true, std::runtime_error,
                               "Need ML & Epetra support");
#endif
  }

#ifdef TPETRA_ENABLE_TEMPLATE_ORDINALS
  static void Generate_ML_RefMaxwellPreconditioner(Teuchos::RCP<Xpetra::Matrix<double,int,GlobalOrdinal,Kokkos::Compat::KokkosSerialWrapperNode> >& SM,
                                                   Teuchos::RCP<Xpetra::Matrix<double,int,GlobalOrdinal,Kokkos::Compat::KokkosSerialWrapperNode> >& D0,
                                                   Teuchos::RCP<Xpetra::Matrix<double,int,GlobalOrdinal,Kokkos::Compat::KokkosSerialWrapperNode> >& Ms,
                                                   Teuchos::RCP<Xpetra::Matrix<double,int,GlobalOrdinal,Kokkos::Compat::KokkosSerialWrapperNode> >& M0inv,
                                                   Teuchos::RCP<Xpetra::Matrix<double,int,GlobalOrdinal,Kokkos::Compat::KokkosSerialWrapperNode> >& M1,
                                                   Teuchos::RCP<Xpetra::MultiVector<double,int,GlobalOrdinal,Kokkos::Compat::KokkosSerialWrapperNode> >& nullspace,
                                                   Teuchos::RCP<Xpetra::MultiVector<double,int,GlobalOrdinal,Kokkos::Compat::KokkosSerialWrapperNode> >& node_material,
#else
  static void Generate_ML_RefMaxwellPreconditioner(Teuchos::RCP<Xpetra::Matrix<double,Kokkos::Compat::KokkosSerialWrapperNode> >& SM,
                                                   Teuchos::RCP<Xpetra::Matrix<double,Kokkos::Compat::KokkosSerialWrapperNode> >& D0,
                                                   Teuchos::RCP<Xpetra::Matrix<double,Kokkos::Compat::KokkosSerialWrapperNode> >& Ms,
                                                   Teuchos::RCP<Xpetra::Matrix<double,Kokkos::Compat::KokkosSerialWrapperNode> >& M0inv,
                                                   Teuchos::RCP<Xpetra::Matrix<double,Kokkos::Compat::KokkosSerialWrapperNode> >& M1,
                                                   Teuchos::RCP<Xpetra::MultiVector<double,Kokkos::Compat::KokkosSerialWrapperNode> >& nullspace,
                                                   Teuchos::RCP<Xpetra::MultiVector<double,Kokkos::Compat::KokkosSerialWrapperNode> >& node_material,
#endif
                                                   Teuchos::RCP<Xpetra::MultiVector<typename Teuchos::ScalarTraits<double>::coordinateType,int,GlobalOrdinal,Kokkos::Compat::KokkosSerialWrapperNode> >& coords,
                                                   Teuchos::ParameterList & mueluList,
#ifdef TPETRA_ENABLE_TEMPLATE_ORDINALS
                                                   Teuchos::RCP<Xpetra::Operator<double,int,GlobalOrdinal,Kokkos::Compat::KokkosSerialWrapperNode> >& mlopX) {
#else
                                                   Teuchos::RCP<Xpetra::Operator<double,Kokkos::Compat::KokkosSerialWrapperNode> >& mlopX) {
#endif
#if defined(HAVE_MUELU_ML)
    typedef double SC;
    typedef int LO;
    typedef GlobalOrdinal GO;
    typedef Kokkos::Compat::KokkosSerialWrapperNode NO;
    typedef typename Teuchos::ScalarTraits<SC>::coordinateType coordinate_type;

#ifdef TPETRA_ENABLE_TEMPLATE_ORDINALS
    RCP<const Epetra_CrsMatrix> epetraSM    = Xpetra::Helpers<SC, LO, GO, NO>::Op2EpetraCrs(SM);
    RCP<const Epetra_CrsMatrix> epetraD0    = Xpetra::Helpers<SC, LO, GO, NO>::Op2EpetraCrs(D0);
    RCP<const Epetra_CrsMatrix> epetraM0inv = Xpetra::Helpers<SC, LO, GO, NO>::Op2EpetraCrs(M0inv);
#else
    RCP<const Epetra_CrsMatrix> epetraSM    = Xpetra::Helpers<SC, NO>::Op2EpetraCrs(SM);
    RCP<const Epetra_CrsMatrix> epetraD0    = Xpetra::Helpers<SC, NO>::Op2EpetraCrs(D0);
    RCP<const Epetra_CrsMatrix> epetraM0inv = Xpetra::Helpers<SC, NO>::Op2EpetraCrs(M0inv);
#endif
    RCP<const Epetra_CrsMatrix> epetraMs;
#ifdef TPETRA_ENABLE_TEMPLATE_ORDINALS
    RCP<const Epetra_CrsMatrix> epetraM1    = Xpetra::Helpers<SC, LO, GO, NO>::Op2EpetraCrs(M1);
#else
    RCP<const Epetra_CrsMatrix> epetraM1    = Xpetra::Helpers<SC, NO>::Op2EpetraCrs(M1);
#endif
    if (!Ms.is_null())
#ifdef TPETRA_ENABLE_TEMPLATE_ORDINALS
      epetraMs    = Xpetra::Helpers<SC, LO, GO, NO>::Op2EpetraCrs(Ms);
#else
      epetraMs    = Xpetra::Helpers<SC, NO>::Op2EpetraCrs(Ms);
#endif
    else
      epetraMs = epetraM1;
    mueluList.set("D0",epetraD0);
    mueluList.set("Ms",epetraMs);
    mueluList.set("M0inv",epetraM0inv);
    mueluList.set("M1",epetraM1);
    if(!coords.is_null()) {
#ifdef TPETRA_ENABLE_TEMPLATE_ORDINALS
      RCP<const Epetra_MultiVector> epetraCoord =  MueLu::Utilities<coordinate_type,LO,GO,NO>::MV2EpetraMV(coords);
#else
      RCP<const Epetra_MultiVector> epetraCoord =  MueLu::Utilities<coordinate_type,NO>::MV2EpetraMV(coords);
#endif
      if(epetraCoord->NumVectors() > 0)  mueluList.sublist("refmaxwell: 11list").set("x-coordinates",(*epetraCoord)[0]);
      if(epetraCoord->NumVectors() > 1)  mueluList.sublist("refmaxwell: 11list").set("y-coordinates",(*epetraCoord)[1]);
      if(epetraCoord->NumVectors() > 2)  mueluList.sublist("refmaxwell: 11list").set("z-coordinates",(*epetraCoord)[2]);
    }
    if(!node_material.is_null()) {
#ifdef TPETRA_ENABLE_TEMPLATE_ORDINALS
      RCP<const Epetra_MultiVector> epetraMaterial =  MueLu::Utilities<coordinate_type,LO,GO,NO>::MV2EpetraMV(node_material);
#else
      RCP<const Epetra_MultiVector> epetraMaterial =  MueLu::Utilities<coordinate_type,NO>::MV2EpetraMV(node_material);
#endif
      mueluList.sublist("refmaxwell: 11list").set("material coordinates",(*epetraMaterial)[0]);
    }
    if(!nullspace.is_null()) {
#ifdef TPETRA_ENABLE_TEMPLATE_ORDINALS
      RCP<const Epetra_MultiVector> epetraNullspace =  MueLu::Utilities<SC,LO,GO,NO>::MV2EpetraMV(nullspace);
#else
      RCP<const Epetra_MultiVector> epetraNullspace =  MueLu::Utilities<SC,NO>::MV2EpetraMV(nullspace);
#endif
      mueluList.sublist("refmaxwell: 11list").set("null space: dimension",epetraNullspace->NumVectors());
      mueluList.sublist("refmaxwell: 11list").set("null space: vectors",(*epetraNullspace)[0]);
      mueluList.sublist("refmaxwell: 11list").set("null space: type","pre-computed");
    }

    RCP<Epetra_Operator> mlop  = rcp<Epetra_Operator>(new ML_Epetra::RefMaxwellPreconditioner(*epetraSM,mueluList,true));
#if defined(HAVE_MUELU_BELOS)
    // NOTE: Belos needs the Apply() and AppleInverse() routines of ML swapped.  So...
    mlop = rcp<Belos::EpetraPrecOp>(new Belos::EpetraPrecOp(mlop));
#endif

    mlopX = rcp(new Xpetra::EpetraOperator<GO,NO>(mlop));
#else
    TEUCHOS_TEST_FOR_EXCEPTION(true, std::runtime_error,
                               "Need ML & Epetra support");
#endif
  }
};
#endif // HAVE_MUELU_EPETRA

// Setup & solve wrappers struct
// Because C++ doesn't support partial template specialization of functions.
// By default, do not try to run Stratimikos, since that only works for Scalar=double.
#ifdef TPETRA_ENABLE_TEMPLATE_ORDINALS
template<typename Scalar,class LocalOrdinal,class GlobalOrdinal,class Node>
#else
template<typename Scalar,class Node>
#endif
struct SetupSolveWrappers {
#ifndef TPETRA_ENABLE_TEMPLATE_ORDINALS
  using LocalOrdinal = typename Tpetra::Map<>::local_ordinal_type;
  using GlobalOrdinal = typename Tpetra::Map<>::global_ordinal_type;
#endif
  static bool SetupSolve(std::map<std::string, void*> inputs);
};


// Partial template specialization on SC=double
// This code branch gives the option to run with Stratimikos.
#ifdef TPETRA_ENABLE_TEMPLATE_ORDINALS
template<class LocalOrdinal, class GlobalOrdinal, class Node>
struct SetupSolveWrappers<double,LocalOrdinal,GlobalOrdinal,Node> {
#else
template<class Node>
struct SetupSolveWrappers<double,Node> {
  using LocalOrdinal = typename Tpetra::Map<>::local_ordinal_type;
  using GlobalOrdinal = typename Tpetra::Map<>::global_ordinal_type;
#endif
  static bool SetupSolve(std::map<std::string, void*> inputs);
};


// By default, do not try to run Stratimikos, since that only works for Scalar=double.
#ifdef TPETRA_ENABLE_TEMPLATE_ORDINALS
template<class Scalar, class LocalOrdinal, class GlobalOrdinal, class Node>
bool SetupSolveWrappers<Scalar,LocalOrdinal,GlobalOrdinal,Node>::SetupSolve(std::map<std::string, void*> inputs) {
#else
template<class Scalar, class Node>
bool SetupSolveWrappers<Scalar,Node>::SetupSolve(std::map<std::string, void*> inputs) {
#endif
#include <MueLu_UseShortNames.hpp>

#ifdef TPETRA_ENABLE_TEMPLATE_ORDINALS
  typedef Xpetra::MultiVector<typename Teuchos::ScalarTraits<Scalar>::magnitudeType, LO, GO, NO> coordMV;
#else
  typedef Xpetra::MultiVector<typename Teuchos::ScalarTraits<Scalar>::magnitudeType,NO> coordMV;
#endif

  RCP<Matrix>            SM_Matrix       = *static_cast<RCP<Matrix>*>(inputs["SM"]);
  RCP<Matrix>            D0_Matrix       = *static_cast<RCP<Matrix>*>(inputs["D0"]);
  RCP<Matrix>            M1_Matrix       = *static_cast<RCP<Matrix>*>(inputs["M1"]);
  RCP<Matrix>            Ms_Matrix       = *static_cast<RCP<Matrix>*>(inputs["Ms"]);
  RCP<Matrix>            M0inv_Matrix    = *static_cast<RCP<Matrix>*>(inputs["M0inv"]);
  RCP<Matrix>            Kn_Matrix       = *static_cast<RCP<Matrix>*>(inputs["Kn"]);

  RCP<coordMV>           coords          = *static_cast<RCP<coordMV>*>(inputs["coordinates"]);
  RCP<MultiVector>       nullspace       = *static_cast<RCP<MultiVector>*>(inputs["nullspace"]);
  RCP<MultiVector>       material        = *static_cast<RCP<MultiVector>*>(inputs["material"]);

  RCP<MultiVector>       B               = *static_cast<RCP<MultiVector>*>(inputs["B"]);
  RCP<MultiVector>       X               = *static_cast<RCP<MultiVector>*>(inputs["X"]);
  RCP<MultiVector>       X0              = *static_cast<RCP<MultiVector>*>(inputs["X0"]);

  Teuchos::ParameterList params          = *static_cast<Teuchos::ParameterList*>(inputs["params"]);
  Teuchos::ParameterList belosParams     = *static_cast<Teuchos::ParameterList*>(inputs["belosParams"]);
  std::string            solverName      = *static_cast<std::string*>(inputs["solverName"]);
  std::string            belosSolverType = *static_cast<std::string*>(inputs["belosSolverType"]);
  std::string            precType        = *static_cast<std::string*>(inputs["precType"]);
  int                    numResolves     = *static_cast<int*>(inputs["numResolves"]);

  RCP<const Teuchos::Comm<int> > comm    = *static_cast<RCP<const Teuchos::Comm<int> >*>(inputs["comm"]);
  RCP<Teuchos::FancyOStream> out         = *static_cast<RCP<Teuchos::FancyOStream>*>(inputs["out"]);

  bool success = false;

  auto tm2 = TimeMonitor::getNewTimer("Maxwell: 2 - Build solver and preconditioner");
#ifdef HAVE_MUELU_BELOS
  if (solverName == "Belos") {
    // construct preconditioner
    RCP<Operator> preconditioner;
    if (precType=="MueLu-RefMaxwell") {
#ifdef TPETRA_ENABLE_TEMPLATE_ORDINALS
      preconditioner = rcp( new MueLu::RefMaxwell<SC,LO,GO,NO>(SM_Matrix,D0_Matrix,Ms_Matrix,M0inv_Matrix,
#else
      preconditioner = rcp( new MueLu::RefMaxwell<SC,NO>(SM_Matrix,D0_Matrix,Ms_Matrix,M0inv_Matrix,
#endif
                                                               M1_Matrix,nullspace,coords,params) );
#ifdef HAVE_MUELU_TPETRA
      {
        // A test to make sure we can wrap this guy as a MueLu::TpetraOperator
        RCP<Operator> precOp = Teuchos::rcp_dynamic_cast<Operator>(preconditioner);
#ifdef TPETRA_ENABLE_TEMPLATE_ORDINALS
        MueLu::TpetraOperator<SC,LO,GO,NO> OpT(precOp);
#else
        MueLu::TpetraOperator<SC,NO> OpT(precOp);
#endif
      }
#endif // HAVE_MUELU_TPETRA
    }
    else
      *out << "Preconditioner not supported\n";


#ifdef HAVE_MUELU_TPETRA
    {
      // A test to make sure we can wrap this guy as a MueLu::TpetraOperator
      RCP<Operator> precOp = Teuchos::rcp_dynamic_cast<Operator>(preconditioner);
#ifdef TPETRA_ENABLE_TEMPLATE_ORDINALS
      MueLu::TpetraOperator<SC,LO,GO,NO> OpT(precOp);
#else
      MueLu::TpetraOperator<SC,NO> OpT(precOp);
#endif
    }
#endif

    // Belos linear problem
    typedef MultiVector          MV;
    typedef Belos::OperatorT<MV> OP;
#ifdef TPETRA_ENABLE_TEMPLATE_ORDINALS
    Teuchos::RCP<OP> belosOp   = Teuchos::rcp(new Belos::XpetraOp<SC, LO, GO, NO>(SM_Matrix)); // Turns a Xpetra::Matrix object into a Belos operator
#else
    Teuchos::RCP<OP> belosOp   = Teuchos::rcp(new Belos::XpetraOp<SC, NO>(SM_Matrix)); // Turns a Xpetra::Matrix object into a Belos operator
#endif

    RCP<Belos::LinearProblem<SC, MV, OP> > problem = rcp( new Belos::LinearProblem<SC, MV, OP>() );
    problem -> setOperator( belosOp );
    Teuchos::RCP<OP> belosPrecOp;
    if (precType != "none") {
#ifdef TPETRA_ENABLE_TEMPLATE_ORDINALS
      belosPrecOp = Teuchos::rcp(new Belos::XpetraOp<SC, LO, GO, NO>(preconditioner)); // Turns a Xpetra::Matrix object into a Belos operator
#else
      belosPrecOp = Teuchos::rcp(new Belos::XpetraOp<SC, NO>(preconditioner)); // Turns a Xpetra::Matrix object into a Belos operator
#endif
      problem -> setRightPrec( belosPrecOp );
    }
    problem -> setProblem( X, B );

    bool set = problem->setProblem();
    if (set == false) {
      *out << "\nERROR:  Belos::LinearProblem failed to set up correctly!" << std::endl;
      return false;
    }

    // Belos solver
    RCP< Belos::SolverManager<SC, MV, OP> > solver;
    RCP< Belos::SolverFactory<SC, MV,OP> > factory = rcp( new  Belos::SolverFactory<SC,MV,OP>() );
    solver = factory->create(belosSolverType,Teuchos::rcpFromRef(belosParams.sublist(belosSolverType)));

    comm->barrier();
    tm2 = Teuchos::null;

    auto tm3 = TimeMonitor::getNewTimer("Maxwell: 3 - Solve");

    // set problem and solve
    solver -> setProblem( problem );
    for(int solveno = 0; solveno<=numResolves; solveno++) {
      if (X0.is_null())
        X->putScalar(Teuchos::ScalarTraits<Scalar>::zero());
      else
        X = X0;
      Belos::ReturnType status = solver -> solve();
      int iters = solver -> getNumIters();
      success = (iters<50 && status == Belos::Converged);
      if (success)
        *out << "SUCCESS! Belos converged in " << iters << " iterations." << std::endl;
      else
        *out << "FAILURE! Belos did not converge fast enough." << std::endl;
    }
  }
#endif // HAVE_MUELU_BELOS
  comm->barrier();

  return success;
} // SetupSolve


// This code branch gives the option to run with Stratimikos.
#ifdef TPETRA_ENABLE_TEMPLATE_ORDINALS
template<class LocalOrdinal, class GlobalOrdinal, class Node>
bool SetupSolveWrappers<double,LocalOrdinal,GlobalOrdinal,Node>::SetupSolve(std::map<std::string, void*> inputs) {
#else
template<class Node>
bool SetupSolveWrappers<double,Node>::SetupSolve(std::map<std::string, void*> inputs) {
#endif
  typedef double Scalar;
#include <MueLu_UseShortNames.hpp>

#ifdef TPETRA_ENABLE_TEMPLATE_ORDINALS
  typedef Xpetra::MultiVector<typename Teuchos::ScalarTraits<Scalar>::magnitudeType, LO, GO, NO> coordMV;
#else
  typedef Xpetra::MultiVector<typename Teuchos::ScalarTraits<Scalar>::magnitudeType,NO> coordMV;
#endif

  RCP<Matrix>            SM_Matrix       = *static_cast<RCP<Matrix>*>(inputs["SM"]);
  RCP<Matrix>            D0_Matrix       = *static_cast<RCP<Matrix>*>(inputs["D0"]);
  RCP<Matrix>            M1_Matrix       = *static_cast<RCP<Matrix>*>(inputs["M1"]);
  RCP<Matrix>            Ms_Matrix       = *static_cast<RCP<Matrix>*>(inputs["Ms"]);
  RCP<Matrix>            M0inv_Matrix    = *static_cast<RCP<Matrix>*>(inputs["M0inv"]);
  RCP<Matrix>            Kn_Matrix       = *static_cast<RCP<Matrix>*>(inputs["Kn"]);

  RCP<coordMV>           coords          = *static_cast<RCP<coordMV>*>(inputs["coordinates"]);
  RCP<MultiVector>       nullspace       = *static_cast<RCP<MultiVector>*>(inputs["nullspace"]);
  RCP<MultiVector>       material        = *static_cast<RCP<MultiVector>*>(inputs["material"]);

  RCP<MultiVector>       B               = *static_cast<RCP<MultiVector>*>(inputs["B"]);
  RCP<MultiVector>       X               = *static_cast<RCP<MultiVector>*>(inputs["X"]);
  RCP<MultiVector>       X0              = *static_cast<RCP<MultiVector>*>(inputs["X0"]);

  Teuchos::ParameterList params          = *static_cast<Teuchos::ParameterList*>(inputs["params"]);
  Teuchos::ParameterList belosParams     = *static_cast<Teuchos::ParameterList*>(inputs["belosParams"]);
  std::string            solverName      = *static_cast<std::string*>(inputs["solverName"]);
  std::string            belosSolverType = *static_cast<std::string*>(inputs["belosSolverType"]);
  std::string            precType        = *static_cast<std::string*>(inputs["precType"]);
  int                    numResolves     = *static_cast<int*>(inputs["numResolves"]);

  RCP<const Teuchos::Comm<int> > comm    = *static_cast<RCP<const Teuchos::Comm<int> >*>(inputs["comm"]);
  RCP<Teuchos::FancyOStream> out         = *static_cast<RCP<Teuchos::FancyOStream>*>(inputs["out"]);

  bool success = false;

  auto tm2 = TimeMonitor::getNewTimer("Maxwell: 2 - Build solver and preconditioner");
#ifdef HAVE_MUELU_BELOS
  if (solverName == "Belos") {
    // construct preconditioner
    RCP<Operator> preconditioner;
    if (precType=="MueLu-RefMaxwell") {
#ifdef TPETRA_ENABLE_TEMPLATE_ORDINALS
      preconditioner = rcp( new MueLu::RefMaxwell<SC,LO,GO,NO>(SM_Matrix,D0_Matrix,Ms_Matrix,M0inv_Matrix,
#else
      preconditioner = rcp( new MueLu::RefMaxwell<SC,NO>(SM_Matrix,D0_Matrix,Ms_Matrix,M0inv_Matrix,
#endif
                                                               M1_Matrix,nullspace,coords,params) );
    }
#ifdef HAVE_MUELU_EPETRA
    else if (precType=="ML-RefMaxwell") {
      Xpetra::UnderlyingLib  lib = *static_cast<Xpetra::UnderlyingLib*>(inputs["lib"]);
      TEUCHOS_ASSERT(lib==Xpetra::UseEpetra);
#ifdef TPETRA_ENABLE_TEMPLATE_ORDINALS
      EpetraSolvers_Wrapper<SC, LO, GO, NO>::Generate_ML_RefMaxwellPreconditioner(SM_Matrix,D0_Matrix,Ms_Matrix,M0inv_Matrix,
#else
      EpetraSolvers_Wrapper<SC, NO>::Generate_ML_RefMaxwellPreconditioner(SM_Matrix,D0_Matrix,Ms_Matrix,M0inv_Matrix,
#endif
                                                                                  M1_Matrix,nullspace,material,coords,params,preconditioner);
    } else if (precType=="ML-Maxwell") {
      Xpetra::UnderlyingLib  lib = *static_cast<Xpetra::UnderlyingLib*>(inputs["lib"]);
      TEUCHOS_ASSERT(lib==Xpetra::UseEpetra);
#ifdef TPETRA_ENABLE_TEMPLATE_ORDINALS
      EpetraSolvers_Wrapper<SC, LO, GO, NO>::Generate_ML_MaxwellPreconditioner(SM_Matrix,D0_Matrix,Kn_Matrix,
#else
      EpetraSolvers_Wrapper<SC, NO>::Generate_ML_MaxwellPreconditioner(SM_Matrix,D0_Matrix,Kn_Matrix,
#endif
                                                                               nullspace,coords,params,preconditioner);
    }
#endif

#ifdef HAVE_MUELU_TPETRA
    {
      // A test to make sure we can wrap this guy as a MueLu::TpetraOperator
      RCP<Operator> precOp = Teuchos::rcp_dynamic_cast<Operator>(preconditioner);
#ifdef TPETRA_ENABLE_TEMPLATE_ORDINALS
      MueLu::TpetraOperator<SC,LO,GO,NO> OpT(precOp);
#else
      MueLu::TpetraOperator<SC,NO> OpT(precOp);
#endif
    }
#endif

    // Belos linear problem
    typedef MultiVector          MV;
    typedef Belos::OperatorT<MV> OP;
#ifdef TPETRA_ENABLE_TEMPLATE_ORDINALS
    RCP<OP> belosOp   = Teuchos::rcp(new Belos::XpetraOp<SC, LO, GO, NO>(SM_Matrix)); // Turns a Xpetra::Matrix object into a Belos operator
#else
    RCP<OP> belosOp   = Teuchos::rcp(new Belos::XpetraOp<SC, NO>(SM_Matrix)); // Turns a Xpetra::Matrix object into a Belos operator
#endif

    RCP<Belos::LinearProblem<SC, MV, OP> > problem = rcp( new Belos::LinearProblem<SC, MV, OP>() );
    problem -> setOperator( belosOp );
    RCP<OP> belosPrecOp;
    if (precType != "none") {
#ifdef TPETRA_ENABLE_TEMPLATE_ORDINALS
      belosPrecOp = Teuchos::rcp(new Belos::XpetraOp<SC, LO, GO, NO>(preconditioner)); // Turns a Xpetra::Matrix object into a Belos operator
#else
      belosPrecOp = Teuchos::rcp(new Belos::XpetraOp<SC, NO>(preconditioner)); // Turns a Xpetra::Matrix object into a Belos operator
#endif
      problem -> setRightPrec( belosPrecOp );
    }
    problem -> setProblem( X, B );

    bool set = problem->setProblem();
    if (set == false) {
      *out << "\nERROR:  Belos::LinearProblem failed to set up correctly!" << std::endl;
      return false;
    }

    // Belos solver
    RCP< Belos::SolverManager<SC, MV, OP> > solver;
    RCP< Belos::SolverFactory<SC, MV,OP> > factory = rcp( new  Belos::SolverFactory<SC,MV,OP>() );
    solver = factory->create(belosSolverType,Teuchos::rcpFromRef(belosParams.sublist(belosSolverType)));

    comm->barrier();
    tm2 = Teuchos::null;

    auto tm3 = TimeMonitor::getNewTimer("Maxwell: 3 - Solve");

    // set problem and solve
    solver -> setProblem( problem );
    for(int solveno = 0; solveno<=numResolves; solveno++) {
      if (X0.is_null())
        X->putScalar(Teuchos::ScalarTraits<Scalar>::zero());
      else
        X = X0;
      Belos::ReturnType status = solver -> solve();
      int iters = solver -> getNumIters();
      success = (iters<50 && status == Belos::Converged);
      if (success)
        *out << "SUCCESS! Belos converged in " << iters << " iterations." << std::endl;
      else
        *out << "FAILURE! Belos did not converge fast enough." << std::endl;
    }
  }
#endif // HAVE_MUELU_BELOS
#if defined(HAVE_MUELU_STRATIMIKOS) && defined(HAVE_MUELU_THYRA)
  if (solverName == "Stratimikos") {
    // Build the rest of the Stratimikos list
    Teuchos::ParameterList stratimikosParams;
    stratimikosParams.set("Linear Solver Type","Belos");
    stratimikosParams.sublist("Linear Solver Types").sublist("Belos").set("Solver Type", belosSolverType);
    stratimikosParams.sublist("Linear Solver Types").sublist("Belos").set("Solver Types", belosParams);
    stratimikosParams.sublist("Linear Solver Types").sublist("Belos").sublist("VerboseObject").set("Verbosity Level", "medium");
    stratimikosParams.set("Preconditioner Type","MueLuRefMaxwell");
    params.set("parameterlist: syntax","muelu");
    stratimikosParams.sublist("Preconditioner Types").set("MueLuRefMaxwell",params);
    // Add matrices to parameterlist
    stratimikosParams.sublist("Preconditioner Types").sublist("MueLuRefMaxwell").set("D0",         D0_Matrix);
    stratimikosParams.sublist("Preconditioner Types").sublist("MueLuRefMaxwell").set("M0inv",      M0inv_Matrix);
    stratimikosParams.sublist("Preconditioner Types").sublist("MueLuRefMaxwell").set("M1",         M1_Matrix);
    stratimikosParams.sublist("Preconditioner Types").sublist("MueLuRefMaxwell").set("Ms",         Ms_Matrix);
    stratimikosParams.sublist("Preconditioner Types").sublist("MueLuRefMaxwell").set("Coordinates",coords);

    // Build Thyra linear algebra objects
#ifdef TPETRA_ENABLE_TEMPLATE_ORDINALS
    RCP<const Thyra::LinearOpBase<Scalar> > thyraA = Xpetra::ThyraUtils<Scalar,LocalOrdinal,GlobalOrdinal,Node>::toThyra(Teuchos::rcp_dynamic_cast<CrsMatrixWrap>(SM_Matrix)->getCrsMatrix());
    RCP<      Thyra::VectorBase<Scalar> >thyraX = Teuchos::rcp_const_cast<Thyra::VectorBase<Scalar> >(Xpetra::ThyraUtils<Scalar,LocalOrdinal,GlobalOrdinal,Node>::toThyraVector(X->getVectorNonConst(0)));
#else
    RCP<const Thyra::LinearOpBase<Scalar> > thyraA = Xpetra::ThyraUtils<Scalar,Node>::toThyra(Teuchos::rcp_dynamic_cast<CrsMatrixWrap>(SM_Matrix)->getCrsMatrix());
    RCP<      Thyra::VectorBase<Scalar> >thyraX = Teuchos::rcp_const_cast<Thyra::VectorBase<Scalar> >(Xpetra::ThyraUtils<Scalar,Node>::toThyraVector(X->getVectorNonConst(0)));
#endif
    // TODO: Why do we loose a reference when running this with Epetra?
#ifdef TPETRA_ENABLE_TEMPLATE_ORDINALS
    RCP<const Thyra::VectorBase<Scalar> >thyraB = Xpetra::ThyraUtils<Scalar,LocalOrdinal,GlobalOrdinal,Node>::toThyraVector(B->getVector(0));
#else
    RCP<const Thyra::VectorBase<Scalar> >thyraB = Xpetra::ThyraUtils<Scalar,Node>::toThyraVector(B->getVector(0));
#endif

    // Build Stratimikos solver
    Stratimikos::DefaultLinearSolverBuilder linearSolverBuilder;  // This is the Stratimikos main class (= factory of solver factory).
#ifdef TPETRA_ENABLE_TEMPLATE_ORDINALS
    Stratimikos::enableMueLuRefMaxwell<LocalOrdinal,GlobalOrdinal,Node>(linearSolverBuilder);                // Register MueLu as a Stratimikos preconditioner strategy.
#else
    Stratimikos::enableMueLuRefMaxwell<Node>(linearSolverBuilder);                // Register MueLu as a Stratimikos preconditioner strategy.
#endif
    linearSolverBuilder.setParameterList(rcp(&stratimikosParams,false));              // Setup solver parameters using a Stratimikos parameter list.

    // Build a new "solver factory" according to the previously specified parameter list.
    RCP<Thyra::LinearOpWithSolveFactoryBase<Scalar> > solverFactory = Thyra::createLinearSolveStrategy(linearSolverBuilder);

    // Build a Thyra operator corresponding to A^{-1} computed using the Stratimikos solver.
    Teuchos::RCP<Thyra::LinearOpWithSolveBase<Scalar> > thyraInverseA = Thyra::linearOpWithSolve(*solverFactory, thyraA);

    comm->barrier();
    tm2 = Teuchos::null;

    auto tm5 = TimeMonitor::getNewTimer("Maxwell: 3 - Solve");

    // Solve Ax = b.
    Thyra::SolveStatus<Scalar> status = Thyra::solve<Scalar>(*thyraInverseA, Thyra::NOTRANS, *thyraB, thyraX.ptr());
    std::cout << status << std::endl;

    success = (status.solveStatus == Thyra::SOLVE_STATUS_CONVERGED);
  } // HAVE_MUELU_STRATIMIKOS && HAVE_MUELU_THYRA
#endif
  comm->barrier();

  return success;
} // SetupSolve


#ifdef TPETRA_ENABLE_TEMPLATE_ORDINALS
template<typename Scalar,class LocalOrdinal,class GlobalOrdinal,class Node>
#else
template<typename Scalar,class Node>
#endif
int main_(Teuchos::CommandLineProcessor &clp, Xpetra::UnderlyingLib lib, int argc, char *argv[]) {
#include <MueLu_UseShortNames.hpp>

  RCP< const Teuchos::Comm<int> > comm = Teuchos::DefaultComm<int>::getComm();
  RCP<Teuchos::FancyOStream> out = Teuchos::fancyOStream(Teuchos::rcpFromRef(std::cout));
  out->setOutputToRootOnly(0);

  bool        printTimings      = true;               clp.setOption("timings", "notimings",  &printTimings,      "print timings to screen");
  std::string timingsFormat     = "table-fixed";      clp.setOption("time-format",           &timingsFormat,     "timings format (table-fixed | table-scientific | yaml)");
  double scaling                = 1.0;                clp.setOption("scaling",               &scaling,           "scale mass term");
  std::string solverName        = "Belos";            clp.setOption("solverName",            &solverName,        "Name of iterative linear solver "
                                                                                                                 "to use for solving the linear system. "
                                                                                                                 "(\"Belos\" or \"Stratimikos\")");
  std::string belosSolverType   = "Block CG";         clp.setOption("belosSolverType",       &belosSolverType,   "Name of the Belos linear solver");
  std::string precType          = "MueLu-RefMaxwell"; clp.setOption("precType",              &precType,          "preconditioner to use (MueLu-RefMaxwell|ML-RefMaxwell|none)");
  std::string xml               = "";                 clp.setOption("xml",                   &xml,               "xml file with solver parameters (default: \"Maxwell.xml\")");
  std::string belos_xml         = "Belos.xml";        clp.setOption("belos_xml",             &belos_xml,         "xml file with Belos solver parameters (default: \"Belos.xml\")");
  std::string stratimikos_xml   = "Stratimikos.xml";  clp.setOption("stratimikos_xml",       &stratimikos_xml,   "xml file with Stratimikos solver parameters (default: \"Stratimikos.xml\")");
  int         numResolves       = 0;                  clp.setOption("resolve",               &numResolves,       "#times to redo solve");
  double      tol               = 1e-10;              clp.setOption("tol",                   &tol,               "solver convergence tolerance");
  int         maxIts            = 200;                clp.setOption("its",                   &maxIts,            "maximum number of solver iterations");
  bool        use_stacked_timer = false;              clp.setOption("stacked-timer",
                                                                    "no-stacked-timer",      &use_stacked_timer, "use stacked timer");

  std::string S_file, D0_file, M1_file, M0_file;
  if (!Teuchos::ScalarTraits<Scalar>::isComplex) {
    S_file  = "S.mat";
    D0_file = "D0.mat";
    M1_file = "M1.mat";
    M0_file = "M0.mat";
  } else {
    S_file  = "S_complex.mat";
    D0_file = "D0_complex.mat";
    M1_file = "M1_complex.mat";
    M0_file = "M0_complex.mat";
  }
                                                      clp.setOption("S",                     &S_file);
  std::string SM_file           = "";                 clp.setOption("SM",                    &SM_file);
  std::string Kn_file           = "";                 clp.setOption("Kn",                    &Kn_file);
                                                      clp.setOption("D0",                    &D0_file);
                                                      clp.setOption("M1",                    &M1_file);
  std::string Ms_file           = "";                 clp.setOption("Ms",                    &Ms_file);
                                                      clp.setOption("M0",                    &M0_file);
  std::string M0inv_file        = "";                 clp.setOption("M0inv",                 &M0inv_file);

  std::string coords_file       = "coords.mat";       clp.setOption("coords",                &coords_file);
  std::string nullspace_file    = "";                 clp.setOption("nullspace",             &nullspace_file);
  std::string material_file     = "";                 clp.setOption("material",              &material_file);

  std::string rhs_file          = "";                 clp.setOption("rhs",                   &rhs_file);
  std::string x0_file           = "";                 clp.setOption("x0",                    &x0_file);

  clp.recogniseAllOptions(true);
  switch (clp.parse(argc, argv)) {
    case Teuchos::CommandLineProcessor::PARSE_HELP_PRINTED:        return EXIT_SUCCESS;
    case Teuchos::CommandLineProcessor::PARSE_ERROR:
    case Teuchos::CommandLineProcessor::PARSE_UNRECOGNIZED_OPTION: return EXIT_FAILURE;
    case Teuchos::CommandLineProcessor::PARSE_SUCCESSFUL:          break;
  }

  if (xml == ""){
    if (precType == "MueLu-RefMaxwell")
      xml = "Maxwell.xml";
    else if (precType == "ML-RefMaxwell")
      xml = "Maxwell_ML.xml";
    else if (precType == "ML-Maxwell")
      xml = "Maxwell_ML1.xml";
    else if (precType == "Hypre")
      xml = "Hypre.xml";
  }

  RCP<Teuchos::StackedTimer> stacked_timer;
  if (use_stacked_timer)
    stacked_timer = rcp(new Teuchos::StackedTimer("Maxwell Driver"));
  TimeMonitor::setStackedTimer(stacked_timer);

  auto globalTimeMonitor = TimeMonitor::getNewTimer("Maxwell: S - Global Time");
  auto tm                = TimeMonitor::getNewTimer("Maxwell: 1 - Read and Build Matrices");

  // Read matrices in from files
  RCP<Matrix> D0_Matrix, SM_Matrix, M1_Matrix, Ms_Matrix, M0inv_Matrix, Kn_Matrix;

  // gradient matrix
#ifdef TPETRA_ENABLE_TEMPLATE_ORDINALS
  D0_Matrix = Xpetra::IO<SC, LO, GO, NO>::Read(D0_file, lib, comm);
#else
  D0_Matrix = Xpetra::IO<SC, NO>::Read(D0_file, lib, comm);
#endif

  // maps for nodal and edge matrices
  RCP<const Map> node_map = D0_Matrix->getDomainMap();
  RCP<const Map> edge_map = D0_Matrix->getRangeMap();

  // build stiffness plus mass matrix (SM_Matrix)
  if (SM_file == "") {
    // edge stiffness matrix
#ifdef TPETRA_ENABLE_TEMPLATE_ORDINALS
    RCP<Matrix> S_Matrix = Xpetra::IO<SC, LO, GO, NO>::Read(S_file, edge_map);
    M1_Matrix = Xpetra::IO<SC, LO, GO, NO>::Read(M1_file, edge_map);
    Xpetra::MatrixMatrix<Scalar,LocalOrdinal,GlobalOrdinal,Node>::TwoMatrixAdd(*S_Matrix,false,(SC)1.0,*M1_Matrix,false,scaling,SM_Matrix,*out);
#else
    RCP<Matrix> S_Matrix = Xpetra::IO<SC, NO>::Read(S_file, edge_map);
    M1_Matrix = Xpetra::IO<SC, NO>::Read(M1_file, edge_map);
    Xpetra::MatrixMatrix<Scalar,Node>::TwoMatrixAdd(*S_Matrix,false,(SC)1.0,*M1_Matrix,false,scaling,SM_Matrix,*out);
#endif
    SM_Matrix->fillComplete();
  } else {
#ifdef TPETRA_ENABLE_TEMPLATE_ORDINALS
    SM_Matrix = Xpetra::IO<SC, LO, GO, NO>::Read(SM_file, edge_map);
#else
    SM_Matrix = Xpetra::IO<SC, NO>::Read(SM_file, edge_map);
#endif
    if (M1_file != "")
#ifdef TPETRA_ENABLE_TEMPLATE_ORDINALS
      M1_Matrix = Xpetra::IO<SC, LO, GO, NO>::Read(M1_file, edge_map);
#else
      M1_Matrix = Xpetra::IO<SC, NO>::Read(M1_file, edge_map);
#endif
  }
  if (Ms_file != "")
#ifdef TPETRA_ENABLE_TEMPLATE_ORDINALS
    Ms_Matrix = Xpetra::IO<SC, LO, GO, NO>::Read(Ms_file, edge_map);
#else
    Ms_Matrix = Xpetra::IO<SC, NO>::Read(Ms_file, edge_map);
#endif
  else
    Ms_Matrix = M1_Matrix;
  if (Kn_file!="")
#ifdef TPETRA_ENABLE_TEMPLATE_ORDINALS
    Kn_Matrix = Xpetra::IO<SC, LO, GO, NO>::Read(Kn_file, node_map);
#else
    Kn_Matrix = Xpetra::IO<SC, NO>::Read(Kn_file, node_map);
#endif

  if ((M0inv_file == "") && (M0_file != "")) {
    // nodal mass matrix
#ifdef TPETRA_ENABLE_TEMPLATE_ORDINALS
    RCP<Matrix> M0_Matrix = Xpetra::IO<SC, LO, GO, NO>::Read(M0_file, node_map);
#else
    RCP<Matrix> M0_Matrix = Xpetra::IO<SC, NO>::Read(M0_file, node_map);
#endif
    // build lumped mass matrix inverse (M0inv_Matrix)
    RCP<Vector> diag = Utilities::GetLumpedMatrixDiagonal(M0_Matrix);
    RCP<CrsMatrixWrap> M0inv_MatrixWrap = rcp(new CrsMatrixWrap(node_map, node_map, 0));
    RCP<CrsMatrix> M0inv_CrsMatrix = M0inv_MatrixWrap->getCrsMatrix();
    Teuchos::ArrayRCP<size_t> rowPtr;
    Teuchos::ArrayRCP<LO> colInd;
    Teuchos::ArrayRCP<SC> values;
    Teuchos::ArrayRCP<const SC> diags = diag->getData(0);
    size_t nodeNumElements = node_map->getNodeNumElements();
    M0inv_CrsMatrix->allocateAllValues(nodeNumElements, rowPtr, colInd, values);
    SC ONE = Teuchos::ScalarTraits<Scalar>::one();
    for (size_t i = 0; i < nodeNumElements; i++) {
      rowPtr[i] = i;  colInd[i] = i;  values[i] = ONE / diags[i];
    }
    rowPtr[nodeNumElements] = nodeNumElements;
    M0inv_CrsMatrix->setAllValues(rowPtr, colInd, values);
    M0inv_CrsMatrix->expertStaticFillComplete(node_map, node_map);
    M0inv_Matrix = Teuchos::rcp_dynamic_cast<Matrix>(M0inv_MatrixWrap);
  } else if (M0inv_file != "") {
#ifdef TPETRA_ENABLE_TEMPLATE_ORDINALS
    M0inv_Matrix = Xpetra::IO<SC, LO, GO, NO>::Read(M0inv_file, node_map);
#else
    M0inv_Matrix = Xpetra::IO<SC, NO>::Read(M0inv_file, node_map);
#endif
  }

  // coordinates
#ifdef TPETRA_ENABLE_TEMPLATE_ORDINALS
  RCP<Xpetra::MultiVector<typename Teuchos::ScalarTraits<Scalar>::magnitudeType, LO, GO, NO> > coords = Xpetra::IO<typename Teuchos::ScalarTraits<Scalar>::magnitudeType, LO, GO, NO>::ReadMultiVector(coords_file, node_map);
#else
  RCP<Xpetra::MultiVector<typename Teuchos::ScalarTraits<Scalar>::magnitudeType,NO> > coords = Xpetra::IO<typename Teuchos::ScalarTraits<Scalar>::magnitudeType,NO>::ReadMultiVector(coords_file, node_map);
#endif

  RCP<MultiVector> nullspace = Teuchos::null;
  if (nullspace_file != "")
#ifdef TPETRA_ENABLE_TEMPLATE_ORDINALS
    nullspace = Xpetra::IO<SC, LO, GO, NO>::ReadMultiVector(nullspace_file, edge_map);
#else
    nullspace = Xpetra::IO<SC, NO>::ReadMultiVector(nullspace_file, edge_map);
#endif

  RCP<MultiVector> material = Teuchos::null;
  if (material_file != "") {
#ifdef TPETRA_ENABLE_TEMPLATE_ORDINALS
    material  = Xpetra::IO<SC, LO, GO, NO>::ReadMultiVector(material_file, node_map);
#else
    material  = Xpetra::IO<SC, NO>::ReadMultiVector(material_file, node_map);
#endif
  }

  // set parameters
  Teuchos::ParameterList params, belosParams;
  Teuchos::updateParametersFromXmlFileAndBroadcast(xml,      Teuchos::Ptr<Teuchos::ParameterList>(&params),     *comm);
  Teuchos::updateParametersFromXmlFileAndBroadcast(belos_xml,Teuchos::Ptr<Teuchos::ParameterList>(&belosParams),*comm);
  belosParams.sublist(belosSolverType).set("Maximum Iterations", maxIts);
  belosParams.sublist(belosSolverType).set("Convergence Tolerance",tol);

  // setup LHS, RHS
  RCP<MultiVector> X, X0, B;
  if (rhs_file == "") {
    B = MultiVectorFactory::Build(edge_map,1);
    RCP<MultiVector> vec = MultiVectorFactory::Build(edge_map,1);
    vec -> putScalar(Teuchos::ScalarTraits<Scalar>::one());
    SM_Matrix->apply(*vec,*B);
  } else
#ifdef TPETRA_ENABLE_TEMPLATE_ORDINALS
    B = Xpetra::IO<SC, LO, GO, NO>::ReadMultiVector(rhs_file, edge_map);
#else
    B = Xpetra::IO<SC, NO>::ReadMultiVector(rhs_file, edge_map);
#endif

  X = MultiVectorFactory::Build(edge_map,1);
  X -> putScalar(Teuchos::ScalarTraits<Scalar>::zero());
  if (x0_file != "")
#ifdef TPETRA_ENABLE_TEMPLATE_ORDINALS
    X0 = Xpetra::IO<SC, LO, GO, NO>::ReadMultiVector(x0_file, edge_map);
#else
    X0 = Xpetra::IO<SC, NO>::ReadMultiVector(x0_file, edge_map);
#endif

  comm->barrier();
  tm = Teuchos::null;

  std::map<std::string, void*> inputs;
  inputs["SM"]              = &SM_Matrix;
  inputs["D0"]              = &D0_Matrix;
  inputs["M1"]              = &M1_Matrix;
  inputs["Ms"]              = &Ms_Matrix;
  inputs["M0inv"]           = &M0inv_Matrix;
  inputs["Kn"]              = &Kn_Matrix;

  inputs["coordinates"]     = &coords;
  inputs["nullspace"]       = &nullspace;
  inputs["material"]        = &material;

  inputs["B"]               = &B;
  inputs["X"]               = &X;
  inputs["X0"]              = &X0;

  inputs["params"]          = &params;
  inputs["solverName"]      = &solverName;
  inputs["belosSolverType"] = &belosSolverType;
  inputs["precType"]        = &precType;
  inputs["belosParams"]     = &belosParams;
  inputs["numResolves"]     = &numResolves;

  inputs["lib"]             = &lib;
  inputs["comm"]            = &comm;
  inputs["out"]             = &out;

#ifdef TPETRA_ENABLE_TEMPLATE_ORDINALS
  bool success = SetupSolveWrappers<Scalar,LocalOrdinal,GlobalOrdinal,Node>::SetupSolve(inputs);
#else
  bool success = SetupSolveWrappers<Scalar,Node>::SetupSolve(inputs);
#endif

  globalTimeMonitor = Teuchos::null;

  if (printTimings) {
    if (use_stacked_timer) {
      stacked_timer->stop("Maxwell Driver");
      Teuchos::StackedTimer::OutputOptions options;
      options.output_fraction = options.output_histogram = options.output_minmax = true;
      stacked_timer->report(*out, comm, options);
    } else {
      RCP<Teuchos::ParameterList> reportParams = rcp(new Teuchos::ParameterList);
      if (timingsFormat == "yaml") {
        reportParams->set("Report format",             "YAML");            // "Table" or "YAML"
        reportParams->set("YAML style",                "compact");         // "spacious" or "compact"
      }
      reportParams->set("How to merge timer sets",   "Union");
      reportParams->set("alwaysWriteLocal",          false);
      reportParams->set("writeGlobalStats",          true);
      reportParams->set("writeZeroTimers",           false);
      // FIXME: no "ignoreZeroTimers"

      const std::string filter = "";

      std::ios_base::fmtflags ff(out->flags());
      if (timingsFormat == "table-fixed") *out << std::fixed;
      else * out << std::scientific;
      TimeMonitor::report(comm.ptr(), *out, filter, reportParams);
      *out << std::setiosflags(ff);
    }
  }

  TimeMonitor::clearCounters();

  return ( success ? EXIT_SUCCESS : EXIT_FAILURE );
}

//- -- --------------------------------------------------------
#define MUELU_AUTOMATIC_TEST_ETI_NAME main_
#include "MueLu_Test_ETI.hpp"

int main(int argc, char *argv[]) {
  return Automatic_Test_ETI(argc,argv);
}
