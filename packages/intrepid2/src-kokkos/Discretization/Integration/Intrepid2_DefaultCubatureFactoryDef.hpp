// @HEADER
// ************************************************************************
//
//                           Intrepid2 Package
//                 Copyright (2007) Sandia Corporation
//
// Under terms of Contract DE-AC04-94AL85000, there is a non-exclusive
// license for use of this work by or on behalf of the U.S. Government.
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
// Questions? Contact Kyungjoo Kim  (kyukim@sandia.gov), or
//                    Mauro Perego  (mperego@sandia.gov)
//
// ************************************************************************
// @HEADER

/** \file   Intrepid_DefaultCubatureFactoryDef.hpp
    \brief  Definition file for the class Intrepid2::DefaultCubatureFactory.
    \author Created by P. Bochev and D. Ridzal.
            Kokkorized by Kyungjoo Kim
*/

#ifndef __INTREPID2_DEFAULT_CUBATURE_FACTORY_DEF_HPP__
#define __INTREPID2_DEFAULT_CUBATURE_FACTORY_DEF_HPP__

namespace Intrepid2 {

  // first create method
  template<typename SpT>
  Teuchos::RCP<Cubature<SpT> > 
  DefaultCubatureFactory::
  create<SpT>( const shards::CellTopology       cellTopology,
               const std::vector<ordinal_type> &degree ) {
    
    // Create generic cubature.
    Teuchos::RCP<Cubature<SpT> > r_val;

    switch (cellTopology.getBasekey()) {
    case shards::Line<>::key:
      INTREPID2_TEST_FOR_EXCEPTION( (degree.size() < 1), std::invalid_argument,
                                    ">>> ERROR (DefaultCubatureFactory): Provided degree array is of insufficient length.");
      r_val = Teuchos::rcp(new CubatureDirectLineGauss<SpT>(degree[0]));
      break;

    // case shards::Triangle<>::key:
    //   INTREPID2_TEST_FOR_EXCEPTION( (degree.size() < 1), std::invalid_argument,
    //                                 ">>> ERROR (DefaultCubatureFactory): Provided degree array is of insufficient length.");
    //   r_val = Teuchos::rcp(new CubatureDirectTriDefault<SpT>(degree[0]));
    //   break;

    case shards::Quadrilateral<>::key:
      INTREPID2_TEST_FOR_EXCEPTION( (degree.size() < 2), std::invalid_argument,
                                    ">>> ERROR (DefaultCubatureFactory): Provided degree array is of insufficient length.");
      {
        const auto x_line = CubatureDirectLineGauss<SpT>(degree[0]);
        const auto y_line = ( degree[1] == degree[0] ? x_line : CubatureDirectLineGauss<SpT>(degree[1]) );
        r_val = Teuchos::rcp(new CubatureTensor<SpT>( x_line, y_line ));
      }
      break;

    // case shards::Tetrahedron<>::key:
    //   if (cellTopology.getCellTopologyData()->key == shards::Tetrahedron<11>::key)
    //     {
    //       INTREPID2_TEST_FOR_EXCEPTION( (degree.size() < 1), std::invalid_argument,
    //                                     ">>> ERROR (DefaultCubatureFactory): Provided degree array is of insufficient length.");
    //       //r_val = Teuchos::rcp(new CubatureCompositeTet<SpT>(degree[0]));
    //       r_val = Teuchos::rcp(new CubatureDirectTetDefault<SpT>(degree[0]));
    //     } 
    //   else
    //     {
    //       INTREPID2_TEST_FOR_EXCEPTION( (degree.size() < 1), std::invalid_argument,
    //                                     ">>> ERROR (DefaultCubatureFactory): Provided degree array is of insufficient length.");
    //       r_val = Teuchos::rcp(new CubatureDirectTetDefault<SpT>(degree[0]));
    //     }
    //   break;

    case shards::Hexahedron<>::key:
      INTREPID2_TEST_FOR_EXCEPTION( (degree.size() < 3), std::invalid_argument,
                                    ">>> ERROR (DefaultCubatureFactory): Provided degree array is of insufficient length.");
      {
        const auto x_line = CubatureDirectLineGauss<SpT>(degree[0]);
        const auto y_line = ( degree[1] == degree[0] ? x_line : CubatureDirectLineGauss<SpT>(degree[1]) );
        const auto z_line = ( degree[2] == degree[0] ? x_line : 
                              degree[2] == degree[1] ? y_line : CubatureDirectLineGauss<SpT>(degree[2]) );

        r_val = Teuchos::rcp(new CubatureTensor<SpT>( x_line, y_line, z_line ));
      }
      break;

    // case shards::Wedge<>::key:
    //   INTREPID2_TEST_FOR_EXCEPTION( (degree.size() < 2), std::invalid_argument,
    //                                 ">>> ERROR (DefaultCubatureFactory): Provided degree array is of insufficient length.")
    //     {
    //       std::vector< Teuchos::RCP< Cubature<SpT> > > miscCubs(2);
    //       miscCubs[0]  = Teuchos::rcp(new CubatureDirectTriDefault<SpT>(degree[0]));
    //       miscCubs[1]  = Teuchos::rcp(new CubatureDirectLineGauss<SpT>(degree[1]));
    //       r_val = Teuchos::rcp(new CubatureTensor<SpT>(miscCubs));
    //     }
    //   break;

    // case shards::Pyramid<>::key:
    //   INTREPID2_TEST_FOR_EXCEPTION( (degree.size() < 3), std::invalid_argument,
    //                                 ">>> ERROR (DefaultCubatureFactory): Provided degree array is of insufficient length.");
    //   {
    //     std::vector< Teuchos::RCP< Cubature<SpT> > > lineCubs(3);
    //     lineCubs[0]  = Teuchos::rcp(new CubatureDirectLineGauss<SpT>(degree[0]));
    //     lineCubs[1]  = Teuchos::rcp(new CubatureDirectLineGauss<SpT>(degree[1]));
    //     lineCubs[2]  = Teuchos::rcp(new CubatureDirectLineGaussJacobi20<SpT>(degree[2]));
    //     r_val = Teuchos::rcp(new CubatureTensorPyr<SpT>(lineCubs));
    //   }
    //   break;

    default:
      INTREPID2_TEST_FOR_EXCEPTION( ( (cellTopology.getBaseCellTopologyData()->key != shards::Line<>::key)             &&
                                      (cellTopology.getBaseCellTopologyData()->key != shards::Triangle<>::key)         &&
                                      (cellTopology.getBaseCellTopologyData()->key != shards::Quadrilateral<>::key)    &&
                                      (cellTopology.getBaseCellTopologyData()->key != shards::Tetrahedron<>::key)      &&
                                      (cellTopology.getBaseCellTopologyData()->key != shards::Hexahedron<>::key)       &&
                                      (cellTopology.getBaseCellTopologyData()->key != shards::Pyramid<>::key)       &&
                                      (cellTopology.getBaseCellTopologyData()->key != shards::Wedge<>::key) ),
                                    std::invalid_argument,
                                    ">>> ERROR (DefaultCubatureFactory): Invalid cell topology prevents cubature creation.");
    }

    return r_val;
  }


  template<typename SpT>
  Teuchos::RCP<Cubature<SpT> > 
  DefaultCubatureFactory::
  create<SpT>(const shards::CellTopology &cellTopology,
                        const ordinal_type          degree) {
    // uniform order for 3 axes
    std::vector<int> degreeArray;
    degreeArray.assign(3, degree);

    return create(cellTopology, degreeArray);
  }


  // template<typename SpT>
  // template<typename cellVertexValueType, class ...cellVertexProperties>
  // Teuchos::RCP<Cubature<SpT> > 
  // DefaultCubatureFactory::
  // create<SpT>(const shards::CellTopology& cellTopology,
  //                       const Kokkos::DynRankView<cellVertexValueType,cellVertexProperties> cellVertices,
  //                       ordinal_type degree){
  //   return Teuchos::rcp(new CubaturePolygon<SpT>(cellTopology,cellVertices, degree));
  // }

} // namespace Intrepid2

#endif
