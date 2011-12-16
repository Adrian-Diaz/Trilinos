#ifndef PANZER_ZERO_SENSITIVITIES_HPP
#define PANZER_ZERO_SENSITIVITIES_HPP

#include <iostream>
#include "Panzer_Traits.hpp" // for scalar types

namespace panzer {

  //! Zero out AD types so that we can drop sensitivity contributions
  template<typename ScalarT> 
  void zeroSensitivities(ScalarT& s);

  //! Specialization for Residual
  inline void zeroSensitivities(double& s)
  {
  
  }

  //! Specialization for Jacobian
  inline void zeroSensitivities(Sacado::Fad::DFad<double>& s) 
  {
    //cout << "Zeroing out fad!" << endl;
    //s.setIsConstant(true);
    s.zero();
  }

  //! Specialization for Jacobian
  inline void zeroSensitivities(Sacado::CacheFad::DFad<double>& s) 
  {
    //cout << "Zeroing out fad!" << endl;
    //s.setIsConstant(true);
    s.zero();
  }

  //! Specialization for Jacobian
  inline void zeroSensitivities(Sacado::ELRFad::DFad<double>& s) 
  {
    //cout << "Zeroing out fad!" << endl;
    //s.setIsConstant(true);
    s.zero();
  }

  //! Specialization for Jacobian
  inline void zeroSensitivities(Sacado::ELRCacheFad::DFad<double>& s) 
  {
    //cout << "Zeroing out fad!" << endl;
    //s.setIsConstant(true);
    s.zero();
  }

#ifdef HAVE_STOKHOS
  //! Specialization for Residual
  inline void zeroSensitivities(Traits::SGType & s)
  {
  }


  //! Specialization for Residual
  inline void zeroSensitivities(Traits::SGFadType & s)
  {
    s.zero();
  }
#endif

}

#endif
