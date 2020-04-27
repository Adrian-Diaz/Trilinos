// @HEADER
// ************************************************************************
//
//               Rapid Optimization Library (ROL) Package
//                 Copyright (2014) Sandia Corporation
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
// Questions? Contact lead developers:
//              Drew Kouri   (dpkouri@sandia.gov) and
//              Denis Ridzal (dridzal@sandia.gov)
//
// ************************************************************************
// @HEADER

#ifndef ROL_ALGORITHM_B_FACTORY_H
#define ROL_ALGORITHM_B_FACTORY_H

#include "ROL_GradientAlgorithm_B.hpp"
#include "ROL_NewtonKrylovAlgorithm_B.hpp"
#include "ROL_LinMoreAlgorithm_B.hpp"
#include "ROL_MoreauYosidaAlgorithm_B.hpp"
#include "ROL_Types.hpp"

namespace ROL {

/** \enum   ROL::EAlgorithmB
    \brief  Enumeration of bound constrained algorithm types.

    \arg    ALGORITHM_B_LINESEARCH     describe
    \arg    ALGORITHM_B_TRUSTREGION    describe
    \arg    ALGORITHM_B_MOREAUYOSIDA   describe
 */
enum EAlgorithmB{
  ALGORITHM_B_LINESEARCH = 0,
  ALGORITHM_B_TRUSTREGION,
  ALGORITHM_B_MOREAUYOSIDA,
  ALGORITHM_B_LAST
};

inline std::string EAlgorithmBToString(EAlgorithmB alg) {
  std::string retString;
  switch(alg) {
    case ALGORITHM_B_LINESEARCH:   retString = "Line Search";       break;
    case ALGORITHM_B_TRUSTREGION:  retString = "Trust Region";      break;
    case ALGORITHM_B_MOREAUYOSIDA: retString = "Moreau-Yosida";     break;
    case ALGORITHM_B_LAST:         retString = "Last Type (Dummy)"; break;
    default:                       retString = "INVALID EAlgorithmB";
  }
  return retString;
}

/** \brief  Verifies validity of a AlgorithmB enum.
  
    \param  ls  [in]  - enum of the AlgorithmB
    \return 1 if the argument is a valid AlgorithmB; 0 otherwise.
  */
inline int isValidAlgorithmB(EAlgorithmB alg){
  return( (alg == ALGORITHM_B_LINESEARCH)   ||
          (alg == ALGORITHM_B_TRUSTREGION)  ||
          (alg == ALGORITHM_B_MOREAUYOSIDA) ||
          (alg == ALGORITHM_B_LAST)
        );
}

inline EAlgorithmB & operator++(EAlgorithmB &type) {
  return type = static_cast<EAlgorithmB>(type+1);
}

inline EAlgorithmB operator++(EAlgorithmB &type, int) {
  EAlgorithmB oldval = type;
  ++type;
  return oldval;
}

inline EAlgorithmB & operator--(EAlgorithmB &type) {
  return type = static_cast<EAlgorithmB>(type-1);
}

inline EAlgorithmB operator--(EAlgorithmB &type, int) {
  EAlgorithmB oldval = type;
  --type;
  return oldval;
}

inline EAlgorithmB StringToEAlgorithmB(std::string s) {
  s = removeStringFormat(s);
  for ( EAlgorithmB alg = ALGORITHM_B_LINESEARCH; alg < ALGORITHM_B_LAST; alg++ ) {
    if ( !s.compare(removeStringFormat(EAlgorithmBToString(alg))) ) {
      return alg;
    }
  }
  return ALGORITHM_B_TRUSTREGION;
}

template<typename Real>
inline Ptr<Algorithm_B<Real>> AlgorithmBFactory(ParameterList &parlist) {
  EAlgorithmB ealg = StringToEAlgorithmB(parlist.sublist("Step").get("Type","Trust Region"));
  std::string desc
    = parlist.sublist("Step").sublist("Line Search").sublist("Descent Method").get("Type","Newton-Krylov");
  switch(ealg) {
    case ALGORITHM_B_LINESEARCH:
      if (desc=="Newton-Krylov" || desc=="Newton" || desc=="Quasi-Newton Method")
        return makePtr<NewtonKrylovAlgorithm_B<Real>>(parlist);
      else
        return makePtr<GradientAlgorithm_B<Real>>(parlist);
    case ALGORITHM_B_TRUSTREGION:  return makePtr<LinMoreAlgorithm_B<Real>>(parlist);
    case ALGORITHM_B_MOREAUYOSIDA: return makePtr<MoreauYosidaAlgorithm_B<Real>>(parlist);
    default:                       return nullPtr;
  }
}
} // namespace ROL

#endif
