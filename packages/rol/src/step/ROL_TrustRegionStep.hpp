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

#ifndef ROL_TRUSTREGIONSTEP_H
#define ROL_TRUSTREGIONSTEP_H

#include "ROL_Types.hpp"
#include "ROL_Step.hpp"
#include "ROL_Secant.hpp"
#include "ROL_TrustRegion.hpp"
#include <sstream>
#include <iomanip>

/** \class ROL::TrustRegionStep
    \brief Provides the interface to compute optimization steps
           with trust regions.
*/


namespace ROL {

template <class Real>
class TrustRegionStep : public Step<Real> {
private:

  Teuchos::RCP<Secant<Real> >       secant_;
  Teuchos::RCP<TrustRegion<Real> >  trustRegion_;

  ETrustRegion      etr_;        // Trust-Region Subproblem Solver Type
  ESecant           esec_;       // Secant Type

  bool useSecantHessVec_;
  bool useSecantPrecond_;

  Real              del_;        // Trust-Region Radius
  std::vector<bool> useInexact_; // Inexactness Information
  int               TRflag_  ;   // Trust-Region Exit Flag
  int               TR_nfval_;   // Trust-Region Function Evaluation Number
  int               TR_ngrad_;   // Trust-Region Gradient Evaluation Number
  int               CGflag_;     // CG Termination Flag
  int               CGiter_;     // CG Iteration Count

public:

  virtual ~TrustRegionStep() {}

  TrustRegionStep( Teuchos::ParameterList & parlist ) {
    // Enumerations
    etr_   = StringToETrustRegion( parlist.get("Trust-Region Subproblem Solver Type",  "Cauchy Point"));  
    esec_  = StringToESecant(      parlist.get("Secant Type",                          "Limited-Memory BFGS"));
    // Secant Information
    useSecantPrecond_ = parlist.get("Use Secant Preconditioning", false);
    useSecantHessVec_ = parlist.get("Use Secant Hessian-Times-A-Vector", false);
    // Trust-Region Parameters
    del_   = parlist.get("Initial Trust-Region Radius",          -1.0);
    // Inexactness Information
    useInexact_.clear();
    useInexact_.push_back(parlist.get("Use Inexact Objective Function", false));
    useInexact_.push_back(parlist.get("Use Inexact Gradient", false));
    useInexact_.push_back(parlist.get("Use Inexact Hessian-Times-A-Vector", false));
     
    // Initialize Trust Region Subproblem Solver Object
    trustRegion_ = Teuchos::rcp( new TrustRegion<Real>(parlist) );

    // Secant Parameters
    secant_ = Teuchos::null;
    if ( useSecantPrecond_ || useSecantHessVec_ ) {
      int L      = parlist.get("Maximum Secant Storage",10);
      int BBtype = parlist.get("Barzilai-Borwein Type",1);
      secant_ = getSecant<Real>(esec_,L,BBtype);
    }
  }

  TrustRegionStep( Teuchos::RCP<Secant<Real> > &secant, Teuchos::ParameterList &parlist ) 
    : secant_(secant) {
    // Enumerations
    etr_   = StringToETrustRegion( parlist.get("Trust-Region Subproblem Solver Type",  "Cauchy Point"));  
    esec_  = StringToESecant(      parlist.get("Secant Type",                          "Limited-Memory BFGS"));
    // Secant Information
    useSecantPrecond_ = parlist.get("Use Secant Preconditioning", false);
    useSecantHessVec_ = parlist.get("Use Secant Hessian-Times-A-Vector", false);
    // Trust-Region Parameters
    del_   = parlist.get("Initial Trust-Region Radius",          -1.0);
    // Inexactness Information
    useInexact_.clear();
    useInexact_.push_back(parlist.get("Use Inexact Objective Function", false));
    useInexact_.push_back(parlist.get("Use Inexact Gradient", false));
    useInexact_.push_back(parlist.get("Use Inexact Hessian-Times-A-Vector", false));
     
    // Initialize Trust Region Subproblem Solver Object
    trustRegion_ = Teuchos::rcp( new TrustRegion<Real>(parlist) );
  }

  /** \brief Initialize step.
  */
  void initialize( Vector<Real> &x, Objective<Real> &obj, Constraints<Real> &con, 
                   AlgorithmState<Real> &algo_state ) {
    algo_state.nfval = 0;
    algo_state.ngrad = 0;

    Real ftol = std::sqrt(ROL_EPSILON);
    Real htol = std::sqrt(ROL_EPSILON);

    Teuchos::RCP<StepState<Real> >& state = Step<Real>::get_state();

    state->descentVec  = x.clone();
    state->gradientVec = x.clone();

    obj.update(x,true,algo_state.iter);    
    if ( this->useInexact_[1] ) {
      Real gtol = 2.0*this->del_;
      algo_state.gnorm = this->del_;
      while ( gtol > std::min(algo_state.gnorm,this->del_) ) {
        gtol = std::min(algo_state.gnorm,this->del_);
        obj.gradient(*(state->gradientVec),x,gtol);
        algo_state.ngrad++;
        algo_state.gnorm = (state->gradientVec)->norm();
      }  
    }
    else {
      Real gtol = std::sqrt(ROL_EPSILON);
      obj.gradient(*(state->gradientVec),x,gtol);
      algo_state.ngrad++;
      algo_state.gnorm = (state->gradientVec)->norm();
    }
    algo_state.snorm = 1.e10;
    algo_state.value = obj.value(x,ftol);
    algo_state.nfval++;

    // Evaluate Objective Function at Cauchy Point
    if ( this->del_ <= 0.0 ) {
      Teuchos::RCP<Vector<Real> > Bg = x.clone();
      if ( this->useSecantHessVec_ ) {
        this->secant_->applyB(*Bg,*(state->gradientVec),x);
      }
      else {
        obj.hessVec(*Bg,*(state->gradientVec),x,htol);
      }
      Real gBg   = Bg->dot(*(state->gradientVec));
      Real alpha = 1.0;
      if ( gBg > ROL_EPSILON ) {
        alpha = algo_state.gnorm*algo_state.gnorm/gBg;
      }
      Teuchos::RCP<Vector<Real> > cp = x.clone();
      cp->set(*(state->gradientVec)); 
      cp->scale(-alpha);
      obj.update(*cp);
      Real fnew = obj.value(*cp,ftol);
      algo_state.nfval++;
      // Perform Quadratic Interpolation to Determine Initial Trust Region Radius
      Real gs    = (state->gradientVec)->dot(*cp);
      this->del_ = -gs/(fnew - algo_state.value - gs)*alpha*algo_state.gnorm;
    }
  }

  /** \brief Compute step.
  */
  void compute( Vector<Real> &s, const Vector<Real> &x, Objective<Real> &obj, Constraints<Real> &con, 
                AlgorithmState<Real> &algo_state ) {
    this->CGflag_ = 0;
    this->CGiter_ = 0;
    this->trustRegion_->run(s,algo_state.snorm,this->del_,this->CGflag_,this->CGiter_,
                            x,*(Step<Real>::state_->gradientVec),algo_state.gnorm,obj,this->secant_);
  }

  /** \brief Update step, if successful.
  */
  void update( Vector<Real> &x, const Vector<Real> &s, Objective<Real> &obj, Constraints<Real> &con, 
               AlgorithmState<Real> &algo_state ) {
    Real tol = std::sqrt(ROL_EPSILON);

    this->TRflag_   = 0;
    this->TR_nfval_ = 0;
    this->TR_ngrad_ = 0;
    Real fold = algo_state.value;
    Real fnew = 0.0;
    this->trustRegion_->update(x,fnew,this->del_,this->TR_nfval_,this->TR_ngrad_,this->TRflag_,
                              s,algo_state.snorm,fold,*(Step<Real>::state_->gradientVec),obj,this->secant_);
    algo_state.value = fnew;
    algo_state.nfval += this->TR_nfval_;
    algo_state.ngrad += this->TR_ngrad_;
    algo_state.iter++;


    // Compute new gradient
    Teuchos::RCP<Vector<Real> > gp;
    if ( this->TRflag_ == 0 || this->TRflag_ == 1 ) {  
      obj.update(x,true,algo_state.iter);
      if ( this->secant_ != Teuchos::null ) {
        gp = x.clone();
        gp->set(*(Step<Real>::state_->gradientVec));
      }
      obj.gradient(*(Step<Real>::state_->gradientVec),x,tol);
      algo_state.ngrad++;
      if ( this->secant_ != Teuchos::null ) {
        secant_->update(*(Step<Real>::state_->gradientVec),*gp,s,algo_state.snorm,algo_state.iter+1);
      }
    }    
  
    // Update algorithm state
    (algo_state.iterateVec)->set(x);
    if ( con.isActivated() ) {
      Teuchos::RCP<Vector<Real> > xnew = x.clone();
      xnew->set(x);
      xnew->axpy(-1.0,*(Step<Real>::state_->gradientVec));
      con.project(*xnew);
      xnew->axpy(-1.0,x);
      algo_state.gnorm = xnew->norm();
    }
    else {
      algo_state.gnorm = (Step<Real>::state_->gradientVec)->norm();
    }
  }

  /** \brief Print iterate header.
  */
  std::string printHeader( void ) const  {
    std::stringstream hist;
    hist << "  ";
    hist << std::setw(6)  << std::left << "iter";
    hist << std::setw(15) << std::left << "value";
    hist << std::setw(15) << std::left << "gnorm";
    hist << std::setw(15) << std::left << "snorm";
    hist << std::setw(15) << std::left << "delta";
    hist << std::setw(10) << std::left << "#fval";
    hist << std::setw(10) << std::left << "#grad";
    hist << std::setw(10) << std::left << "tr_flag";
    if ( this->etr_ == TRUSTREGION_TRUNCATEDCG ) {
      hist << std::setw(10) << std::left << "iterCG";
      hist << std::setw(10) << std::left << "flagCG";
    }
    hist << "\n";
    return hist.str();
  }

  std::string printName( void ) const {
    std::stringstream hist;
    hist << "\n" << ETrustRegionToString(this->etr_) << " Trust-Region solver";
    if ( this->useSecantPrecond_ || this->useSecantHessVec_ ) {
      if ( this->useSecantPrecond_ && !this->useSecantHessVec_ ) {
        hist << " with " << ESecantToString(this->esec_) << " preconditioning\n";
      }
      else if ( !this->useSecantPrecond_ && this->useSecantHessVec_ ) {
        hist << " with " << ESecantToString(this->esec_) << " Hessian approximation\n";
      }
      else {
        hist << " with " << ESecantToString(this->esec_) << " preconditioning and Hessian approximation\n";
      }
    }
    else {
      hist << "\n";
    }
    return hist.str();
  }

  /** \brief Print iterate status.
  */
  std::string print( AlgorithmState<Real> & algo_state, bool printHeader = false ) const  {
    std::stringstream hist;
    hist << std::scientific << std::setprecision(6);
    if ( algo_state.iter == 0 ) {
      hist << this->printName();
    }
    if ( printHeader ) {
      hist << this->printHeader();
    }
    if ( algo_state.iter == 0 ) {
      hist << "  ";
      hist << std::setw(6)  << std::left << algo_state.iter;
      hist << std::setw(15) << std::left << algo_state.value;
      hist << std::setw(15) << std::left << algo_state.gnorm;
      hist << std::setw(15) << std::left << " "; 
      hist << std::setw(15) << std::left << this->del_; 
      hist << "\n";
    }
    else {
      hist << "  "; 
      hist << std::setw(6)  << std::left << algo_state.iter;  
      hist << std::setw(15) << std::left << algo_state.value; 
      hist << std::setw(15) << std::left << algo_state.gnorm; 
      hist << std::setw(15) << std::left << algo_state.snorm; 
      hist << std::setw(15) << std::left << this->del_; 
      hist << std::setw(10) << std::left << algo_state.nfval;              
      hist << std::setw(10) << std::left << algo_state.ngrad;              
      hist << std::setw(10) << std::left << this->TRflag_;              
      if ( this->etr_ == TRUSTREGION_TRUNCATEDCG ) {
        hist << std::setw(10) << std::left << this->CGiter_;
        hist << std::setw(10) << std::left << this->CGflag_;
      }
      hist << "\n";
    }
    return hist.str();
  }

}; // class Step

} // namespace ROL

#endif
