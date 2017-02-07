#ifndef TEMPUS_TEST_BALLPARABOLIC_MODEL_IMPL_HPP
#define TEMPUS_TEST_BALLPARABOLIC_MODEL_IMPL_HPP

#include "Teuchos_StandardParameterEntryValidators.hpp"

#include "Thyra_DefaultSpmdVectorSpace.hpp"
#include "Thyra_DetachedVectorView.hpp"
#include "Thyra_DetachedMultiVectorView.hpp"
#include "Thyra_DefaultSerialDenseLinearOpWithSolveFactory.hpp"
#include "Thyra_DefaultMultiVectorLinearOpWithSolve.hpp"
#include "Thyra_DefaultLinearOpSource.hpp"
#include "Thyra_VectorStdOps.hpp"

#include <iostream>

using Teuchos::RCP;
using Teuchos::rcp;
using Teuchos::ParameterList;
using Thyra::VectorBase;


namespace Tempus_Test {

template<class Scalar>
BallParabolicModel<Scalar>::
BallParabolicModel(RCP<ParameterList> pList_)
{
  isInitialized_ = false;
  dim_ = 1;
  Np_ = 1; // Number of parameter vectors (1)
  np_ = 3; // Number of parameters in this vector (3)
  Ng_ = 1; // Number of observation functions (1)
  ng_ = 1; // Number of elements in this observation function (1)
  acceptModelParams_ = false;
  haveIC_ = true;
  a_ = 0.0;
  f_ = 1.0;
  L_ = 1.0;
  t0_ic_ = 0.0;

  // Create x_space and f_space
  x_space_ = Thyra::defaultSpmdVectorSpace<Scalar>(dim_);
  f_space_ = Thyra::defaultSpmdVectorSpace<Scalar>(dim_);
  // Create p_space and g_space
  p_space_ = Thyra::defaultSpmdVectorSpace<Scalar>(np_);
  g_space_ = Thyra::defaultSpmdVectorSpace<Scalar>(ng_);

  setParameterList(pList_);
}

template<class Scalar>
ModelEvaluatorBase::InArgs<Scalar>
BallParabolicModel<Scalar>::
getExactSolution(double t) const
{
  TEUCHOS_TEST_FOR_EXCEPTION( !isInitialized_, std::logic_error,
      "Error, setupInOutArgs_ must be called first!\n");
  ModelEvaluatorBase::InArgs<Scalar> inArgs = inArgs_;
  double exact_t = t;
  inArgs.set_t(exact_t);
  RCP<VectorBase<Scalar> > exact_x = createMember(x_space_);
  { // scope to delete DetachedVectorView
    Thyra::DetachedVectorView<Scalar> exact_x_view(*exact_x);
    exact_x_view[0] = t*(1.0-0.5*t);  
  }
  inArgs.set_x(exact_x);
  RCP<VectorBase<Scalar> > exact_x_dot = createMember(x_space_);
  { // scope to delete DetachedVectorView
    Thyra::DetachedVectorView<Scalar> exact_x_dot_view(*exact_x_dot);
    exact_x_dot_view[0] = 1.0-t;
  }
  inArgs.set_x_dot(exact_x_dot);
  RCP<VectorBase<Scalar> > exact_x_dot_dot = createMember(x_space_);
  { // scope to delete DetachedVectorView
    Thyra::DetachedVectorView<Scalar> exact_x_dot_dot_view(*exact_x_dot_dot);
    exact_x_dot_dot_view[0] = -1.0;
  }
  inArgs.set_x_dot_dot(exact_x_dot_dot);
  return(inArgs);
}

//
// 06/24/09 tscoffe:
// These are the exact sensitivities for the problem assuming the initial conditions are specified as:
//    s(0) = [1;0]    s(1) = [0;b/L]                 s(2) = [0;-b*f/(L*L)]
// sdot(0) = [0;0] sdot(1) = [0;-3*f*f*b/(L*L*L)] sdot(2) = [0;3*f*f*f*b/(L*L*L*L)]
//
template<class Scalar>
ModelEvaluatorBase::InArgs<Scalar>
BallParabolicModel<Scalar>::
getExactSensSolution(int j, double t) const
{
  TEUCHOS_TEST_FOR_EXCEPTION( !isInitialized_, std::logic_error,
      "Error, setupInOutArgs_ must be called first!\n");
  ModelEvaluatorBase::InArgs<Scalar> inArgs = inArgs_;
  if (!acceptModelParams_) {
    return inArgs;
  }
  TEUCHOS_ASSERT_IN_RANGE_UPPER_EXCLUSIVE( j, 0, np_ );
  double exact_t = t;
  Scalar b = b_;
  Scalar f = f_;
  Scalar L = L_;
  Scalar phi = phi_;
  inArgs.set_t(exact_t);
  RCP<VectorBase<Scalar> > exact_s = createMember(x_space_);
  { // scope to delete DetachedVectorView
    Thyra::DetachedVectorView<Scalar> exact_s_view(*exact_s);
    if (j == 0) { // dx/da
      exact_s_view[0] = 1.0;
      exact_s_view[1] = 0.0;
    } else if (j == 1) { // dx/df
      exact_s_view[0] = (b/L)*t*cos((f/L)*t+phi);
      exact_s_view[1] = (b/L)*cos((f/L)*t+phi)-(b*f*t/(L*L))*sin((f/L)*t+phi);
    } else { // dx/dL
      exact_s_view[0] = -(b*f*t/(L*L))*cos((f/L)*t+phi);
      exact_s_view[1] = -(b*f/(L*L))*cos((f/L)*t+phi)+(b*f*f*t/(L*L*L))*sin((f/L)*t+phi);
    }
  }
  inArgs.set_x(exact_s);
  RCP<VectorBase<Scalar> > exact_s_dot = createMember(x_space_);
  { // scope to delete DetachedVectorView
    Thyra::DetachedVectorView<Scalar> exact_s_dot_view(*exact_s_dot);
    if (j == 0) { // dxdot/da
      exact_s_dot_view[0] = 0.0;
      exact_s_dot_view[1] = 0.0;
    } else if (j == 1) { // dxdot/df
      exact_s_dot_view[0] = (b/L)*cos((f/L)*t+phi)-(b*f*t/(L*L))*sin((f/L)*t+phi);
      exact_s_dot_view[1] = -(2.0*b*f/(L*L))*sin((f/L)*t+phi)-(b*f*f*t/(L*L*L))*cos((f/L)*t+phi);
    } else { // dxdot/dL
      exact_s_dot_view[0] = -(b*f/(L*L))*cos((f/L)*t+phi)+(b*f*f*t/(L*L*L))*sin((f/L)*t+phi);
      exact_s_dot_view[1] = (2.0*b*f*f/(L*L*L))*sin((f/L)*t+phi)+(b*f*f*f*t/(L*L*L*L))*cos((f/L)*t+phi);
    }
  }
  inArgs.set_x_dot(exact_s_dot);
  return(inArgs);
}

template<class Scalar>
RCP<const Thyra::VectorSpaceBase<Scalar> >
BallParabolicModel<Scalar>::
get_x_space() const
{
  return x_space_;
}


template<class Scalar>
RCP<const Thyra::VectorSpaceBase<Scalar> >
BallParabolicModel<Scalar>::
get_f_space() const
{
  return f_space_;
}


template<class Scalar>
ModelEvaluatorBase::InArgs<Scalar>
BallParabolicModel<Scalar>::
getNominalValues() const
{
  TEUCHOS_TEST_FOR_EXCEPTION( !isInitialized_, std::logic_error,
      "Error, setupInOutArgs_ must be called first!\n");
  return nominalValues_;
}


template<class Scalar>
RCP<Thyra::LinearOpWithSolveBase<Scalar> >
BallParabolicModel<Scalar>::
create_W() const
{
  RCP<const Thyra::LinearOpWithSolveFactoryBase<Scalar> > W_factory = this->get_W_factory();
  RCP<Thyra::LinearOpBase<Scalar> > matrix = this->create_W_op();
  {
    // 01/20/09 tscoffe:  This is a total hack to provide a full rank matrix to
    // linearOpWithSolve because it ends up factoring the matrix during
    // initialization, which it really shouldn't do, or I'm doing something
    // wrong here.   The net effect is that I get exceptions thrown in
    // optimized mode due to the matrix being rank deficient unless I do this.
    RCP<Thyra::MultiVectorBase<Scalar> > multivec = Teuchos::rcp_dynamic_cast<Thyra::MultiVectorBase<Scalar> >(matrix,true);
    {
      RCP<VectorBase<Scalar> > vec = Thyra::createMember(x_space_);
      {
        Thyra::DetachedVectorView<Scalar> vec_view( *vec );
        vec_view[0] = 0.0;
        vec_view[1] = 1.0;
      }
      V_V(multivec->col(0).ptr(),*vec);
      {
        Thyra::DetachedVectorView<Scalar> vec_view( *vec );
        vec_view[0] = 1.0;
        vec_view[1] = 0.0;
      }
      V_V(multivec->col(1).ptr(),*vec);
    }
  }
  RCP<Thyra::LinearOpWithSolveBase<Scalar> > W =
    Thyra::linearOpWithSolve<Scalar>(
      *W_factory,
      matrix
      );
  return W;
}
//RCP<Thyra::LinearOpWithSolveBase<Scalar> >
//BallParabolicModel<Scalar>::
//create_W() const
//{
//  return Thyra::multiVectorLinearOpWithSolve<Scalar>();
//}


template<class Scalar>
RCP<Thyra::LinearOpBase<Scalar> >
BallParabolicModel<Scalar>::
create_W_op() const
{
  RCP<Thyra::MultiVectorBase<Scalar> > matrix = Thyra::createMembers(x_space_, dim_);
  return(matrix);
}


template<class Scalar>
RCP<const Thyra::LinearOpWithSolveFactoryBase<Scalar> >
BallParabolicModel<Scalar>::
get_W_factory() const
{
  RCP<Thyra::LinearOpWithSolveFactoryBase<Scalar> > W_factory =
    Thyra::defaultSerialDenseLinearOpWithSolveFactory<Scalar>();
  return W_factory;
}
//  RCP<const Thyra::LinearOpBase<Scalar> > fwdOp = this->create_W_op();
//  RCP<Thyra::LinearOpWithSolveBase<Scalar> > W =
//    Thyra::linearOpWithSolve<Scalar>(
//      *W_factory,
//      fwdOp
//      );
//  W_factory->initializeOp(
//      Thyra::defaultLinearOpSource<Scalar>(fwdOp),
//      &*W,
//      Thyra::SUPPORT_SOLVE_UNSPECIFIED
//      );


template<class Scalar>
ModelEvaluatorBase::InArgs<Scalar>
BallParabolicModel<Scalar>::
createInArgs() const
{
  setupInOutArgs_();
  return inArgs_;
}


// Private functions overridden from ModelEvaluatorDefaultBase


template<class Scalar>
ModelEvaluatorBase::OutArgs<Scalar>
BallParabolicModel<Scalar>::
createOutArgsImpl() const
{
  setupInOutArgs_();
  return outArgs_;
}


template<class Scalar>
void
BallParabolicModel<Scalar>::
evalModelImpl(
  const ModelEvaluatorBase::InArgs<Scalar> &inArgs,
  const ModelEvaluatorBase::OutArgs<Scalar> &outArgs
  ) const
{
  TEUCHOS_TEST_FOR_EXCEPTION( !isInitialized_, std::logic_error,
      "Error, setupInOutArgs_ must be called first!\n");

  const RCP<const VectorBase<Scalar> > x_in = inArgs.get_x().assert_not_null();
  Thyra::ConstDetachedVectorView<Scalar> x_in_view( *x_in );

  //double t = inArgs.get_t();
  Scalar a = a_;
  Scalar f = f_;
  Scalar L = L_;
  if (acceptModelParams_) {
    const RCP<const VectorBase<Scalar> > p_in =
      inArgs.get_p(0).assert_not_null();
    Thyra::ConstDetachedVectorView<Scalar> p_in_view( *p_in );
    a = p_in_view[0];
    f = p_in_view[1];
    L = p_in_view[2];
  }

  Scalar beta = inArgs.get_beta();

  const RCP<VectorBase<Scalar> > f_out = outArgs.get_f();
  const RCP<Thyra::LinearOpBase<Scalar> > W_out = outArgs.get_W_op();
  RCP<Thyra::MultiVectorBase<Scalar> > DfDp_out;
  if (acceptModelParams_) {
    Thyra::ModelEvaluatorBase::Derivative<Scalar> DfDp = outArgs.get_DfDp(0);
    DfDp_out = DfDp.getMultiVector();
  }

  if (inArgs.get_x_dot().is_null()) {

    // Evaluate the Explicit ODE f(x,t) [= 0]
    if (!is_null(f_out)) {
      Thyra::DetachedVectorView<Scalar> f_out_view( *f_out );
      f_out_view[0] = x_in_view[1];
      f_out_view[1] = (f/L)*(f/L)*(a-x_in_view[0]);
    }
    if (!is_null(W_out)) {
      RCP<Thyra::MultiVectorBase<Scalar> > matrix =
        Teuchos::rcp_dynamic_cast<Thyra::MultiVectorBase<Scalar> >(W_out,true);
      Thyra::DetachedMultiVectorView<Scalar> matrix_view( *matrix );
      matrix_view(0,0) = 0.0;               // d(f0)/d(x0_n)
      matrix_view(0,1) = +beta;             // d(f0)/d(x1_n)
      matrix_view(1,0) = -beta*(f/L)*(f/L); // d(f1)/d(x0_n)
      matrix_view(1,1) = 0.0;               // d(f1)/d(x1_n)
      // Note: alpha = d(xdot)/d(x_n) and beta = d(x)/d(x_n)
    }
    if (!is_null(DfDp_out)) {
      Thyra::DetachedMultiVectorView<Scalar> DfDp_out_view( *DfDp_out );
      DfDp_out_view(0,0) = 0.0;
      DfDp_out_view(0,1) = 0.0;
      DfDp_out_view(0,2) = 0.0;
      DfDp_out_view(1,0) = (f/L)*(f/L);
      DfDp_out_view(1,1) = (2.0*f/(L*L))*(a-x_in_view[0]);
      DfDp_out_view(1,2) = -(2.0*f*f/(L*L*L))*(a-x_in_view[0]);
    }
  } else {

    // Evaluate the implicit ODE f(xdot, x, t) [= 0]
    RCP<const VectorBase<Scalar> > x_dot_in;
    x_dot_in = inArgs.get_x_dot().assert_not_null();
    Scalar alpha = inArgs.get_alpha();
    if (!is_null(f_out)) {
      Thyra::DetachedVectorView<Scalar> f_out_view( *f_out );
      Thyra::ConstDetachedVectorView<Scalar> x_dot_in_view( *x_dot_in );
      f_out_view[0] = x_dot_in_view[0] - x_in_view[1];
      f_out_view[1] = x_dot_in_view[1] - (f/L)*(f/L)*(a-x_in_view[0]);
    }
    if (!is_null(W_out)) {
      RCP<Thyra::MultiVectorBase<Scalar> > matrix =
        Teuchos::rcp_dynamic_cast<Thyra::MultiVectorBase<Scalar> >(W_out,true);
      Thyra::DetachedMultiVectorView<Scalar> matrix_view( *matrix );
      matrix_view(0,0) = alpha;             // d(f0)/d(x0_n)
      matrix_view(0,1) = -beta;             // d(f0)/d(x1_n)
      matrix_view(1,0) = +beta*(f/L)*(f/L); // d(f1)/d(x0_n)
      matrix_view(1,1) = alpha;             // d(f1)/d(x1_n)
      // Note: alpha = d(xdot)/d(x_n) and beta = d(x)/d(x_n)
    }
    if (!is_null(DfDp_out)) {
      Thyra::DetachedMultiVectorView<Scalar> DfDp_out_view( *DfDp_out );
      DfDp_out_view(0,0) = 0.0;
      DfDp_out_view(0,1) = 0.0;
      DfDp_out_view(0,2) = 0.0;
      DfDp_out_view(1,0) = -(f/L)*(f/L);
      DfDp_out_view(1,1) = -(2.0*f/(L*L))*(a-x_in_view[0]);
      DfDp_out_view(1,2) = +(2.0*f*f/(L*L*L))*(a-x_in_view[0]);
    }
  }
}

template<class Scalar>
RCP<const Thyra::VectorSpaceBase<Scalar> >
BallParabolicModel<Scalar>::
get_p_space(int l) const
{
  if (!acceptModelParams_) {
    return Teuchos::null;
  }
  TEUCHOS_ASSERT_IN_RANGE_UPPER_EXCLUSIVE( l, 0, Np_ );
  return p_space_;
}

template<class Scalar>
RCP<const Teuchos::Array<std::string> >
BallParabolicModel<Scalar>::
get_p_names(int l) const
{
  if (!acceptModelParams_) {
    return Teuchos::null;
  }
  TEUCHOS_ASSERT_IN_RANGE_UPPER_EXCLUSIVE( l, 0, Np_ );
  RCP<Teuchos::Array<std::string> > p_strings =
    rcp(new Teuchos::Array<std::string>());
  p_strings->push_back("Model Coefficient:  a");
  p_strings->push_back("Model Coefficient:  f");
  p_strings->push_back("Model Coefficient:  L");
  return p_strings;
}

template<class Scalar>
RCP<const Thyra::VectorSpaceBase<Scalar> >
BallParabolicModel<Scalar>::
get_g_space(int j) const
{
  TEUCHOS_ASSERT_IN_RANGE_UPPER_EXCLUSIVE( j, 0, Ng_ );
  return g_space_;
}

// private

template<class Scalar>
void
BallParabolicModel<Scalar>::
setupInOutArgs_() const
{
  if (isInitialized_) {
    return;
  }

  {
    // Set up prototypical InArgs
    ModelEvaluatorBase::InArgsSetup<Scalar> inArgs;
    inArgs.setModelEvalDescription(this->description());
    inArgs.setSupports( ModelEvaluatorBase::IN_ARG_t );
    inArgs.setSupports( ModelEvaluatorBase::IN_ARG_x );
    inArgs.setSupports( ModelEvaluatorBase::IN_ARG_x_dot );
    inArgs.setSupports( ModelEvaluatorBase::IN_ARG_x_dot_dot); 
    inArgs.setSupports( ModelEvaluatorBase::IN_ARG_alpha );
    inArgs.setSupports( ModelEvaluatorBase::IN_ARG_beta );
    inArgs.setSupports( ModelEvaluatorBase::IN_ARG_W_x_dot_dot_coeff); 
    if (acceptModelParams_) {
      inArgs.set_Np(Np_);
    }
    inArgs_ = inArgs;
  }

  {
    // Set up prototypical OutArgs
    ModelEvaluatorBase::OutArgsSetup<Scalar> outArgs;
    outArgs.setModelEvalDescription(this->description());
    outArgs.setSupports( ModelEvaluatorBase::OUT_ARG_f );
    outArgs.setSupports( ModelEvaluatorBase::OUT_ARG_W_op );
    if (acceptModelParams_) {
      outArgs.set_Np_Ng(Np_,Ng_);
      outArgs.setSupports( ModelEvaluatorBase::OUT_ARG_DfDp,0,
                           Thyra::ModelEvaluatorBase::DERIV_MV_BY_COL );
    }
    outArgs_ = outArgs;
  }

  // Set up nominal values
  nominalValues_ = inArgs_;
  if (haveIC_)
  {
    nominalValues_.set_t(t0_ic_);
    const RCP<VectorBase<Scalar> > x_ic = createMember(x_space_);
    { // scope to delete DetachedVectorView
      Thyra::DetachedVectorView<Scalar> x_ic_view( *x_ic );
      x_ic_view[0] = a_+b_*sin((f_/L_)*t0_ic_+phi_);
      x_ic_view[1] = b_*(f_/L_)*cos((f_/L_)*t0_ic_+phi_);
    }
    nominalValues_.set_x(x_ic);
    if (acceptModelParams_) {
      const RCP<VectorBase<Scalar> > p_ic = createMember(p_space_);
      {
        Thyra::DetachedVectorView<Scalar> p_ic_view( *p_ic );
        p_ic_view[0] = a_;
        p_ic_view[1] = f_;
        p_ic_view[2] = L_;
      }
      nominalValues_.set_p(0,p_ic);
    }
    const RCP<VectorBase<Scalar> > x_dot_ic = createMember(x_space_);
    { // scope to delete DetachedVectorView
      Thyra::DetachedVectorView<Scalar> x_dot_ic_view( *x_dot_ic );
      x_dot_ic_view[0] = b_*(f_/L_)*cos((f_/L_)*t0_ic_+phi_);
      x_dot_ic_view[1] = -b_*(f_/L_)*(f_/L_)*sin((f_/L_)*t0_ic_+phi_);
    }
    nominalValues_.set_x_dot(x_dot_ic);
  }

  isInitialized_ = true;

}

template<class Scalar>
void
BallParabolicModel<Scalar>::
setParameterList(RCP<ParameterList> const& paramList)
{
  using Teuchos::get;
  RCP<ParameterList> tmpPL = rcp(new ParameterList("BallParabolicModel"));
  if (paramList != Teuchos::null) tmpPL = paramList;
  tmpPL->validateParametersAndSetDefaults(*this->getValidParameters());
  this->setMyParamList(tmpPL);
  RCP<ParameterList> pl = this->getMyNonconstParamList();
  bool acceptModelParams = get<bool>(*pl,"Accept model parameters");
  bool haveIC = get<bool>(*pl,"Provide nominal values");
  if ( (acceptModelParams != acceptModelParams_) ||
       (haveIC != haveIC_)
     ) {
    isInitialized_ = false;
  }
  acceptModelParams_ = acceptModelParams;
  haveIC_ = haveIC;
  setupInOutArgs_();
}

template<class Scalar>
RCP<const ParameterList>
BallParabolicModel<Scalar>::
getValidParameters() const
{
  static RCP<const ParameterList> validPL;
  if (is_null(validPL)) {
    RCP<ParameterList> pl = Teuchos::parameterList();
    pl->set("Accept model parameters", false);
    pl->set("Provide nominal values", true);
    Teuchos::setDoubleParameter(
        "Coeff a", 0.0, "Coefficient a in model", &*pl);
    Teuchos::setDoubleParameter(
        "Coeff f", 1.0, "Coefficient f in model", &*pl);
    Teuchos::setDoubleParameter(
        "Coeff L", 1.0, "Coefficient L in model", &*pl);
    Teuchos::setDoubleParameter(
        "IC x0", 0.0, "Initial Condition for x0", &*pl);
    Teuchos::setDoubleParameter(
        "IC x1", 1.0, "Initial Condition for x1", &*pl);
    Teuchos::setDoubleParameter(
        "IC t0", 0.0, "Initial time t0", &*pl);
    validPL = pl;
  }
  return validPL;
}

} // namespace Tempus_Test
#endif // TEMPUS_TEST_BALLPARABOLIC_MODEL_IMPL_HPP
