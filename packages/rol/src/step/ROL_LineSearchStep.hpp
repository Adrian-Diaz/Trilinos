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

#ifndef ROL_LINESEARCHSTEP_H
#define ROL_LINESEARCHSTEP_H

#include "ROL_Types.hpp"
#include "ROL_Step.hpp"
#include "ROL_Secant.hpp"
#include "ROL_Krylov.hpp"
#include "ROL_NonlinearCG.hpp"
#include "ROL_LineSearch.hpp"
#include <sstream>
#include <iomanip>

/** \class ROL::LineSearchStep
    \brief Provides the interface to compute optimization steps
           with line search.
*/


namespace ROL {

template <class Real>
class LineSearchStep : public Step<Real> {
private:

  Teuchos::RCP<Secant<Real> >      secant_;
  Teuchos::RCP<Krylov<Real> >      krylov_;
  Teuchos::RCP<NonlinearCG<Real> > nlcg_;
  Teuchos::RCP<LineSearch<Real> >  lineSearch_;

  int iterKrylov_;
  int flagKrylov_;

  ELineSearch         els_;
  ENonlinearCG        enlcg_; 
  ECurvatureCondition econd_;
  EDescent            edesc_;
  ESecant             esec_;
  EKrylov             ekv_;
 
  int ls_nfval_;
  int ls_ngrad_;

  bool useSecantPrecond_;

  std::vector<bool> useInexact_;

public:

  virtual ~LineSearchStep() {}

  LineSearchStep( Teuchos::ParameterList &parlist ) {
    // Enumerations
    edesc_ = StringToEDescent(            parlist.get("Descent Type",                   "Quasi-Newton Method"));
    enlcg_ = StringToENonlinearCG(        parlist.get("Nonlinear CG Type",              "Hagar-Zhang"));
    els_   = StringToELineSearch(         parlist.get("Linesearch Type",                "Cubic Interpolation"));
    econd_ = StringToECurvatureCondition( parlist.get("Linesearch Curvature Condition", "Strong Wolfe Conditions"));
    esec_  = StringToESecant(             parlist.get("Secant Type",                    "Limited-Memory BFGS"));
    ekv_   = StringToEKrylov(             parlist.get("Krylov Type",                    "Conjugate Gradients"));

    // Inexactness Information
    useInexact_.clear();
    useInexact_.push_back(parlist.get("Use Inexact Objective Function", false));
    useInexact_.push_back(parlist.get("Use Inexact Gradient", false));
    useInexact_.push_back(parlist.get("Use Inexact Hessian-Times-A-Vector", false));
     
    // Initialize Linesearch Object
    lineSearch_ = Teuchos::rcp( new LineSearch<Real>(parlist) );

    // Initialize Krylov Object
    useSecantPrecond_ = parlist.get("Use Secant Preconditioner", false);
    krylov_ = Teuchos::null;
    if ( edesc_ == DESCENT_NEWTONKRYLOV ) {
      Real CGtol1 = parlist.get("Absolute Krylov Tolerance", 1.e-4);
      Real CGtol2 = parlist.get("Relative Krylov Tolerance", 1.e-2);
      int maxitCG = parlist.get("Maximum Number of Krylov Iterations", 20);
      krylov_ = Teuchos::rcp( new Krylov<Real>(ekv_,CGtol1,CGtol2,maxitCG,useInexact_[2],useSecantPrecond_) );
      iterKrylov_ = 0;
      flagKrylov_ = 0;
    }

    // Initialize Secant Object
    secant_ = Teuchos::null;
    if ( edesc_ == DESCENT_SECANT || (edesc_ == DESCENT_NEWTONKRYLOV && useSecantPrecond_) ) {
      int L      = parlist.get("Maximum Secant Storage",10);
      int BBtype = parlist.get("Barzilai-Borwein Type",1);
      secant_ = getSecant<Real>(esec_,L,BBtype);
    }

    // Initialize Nonlinear CG Object
    nlcg_ = Teuchos::null;
    if ( edesc_ == DESCENT_NONLINEARCG ) {
      nlcg_ = Teuchos::rcp( new NonlinearCG<Real>(enlcg_) );
    }
  }

  LineSearchStep( Teuchos::RCP<Secant<Real> > &secant, Teuchos::ParameterList &parlist ) :
    secant_(secant) {
    // Enumerations
    edesc_ = StringToEDescent(            parlist.get("Descent Type",                   "Quasi-Newton Method"));
    enlcg_ = StringToENonlinearCG(        parlist.get("Nonlinear CG Type",              "Hagar-Zhang"));
    els_   = StringToELineSearch(         parlist.get("Linesearch Type",                "Cubic Interpolation"));
    econd_ = StringToECurvatureCondition( parlist.get("Linesearch Curvature Condition", "Strong Wolfe Conditions"));
    esec_  = StringToESecant(             parlist.get("Secant Type",                    "Limited-Memory BFGS"));
    ekv_   = StringToEKrylov(             parlist.get("Krylov Type",                    "Conjugate Gradients"));

    // Inexactness Information
    useInexact_.clear();
    useInexact_.push_back(parlist.get("Use Inexact Objective Function", false));
    useInexact_.push_back(parlist.get("Use Inexact Gradient", false));
    useInexact_.push_back(parlist.get("Use Inexact Hessian-Times-A-Vector", false));
     
    // Initialize Linesearch Object
    lineSearch_ = Teuchos::rcp( new LineSearch<Real>(parlist) );

    // Initialize Krylov Object
    useSecantPrecond_ = parlist.get("Use Secant Preconditioner", false);
    krylov_ = Teuchos::null;
    if ( edesc_ == DESCENT_NEWTONKRYLOV ) {
      Real CGtol1 = parlist.get("Absolute Krylov Tolerance", 1.e-4);
      Real CGtol2 = parlist.get("Relative Krylov Tolerance", 1.e-2);
      int maxitCG = parlist.get("Maximum Number of Krylov Iterations", 20);
      krylov_ = Teuchos::rcp( new Krylov<Real>(ekv_,CGtol1,CGtol2,maxitCG,useInexact_[2],useSecantPrecond_) );
      iterKrylov_ = 0;
      flagKrylov_ = 0;
    }

    // Initialize Nonlinear CG Object
    nlcg_ = Teuchos::null;
    if ( edesc_ == DESCENT_NONLINEARCG ) {
      nlcg_ = Teuchos::rcp( new NonlinearCG<Real>(enlcg_) );
    }
  }

  /** \brief Compute step.
  */
  void compute( Vector<Real> &s, const Vector<Real> &x, Objective<Real> &obj, Constraints<Real> &con, 
                AlgorithmState<Real> &algo_state ) {
    // Compute step s
    if ( this->edesc_ == DESCENT_NEWTONKRYLOV ) {
      this->flagKrylov_ = 0;
      this->krylov_->run(s,this->iterKrylov_,this->flagKrylov_,
                         *(Step<Real>::state_->gradientVec),x,obj,con,this->secant_);
    }
    else if ( this->edesc_ == DESCENT_NEWTON ) {
      Real tol = std::sqrt(ROL_EPSILON);
      if ( con.isActivated() ) {
        Teuchos::RCP<Vector<Real> > gnew = x.clone();
        gnew->set(*(Step<Real>::state_->gradientVec));
        con.pruneActive(*gnew,*(Step<Real>::state_->gradientVec),x);
        obj.invHessVec(s,*gnew,x,tol);
        con.pruneActive(s,*(Step<Real>::state_->gradientVec),x);
        gnew->set(*(Step<Real>::state_->gradientVec));
        con.pruneInactive(*gnew,*(Step<Real>::state_->gradientVec),x);
        s.plus(*gnew);
      }
      else {
        obj.invHessVec(s,*(Step<Real>::state_->gradientVec),x,tol);
      }
    }
    else if ( this->edesc_ == DESCENT_SECANT ) {
      if ( con.isActivated() ) {
        Teuchos::RCP<Vector<Real> > gnew = x.clone();
        gnew->set(*(Step<Real>::state_->gradientVec));
        con.pruneActive(*gnew,*(Step<Real>::state_->gradientVec),x);
        this->secant_->applyH(s,*gnew,x);
        con.pruneActive(s,*(Step<Real>::state_->gradientVec),x);
        gnew->set(*(Step<Real>::state_->gradientVec));
        con.pruneInactive(*gnew,*(Step<Real>::state_->gradientVec),x);
        s.plus(*gnew);
      }
      else {
        this->secant_->applyH(s,*(Step<Real>::state_->gradientVec),x);
      }
    }
    else if ( this->edesc_ == DESCENT_NONLINEARCG ) {
      this->nlcg_->run(s,*(Step<Real>::state_->gradientVec),x,obj);
    }

    // Check if s is a descent direction
    Real gs = 0.0;
    if ( con.isActivated() ) {
      Teuchos::RCP<Vector<Real> > d = x.clone();
      d->set(s);
      con.pruneActive(*d,s,x);
      gs = -(Step<Real>::state_->gradientVec)->dot(*d);
    }
    else {
      gs = -(Step<Real>::state_->gradientVec)->dot(s);
    }
    if ( gs > 0.0 || this->flagKrylov_ == 2 || this->edesc_ == DESCENT_STEEPEST ) {
      s.set(*(Step<Real>::state_->gradientVec));
      if ( con.isActivated() ) {
        Teuchos::RCP<Vector<Real> > d = x.clone();
        d->set(s);
        con.pruneActive(*d,s,x);
        gs = -(Step<Real>::state_->gradientVec)->dot(*d);
      }
      else {
        gs = -(Step<Real>::state_->gradientVec)->dot(s);
      }
    }
    s.scale(-1.0);

    // Perform line search
    Real alpha = 0.0;
    Real fnew  = algo_state.value;
    this->ls_nfval_ = 0;
    this->ls_ngrad_ = 0;
    this->lineSearch_->run(alpha,fnew,this->ls_nfval_,this->ls_ngrad_,gs,s,x,obj,con);
    algo_state.nfval += this->ls_nfval_;
    algo_state.ngrad += this->ls_ngrad_;

    // Compute get scaled descent direction
    s.scale(alpha);
    if ( con.isActivated() ) {
      s.plus(x);
      con.project(s);
      s.axpy(-1.0,x);
    }

    // Update step state information
    (Step<Real>::state_->descentVec)->set(s);

    // Update algorithm state information
    algo_state.snorm = s.norm();
    algo_state.value = fnew;
  }

  /** \brief Update step, if successful.
  */
  void update( Vector<Real> &x, const Vector<Real> &s, Objective<Real> &obj, Constraints<Real> &con,
               AlgorithmState<Real> &algo_state ) {
    Real tol = std::sqrt(ROL_EPSILON);

    // Update iterate
    algo_state.iter++;
    x.axpy(1.0, s);
    obj.update(x,true,algo_state.iter);

    // Compute new gradient
    Teuchos::RCP<Vector<Real> > gp;
    if ( this->edesc_ == DESCENT_SECANT || (this->edesc_ == DESCENT_NEWTONKRYLOV && this->useSecantPrecond_) ) {
      gp = x.clone();
      gp->set(*(Step<Real>::state_->gradientVec));
    }
    obj.gradient(*(Step<Real>::state_->gradientVec),x,tol);
    algo_state.ngrad++;

    // Update Secant Information
    if ( this->edesc_ == DESCENT_SECANT || (this->edesc_ == DESCENT_NEWTONKRYLOV && this->useSecantPrecond_) ) {
      secant_->update(*(Step<Real>::state_->gradientVec),*gp,s,algo_state.snorm,algo_state.iter+1);
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
    hist << std::setw(6) << std::left << "iter";  
    hist << std::setw(15) << std::left << "value";
    hist << std::setw(15) << std::left << "gnorm"; 
    hist << std::setw(15) << std::left << "snorm";
    hist << std::setw(10) << std::left << "#fval";
    hist << std::setw(10) << std::left << "#grad";
    hist << std::setw(10) << std::left << "ls_#fval";
    hist << std::setw(10) << std::left << "ls_#grad";
    if (    this->edesc_ == DESCENT_NEWTONKRYLOV ) {
      hist << std::setw(10) << std::left << "iterCG";
      hist << std::setw(10) << std::left << "flagCG";
    }
    hist << "\n";
    return hist.str();
  }

  std::string printName( void ) const {
    std::stringstream hist;
    hist << "\n" << EDescentToString(this->edesc_) 
         << " with " << ELineSearchToString(this->els_) 
         << " Linesearch satisfying " 
         << ECurvatureConditionToString(this->econd_) << "\n";
    if ( this->edesc_ == DESCENT_NEWTONKRYLOV ) {
      hist << "Krylov Type: " << EKrylovToString(this->ekv_) << "\n";
    }
    if ( this->edesc_ == DESCENT_SECANT || (this->edesc_ == DESCENT_NEWTONKRYLOV && this->useSecantPrecond_) ) {
      hist << "Secant Type: " << ESecantToString(this->esec_) << "\n";
    }
    if ( this->edesc_ == DESCENT_NONLINEARCG ) {
      hist << "Nonlinear CG Type: " << ENonlinearCGToString(this->enlcg_) << "\n";
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
      hist << std::setw(6) << std::left << algo_state.iter;
      hist << std::setw(15) << std::left << algo_state.value;
      hist << std::setw(15) << std::left << algo_state.gnorm;
      hist << "\n";
    }
    else {
      hist << "  "; 
      hist << std::setw(6) << std::left << algo_state.iter;  
      hist << std::setw(15) << std::left << algo_state.value; 
      hist << std::setw(15) << std::left << algo_state.gnorm; 
      hist << std::setw(15) << std::left << algo_state.snorm; 
      hist << std::setw(10) << std::left << algo_state.nfval;              
      hist << std::setw(10) << std::left << algo_state.ngrad;              
      hist << std::setw(10) << std::left << this->ls_nfval_;              
      hist << std::setw(10) << std::left << this->ls_ngrad_;              
      if ( this->edesc_ == DESCENT_NEWTONKRYLOV ) {
        hist << std::setw(10) << std::left << this->iterKrylov_;
        hist << std::setw(10) << std::left << this->flagKrylov_;
      }
      hist << "\n";
    }
    return hist.str();
  }

  // struct StepState (scalars, vectors) map?

  // getState

  // setState

}; // class Step

} // namespace ROL

#endif
