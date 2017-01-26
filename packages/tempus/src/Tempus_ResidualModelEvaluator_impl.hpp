#ifndef Tempus_ResidualModelEvaluator_impl_hpp
#define Tempus_ResidualModelEvaluator_impl_hpp

namespace Tempus {


template <typename Scalar>
Thyra::ModelEvaluatorBase::InArgs<Scalar>
ResidualModelEvaluator<Scalar>::
createInArgs() const
{
  typedef Thyra::ModelEvaluatorBase MEB;

  MEB::InArgsSetup<Scalar> inArgs;
  inArgs.setModelEvalDescription(this->description());
  inArgs.set_Np(transientModel_->Np());
  inArgs.setSupports(MEB::IN_ARG_x);

  return inArgs;
}


template <typename Scalar>
Thyra::ModelEvaluatorBase::OutArgs<Scalar>
ResidualModelEvaluator<Scalar>::
createOutArgsImpl() const
{
  typedef Thyra::ModelEvaluatorBase MEB;

  MEB::OutArgsSetup<Scalar> outArgs;
  outArgs.setModelEvalDescription(this->description());
  outArgs.set_Np_Ng(transientModel_->Np(),0);
  outArgs.setSupports(MEB::OUT_ARG_f);
  outArgs.setSupports(MEB::OUT_ARG_W_op);

  return outArgs;
}


template <typename Scalar>
void
ResidualModelEvaluator<Scalar>::
evalModelImpl(const Thyra::ModelEvaluatorBase::InArgs<Scalar> &inArgs,
              const Thyra::ModelEvaluatorBase::OutArgs<Scalar> &outArgs) const
{
  typedef Thyra::ModelEvaluatorBase MEB;

  using Teuchos::RCP;

  RCP<const Thyra::VectorBase<Scalar> > x = inArgs.get_x();
  RCP<Thyra::VectorBase<Scalar> >   x_dot = Thyra::createMember(get_x_space());
  RCP<Thyra::VectorBase<Scalar> >   x_dot_dot; 
  if (secondOrderScheme_ == true) { 
    x_dot_dot = Thyra::createMember(get_x_space());
  }

  // IKT, FIXME, 1/25/16: add routine to compute relevant variables 
  // for second order schemes (if x_dot_dot != Teuchos::null),
  // and call here (put logic to call this routine instead of computeXDot_). 

  // call functor to compute x dot
  computeXDot_(*x,*x_dot);

  // setup input condition for nonlinear solve
  MEB::InArgs<Scalar> transientInArgs = transientModel_->createInArgs();
  transientInArgs.set_x(x);
  transientInArgs.set_x_dot(x_dot);
  if (x_dot_dot != Teuchos::null) {
    transientInArgs.set_x_dot_dot(x_dot_dot);
  }
  transientInArgs.set_t(t_);
  transientInArgs.set_alpha(alpha_);
  transientInArgs.set_beta(beta_);
  if (x_dot_dot != Teuchos::null) {
    transientInArgs.set_W_x_dot_dot_coeff(omega_);
  }
  for (int i=0; i<transientModel_->Np(); ++i) {
    if (inArgs.get_p(i) != Teuchos::null)
      transientInArgs.set_p(i, inArgs.get_p(i));
  }

  // setup output condition
  MEB::OutArgs<Scalar> transientOutArgs = transientModel_->createOutArgs();
  transientOutArgs.set_f(outArgs.get_f());
  transientOutArgs.set_W_op(outArgs.get_W_op());

  // build residual and jacobian
  transientModel_->evalModel(transientInArgs,transientOutArgs);
}


} // namespace Tempus

#endif  // Tempus_ResidualModelEvaluator_impl_hpp
