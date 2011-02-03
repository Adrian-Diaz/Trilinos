#ifndef PANZER_TRAITS_HPP
#define PANZER_TRAITS_HPP

// Teuchos includes
#include "Teuchos_RCP.hpp"

// mpl (Meta Programming Library) templates
#include "Sacado_mpl_vector.hpp"
#include "Sacado_mpl_find.hpp"
#include "boost/mpl/map.hpp"
#include "boost/mpl/find.hpp"

// Scalar types
#include "Sacado.hpp"

// traits Base Class
#include "Phalanx_Traits_Base.hpp"

// Include User Data Types
#include "Phalanx_Allocator_Contiguous.hpp"
#include "Panzer_Workset.hpp"

// Debugging information
#include "Phalanx_TypeStrings.hpp"

namespace panzer {
  template <typename LocalOrdinalT,typename GlobalOrdinalT>
  class UniqueGlobalIndexer;

  struct Traits : public PHX::TraitsBase {

    typedef int LocalOrdinal; 
    typedef int GlobalOrdinal; 
    
    // ******************************************************************
    // *** Scalar Types
    // ******************************************************************
    
    // Scalar types we plan to use
    typedef double RealType;
    typedef Sacado::Fad::DFad<double> FadType;
    
    // ******************************************************************
    // *** Evaluation Types
    // ******************************************************************
    struct Residual { typedef RealType ScalarT; };
    struct Jacobian { typedef FadType ScalarT;  };
    typedef Sacado::mpl::vector<Residual, Jacobian> EvalTypes;

    // ******************************************************************
    // *** Data Types
    // ******************************************************************
    
    // Create the data types for each evaluation type
    
    // Residual (default scalar type is RealType)
    typedef Sacado::mpl::vector< RealType > ResidualDataTypes;
  
    // Jacobian (default scalar type is Fad<double, double>)
    typedef Sacado::mpl::vector< FadType > JacobianDataTypes;

    // Maps the key EvalType a vector of DataTypes
    typedef boost::mpl::map<
      boost::mpl::pair<Residual, ResidualDataTypes>,
      boost::mpl::pair<Jacobian, JacobianDataTypes>
    >::type EvalToDataMap;

    // ******************************************************************
    // *** Allocator Type
    // ******************************************************************
    typedef PHX::ContiguousAllocator<double> Allocator;

    // ******************************************************************
    // *** User Defined Object Passed in for Evaluation Method
    // ******************************************************************
    struct SD { 
      Teuchos::RCP< std::vector<panzer::Workset> > worksets_;
    };
    typedef SD SetupData;
    typedef panzer::Workset& EvalData;
    typedef void* PreEvalData;
    typedef void* PostEvalData;

  };
 
  // ******************************************************************
  // ******************************************************************
  // Debug strings.  Specialize the Evaluation and Data types for the
  // TypeString object in the phalanx/src/Phalanx_TypeStrings.hpp file.
  // ******************************************************************
  // ******************************************************************

}

namespace PHX {
  
  // Evaluation Types
  template<> struct TypeString<panzer::Traits::Residual> 
  { static const std::string value; };

  template<> struct TypeString<panzer::Traits::Jacobian> 
  { static const std::string value; };

  // Data Types
  template<> struct TypeString<double> 
  { static const std::string value; };

  template<> struct TypeString< Sacado::Fad::DFad<double> > 
  { static const std::string value; };

}

#endif
