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

/** \file
    \brief  Contains definitions of custom data types in ROL.
    \author Created by D. Ridzal and D. Kouri.
 */

#ifndef ROL_TYPES_HPP
#define ROL_TYPES_HPP

#ifdef  HAVE_ROL_DEBUG
#define ROL_VALIDATE( A )  A
#else
#define ROL_VALIDATE( A ) /* empty */
#endif

#include <algorithm>
#include <string>
#include <Teuchos_ScalarTraits.hpp>
#include <Teuchos_TestForException.hpp>

/** \def    ROL_NUM_CHECKDERIV_STEPS
    \brief  Number of steps for derivative checks.
 */
#define ROL_NUM_CHECKDERIV_STEPS 13

namespace ROL {
  
  /** \brief  Platform-dependent machine epsilon. 
   */
  static const double ROL_EPSILON   = std::abs(Teuchos::ScalarTraits<double>::eps());
    
  /** \brief  Tolerance for various equality tests.
   */
  static const double ROL_THRESHOLD = 10.0 * ROL_EPSILON;

  struct removeSpecialCharacters {
    bool operator()(char c) {
      return (c ==' ' || c =='-' || c == '(' || c == ')' || c=='\'' || c=='\r' || c=='\n' || c=='\t');
    }
  };

  inline std::string removeStringFormat( std::string s ) {
    std::string output = s;
    output.erase( std::remove_if( output.begin(), output.end(), removeSpecialCharacters()), output.end() );
    std::transform( output.begin(), output.end(), output.begin(), ::tolower );
    return output;
  }

  /** \enum   ROL::EDescent
      \brief  Enumeration of descent direction types.

      \arg    STEEPEST        describe
      \arg    NONLINEARCG     describe
      \arg    SECANT          describe
      \arg    NEWTON          describe 
      \arg    NEWTONKRYLOV    describe
      \arg    SECANTPRECOND   describe
   */
  enum EDescent{
    DESCENT_STEEPEST = 0,
    DESCENT_NONLINEARCG,
    DESCENT_SECANT,
    DESCENT_NEWTON,
    DESCENT_NEWTONKRYLOV,
    DESCENT_SECANTPRECOND,
    DESCENT_LAST
  };

  inline std::string EDescentToString(EDescent tr) {
    std::string retString;
    switch(tr) {
      case DESCENT_STEEPEST:      retString = "Steepest Descent";                          break;
      case DESCENT_NONLINEARCG:   retString = "Nonlinear CG";                              break;
      case DESCENT_SECANT:        retString = "Quasi-Newton Method";                       break;
      case DESCENT_NEWTON:        retString = "Newton's Method";                           break;
      case DESCENT_NEWTONKRYLOV:  retString = "Newton-Krylov";                             break;
      case DESCENT_SECANTPRECOND: retString = "Newton-Krylov with Secant Preconditioning"; break;
      case DESCENT_LAST:          retString = "Last Type (Dummy)";                         break;
      default:                    retString = "INVALID EDescent";
    }
    return retString;
  }

  /** \brief  Verifies validity of a Secant enum.
    
      \param  tr  [in]  - enum of the Secant
      \return 1 if the argument is a valid Secant; 0 otherwise.
    */
  inline int isValidDescent(EDescent d){
    return( (d == DESCENT_STEEPEST)      ||
            (d == DESCENT_NONLINEARCG)   ||
            (d == DESCENT_SECANT)        ||
            (d == DESCENT_NEWTON)        ||
            (d == DESCENT_NEWTONKRYLOV)  ||
            (d == DESCENT_SECANTPRECOND)
          );
  }

  inline EDescent & operator++(EDescent &type) {
    return type = static_cast<EDescent>(type+1);
  }

  inline EDescent operator++(EDescent &type, int) {
    EDescent oldval = type;
    ++type;
    return oldval;
  }

  inline EDescent & operator--(EDescent &type) {
    return type = static_cast<EDescent>(type-1);
  }

  inline EDescent operator--(EDescent &type, int) {
    EDescent oldval = type;
    --type;
    return oldval;
  }

  inline EDescent StringToEDescent(std::string s) {
    s = removeStringFormat(s);
    for ( EDescent des = DESCENT_STEEPEST; des < DESCENT_LAST; des++ ) {
      if ( !s.compare(removeStringFormat(EDescentToString(des))) ) {
        return des;
      }
    }
    return DESCENT_STEEPEST;
  }
  
  /** \enum   ROL::ESecant
      \brief  Enumeration of secant update algorithms.

      \arg    LBFGS           describe
      \arg    LDFP            describe
      \arg    LSR1            describe 
      \arg    BARZILAIBORWEIN describe
   */
  enum ESecant{
    SECANT_LBFGS = 0,
    SECANT_LDFP,
    SECANT_LSR1,
    SECANT_BARZILAIBORWEIN,
    SECANT_USERDEFINED,
    SECANT_LAST
  };

  inline std::string ESecantToString(ESecant tr) {
    std::string retString;
    switch(tr) {
      case SECANT_LBFGS:           retString = "Limited-Memory BFGS"; break;
      case SECANT_LDFP:            retString = "Limited-Memory DFP";  break;
      case SECANT_LSR1:            retString = "Limited-Memory SR1";  break;
      case SECANT_BARZILAIBORWEIN: retString = "Barzilai-Borwein";    break;
      case SECANT_USERDEFINED:     retString = "User-Defined";        break;
      case SECANT_LAST:            retString = "Last Type (Dummy)";   break;
      default:                     retString = "INVALID ESecant";
    }
    return retString;
  }
  
  /** \brief  Verifies validity of a Secant enum.
    
      \param  tr  [in]  - enum of the Secant
      \return 1 if the argument is a valid Secant; 0 otherwise.
    */
  inline int isValidSecant(ESecant s) {
    return( (s == SECANT_LBFGS)           ||
            (s == SECANT_LDFP)            ||
            (s == SECANT_LSR1)            ||
            (s == SECANT_BARZILAIBORWEIN) ||
            (s == SECANT_USERDEFINED)
          );
  }

  inline ESecant & operator++(ESecant &type) {
    return type = static_cast<ESecant>(type+1);
  }

  inline ESecant operator++(ESecant &type, int) {
    ESecant oldval = type;
    ++type;
    return oldval;
  }

  inline ESecant & operator--(ESecant &type) {
    return type = static_cast<ESecant>(type-1);
  }

  inline ESecant operator--(ESecant &type, int) {
    ESecant oldval = type;
    --type;
    return oldval;
  }

  inline ESecant StringToESecant(std::string s) {
    s = removeStringFormat(s);
    for ( ESecant sec = SECANT_LBFGS; sec < SECANT_LAST; sec++ ) {
      if ( !s.compare(removeStringFormat(ESecantToString(sec))) ) {
        return sec;
      }
    }
    return SECANT_LBFGS;
  }

  /** \enum   ROL::EKrylov
      \brief  Enumeration of Krylov methods.

      \arg    CG        describe
   */
  enum EKrylov{
    KRYLOV_CG = 0,
    KRYLOV_CR,
    KRYLOV_LAST
  };

  inline std::string EKrylovToString(EKrylov kv) {
    std::string retString;
    switch(kv) {
      case KRYLOV_CG:      retString = "Conjugate Gradients"; break;
      case KRYLOV_CR:      retString = "Conjugate Residuals"; break;
      case KRYLOV_LAST:    retString = "Last Type (Dummy)";   break;
      default:             retString = "INVALID EKrylov";
    }
    return retString;
  }

  /** \brief  Verifies validity of a Krylov enum.
    
      \param  kv  [in]  - enum of the Krylov
      \return 1 if the argument is a valid Krylov; 0 otherwise.
    */
  inline int isValidKrylov(EKrylov kv){
    return( (kv == KRYLOV_CG) ||
            (kv == KRYLOV_CR) );
  }

  inline EKrylov & operator++(EKrylov &type) {
    return type = static_cast<EKrylov>(type+1);
  }

  inline EKrylov operator++(EKrylov &type, int) {
    EKrylov oldval = type;
    ++type;
    return oldval;
  }

  inline EKrylov & operator--(EKrylov &type) {
    return type = static_cast<EKrylov>(type-1);
  }

  inline EKrylov operator--(EKrylov &type, int) {
    EKrylov oldval = type;
    --type;
    return oldval;
  }

  inline EKrylov StringToEKrylov(std::string s) {
    s = removeStringFormat(s);
    for ( EKrylov kv = KRYLOV_CG; kv < KRYLOV_LAST; kv++ ) {
      if ( !s.compare(removeStringFormat(EKrylovToString(kv))) ) {
        return kv;
      }
    }
    return KRYLOV_CG;
  }
  
  /** \enum   ROL::ENonlinearCG
      \brief  Enumeration of nonlinear CG algorithms.

      \arg    HESTENES_STIEFEL   describe
      \arg    FLETCHER_REEVES    describe
      \arg    DANIEL             describe 
      \arg    POLAK_RIBIERE      describe
      \arg    FLETCHER_CONJDESC  describe
      \arg    LIU_STOREY         describe
      \arg    DAI_YUAN           describe
      \arg    HAGAR_ZHANG        describe
   */
  enum ENonlinearCG{
    NONLINEARCG_HESTENES_STIEFEL = 0,
    NONLINEARCG_FLETCHER_REEVES,
    NONLINEARCG_DANIEL,
    NONLINEARCG_POLAK_RIBIERE,
    NONLINEARCG_FLETCHER_CONJDESC,
    NONLINEARCG_LIU_STOREY,
    NONLINEARCG_DAI_YUAN,
    NONLINEARCG_HAGAR_ZHANG,
    NONLINEARCG_LAST
  };

  inline std::string ENonlinearCGToString(ENonlinearCG tr) {
    std::string retString;
    switch(tr) {
      case NONLINEARCG_HESTENES_STIEFEL:      retString = "Hestenes-Stiefel";            break;
      case NONLINEARCG_FLETCHER_REEVES:       retString = "Fletcher-Reeves";             break;
      case NONLINEARCG_DANIEL:                retString = "Daniel (uses Hessian)";       break;
      case NONLINEARCG_POLAK_RIBIERE:         retString = "Polak-Ribiere";               break;
      case NONLINEARCG_FLETCHER_CONJDESC:     retString = "Fletcher Conjugate Descent";  break;
      case NONLINEARCG_LIU_STOREY:            retString = "Liu-Storey";                  break;
      case NONLINEARCG_DAI_YUAN:              retString = "Dai-Yuan";                    break;
      case NONLINEARCG_HAGAR_ZHANG:           retString = "Hagar-Zhang";                 break;
      case NONLINEARCG_LAST:                  retString = "Last Type (Dummy)";           break;
      default:                                retString = "INVALID ENonlinearCG";
    }
    return retString;
  }
  
  /** \brief  Verifies validity of a NonlinearCG enum.
    
      \param  tr  [in]  - enum of the NonlinearCG
      \return 1 if the argument is a valid NonlinearCG; 0 otherwise.
    */
  inline int isValidNonlinearCG(ENonlinearCG s) {
    return( (s == NONLINEARCG_HESTENES_STIEFEL)  ||
            (s == NONLINEARCG_FLETCHER_REEVES)   ||
            (s == NONLINEARCG_DANIEL)            ||
            (s == NONLINEARCG_POLAK_RIBIERE)     ||
            (s == NONLINEARCG_FLETCHER_CONJDESC) ||
            (s == NONLINEARCG_LIU_STOREY)        ||
            (s == NONLINEARCG_DAI_YUAN)          ||
            (s == NONLINEARCG_HAGAR_ZHANG)
          );
  }

  inline ENonlinearCG & operator++(ENonlinearCG &type) {
    return type = static_cast<ENonlinearCG>(type+1);
  }

  inline ENonlinearCG operator++(ENonlinearCG &type, int) {
    ENonlinearCG oldval = type;
    ++type;
    return oldval;
  }

  inline ENonlinearCG & operator--(ENonlinearCG &type) {
    return type = static_cast<ENonlinearCG>(type-1);
  }

  inline ENonlinearCG operator--(ENonlinearCG &type, int) {
    ENonlinearCG oldval = type;
    --type;
    return oldval;
  }

  inline ENonlinearCG StringToENonlinearCG(std::string s) {
    s = removeStringFormat(s);
    for ( ENonlinearCG nlcg = NONLINEARCG_HESTENES_STIEFEL; nlcg < NONLINEARCG_LAST; nlcg++ ) {
      if ( !s.compare(removeStringFormat(ENonlinearCGToString(nlcg))) ) {
        return nlcg;
      }
    }
    return NONLINEARCG_HESTENES_STIEFEL;
  }
  
  /** \enum   ROL::ELineSearch
      \brief  Enumeration of line-search types.

      \arg    BACKTRACKING    describe
      \arg    BISECTION       describe
      \arg    GOLDENSECTION   describe
      \arg    CUBICINTERP     describe
      \arg    BRENTS          describe
   */
  enum ELineSearch{
    LINESEARCH_BACKTRACKING = 0,
    LINESEARCH_BISECTION,
    LINESEARCH_GOLDENSECTION,
    LINESEARCH_CUBICINTERP,
    LINESEARCH_BRENTS,
    LINESEARCH_LAST
  };

  inline std::string ELineSearchToString(ELineSearch ls) {
    std::string retString;
    switch(ls) {
      case LINESEARCH_BACKTRACKING:   retString = "Backtracking";        break;
      case LINESEARCH_BISECTION:      retString = "Bisection";           break;
      case LINESEARCH_GOLDENSECTION:  retString = "Golden Section";      break;
      case LINESEARCH_CUBICINTERP:    retString = "Cubic Interpolation"; break;
      case LINESEARCH_BRENTS:         retString = "Brents";              break;
      case LINESEARCH_LAST:           retString = "Last Type (Dummy)";   break;
      default:                        retString = "INVALID ELineSearch";
    }
    return retString;
  }
  
  /** \brief  Verifies validity of a LineSearch enum.
    
      \param  ls  [in]  - enum of the linesearch
      \return 1 if the argument is a valid linesearch; 0 otherwise.
    */
  inline int isValidLineSearch(ELineSearch ls){
    return( (ls == LINESEARCH_BACKTRACKING)  ||
            (ls == LINESEARCH_BISECTION)     ||
            (ls == LINESEARCH_GOLDENSECTION) ||
            (ls == LINESEARCH_CUBICINTERP)   ||
            (ls == LINESEARCH_BRENTS)
          );
  }

  inline ELineSearch & operator++(ELineSearch &type) {
    return type = static_cast<ELineSearch>(type+1);
  }

  inline ELineSearch operator++(ELineSearch &type, int) {
    ELineSearch oldval = type;
    ++type;
    return oldval;
  }

  inline ELineSearch & operator--(ELineSearch &type) {
    return type = static_cast<ELineSearch>(type-1);
  }

  inline ELineSearch operator--(ELineSearch &type, int) {
    ELineSearch oldval = type;
    --type;
    return oldval;
  }

  inline ELineSearch StringToELineSearch(std::string s) {
    s = removeStringFormat(s);
    for ( ELineSearch ls = LINESEARCH_BACKTRACKING; ls < LINESEARCH_LAST; ls++ ) {
      if ( !s.compare(removeStringFormat(ELineSearchToString(ls))) ) {
        return ls;
      }
    }
    return LINESEARCH_BACKTRACKING;
  }

  /** \enum   ROL::ECurvatureCondition
      \brief  Enumeration of line-search curvature conditions.

      \arg    WOLFE           describe
      \arg    STRONGWOLFE     describe
      \arg    GOLDSTEIN       describe
   */
  enum ECurvatureCondition{
    CURVATURECONDITION_WOLFE = 0,
    CURVATURECONDITION_STRONGWOLFE,
    CURVATURECONDITION_GENERALIZEDWOLFE,
    CURVATURECONDITION_APPROXIMATEWOLFE,
    CURVATURECONDITION_GOLDSTEIN,
    CURVATURECONDITION_NULL,
    CURVATURECONDITION_LAST
  };

  inline std::string ECurvatureConditionToString(ECurvatureCondition ls) {
    std::string retString;
    switch(ls) {
      case CURVATURECONDITION_WOLFE:            retString = "Wolfe Conditions";             break;
      case CURVATURECONDITION_STRONGWOLFE:      retString = "Strong Wolfe Conditions";      break;
      case CURVATURECONDITION_GENERALIZEDWOLFE: retString = "Generalized Wolfe Conditions"; break;
      case CURVATURECONDITION_APPROXIMATEWOLFE: retString = "Approximate Wolfe Conditions"; break;
      case CURVATURECONDITION_GOLDSTEIN:        retString = "Goldstein Conditions";         break;
      case CURVATURECONDITION_NULL:             retString = "Null Curvature Condition";     break;
      case CURVATURECONDITION_LAST:             retString = "Last Type (Dummy)";            break;
      default:                                  retString = "INVALID ECurvatureCondition";
    }
    return retString;
  }
  
  /** \brief  Verifies validity of a CurvatureCondition enum.
    
      \param  ls  [in]  - enum of the Curvature Conditions
      \return 1 if the argument is a valid curvature condition; 0 otherwise.
    */
  inline int isValidCurvatureCondition(ECurvatureCondition ls){
    return( (ls == CURVATURECONDITION_WOLFE)            ||
            (ls == CURVATURECONDITION_STRONGWOLFE)      ||
            (ls == CURVATURECONDITION_GENERALIZEDWOLFE) ||
            (ls == CURVATURECONDITION_APPROXIMATEWOLFE) ||
            (ls == CURVATURECONDITION_GOLDSTEIN)        ||
            (ls == CURVATURECONDITION_NULL)
          );
  }

  inline ECurvatureCondition & operator++(ECurvatureCondition &type) {
    return type = static_cast<ECurvatureCondition>(type+1);
  }

  inline ECurvatureCondition operator++(ECurvatureCondition &type, int) {
    ECurvatureCondition oldval = type;
    ++type;
    return oldval;
  }

  inline ECurvatureCondition & operator--(ECurvatureCondition &type) {
    return type = static_cast<ECurvatureCondition>(type-1);
  }

  inline ECurvatureCondition operator--(ECurvatureCondition &type, int) {
    ECurvatureCondition oldval = type;
    --type;
    return oldval;
  }

  inline ECurvatureCondition StringToECurvatureCondition(std::string s) {
    s = removeStringFormat(s);
    for ( ECurvatureCondition cc = CURVATURECONDITION_WOLFE; cc < CURVATURECONDITION_LAST; cc++ ) {
      if ( !s.compare(removeStringFormat(ECurvatureConditionToString(cc))) ) {
        return cc;
      }
    }
    return CURVATURECONDITION_WOLFE;
  }

  /** \enum   ROL::ETrustRegion
      \brief  Enumeration of trust-region solver types.

      \arg    CAUCHYPOINT     describe
      \arg    TRUNCATEDCG     describe
      \arg    DOGLEG          describe
      \arg    DOUBLEDOGLEG    describe
   */
  enum ETrustRegion{
    TRUSTREGION_CAUCHYPOINT = 0,
    TRUSTREGION_TRUNCATEDCG,
    TRUSTREGION_DOGLEG,
    TRUSTREGION_DOUBLEDOGLEG,
    TRUSTREGION_LAST
  };

  inline std::string ETrustRegionToString(ETrustRegion tr) {
    std::string retString;
    switch(tr) {
      case TRUSTREGION_CAUCHYPOINT:   retString = "Cauchy Point";        break;
      case TRUSTREGION_TRUNCATEDCG:   retString = "Truncated CG";        break;
      case TRUSTREGION_DOGLEG:        retString = "Dogleg";              break;
      case TRUSTREGION_DOUBLEDOGLEG:  retString = "Double Dogleg";       break;
      case TRUSTREGION_LAST:          retString = "Last Type (Dummy)";   break;
      default:                        retString = "INVALID ETrustRegion";
    }
    return retString;
  }
  
  /** \brief  Verifies validity of a TrustRegion enum.
    
      \param  tr  [in]  - enum of the TrustRegion
      \return 1 if the argument is a valid TrustRegion; 0 otherwise.
    */
  inline int isValidTrustRegion(ETrustRegion ls){
    return( (ls == TRUSTREGION_CAUCHYPOINT)  ||
            (ls == TRUSTREGION_TRUNCATEDCG)  ||
            (ls == TRUSTREGION_DOGLEG)       ||
            (ls == TRUSTREGION_DOUBLEDOGLEG)
          );
  }

  inline ETrustRegion & operator++(ETrustRegion &type) {
    return type = static_cast<ETrustRegion>(type+1);
  }

  inline ETrustRegion operator++(ETrustRegion &type, int) {
    ETrustRegion oldval = type;
    ++type;
    return oldval;
  }

  inline ETrustRegion & operator--(ETrustRegion &type) {
    return type = static_cast<ETrustRegion>(type-1);
  }

  inline ETrustRegion operator--(ETrustRegion &type, int) {
    ETrustRegion oldval = type;
    --type;
    return oldval;
  }

  inline ETrustRegion StringToETrustRegion(std::string s) {
    s = removeStringFormat(s);
    for ( ETrustRegion tr = TRUSTREGION_CAUCHYPOINT; tr < TRUSTREGION_LAST; tr++ ) {
      if ( !s.compare(removeStringFormat(ETrustRegionToString(tr))) ) {
        return tr;
      }
    }
    return TRUSTREGION_CAUCHYPOINT;
  }

  /** \enum   ROL::ETestObjectives
      \brief  Enumeration of test objective functions.

      \arg    ROSENBROCK           describe
      \arg    FREUDENSTEINANDROTH  describe
      \arg    POWELL               describe
      \arg    SUMOFSQUARES         describe
      \arg    LEASTSQUARES         describe
   */
  enum ETestObjectives {
    TESTOBJECTIVES_ROSENBROCK = 0,
    TESTOBJECTIVES_FREUDENSTEINANDROTH,
    TESTOBJECTIVES_BEALE,
    TESTOBJECTIVES_POWELL,
    TESTOBJECTIVES_SUMOFSQUARES,
    TESTOBJECTIVES_LEASTSQUARES,
    TESTOBJECTIVES_POISSONCONTROL,
    TESTOBJECTIVES_POISSONINVERSION,
    TESTOBJECTIVES_LAST
  };

  inline std::string ETestObjectivesToString(ETestObjectives to) {
    std::string retString;
    switch(to) {
      case TESTOBJECTIVES_ROSENBROCK:          retString = "Rosenbrock's Function";            break;
      case TESTOBJECTIVES_FREUDENSTEINANDROTH: retString = "Freudenstein and Roth's Function"; break;
      case TESTOBJECTIVES_BEALE:               retString = "Beale's Function";                 break;
      case TESTOBJECTIVES_POWELL:              retString = "Powell's Badly Scaled Function";   break;
      case TESTOBJECTIVES_SUMOFSQUARES:        retString = "Sum of Squares Function";          break;
      case TESTOBJECTIVES_LEASTSQUARES:        retString = "Least Squares Function";           break;
      case TESTOBJECTIVES_POISSONCONTROL:      retString = "Poisson Optimal Control";          break;
      case TESTOBJECTIVES_POISSONINVERSION:    retString = "Poisson Inversion Problem";        break;
      case TESTOBJECTIVES_LAST:                retString = "Last Type (Dummy)";                break;
      default:                                 retString = "INVALID ETestObjectives";
    }
    return retString;
  }
  
  /** \brief  Verifies validity of a TestObjectives enum.
    
      \param  ls  [in]  - enum of the TestObjectives
      \return 1 if the argument is a valid TestObjectives; 0 otherwise.
    */
  inline int isValidTestObjectives(ETestObjectives to){
    return( (to == TESTOBJECTIVES_ROSENBROCK)          ||
            (to == TESTOBJECTIVES_FREUDENSTEINANDROTH) ||
            (to == TESTOBJECTIVES_BEALE)               ||
            (to == TESTOBJECTIVES_POWELL)              ||
            (to == TESTOBJECTIVES_SUMOFSQUARES)        ||
            (to == TESTOBJECTIVES_LEASTSQUARES)        ||
            (to == TESTOBJECTIVES_POISSONCONTROL)      ||
            (to == TESTOBJECTIVES_POISSONINVERSION)
          );
  }

  inline ETestObjectives & operator++(ETestObjectives &type) {
    return type = static_cast<ETestObjectives>(type+1);
  }

  inline ETestObjectives operator++(ETestObjectives &type, int) {
    ETestObjectives oldval = type;
    ++type;
    return oldval;
  }

  inline ETestObjectives & operator--(ETestObjectives &type) {
    return type = static_cast<ETestObjectives>(type-1);
  }

  inline ETestObjectives operator--(ETestObjectives &type, int) {
    ETestObjectives oldval = type;
    --type;
    return oldval;
  }

  inline ETestObjectives StringToETestObjectives(std::string s) {
    s = removeStringFormat(s);
    for ( ETestObjectives to = TESTOBJECTIVES_ROSENBROCK; to < TESTOBJECTIVES_LAST; to++ ) {
      if ( !s.compare(removeStringFormat(ETestObjectivesToString(to))) ) {
        return to;
      }
    }
    return TESTOBJECTIVES_ROSENBROCK;
  }

} // namespace ROL


/*! \mainpage ROL Documentation (Development Version)
 
  \image html rol.png "Rapid Optimization Library" width=1in
  \image latex rol.pdf "Rapid Optimization Library" width=1in

  \section intro_sec Introduction

  %ROL, the Rapid Optimization Library, is a Trilinos package for matrix-free
  optimization.
 
  \section overview_sec Overview

  Current release of %ROL includes the following features:
  \li Unconstrained optimization algorithms.

  \section quickstart_sec Quick Start

  The Rosenbrock example (rol/example/rosenbrock/example_01.cpp) demonstrates the use of %ROL.
  It amounts to sixsteps:

  \subsection vector_qs_sec Step 1: Implement linear algebra / vector interface.
  --- or try one of the provided implementations, such as ROL::StdVector in rol/vector.
  
  \code
      ROL::Vector
  \endcode

  \subsection objective_qs_sec Step 2: Implement objective function interface.
  --- or try one of the provided functions, such as ROL::Objective_Rosenbrock in rol/zoo.

  \code
      ROL::Objective
  \endcode

  \subsection step_qs_sec Step 3: Choose optimization step.
  ---  with ParameterList settings in the variable parlist.

  \code
      ROL::LineSearchStep<RealT> step(parlist);
  \endcode

  \subsection status_qs_sec Step 4: Set status test.
  ---  with gradient tolerance \textt{gtol}, step tolerance \texttt{stol} and the maximum
  number of iterations \texttt{maxit}.

  \code
      ROL::StatusTest<RealT> status(gtol, stol, maxit);
  \endcode

  \subsection algo_qs_sec Step 5: Define an algorithm.
  ---  based on the status test and the step.

  \code
      ROL::DefaultAlgorithm<RealT> algo(step,status);
  \endcode

  \subsection run_qs_sec Step 6: Run algorithm.
  ---  starting from the initial iterate \textt{x}, applied to objective function \texttt{obj}.

  \code
      algo.run(x, obj);
  \endcode

  \subsection done_qs_sec Done!

  \section devplans_sec Development Plans

  Constrained optimization, optimization under uncertainty, etc.
*/
  

#endif
