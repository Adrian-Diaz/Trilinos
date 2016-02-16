// @HEADER
//
// ***********************************************************************
//
//             Xpetra: A linear algebra interface package
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
#include <Teuchos_UnitTestHarness.hpp>
#include <Xpetra_UnitTestHelpers.hpp>
#include <Teuchos_Array.hpp>
#include <Teuchos_Tuple.hpp>
#include <Teuchos_CommHelpers.hpp>
#include <Teuchos_ScalarTraits.hpp>
#include <Teuchos_as.hpp>
#include <Teuchos_DefaultComm.hpp>

#ifdef HAVE_MPI
#  include "Epetra_MpiComm.h"
#  include "mpi.h"
#endif
#  include "Epetra_SerialComm.h"

#include <Xpetra_ConfigDefs.hpp>

#ifdef HAVE_XPETRA_EPETRAEXT
// EpetraExt
#include "EpetraExt_CrsMatrixIn.h"
#include "EpetraExt_VectorIn.h"
#include "EpetraExt_VectorOut.h"
#include "EpetraExt_MatrixMatrix.h"
#include "EpetraExt_RowMatrixOut.h"
#endif

#include "BlockedMatrixTestHelpers.hpp" // handling of Epetra block matrices (SplitMap etc...)

#include <Xpetra_DefaultPlatform.hpp>

#include <Xpetra_Map.hpp>
#include <Xpetra_MapExtractorFactory.hpp>
#include <Xpetra_BlockedCrsMatrix.hpp>
#include <Xpetra_MatrixUtils.hpp>

#ifdef HAVE_XPETRA_THYRA
#include <Xpetra_ThyraUtils.hpp>
#endif
#include <Xpetra_Exceptions.hpp>

namespace XpetraBlockMatrixTests {

using Xpetra::DefaultPlatform;


using Xpetra::viewLabel_t;

bool testMpi = true;

Teuchos::RCP<const Teuchos::Comm<int> > getDefaultComm()
{
  if (testMpi) {
    return DefaultPlatform::getDefaultPlatform().getComm();
  }
  return rcp(new Teuchos::SerialComm<int>());
}

/////////////////////////////////////////////////////

TEUCHOS_STATIC_SETUP()
{
  Teuchos::CommandLineProcessor &clp = Teuchos::UnitTestRepository::getCLP();
  clp.addOutputSetupOptions(true);
  clp.setOption(
      "test-mpi", "test-serial", &testMpi,
      "Test MPI (if available) or force test of serial.  In a serial build,"
      " this option is ignored and a serial comm is always used." );
}

//
// UNIT TESTS
//


TEUCHOS_UNIT_TEST_TEMPLATE_6_DECL( ThyraBlockedOperator, ThyraVectorSpace2XpetraMap_Tpetra, M, MA, Scalar, LO, GO, Node )
{
#ifdef HAVE_XPETRA_THYRA
  Teuchos::RCP<const Teuchos::Comm<int> > comm = Teuchos::DefaultComm<int>::getComm();

  // TPetra version
#ifdef HAVE_XPETRA_TPETRA
  {
    Teuchos::RCP<const Xpetra::Map<LO,GO,Node> > map = Xpetra::MapFactory<LO,GO,Node>::Build(Xpetra::UseTpetra, 1000, 0, comm);
    TEST_EQUALITY(Teuchos::is_null(map),false);
    Teuchos::RCP<const Tpetra::Map<LO,GO,Node > > tMap = Xpetra::toTpetra(map);
    TEST_EQUALITY(Teuchos::is_null(tMap),false);

    // transform to Thyra...
    Teuchos::RCP<const Thyra::VectorSpaceBase<Scalar> > thyraVectorSpace = Thyra::createVectorSpace<Scalar, LO, GO, Node>(tMap);
    TEST_EQUALITY(Teuchos::is_null(thyraVectorSpace),false);

    // transform back to Xpetra...
    Teuchos::RCP<Xpetra::Map<LO,GO,Node> > xMap = Xpetra::ThyraUtils<Scalar,LO,GO,Node>::toXpetra(thyraVectorSpace,comm);

    TEST_EQUALITY(Teuchos::is_null(xMap),false);

    TEST_EQUALITY(xMap->isCompatible(*map),true);
    TEST_EQUALITY(xMap->isSameAs(*map),true);
  }
#endif
#endif
}

TEUCHOS_UNIT_TEST_TEMPLATE_6_DECL( ThyraBlockedOperator, ThyraVectorSpace2XpetraMap_Epetra, M, MA, Scalar, LO, GO, Node )
{
#ifdef HAVE_XPETRA_THYRA
  Teuchos::RCP<const Teuchos::Comm<int> > comm = Teuchos::DefaultComm<int>::getComm();

  // Epetra version
#ifdef HAVE_XPETRA_EPETRA
  {
    Teuchos::RCP<const Xpetra::Map<LO,GO,Node> > map = Xpetra::MapFactory<LO,GO,Node>::Build(Xpetra::UseEpetra, 1000, 0, comm);
    TEST_EQUALITY(Teuchos::is_null(map),false);
    const Epetra_Map ret = Xpetra::toEpetra(map);
    Teuchos::RCP<const Epetra_Map> eMap = Teuchos::rcp(&ret,false);
    TEST_EQUALITY(Teuchos::is_null(eMap),false);

    // transform to Thyra...
    Teuchos::RCP<const Thyra::VectorSpaceBase<double> > thyraVectorSpace = Thyra::create_VectorSpace(eMap);
    TEST_EQUALITY(Teuchos::is_null(thyraVectorSpace),false);

    // transform back to Xpetra...
    Teuchos::RCP<Xpetra::Map<LO,GO,Node> > xMap = Xpetra::ThyraUtils<Scalar,LO,GO,Node>::toXpetra(thyraVectorSpace,comm);

    TEST_EQUALITY(Teuchos::is_null(xMap),false);

    TEST_EQUALITY(xMap->isCompatible(*map),true);
    TEST_EQUALITY(xMap->isSameAs(*map),true);
  }
#endif
#endif
}

TEUCHOS_UNIT_TEST_TEMPLATE_6_DECL( ThyraBlockedOperator, ThyraShrinkMaps, M, MA, Scalar, LO, GO, Node )
{
#ifdef HAVE_XPETRA_THYRA
  Teuchos::RCP<const Teuchos::Comm<int> > comm = Teuchos::DefaultComm<int>::getComm();

  M testMap(1,0,comm);
  Xpetra::UnderlyingLib lib = testMap.lib();

  typedef Xpetra::Map<LO, GO, Node> MapClass;
  typedef Xpetra::MapFactory<LO, GO, Node> MapFactoryClass;
  typedef Xpetra::MatrixUtils<Scalar, LO, GO, Node> MatrixUtilsClass;

  // generate non-overlapping map
  Teuchos::Array<GO> myGIDs;
  for(int i = 0; i < 10; i++) {
    myGIDs.push_back(comm->getRank() * 100 + i * 3);
  }
  const Teuchos::RCP<const MapClass> map = MapFactoryClass::Build (lib,Teuchos::OrdinalTraits<GO>::invalid(),myGIDs(),0,comm);

  const Teuchos::RCP<const MapClass> thMap = MatrixUtilsClass::shrinkMapGIDs(*map,*map);

  TEST_EQUALITY(thMap->getGlobalNumElements() , comm->getSize() * 10);
  TEST_EQUALITY(thMap->getNodeNumElements() , 10);
  TEST_EQUALITY(thMap->getMinLocalIndex(), 0);
  TEST_EQUALITY(thMap->getMaxLocalIndex(), 9);
  TEST_EQUALITY(thMap->getMinGlobalIndex(), comm->getRank() * 10);
  TEST_EQUALITY(thMap->getMaxGlobalIndex(), comm->getRank() * 10 + 9);
  TEST_EQUALITY(thMap->getMinAllGlobalIndex(), 0);
  TEST_EQUALITY(thMap->getMaxAllGlobalIndex(), comm->getSize() * 10 - 1);
  //TEST_EQUALITY(thMap->isContiguous(), true);

  // generate overlapping map
  Teuchos::Array<GO> myovlGIDs;
  if(comm->getRank() > 0) {
    myovlGIDs.push_back((comm->getRank()-1) * 100 + 21);
    myovlGIDs.push_back((comm->getRank()-1) * 100 + 24);
    myovlGIDs.push_back((comm->getRank()-1) * 100 + 27);
  }
  for(int i = 0; i < 10; i++) {
    myovlGIDs.push_back(comm->getRank() * 100 + i * 3);
  }

  const Teuchos::RCP<const MapClass> map2 = MapFactoryClass::Build (lib,Teuchos::OrdinalTraits<GO>::invalid(),myovlGIDs(),0,comm);

  const Teuchos::RCP<const MapClass> thMap2 = MatrixUtilsClass::shrinkMapGIDs(*map2,*map);

  TEST_EQUALITY(thMap2->getMinGlobalIndex() , std::max(0,comm->getRank() * 10 - 3));
  TEST_EQUALITY(thMap2->getMaxGlobalIndex() , comm->getRank() * 10 + 9);
  TEST_EQUALITY(thMap2->getNodeNumElements() , (comm->getRank() > 0) ? 13 : 10 );
  TEST_EQUALITY(thMap2->getMinAllGlobalIndex(), 0);
  TEST_EQUALITY(thMap2->getMaxAllGlobalIndex(), comm->getSize() * 10 - 1);
  TEST_EQUALITY(thMap2->getGlobalNumElements() , comm->getSize() * 13 - 3);
#endif
}

TEUCHOS_UNIT_TEST_TEMPLATE_6_DECL( ThyraBlockedOperator, ThyraOperator2XpetraCrsMat, M, MA, Scalar, LO, GO, Node )
{
#ifdef HAVE_XPETRA_THYRA
  Teuchos::RCP<const Teuchos::Comm<int> > comm = Teuchos::DefaultComm<int>::getComm();

  M testMap(1,0,comm);
  Xpetra::UnderlyingLib lib = testMap.lib();

  typedef Xpetra::MapFactory<LO, GO, Node> MapFactoryClass;

  // generate the matrix
  LO nEle = 63;
  const Teuchos::RCP<const Xpetra::Map<LO,GO,Node> > map = MapFactoryClass::Build(lib, nEle, 0, comm);

  Teuchos::RCP<Xpetra::CrsMatrix<Scalar, LO, GO, Node> > matrix =
               Xpetra::CrsMatrixFactory<Scalar,LO,GO,Node>::Build(map, 10);

  LO NumMyElements = map->getNodeNumElements();
  Teuchos::ArrayView<const GO> MyGlobalElements = map->getNodeElementList();

  for (LO i = 0; i < NumMyElements; ++i) {
      matrix->insertGlobalValues(MyGlobalElements[i],
                              Teuchos::tuple<GO>(MyGlobalElements[i]),
                              Teuchos::tuple<Scalar>(1.0) );
  }

  matrix->fillComplete();

  // create a Thyra operator from Xpetra::CrsMatrix
  Teuchos::RCP<const Thyra::LinearOpBase<Scalar> > thyraOp =
      Xpetra::ThyraUtils<Scalar,LO,GO,Node>::toThyra(matrix);

  // transform Thyra operator 2 Xpetra::CrsMatrix
  Teuchos::RCP<const Xpetra::CrsMatrix<Scalar, LO, GO, Node> > xCrsMat =
      Xpetra::ThyraUtils<Scalar,LO,GO,Node>::toXpetra(thyraOp);
  TEUCHOS_TEST_FOR_EXCEPT(Teuchos::is_null(xCrsMat));

  TEST_EQUALITY(xCrsMat->getFrobeniusNorm()   ,matrix->getFrobeniusNorm());
  TEST_EQUALITY(xCrsMat->getGlobalNumRows()   ,matrix->getGlobalNumRows());
  TEST_EQUALITY(xCrsMat->getGlobalNumCols()   ,matrix->getGlobalNumCols());
  TEST_EQUALITY(xCrsMat->getNodeNumRows()     ,matrix->getNodeNumRows()  );
  TEST_EQUALITY(xCrsMat->getGlobalNumEntries(),matrix->getGlobalNumEntries());
  TEST_EQUALITY(xCrsMat->getNodeNumEntries()  ,matrix->getNodeNumEntries());

#endif
}

TEUCHOS_UNIT_TEST_TEMPLATE_6_DECL( ThyraBlockedOperator, ThyraBlockedOperator2XpetraBlockedCrsMat, M, MA, Scalar, LO, GO, Node )
{
#ifdef HAVE_XPETRA_THYRA
#if defined(HAVE_XPETRA_EPETRA) && defined(HAVE_XPETRA_EPETRAEXT)

  Teuchos::RCP<Epetra_Comm> Comm;
#ifdef HAVE_MPI
  Comm = Teuchos::rcp(new Epetra_MpiComm(MPI_COMM_WORLD));
#else
  Comm = Teuchos::rcp(new Epetra_SerialComm);
#endif

  // 1) load all matrices
  Epetra_Map pointmap(36,0,*Comm);

  // generate local maps for loading matrices
  std::vector<int> velgidvec; // global strided maps
  std::vector<int> pregidvec;
  std::vector<int> fullgidvec; // full global map
  for (int i=0; i<pointmap.NumMyElements(); i++)
  {
    // loop over all local ids in pointmap

    // get corresponding global id
    int gid = pointmap.GID(i);

    // store global strided gids
    velgidvec.push_back(3*gid);
    velgidvec.push_back(3*gid+1);
    pregidvec.push_back(3*gid+2);

    // gid for full map
    fullgidvec.push_back(3*gid);
    fullgidvec.push_back(3*gid+1);
    fullgidvec.push_back(3*gid+2);
  }

  // generate strided maps
  Teuchos::RCP<const Epetra_Map> velmap = Teuchos::rcp(new const Epetra_Map(-1, velgidvec.size(), &velgidvec[0], 0, *Comm));
  Teuchos::RCP<const Epetra_Map> premap = Teuchos::rcp(new const Epetra_Map(-1, pregidvec.size(), &pregidvec[0], 0, *Comm));

  // generate full map
  const Teuchos::RCP<const Epetra_Map> fullmap = Teuchos::rcp(new const Epetra_Map(-1, fullgidvec.size(), &fullgidvec[0], 0, *Comm));

  // read in matrices
  Epetra_CrsMatrix* ptrA = 0;
  EpetraExt::MatrixMarketFileToCrsMatrix("A.mat",*fullmap,*fullmap,*fullmap,ptrA);

  Teuchos::RCP<Epetra_CrsMatrix> fullA = Teuchos::rcp(ptrA);
  Teuchos::RCP<Epetra_Vector>    x     = Teuchos::rcp(new Epetra_Vector(*fullmap));
  x->PutScalar(1.0);

  // split fullA into A11,..., A22
  Teuchos::RCP<Epetra_CrsMatrix> A11;
  Teuchos::RCP<Epetra_CrsMatrix> A12;
  Teuchos::RCP<Epetra_CrsMatrix> A21;
  Teuchos::RCP<Epetra_CrsMatrix> A22;

  TEST_EQUALITY(SplitMatrix2x2(fullA,*velmap,*premap,A11,A12,A21,A22),true);

  // build Xpetra objects from Epetra_CrsMatrix objects
  Teuchos::RCP<Xpetra::CrsMatrix<Scalar,LO,GO,Node> > xfuA = Teuchos::rcp(new Xpetra::EpetraCrsMatrixT<GO,Node>(fullA));
  Teuchos::RCP<Xpetra::CrsMatrix<Scalar,LO,GO,Node> > xA11 = Teuchos::rcp(new Xpetra::EpetraCrsMatrixT<GO,Node>(A11));
  Teuchos::RCP<Xpetra::CrsMatrix<Scalar,LO,GO,Node> > xA12 = Teuchos::rcp(new Xpetra::EpetraCrsMatrixT<GO,Node>(A12));
  Teuchos::RCP<Xpetra::CrsMatrix<Scalar,LO,GO,Node> > xA21 = Teuchos::rcp(new Xpetra::EpetraCrsMatrixT<GO,Node>(A21));
  Teuchos::RCP<Xpetra::CrsMatrix<Scalar,LO,GO,Node> > xA22 = Teuchos::rcp(new Xpetra::EpetraCrsMatrixT<GO,Node>(A22));

  // build map extractor
  Teuchos::RCP<Xpetra::EpetraMapT<GO,Node> > xfullmap = Teuchos::rcp(new Xpetra::EpetraMapT<GO,Node>(fullmap));
  Teuchos::RCP<Xpetra::EpetraMapT<GO,Node> > xvelmap  = Teuchos::rcp(new Xpetra::EpetraMapT<GO,Node>(velmap ));
  Teuchos::RCP<Xpetra::EpetraMapT<GO,Node> > xpremap  = Teuchos::rcp(new Xpetra::EpetraMapT<GO,Node>(premap ));

  std::vector<Teuchos::RCP<const Xpetra::Map<LO,GO,Node> > > xmaps;
  xmaps.push_back(xvelmap);
  xmaps.push_back(xpremap);

  Teuchos::RCP<const Xpetra::MapExtractor<Scalar,LO,GO,Node> > map_extractor = Xpetra::MapExtractorFactory<Scalar,LO,GO,Node>::Build(xfullmap,xmaps);

  // build blocked operator
  Teuchos::RCP<Xpetra::BlockedCrsMatrix<Scalar,LO,GO,Node> > bOp = Teuchos::rcp(new Xpetra::BlockedCrsMatrix<Scalar,LO,GO,Node>(map_extractor,map_extractor,10));
  bOp->setMatrix(0,0,xA11);
  bOp->setMatrix(0,1,xA12);
  bOp->setMatrix(1,0,xA21);
  bOp->setMatrix(1,1,xA22);

  bOp->fillComplete();
  TEUCHOS_TEST_FOR_EXCEPT(Teuchos::is_null(bOp));

  // create Thyra operator
  Teuchos::RCP<Thyra::LinearOpBase<Scalar> > thOp =
      Xpetra::ThyraUtils<Scalar,LO,GO,Node>::toThyra(bOp);
  TEUCHOS_TEST_FOR_EXCEPT(Teuchos::is_null(thOp));

  Teuchos::RCP<Thyra::BlockedLinearOpBase<Scalar> > thbOp =
      Teuchos::rcp_dynamic_cast<Thyra::BlockedLinearOpBase<Scalar> >(thOp);
  TEUCHOS_TEST_FOR_EXCEPT(Teuchos::is_null(thbOp));

  Teuchos::RCP<const Thyra::ProductVectorSpaceBase<Scalar> > productRange  = thbOp->productRange();
  Teuchos::RCP<const Thyra::ProductVectorSpaceBase<Scalar> > productDomain = thbOp->productDomain();

  TEST_EQUALITY(productRange->numBlocks()  ,2);
  TEST_EQUALITY(productDomain->numBlocks() ,2);
  TEST_EQUALITY(Teuchos::as<Xpetra::global_size_t>(productRange->dim()) ,xfullmap->getGlobalNumElements());
  TEST_EQUALITY(Teuchos::as<Xpetra::global_size_t>(productDomain->dim()) ,xfullmap->getGlobalNumElements());
  TEST_EQUALITY(Teuchos::as<Xpetra::global_size_t>(productRange->getBlock(0)->dim())  ,xvelmap->getGlobalNumElements());
  TEST_EQUALITY(Teuchos::as<Xpetra::global_size_t>(productDomain->getBlock(0)->dim()) ,xvelmap->getGlobalNumElements());
  TEST_EQUALITY(Teuchos::as<Xpetra::global_size_t>(productRange->getBlock(1)->dim())  ,xpremap->getGlobalNumElements());
  TEST_EQUALITY(Teuchos::as<Xpetra::global_size_t>(productDomain->getBlock(1)->dim()) ,xpremap->getGlobalNumElements());

#endif // end HAVE_XPETRA_EPETRA && HAVE_XPETRA_EPETRAEXT
#endif // end HAVE_XPETRA_THYRA
}

TEUCHOS_UNIT_TEST_TEMPLATE_6_DECL( ThyraBlockedOperator, XpetraBlockedCrsMatConstructor, M, MA, Scalar, LO, GO, Node )
{
#ifdef HAVE_XPETRA_THYRA
#if defined(HAVE_XPETRA_EPETRA) && defined(HAVE_XPETRA_EPETRAEXT)
  Teuchos::RCP<const Teuchos::Comm<int> > comm = Teuchos::DefaultComm<int>::getComm();

  const Teuchos::RCP<const Epetra_Comm> epComm = Xpetra::toEpetra(comm);

  // 1) load all matrices
  Epetra_Map pointmap(36,0,*epComm);

  // generate local maps for loading matrices
  std::vector<int> velgidvec; // global strided maps
  std::vector<int> pregidvec;
  std::vector<int> fullgidvec; // full global map
  for (int i=0; i<pointmap.NumMyElements(); i++)
  {
    // loop over all local ids in pointmap

    // get corresponding global id
    int gid = pointmap.GID(i);

    // store global strided gids
    velgidvec.push_back(3*gid);
    velgidvec.push_back(3*gid+1);
    pregidvec.push_back(3*gid+2);

    // gid for full map
    fullgidvec.push_back(3*gid);
    fullgidvec.push_back(3*gid+1);
    fullgidvec.push_back(3*gid+2);
  }

  // generate strided maps
  Teuchos::RCP<const Epetra_Map> velmap = Teuchos::rcp(new const Epetra_Map(-1, velgidvec.size(), &velgidvec[0], 0, *epComm));
  Teuchos::RCP<const Epetra_Map> premap = Teuchos::rcp(new const Epetra_Map(-1, pregidvec.size(), &pregidvec[0], 0, *epComm));

  // generate full map
  const Teuchos::RCP<const Epetra_Map> fullmap = Teuchos::rcp(new const Epetra_Map(-1, fullgidvec.size(), &fullgidvec[0], 0, *epComm));

  // read in matrices
  Epetra_CrsMatrix* ptrA = 0;
  EpetraExt::MatrixMarketFileToCrsMatrix("A.mat",*fullmap,*fullmap,*fullmap,ptrA);

  Teuchos::RCP<Epetra_CrsMatrix> fullA = Teuchos::rcp(ptrA);
  Teuchos::RCP<Epetra_Vector>    x     = Teuchos::rcp(new Epetra_Vector(*fullmap));
  x->PutScalar(1.0);

  // split fullA into A11,..., A22
  Teuchos::RCP<Epetra_CrsMatrix> A11;
  Teuchos::RCP<Epetra_CrsMatrix> A12;
  Teuchos::RCP<Epetra_CrsMatrix> A21;
  Teuchos::RCP<Epetra_CrsMatrix> A22;

  TEST_EQUALITY(SplitMatrix2x2(fullA,*velmap,*premap,A11,A12,A21,A22),true);

  // build Xpetra objects from Epetra_CrsMatrix objects
  Teuchos::RCP<Xpetra::CrsMatrix<Scalar,LO,GO,Node> > xfuA = Teuchos::rcp(new Xpetra::EpetraCrsMatrixT<GO,Node>(fullA));
  Teuchos::RCP<Xpetra::CrsMatrix<Scalar,LO,GO,Node> > xA11 = Teuchos::rcp(new Xpetra::EpetraCrsMatrixT<GO,Node>(A11));
  Teuchos::RCP<Xpetra::CrsMatrix<Scalar,LO,GO,Node> > xA12 = Teuchos::rcp(new Xpetra::EpetraCrsMatrixT<GO,Node>(A12));
  Teuchos::RCP<Xpetra::CrsMatrix<Scalar,LO,GO,Node> > xA21 = Teuchos::rcp(new Xpetra::EpetraCrsMatrixT<GO,Node>(A21));
  Teuchos::RCP<Xpetra::CrsMatrix<Scalar,LO,GO,Node> > xA22 = Teuchos::rcp(new Xpetra::EpetraCrsMatrixT<GO,Node>(A22));

  // build map extractor
  Teuchos::RCP<Xpetra::EpetraMapT<GO,Node> > xfullmap = Teuchos::rcp(new Xpetra::EpetraMapT<GO,Node>(fullmap));
  Teuchos::RCP<Xpetra::EpetraMapT<GO,Node> > xvelmap  = Teuchos::rcp(new Xpetra::EpetraMapT<GO,Node>(velmap ));
  Teuchos::RCP<Xpetra::EpetraMapT<GO,Node> > xpremap  = Teuchos::rcp(new Xpetra::EpetraMapT<GO,Node>(premap ));

  std::vector<Teuchos::RCP<const Xpetra::Map<LO,GO,Node> > > xmaps;
  xmaps.push_back(xvelmap);
  xmaps.push_back(xpremap);

  Teuchos::RCP<const Xpetra::MapExtractor<Scalar,LO,GO,Node> > map_extractor = Xpetra::MapExtractorFactory<Scalar,LO,GO,Node>::Build(xfullmap,xmaps);

  // build blocked operator
  Teuchos::RCP<Xpetra::BlockedCrsMatrix<Scalar,LO,GO,Node> > bOp = Teuchos::rcp(new Xpetra::BlockedCrsMatrix<Scalar,LO,GO,Node>(map_extractor,map_extractor,10));
  bOp->setMatrix(0,0,xA11);
  bOp->setMatrix(0,1,xA12);
  bOp->setMatrix(1,0,xA21);
  bOp->setMatrix(1,1,xA22);

  bOp->fillComplete();
  TEUCHOS_TEST_FOR_EXCEPT(Teuchos::is_null(bOp));

  // create Thyra operator
  Teuchos::RCP<Thyra::LinearOpBase<Scalar> > thOp =
      Xpetra::ThyraUtils<Scalar,LO,GO,Node>::toThyra(bOp);
  TEUCHOS_TEST_FOR_EXCEPT(Teuchos::is_null(thOp));

  Teuchos::RCP<Thyra::BlockedLinearOpBase<Scalar> > thbOp =
      Teuchos::rcp_dynamic_cast<Thyra::BlockedLinearOpBase<Scalar> >(thOp);
  TEUCHOS_TEST_FOR_EXCEPT(Teuchos::is_null(thbOp));

  Teuchos::RCP<const Thyra::ProductVectorSpaceBase<Scalar> > productRange  = thbOp->productRange();
  Teuchos::RCP<const Thyra::ProductVectorSpaceBase<Scalar> > productDomain = thbOp->productDomain();

  TEST_EQUALITY(productRange->numBlocks()  ,2);
  TEST_EQUALITY(productDomain->numBlocks() ,2);
  TEST_EQUALITY(Teuchos::as<Xpetra::global_size_t>(productRange->dim())  ,xfullmap->getGlobalNumElements());
  TEST_EQUALITY(Teuchos::as<Xpetra::global_size_t>(productDomain->dim()) ,xfullmap->getGlobalNumElements());
  TEST_EQUALITY(Teuchos::as<Xpetra::global_size_t>(productRange->getBlock(0)->dim())  ,xvelmap->getGlobalNumElements());
  TEST_EQUALITY(Teuchos::as<Xpetra::global_size_t>(productDomain->getBlock(0)->dim()) ,xvelmap->getGlobalNumElements());
  TEST_EQUALITY(Teuchos::as<Xpetra::global_size_t>(productRange->getBlock(1)->dim())  ,xpremap->getGlobalNumElements());
  TEST_EQUALITY(Teuchos::as<Xpetra::global_size_t>(productDomain->getBlock(1)->dim()) ,xpremap->getGlobalNumElements());

  //construct a Xpetra::BlockedCrsMatrix object
  Teuchos::RCP<Xpetra::BlockedCrsMatrix<Scalar, LO, GO, Node> > bOp2 =
      Teuchos::rcp(new Xpetra::BlockedCrsMatrix<Scalar, LO, GO, Node>(thbOp,comm));

  TEUCHOS_TEST_FOR_EXCEPT(Teuchos::is_null(bOp2));
  TEST_EQUALITY(bOp2->getGlobalNumRows(),bOp->getGlobalNumRows());
  TEST_EQUALITY(bOp2->getGlobalNumCols(),bOp->getGlobalNumCols());
  TEST_EQUALITY(bOp2->getNodeNumRows(),bOp->getNodeNumRows());
  TEST_EQUALITY(bOp2->getGlobalNumEntries(),bOp->getGlobalNumEntries());
  TEST_EQUALITY(bOp2->getNodeNumEntries(),bOp->getNodeNumEntries());
  TEST_EQUALITY(bOp2->isFillComplete(),bOp->isFillComplete());
  TEST_EQUALITY(bOp2->getDomainMap()->isCompatible(*bOp->getDomainMap()),true);
  TEST_EQUALITY(bOp2->getRangeMap()->isCompatible(*bOp->getRangeMap()),true);
  TEST_EQUALITY(bOp2->getDomainMap(0)->isCompatible(*bOp->getDomainMap(0)),true);
  TEST_EQUALITY(bOp2->getRangeMap(0)->isCompatible(*bOp->getRangeMap(0)),true);
  TEST_EQUALITY(bOp2->getDomainMap(1)->isCompatible(*bOp->getDomainMap(1)),true);
  TEST_EQUALITY(bOp2->getRangeMap(1)->isCompatible(*bOp->getRangeMap(1)),true);
  TEST_EQUALITY(bOp2->getDomainMap()->isSameAs(*bOp->getDomainMap()),true);
  TEST_EQUALITY(bOp2->getRangeMap()->isSameAs(*bOp->getRangeMap()),true);
  TEST_EQUALITY(bOp2->getDomainMap(0)->isSameAs(*bOp->getDomainMap(0)),true);
  TEST_EQUALITY(bOp2->getRangeMap(0)->isSameAs(*bOp->getRangeMap(0)),true);
  TEST_EQUALITY(bOp2->getDomainMap(1)->isSameAs(*bOp->getDomainMap(1)),true);
  TEST_EQUALITY(bOp2->getRangeMap(1)->isSameAs(*bOp->getRangeMap(1)),true);

#endif // end HAVE_XPETRA_EPETRAEXT
#endif // end HAVE_XPETRA_THYRA
}

TEUCHOS_UNIT_TEST_TEMPLATE_6_DECL( ThyraBlockedOperator, SplitMatrixForThyra, M, MA, Scalar, LO, GO, Node )
{
  typedef Xpetra::Map<LO, GO, Node> MapClass;
  typedef Xpetra::MapFactory<LO, GO, Node> MapFactoryClass;
  typedef Xpetra::Vector<Scalar, LO, GO, Node> VectorClass;
  typedef Xpetra::VectorFactory<Scalar, LO, GO, Node> VectorFactoryClass;
  typedef Xpetra::MapExtractor<Scalar,LO,GO,Node> MapExtractorClass;
  typedef Xpetra::MapExtractorFactory<Scalar,LO,GO,Node> MapExtractorFactoryClass;
  typedef Teuchos::ScalarTraits<Scalar> STS;

  // get a comm and node
  Teuchos::RCP<const Teuchos::Comm<int> > comm = getDefaultComm();

  M testMap(1,0,comm);
  Xpetra::UnderlyingLib lib = testMap.lib();

  // generate problem
  GO nEle = 63;
  const Teuchos::RCP<const MapClass> map = MapFactoryClass::Build(lib, nEle, 0, comm);

  LO NumMyElements = map->getNodeNumElements();
  GO NumGlobalElements = map->getGlobalNumElements();
  Teuchos::ArrayView<const GO> MyGlobalElements = map->getNodeElementList();

  Teuchos::RCP<Xpetra::CrsMatrix<Scalar, LO, GO, Node> > A =
      Xpetra::CrsMatrixFactory<Scalar,LO,GO,Node>::Build(map, 3);
  TEUCHOS_TEST_FOR_EXCEPTION(A->isFillComplete() == true || A->isFillActive() == false, std::runtime_error, "");

  for (LO i = 0; i < NumMyElements; i++) {
     if (MyGlobalElements[i] == 0) {
       A->insertGlobalValues(MyGlobalElements[i],
                             Teuchos::tuple<GO>(MyGlobalElements[i], MyGlobalElements[i] +1),
                             Teuchos::tuple<Scalar> (Teuchos::as<Scalar>(i)*STS::one(), -1.0));
     }
     else if (MyGlobalElements[i] == NumGlobalElements - 1) {
       A->insertGlobalValues(MyGlobalElements[i],
                             Teuchos::tuple<GO>(MyGlobalElements[i] -1, MyGlobalElements[i]),
                             Teuchos::tuple<Scalar> (-1.0, Teuchos::as<Scalar>(i)*STS::one()));
     }
     else {
       A->insertGlobalValues(MyGlobalElements[i],
                             Teuchos::tuple<GO>(MyGlobalElements[i] -1, MyGlobalElements[i], MyGlobalElements[i] +1),
                             Teuchos::tuple<Scalar> (-1.0, Teuchos::as<Scalar>(i)*STS::one(), -1.0));
     }
  }

  A->fillComplete();
  TEUCHOS_TEST_FOR_EXCEPTION(A->isFillComplete() == false || A->isFillActive() == true, std::runtime_error, "");

  Teuchos::RCP<Xpetra::Matrix<Scalar, LO, GO, Node> > mat =
      Teuchos::rcp(new Xpetra::CrsMatrixWrap<Scalar, LO, GO, Node>(A));

  Teuchos::Array<GO> gids1;
  Teuchos::Array<GO> gids2;
  for(LO i=0; i<NumMyElements; i++) {
    if(i % 3 < 2)
      gids1.push_back(map->getGlobalElement(i));
    else
      gids2.push_back(map->getGlobalElement(i));
  }

  const Teuchos::RCP<const MapClass> map1 = MapFactoryClass::Build (lib,
      Teuchos::OrdinalTraits<GO>::invalid(),
      gids1.view(0,gids1.size()),
      0,
      comm);
  const Teuchos::RCP<const MapClass> map2 = MapFactoryClass::Build (lib,
      Teuchos::OrdinalTraits<GO>::invalid(),
      gids2.view(0,gids2.size()),
      0,
      comm);

  std::vector<Teuchos::RCP<const MapClass> > xmaps;
  xmaps.push_back(map1);
  xmaps.push_back(map2);

  Teuchos::RCP<const MapExtractorClass> map_extractor = MapExtractorFactoryClass::Build(map,xmaps);

  Teuchos::RCP<Xpetra::BlockedCrsMatrix<Scalar,LO,GO,Node> > bOp =
      Xpetra::MatrixUtils<Scalar, LO, GO, Node>::SplitMatrix(*mat,map_extractor,map_extractor,Teuchos::null,true);

  // build gloabl vector with one entries
  Teuchos::RCP<VectorClass> ones = VectorFactoryClass::Build(map, true);
  Teuchos::RCP<VectorClass> exp  = VectorFactoryClass::Build(map, true);
  Teuchos::RCP<VectorClass> res  = VectorFactoryClass::Build(map, true);
  Teuchos::RCP<VectorClass> rnd  = VectorFactoryClass::Build(map, true);
  ones->putScalar(STS::one());
  rnd->randomize();

  A->apply(*ones, *exp);
  bOp->apply(*ones, *res);
  res->update(-STS::one(),*exp,STS::one());
  TEUCHOS_TEST_COMPARE(res->norm2(), <, 1e-16, out, success);
  TEUCHOS_TEST_COMPARE(res->normInf(), <, 1e-16, out, success);

  A->apply(*rnd, *exp);
  bOp->apply(*rnd, *res);
  res->update(-STS::one(),*exp,STS::one());
  TEUCHOS_TEST_COMPARE(res->norm2(), <, 5e-14, out, success);
  TEUCHOS_TEST_COMPARE(res->normInf(), <, 5e-14, out, success);
}

//
// INSTANTIATIONS
//
#ifdef HAVE_XPETRA_TPETRA

  #define XPETRA_TPETRA_TYPES( S, LO, GO, N) \
    typedef typename Xpetra::TpetraMap<LO,GO,N> M##LO##GO##N; \
    typedef typename Xpetra::TpetraCrsMatrix<S,LO,GO,N> MA##S##LO##GO##N;

#endif

#ifdef HAVE_XPETRA_EPETRA

  #define XPETRA_EPETRA_TYPES( S, LO, GO, N) \
    typedef typename Xpetra::EpetraMapT<GO,N> M##LO##GO##N; \
    typedef typename Xpetra::EpetraCrsMatrixT<GO,N> MA##S##LO##GO##N;

#endif

// List of tests which run both with Epetra and Tpetra
#define XP_MATRIX_INSTANT(S,LO,GO,N) \
    TEUCHOS_UNIT_TEST_TEMPLATE_6_INSTANT( ThyraBlockedOperator, ThyraOperator2XpetraCrsMat, M##LO##GO##N , MA##S##LO##GO##N, S, LO, GO, N ) \
    TEUCHOS_UNIT_TEST_TEMPLATE_6_INSTANT( ThyraBlockedOperator, ThyraShrinkMaps,            M##LO##GO##N , MA##S##LO##GO##N, S, LO, GO, N )
    //TEUCHOS_UNIT_TEST_TEMPLATE_6_INSTANT( ThyraBlockedOperator, SplitMatrixForThyra, M##LO##GO##N , MA##S##LO##GO##N, S, LO, GO, N )

// List of tests which run only with Tpetra
#define XP_TPETRA_MATRIX_INSTANT(S,LO,GO,N) \
    TEUCHOS_UNIT_TEST_TEMPLATE_6_INSTANT( ThyraBlockedOperator, ThyraVectorSpace2XpetraMap_Tpetra, M##LO##GO##N , MA##S##LO##GO##N, S, LO, GO, N )

// List of tests which run only with Epetra
#define XP_EPETRA_MATRIX_INSTANT(S,LO,GO,N) \
    TEUCHOS_UNIT_TEST_TEMPLATE_6_INSTANT( ThyraBlockedOperator, ThyraVectorSpace2XpetraMap_Epetra, M##LO##GO##N , MA##S##LO##GO##N, S, LO, GO, N ) \
    TEUCHOS_UNIT_TEST_TEMPLATE_6_INSTANT( ThyraBlockedOperator, ThyraBlockedOperator2XpetraBlockedCrsMat, M##LO##GO##N , MA##S##LO##GO##N, S, LO, GO, N ) \
    TEUCHOS_UNIT_TEST_TEMPLATE_6_INSTANT( ThyraBlockedOperator, XpetraBlockedCrsMatConstructor, M##LO##GO##N , MA##S##LO##GO##N, S, LO, GO, N )

#if defined(HAVE_XPETRA_TPETRA)

#include <TpetraCore_config.h>
#include <TpetraCore_ETIHelperMacros.h>

TPETRA_ETI_MANGLING_TYPEDEFS()
TPETRA_INSTANTIATE_SLGN_NO_ORDINAL_SCALAR ( XPETRA_TPETRA_TYPES )
TPETRA_INSTANTIATE_SLGN_NO_ORDINAL_SCALAR ( XP_MATRIX_INSTANT )
TPETRA_INSTANTIATE_SLGN_NO_ORDINAL_SCALAR ( XP_TPETRA_MATRIX_INSTANT )

#endif


#if defined(HAVE_XPETRA_EPETRA)

#include "Xpetra_Map.hpp" // defines EpetraNode
typedef Xpetra::EpetraNode EpetraNode;
#ifndef XPETRA_EPETRA_NO_32BIT_GLOBAL_INDICES
XPETRA_EPETRA_TYPES(double,int,int,EpetraNode)
XP_MATRIX_INSTANT(double,int,int,EpetraNode)
XP_EPETRA_MATRIX_INSTANT(double,int,int,EpetraNode)
#endif
// EpetraExt routines are not working with 64 bit
/*#ifndef XPETRA_EPETRA_NO_64BIT_GLOBAL_INDICES
typedef long long LongLong;
XPETRA_EPETRA_TYPES(double,int,LongLong,EpetraNode)
XP_EPETRA_MATRIX_INSTANT(double,int,LongLong,EpetraNode)
#endif*/

#endif

}
