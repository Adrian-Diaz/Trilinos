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

#ifndef ROL_SLACKLESSCONSTRAINT_HPP
#define ROL_SLACKLESSCONSTRAINT_HPP

#include "ROL_Constraint.hpp"
#include "ROL_PartitionedVector.hpp"

/** @ingroup func_group
 *  \class ROL::SlacklessConstraint
 *  \brief This class strips out the slack variables from constraint evaluations
 *         to create the new constraint  \f$ C(x,s) = c(x) \f$
 */

namespace ROL {

template<typename Real> 
class SlacklessConstraint : public Constraint<Real> {
private: 
  const Ptr<Constraint<Real>> con_;

public:
  SlacklessConstraint( const Ptr<Constraint<Real>> &con );
 
  void update( const Vector<Real> &x, UpdateType type, int iter = -1 ) override;
  void update( const Vector<Real> &x, bool flag = true, int iter = -1 ) override;
  void value(Vector<Real> &c, const Vector<Real> &x, Real &tol ) override;
  void applyJacobian( Vector<Real> &jv, const Vector<Real> &v, const Vector<Real> &x, Real &tol ) override;
  void applyAdjointJacobian( Vector<Real> &ajv, const Vector<Real> &v, const Vector<Real> &x, const Vector<Real> &dualv, Real &tol ) override;
  void applyAdjointHessian( Vector<Real> &ahuv, const Vector<Real> &u, const Vector<Real> &v, const Vector<Real> &x, Real &tol ) override;

// Definitions for parametrized (stochastic) constraint functions
public:
  void setParameter(const std::vector<Real> &param) override;

private:
  Ptr<Vector<Real>> getOpt( Vector<Real> &xs ) const;
  Ptr<const Vector<Real>> getOpt( const Vector<Real> &xs ) const;
  void zeroSlack( Vector<Real> &x ) const;

}; // class SlacklessConstraint 

} // namespace ROL

#include "ROL_SlacklessConstraint_Def.hpp"

#endif // ROL__SLACKLESSCONSTRAINT_HPP

