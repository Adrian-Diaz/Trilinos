
#ifndef PANZER_BASIS_VALUES_DECL_HPP
#define PANZER_BASIS_VALUES_DECL_HPP

#include "Teuchos_RCP.hpp"

#include "Intrepid_Basis.hpp"
#include "Panzer_BasisIRLayout.hpp"

namespace panzer {

  template<typename Scalar,typename Array>
  struct BasisValues { 
    
    //! Sizes/allocates memory for arrays
    void setupArrays(const Teuchos::RCP<panzer::BasisIRLayout>& basis);
   
    void evaluateValues(const Array& cub_points,
			const Array& jac_inv,
			const Array& weighted_measure,
			const Array& node_coordinates);

    Array basis_ref;           // <BASIS,IP>
    Array basis;               // <Cell,BASIS,IP>
    Array grad_basis_ref;      // <BASIS,IP,Dim>
    Array grad_basis;          // <Cell,BASIS,IP,Dim>
    Array weighted_basis;      // <Cell,BASIS,IP>
    Array weighted_grad_basis; // <Cell,BASIS,IP,Dim>

    /** Carterisan coordinates for basis coefficients

        NOTE: This quantity is not always available.  Certain bases
        may not have a corresponding coordiante value
    */
    Array basis_coordinates_ref;     // <Cell,BASIS>

    /** Carterisan coordinates for basis coefficients

        NOTE: This quantity is not always available.  Certain bases
        may not have a corresponding coordiante value
    */
    Array basis_coordinates;         // <Cell,BASIS,Dim>

    Teuchos::RCP<panzer::BasisIRLayout> panzer_basis;
    
    Teuchos::RCP<Intrepid::Basis<double,Array> > intrepid_basis;
  };

} // namespace panzer

#include "Panzer_BasisValues_impl.hpp"

#endif

