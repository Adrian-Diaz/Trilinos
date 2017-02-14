// @HEADER
// ****************************************************************************
//                Tempus: Copyright (2017) Sandia Corporation
//
// Distributed under BSD 3-clause license (See accompanying file Copyright.txt)
// ****************************************************************************
// @HEADER

#ifndef Tempus_StepperForwardEuler_decl_hpp
#define Tempus_StepperForwardEuler_decl_hpp

#include "Tempus_config.hpp"
#include "Tempus_Stepper.hpp"

namespace Tempus {


/** \brief Forward Euler time stepper.
 *  Forward Euler is an explicit time stepper (i.e., no solver used).
 */
template<class Scalar>
class StepperForwardEuler : virtual public Tempus::Stepper<Scalar>
{
public:

  /// Constructor
  StepperForwardEuler(
    Teuchos::RCP<Teuchos::ParameterList>                      pList,
    const Teuchos::RCP<const Thyra::ModelEvaluator<Scalar> >& transientModel);

  /// \name Basic stepper methods
  //@{
    virtual void setModel(
      const Teuchos::RCP<const Thyra::ModelEvaluator<Scalar> >& transientModel);
    virtual void setNonConstModel(
      const Teuchos::RCP<Thyra::ModelEvaluator<Scalar> >& transientModel);
    virtual Teuchos::RCP<const Thyra::ModelEvaluator<Scalar> >
      getModel(){return eODEModel_;}


    /// Initialize during construction and after changing input parameters.
    virtual void initialize(){}

    /// Take the specified timestep, dt, and return true if successful.
    virtual void takeStep(
      const Teuchos::RCP<SolutionHistory<Scalar> >& solutionHistory);

    /// Get a default (initial) StepperState
    virtual Teuchos::RCP<Tempus::StepperState<Scalar> > getDefaultStepperState();
    virtual Scalar getOrder() const {return 1.0;}
    virtual Scalar getOrderMin() const {return 1.0;}
    virtual Scalar getOrderMax() const {return 1.0;}
  //@}

  /// \name ParameterList methods
  //@{
    void setParameterList(const Teuchos::RCP<Teuchos::ParameterList> & pl);
    Teuchos::RCP<Teuchos::ParameterList> getNonconstParameterList();
    Teuchos::RCP<Teuchos::ParameterList> unsetParameterList();
    Teuchos::RCP<const Teuchos::ParameterList> getValidParameters() const;
  //@}

  /// \name Overridden from Teuchos::Describable
  //@{
    virtual std::string description() const;
    virtual void describe(Teuchos::FancyOStream        & out,
                          const Teuchos::EVerbosityLevel verbLevel) const;
  //@}

private:

  /// Default Constructor -- not allowed
  StepperForwardEuler();

protected:

  Teuchos::RCP<Teuchos::ParameterList>               pList_;
  /// Explicit ODE ModelEvaluator
  Teuchos::RCP<const Thyra::ModelEvaluator<Scalar> > eODEModel_;

  Thyra::ModelEvaluatorBase::InArgs<Scalar>  inArgs_;
  Thyra::ModelEvaluatorBase::OutArgs<Scalar> outArgs_;
};
} // namespace Tempus

#endif // Tempus_StepperForwardEuler_decl_hpp
