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

#ifndef ROL_LINMOREALGORITHM_B_DEF_H
#define ROL_LINMOREALGORITHM_B_DEF_H

namespace ROL {

template<typename Real>
LinMoreAlgorithm_B<Real>::LinMoreAlgorithm_B(ParameterList &list,
                                             const Ptr<Secant<Real>> &secant) {
  // Set status test
  status_->reset();
  status_->add(makePtr<StatusTest<Real>>(list));

  ParameterList &trlist = list.sublist("Step").sublist("Trust Region");
  // Trust-Region Parameters
  state_->searchSize = trlist.get("Initial Radius",            -1.0);
  delMax_ = trlist.get("Maximum Radius",                       1.e8);
  eta0_   = trlist.get("Step Acceptance Threshold",            0.05);
  eta1_   = trlist.get("Radius Shrinking Threshold",           0.05);
  eta2_   = trlist.get("Radius Growing Threshold",             0.9);
  gamma0_ = trlist.get("Radius Shrinking Rate (Negative rho)", 0.0625);
  gamma1_ = trlist.get("Radius Shrinking Rate (Positive rho)", 0.25);
  gamma2_ = trlist.get("Radius Growing Rate",                  2.5);
  TRsafe_ = trlist.get("Safeguard Size",                       100.0);
  eps_    = TRsafe_*ROL_EPSILON<Real>();
  // Krylov Parameters
  maxit_ = list.sublist("General").sublist("Krylov").get("Iteration Limit",    20);
  tol1_  = list.sublist("General").sublist("Krylov").get("Absolute Tolerance", 1e-4);
  tol2_  = list.sublist("General").sublist("Krylov").get("Relative Tolerance", 1e-2);
  // Algorithm-Specific Parameters
  minit_     = trlist.sublist("Lin-More").get("Maximum Number of Minor Iterations",    10);
  extlim_    = trlist.sublist("Lin-More").get("Maximum Number of Extrapolation Steps", 10);
  interpf_   = trlist.sublist("Lin-More").get("Cauchy Point Backtracking Rate",        0.1);
  extrapf_   = trlist.sublist("Lin-More").get("Cauchy Point Extrapolation Rate",       10.0);
  mu0_       = trlist.sublist("Lin-More").get("Sufficient Decrease Parameter",         1e-2);
  interpfPS_ = trlist.sublist("Lin-More").get("Projected Search Backtracking Rate",    0.5);
  // Output Parameters
  verbosity_   = list.sublist("General").get("Output Level",0);
  printHeader_ = verbosity_ > 2;
  // Secant Information
  useSecantPrecond_ = list.sublist("General").sublist("Secant").get("Use as Preconditioner", false);
  useSecantHessVec_ = list.sublist("General").sublist("Secant").get("Use as Hessian",        false);
  // Initialize trust region model
  model_ = makePtr<TrustRegionModel_U<Real>>(list,secant);
  useSecantPrecond_ = list.sublist("General").sublist("Secant").get("Use as Preconditioner", false);
  useSecantHessVec_ = list.sublist("General").sublist("Secant").get("Use as Hessian",        false);
  if (secant == nullPtr) {
    esec_ = StringToESecant(list.sublist("General").sublist("Secant").get("Type","Limited-Memory BFGS"));
  }
}

template<typename Real>
void LinMoreAlgorithm_B<Real>::initialize(Vector<Real>          &x,
                                          const Vector<Real>    &g,
                                          Objective<Real>       &obj,
                                          BoundConstraint<Real> &bnd,
                                          std::ostream &outStream) {
  const Real one(1);
  hasEcon_ = true;
  if (proj_ == nullPtr) {
    proj_ = makePtr<PolyhedralProjection<Real>>(makePtrFromRef(x),makePtrFromRef(bnd));
    hasEcon_ = false;
  }
  // Initialize data
  Algorithm_B<Real>::initialize(x,g);
  // Update approximate gradient and approximate objective function.
  Real ftol = static_cast<Real>(0.1)*ROL_OVERFLOW<Real>(); 
  proj_->project(x);
  state_->iterateVec->set(x);
  obj.update(x,true,state_->iter);
  state_->value = obj.value(x,ftol); 
  state_->nfval++;
  obj.gradient(*state_->gradientVec,x,ftol);
  state_->ngrad++;
  state_->stepVec->set(x);
  state_->stepVec->axpy(-one,state_->gradientVec->dual());
  proj_->project(*state_->stepVec);
  state_->stepVec->axpy(-one,x);
  state_->gnorm = state_->stepVec->norm();
  state_->snorm = ROL_INF<Real>();
  // Compute initial trust region radius if desired.
  if ( state_->searchSize <= static_cast<Real>(0) ) {
    state_->searchSize = state_->gradientVec->norm();
  }
  // Initialize null space projection
  if (hasEcon_) {
    rcon_ = makePtr<ReducedLinearConstraint<Real>>(proj_->getLinearConstraint(),
                                                   makePtrFromRef(bnd),
                                                   makePtrFromRef(x));
    ns_   = makePtr<NullSpaceOperator<Real>>(rcon_,x,
                                             proj_->getMultiplier()->dual());
  }
}

template<typename Real>
std::vector<std::string> LinMoreAlgorithm_B<Real>::run(Vector<Real>          &x,
                                                       const Vector<Real>    &g, 
                                                       Objective<Real>       &obj,
                                                       BoundConstraint<Real> &bnd,
                                                       std::ostream          &outStream ) {
  const Real zero(0), half(0.5), one(1);
  Real tol0 = std::sqrt(ROL_EPSILON<Real>());
  Real gfnorm(0), gfnormf(0), tol(0), stol(0), gs(0);
  Real ftrial(0), sHs(0), pRed(0), rho(1), alpha(1);
  int flagCG(0), iterCG(0);
  // Initialize trust-region data
  std::vector<std::string> output;
  initialize(x,g,obj,bnd,outStream);
  Ptr<Vector<Real>> s = x.clone(), gvec = g.clone();
  Ptr<Vector<Real>> pwa1 = x.clone(), pwa2 = x.clone(), pwa3 = x.clone();
  Ptr<Vector<Real>> dwa1 = g.clone(), dwa2 = g.clone(), dwa3 = g.clone();

  // Output
  output.push_back(print(true));
  if (verbosity_ > 0) outStream << print(true);

  while (status_->check(*state_)) {
    // Build trust-region model
    model_->setData(obj,x,*state_->gradientVec);

    /**** SOLVE TRUST-REGION SUBPROBLEM ****/
    // Compute Cauchy point (TRON notation: state_->iterateVec = x[1])
    state_->snorm = dcauchy(*s,alpha,x,*state_->gradientVec,state_->searchSize,
                    *model_,*dwa1,outStream); // Solve 1D optimization problem for alpha
    state_->iterateVec->set(x);               // TRON notation: model.getIterate() = x[0]
    state_->iterateVec->plus(*s);             // Set state_->iterateVec = x[0] + alpha*g
    proj_->project(*state_->iterateVec);      // Project state_->iterateVec onto feasible set

    // Model gradient at s = x[1] - x[0]
    state_->stepVec->set(*state_->iterateVec);
    state_->stepVec->axpy(-one,x); // s = x[i+1]-x[0]
    model_->hessVec(*gvec,*state_->stepVec,x,tol0);
    gvec->plus(*state_->gradientVec);
    bnd.pruneActive(*gvec,*state_->iterateVec,zero);
    if (hasEcon_) {
      applyFreePrecond(*pwa1,*gvec,*state_->iterateVec,*model_,bnd,tol0,*dwa1);
      gfnorm = pwa1->norm();
    }
    else {
      gfnorm = gvec->norm();
    }
    SPiter_ = 0; SPflag_ = 0;
    if (verbosity_ > 1) {
      outStream << std::endl;
      outStream << "    Norm of free gradient components: " << gfnorm << std::endl;
    }

    // Trust-region subproblem solve loop
    for (int i = 0; i < minit_; ++i) {
      // Run Truncated CG
      flagCG = 0; iterCG = 0;
      tol  = std::min(tol1_,tol2_*gfnorm);
      stol = tol; //zero;
      state_->snorm = dtrpcg(*s,flagCG,iterCG,*gvec,*state_->iterateVec,
                      state_->searchSize,*model_,bnd,tol,stol,maxit_,
                      *pwa1,*dwa1,*pwa2,*dwa2,*pwa3,*dwa3,outStream);
      SPiter_ += iterCG;
      if (verbosity_ > 1) {
        outStream << std::endl;
        outStream << "  Computation of CG step"               << std::endl;
        outStream << "    Current face (i):                 " << i             << std::endl;
        outStream << "    CG step length:                   " << state_->snorm << std::endl;
        outStream << "    Number of CG iterations:          " << iterCG        << std::endl;
        outStream << "    CG flag:                          " << flagCG        << std::endl;
        outStream << "    Total number of iterations:       " << SPiter_       << std::endl;
      }

      // Projected search
      state_->snorm = dprsrch(*state_->iterateVec,*s,*gvec,*model_,bnd,*pwa1,*dwa1,*pwa2);

      // Model gradient at s = x[i+1] - x[0]
      state_->stepVec->set(*state_->iterateVec);
      state_->stepVec->axpy(-one,x); // s = x[i+1]-x[0]
      model_->hessVec(*gvec,*state_->stepVec,x,tol0);
      sHs = state_->stepVec->dot(gvec->dual());
      gvec->plus(*state_->gradientVec);
      bnd.pruneActive(*gvec,*state_->iterateVec,zero);
      if (hasEcon_) {
        applyFreePrecond(*pwa1,*gvec,*state_->iterateVec,*model_,bnd,tol0,*dwa1);
        gfnormf = pwa1->norm();
      }
      else {
        gfnormf = gvec->norm();
      }
      if (verbosity_ > 1) {
        outStream << std::endl;
        outStream << "    Step length (beta*s):             " << state_->snorm << std::endl;
        outStream << "    Norm of free gradient components: " << gfnormf       << std::endl;
        outStream << std::endl;
      }

      // Termination check
      if (gfnormf <= tol2_*gfnorm) {
        SPflag_ = 0;
        break;
      }
      else if (SPiter_ >= maxit_) {
        SPflag_ = 1;
        break;
      }
      else if (flagCG == 2) {
        SPflag_ = 2;
        break;
      }
      else if (flagCG == 3) {
        SPflag_ = 3;
        break;
      }

      // Update free gradient norm
      gfnorm = gfnormf;
    }
    // Update norm of step and update model predicted reduction
    //state_->snorm = state_->stepVec->norm();
    //model_->hessVec(*dwa1,*state_->stepVec,x,tol0);
    gs   = state_->stepVec->dot(state_->gradientVec->dual());
    pRed = -half*sHs - gs;
    //pRed = -(half * state_->stepVec->dot(dwa1->dual()) + gs);

    // Compute trial objective value
    x.plus(*state_->stepVec);
    obj.update(x,false);
    ftrial = obj.value(x,tol0);
    state_->nfval++;

    // Compute ratio of acutal and predicted reduction
    TRflag_ = TRUtils::SUCCESS;
    TRUtils::analyzeRatio<Real>(rho,TRflag_,state_->value,ftrial,pRed,eps_,outStream,verbosity_>1);

    // Update algorithm state
    state_->iter++;
    // Accept/reject step and update trust region radius
    if ((rho < eta0_ && TRflag_ == TRUtils::SUCCESS)
        || (TRflag_ >= 2)) { // Step Rejected
      x.set(*state_->iterateVec);
      obj.update(x,false,state_->iter);
      if (rho < zero && TRflag_ != TRUtils::TRNAN) {
        // Negative reduction, interpolate to find new trust-region radius
        state_->searchSize = TRUtils::interpolateRadius<Real>(*state_->gradientVec,*state_->stepVec,
          state_->snorm,pRed,state_->value,ftrial,state_->searchSize,gamma0_,gamma1_,eta2_,
          outStream,verbosity_>1);
      }
      else { // Shrink trust-region radius
        state_->searchSize = gamma1_*std::min(state_->snorm,state_->searchSize);
      }
    }
    else if ((rho >= eta0_ && TRflag_ != TRUtils::NPOSPREDNEG)
             || (TRflag_ == TRUtils::POSPREDNEG)) { // Step Accepted
      state_->value = ftrial;
      obj.update(x,true,state_->iter);
      // Increase trust-region radius
      if (rho >= eta2_) state_->searchSize *= gamma2_;
      // Compute gradient at new iterate
      gvec->set(*state_->gradientVec);
      obj.gradient(*state_->gradientVec,x,tol0);
      state_->ngrad++;
      state_->gnorm = Algorithm_B<Real>::optimalityCriterion(x,*state_->gradientVec,*state_->iterateVec);
      state_->iterateVec->set(x);
      // Update secant information in trust-region model
      model_->update(x,*state_->stepVec,*gvec,*state_->gradientVec,
                     state_->snorm,state_->iter);
    }

    // Update Output
    output.push_back(print(printHeader_));
    if (verbosity_ > 0) outStream << print(printHeader_);
  }
  output.push_back(Algorithm_B<Real>::printExitStatus());
  if (verbosity_ > 0) outStream << Algorithm_B<Real>::printExitStatus();
  return output;
}

template<typename Real>
Real LinMoreAlgorithm_B<Real>::dgpstep(Vector<Real> &s, const Vector<Real> &w,
                                 const Vector<Real> &x, const Real alpha) const {
  s.set(x); s.axpy(alpha,w);
  proj_->project(s);
  s.axpy(static_cast<Real>(-1),x);
  return s.norm();
}

template<typename Real>
Real LinMoreAlgorithm_B<Real>::dcauchy(Vector<Real> &s,
                                       Real &alpha,
                                       const Vector<Real> &x,
                                       const Vector<Real> &g,
                                       const Real del,
                                       TrustRegionModel_U<Real> &model,
                                       Vector<Real> &dwa,
                                       std::ostream &outStream) {
  const Real half(0.5);
  // const Real zero(0); // Unused
  Real tol = std::sqrt(ROL_EPSILON<Real>());
  bool interp = false;
  Real q(0), gs(0), snorm(0);
  // Compute s = P(x[0] - alpha g[0].dual())
  snorm = dgpstep(s,g,x,-alpha);
  if (snorm > del) {
    interp = true;
  }
  else {
    model.hessVec(dwa,s,x,tol);
    gs = s.dot(g);
    q  = half * s.dot(dwa.dual()) + gs;
    interp = (q > mu0_*gs);
  }
  // Either increase or decrease alpha to find approximate Cauchy point
  int cnt = 0;
  if (interp) {
    bool search = true;
    while (search) {
      alpha *= interpf_;
      snorm = dgpstep(s,g,x,-alpha);
      if (snorm <= del) {
        model.hessVec(dwa,s,x,tol);
        gs = s.dot(g);
        q  = half * s.dot(dwa.dual()) + gs;
        search = (q > mu0_*gs);
      }
    }
  }
  else {
    bool search = true;
    Real alphas = alpha;
    while (search) {
      alpha *= extrapf_;
      snorm = dgpstep(s,g,x,-alpha);
      if (snorm <= del && cnt < extlim_) {
        model.hessVec(dwa,s,x,tol);
        gs = s.dot(g);
        q  = half * s.dot(dwa.dual()) + gs;
        if (q <= mu0_*gs) {
          search = true;
          alphas = alpha;
        }
        else {
          search = false;
        }
      }
      else {
        search = false;
      }
      cnt++;
    }
    alpha = alphas;
    snorm = dgpstep(s,g,x,-alpha);
  }
  if (verbosity_ > 1) {
    outStream << std::endl;
    outStream << "  Cauchy point"                         << std::endl;
    outStream << "    Step length (alpha):              " << alpha << std::endl;
    outStream << "    Step length (alpha*g):            " << snorm << std::endl;
    if (!interp) {
      outStream << "    Number of extrapolation steps:    " << cnt << std::endl;
    }
  }
  return snorm;
}

template<typename Real>
Real LinMoreAlgorithm_B<Real>::dprsrch(Vector<Real> &x, Vector<Real> &s,
                                       const Vector<Real> &g,
                                       TrustRegionModel_U<Real> &model,
                                       BoundConstraint<Real> &bnd,
                                       Vector<Real> &pwa, Vector<Real> &dwa,
                                       Vector<Real> &pwa1,
                                       std::ostream &outStream) {
  const Real half(0.5);
  Real tol = std::sqrt(ROL_EPSILON<Real>());
  Real beta(1), snorm(0), q(0), gs(0);
  int nsteps = 0;
  // Reduce beta until sufficient decrease is satisfied
  bool search = true;
  while (search) {
    nsteps++;
    snorm = dgpstep(pwa,s,x,beta);
    applyFreeHessian(dwa,pwa,x,model,bnd,tol,pwa1);
    gs = pwa.dot(g.dual());
    q  = half * s.dot(dwa.dual()) + gs;
    if (q <= mu0_*gs) { // || nsteps > 10) {
      search = false;
    }
    else {
      beta *= interpfPS_;
    }
  }
  snorm = dgpstep(pwa,s,x,beta);
  s.set(pwa);
  x.plus(s);
  if (verbosity_ > 1) {
    outStream << std::endl;
    outStream << "  Projected search"                     << std::endl;
    outStream << "    Step length (beta):               " << beta       << std::endl;
    outStream << "    Number of steps:                  " << nsteps     << std::endl;
  }
  return snorm;
}

template<typename Real>
Real LinMoreAlgorithm_B<Real>::dtrqsol(const Real xtx,
                                       const Real ptp,
                                       const Real ptx,
                                       const Real del) const {
  const Real zero(0);
  Real dsq = del*del;
  Real rad = ptx*ptx + ptp*(dsq-xtx);
  rad = std::sqrt(std::max(rad,zero));
  Real sigma(0);
  if (ptx > zero) {
    sigma = (dsq-xtx)/(ptx+rad);
  }
  else if (rad > zero) {
    sigma = (rad-ptx)/ptp;
  }
  else {
    sigma = zero;
  }
  return sigma;
}

template<typename Real>
Real LinMoreAlgorithm_B<Real>::dtrpcg(Vector<Real> &w, int &iflag, int &iter,
                                      const Vector<Real> &g, const Vector<Real> &x,
                                      const Real del, TrustRegionModel_U<Real> &model,
                                      BoundConstraint<Real> &bnd,
                                      const Real tol, const Real stol, const int itermax,
                                      Vector<Real> &p, Vector<Real> &q, Vector<Real> &r,
                                      Vector<Real> &t, Vector<Real> &pwa, Vector<Real> &dwa,
                                      std::ostream &outStream) const {
  // p = step
  // q = hessian applied to step p
  // t = residual
  // r = preconditioned residual
  Real tol0 = std::sqrt(ROL_EPSILON<Real>());
  const Real zero(0), one(1), two(2);
  Real rho(0), tnorm(0), rnorm0(0), kappa(0), beta(0), sigma(0), alpha(0), rtr(0); //, rnorm(0)
  Real sMs(0), pMp(0), sMp(0);
  iter = 0; iflag = 0;
  // Initialize step
  w.zero();
  // Compute residual
  t.set(g); t.scale(-one);
  // Preconditioned residual
  applyFreePrecond(r,t,x,model,bnd,tol0,dwa);
  rho    = r.dot(t.dual());
  rnorm0 = std::sqrt(rho);
  if ( rnorm0 == zero ) {
    return zero;
  }
  // Initialize direction
  p.set(r);
  pMp = (!hasEcon_ ? rho : p.dot(p)); // If no equality constraint, used preconditioned norm
  // Iterate CG
  for (iter = 0; iter < itermax; ++iter) {
    // Apply Hessian to direction dir
    applyFreeHessian(q,p,x,model,bnd,tol0,pwa);
    // Compute sigma such that ||s+sigma*dir|| = del
    kappa = p.dot(q.dual());
    alpha = (kappa>zero) ? rho/kappa : zero;
    sigma = dtrqsol(sMs,pMp,sMp,del);
    // Check for negative curvature or if iterate exceeds trust region
    if (kappa <= zero || alpha >= sigma) {
      w.axpy(sigma,p);
      iflag = (kappa<=zero) ? 2 : 3;
      break;
    }
    // Update iterate and residuals
    w.axpy(alpha,p);
    t.axpy(-alpha,q);
    applyFreePrecond(r,t,x,model,bnd,tol0,dwa);
    // Exit if residual tolerance is met
    rtr   = r.dot(t.dual());
    //rnorm = std::sqrt(std::abs(rtr));
    tnorm = t.norm();
    //if (rnorm <= stol || tnorm <= tol) {
    if (rtr <= stol*stol || tnorm <= tol) {
      iflag = 0;
      break;
    }
    // Compute p = r + beta * p
    beta = rtr/rho;
    p.scale(beta); p.plus(r);
    rho  = rtr;
    // Update dot products
    //   sMs = <s, inv(M)s> or <s, s> if equality constraint present
    //   sMp = <s, inv(M)p> or <s, p> if equality constraint present
    //   pMp = <p, inv(M)p> or <p, p> if equality constraint present
    sMs  = sMs + two*alpha*sMp + alpha*alpha*pMp;
    sMp  = beta*(sMp + alpha*pMp);
    pMp  = rho + beta*beta*pMp;
  }
  // Check iteration count
  if (iter == itermax) {
    iflag = 1;
  }
  if (iflag != 1) { 
    iter++;
  }
  return w.norm();
}

template<typename Real>
void LinMoreAlgorithm_B<Real>::applyFreeHessian(Vector<Real> &hv,
                                                const Vector<Real> &v,
                                                const Vector<Real> &x,
                                                TrustRegionModel_U<Real> &model,
                                                BoundConstraint<Real> &bnd,
                                                Real &tol,
                                                Vector<Real> &pwa) const {
  const Real zero(0);
  pwa.set(v);
  bnd.pruneActive(pwa,x,zero);
  model.hessVec(hv,pwa,x,tol);
  bnd.pruneActive(hv,x,zero);
}

template<typename Real>
void LinMoreAlgorithm_B<Real>::applyFreePrecond(Vector<Real> &hv,
                                                const Vector<Real> &v,
                                                const Vector<Real> &x,
                                                TrustRegionModel_U<Real> &model,
                                                BoundConstraint<Real> &bnd,
                                                Real &tol,
                                                Vector<Real> &dwa) const {
  if (!hasEcon_) {
    const Real zero(0);
    dwa.set(v);
    bnd.pruneActive(dwa,x,zero);
    model.precond(hv,dwa,x,tol);
    bnd.pruneActive(hv,x,zero);
  }
  else {
    // Perform null space projection
    rcon_->setX(makePtrFromRef(x));
    ns_->update(x);
    ns_->apply(hv,v,tol);
  }
}

template<typename Real>
std::string LinMoreAlgorithm_B<Real>::printHeader( void ) const {
  std::stringstream hist;
  if (verbosity_ > 1) {
    hist << std::string(109,'-') << std::endl;
    hist << " Lin-More trust-region method status output definitions" << std::endl << std::endl;
    hist << "  iter    - Number of iterates (steps taken)" << std::endl;
    hist << "  value   - Objective function value" << std::endl; 
    hist << "  gnorm   - Norm of the gradient" << std::endl;
    hist << "  snorm   - Norm of the step (update to optimization vector)" << std::endl;
    hist << "  delta   - Trust-Region radius" << std::endl;
    hist << "  #fval   - Number of times the objective function was evaluated" << std::endl;
    hist << "  #grad   - Number of times the gradient was computed" << std::endl;
    hist << std::endl;
    hist << "  tr_flag - Trust-Region flag" << std::endl;
    for( int flag = TRUtils::SUCCESS; flag != TRUtils::UNDEFINED; ++flag ) {
      hist << "    " << NumberToString(flag) << " - "
           << TRUtils::ETRFlagToString(static_cast<TRUtils::ETRFlag>(flag)) << std::endl;
    }
    hist << std::endl;
    hist << "  iterCG - Number of Truncated CG iterations" << std::endl << std::endl;
    hist << "  flagGC - Trust-Region Truncated CG flag" << std::endl;
    for( int flag = CG_FLAG_SUCCESS; flag != CG_FLAG_UNDEFINED; ++flag ) {
      hist << "    " << NumberToString(flag) << " - "
           << ECGFlagToString(static_cast<ECGFlag>(flag)) << std::endl;
    }            
    hist << std::string(114,'-') << std::endl;
  }
  hist << "  ";
  hist << std::setw(6)  << std::left << "iter";
  hist << std::setw(15) << std::left << "value";
  hist << std::setw(15) << std::left << "gnorm";
  hist << std::setw(15) << std::left << "snorm";
  hist << std::setw(15) << std::left << "delta";
  hist << std::setw(10) << std::left << "#fval";
  hist << std::setw(10) << std::left << "#grad";
  hist << std::setw(10) << std::left << "tr_flag";
  hist << std::setw(10) << std::left << "iterCG";
  hist << std::setw(10) << std::left << "flagCG";
  hist << std::endl;
  return hist.str();
}

template<typename Real>
std::string LinMoreAlgorithm_B<Real>::printName( void ) const {
  std::stringstream hist;
  hist << std::endl << "Lin-More Trust-Region Method" << std::endl;
  return hist.str();
}

template<typename Real>
std::string LinMoreAlgorithm_B<Real>::print( const bool print_header ) const {
  std::stringstream hist;
  hist << std::scientific << std::setprecision(6);
  if ( state_->iter == 0 ) {
    hist << printName();
  }
  if ( print_header ) {
    hist << printHeader();
  }
  if ( state_->iter == 0 ) {
    hist << "  ";
    hist << std::setw(6)  << std::left << state_->iter;
    hist << std::setw(15) << std::left << state_->value;
    hist << std::setw(15) << std::left << state_->gnorm;
    hist << std::setw(15) << std::left << " ";
    hist << std::setw(15) << std::left << state_->searchSize;
    hist << std::endl;
  }
  else {
    hist << "  ";
    hist << std::setw(6)  << std::left << state_->iter;
    hist << std::setw(15) << std::left << state_->value;
    hist << std::setw(15) << std::left << state_->gnorm;
    hist << std::setw(15) << std::left << state_->snorm;
    hist << std::setw(15) << std::left << state_->searchSize;
    hist << std::setw(10) << std::left << state_->nfval;
    hist << std::setw(10) << std::left << state_->ngrad;
    hist << std::setw(10) << std::left << TRflag_;
    hist << std::setw(10) << std::left << SPiter_;
    hist << std::setw(10) << std::left << SPflag_;
    hist << std::endl;
  }
  return hist.str();
}

} // namespace ROL

#endif
