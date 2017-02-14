// @HEADER
// ****************************************************************************
//                Tempus: Copyright (2017) Sandia Corporation
//
// Distributed under BSD 3-clause license (See accompanying file Copyright.txt)
// ****************************************************************************
// @HEADER

#ifndef Tempus_TimeStepControl_decl_hpp
#define Tempus_TimeStepControl_decl_hpp

// Teuchos
#include "Teuchos_VerboseObject.hpp"
#include "Teuchos_Describable.hpp"
#include "Teuchos_ParameterListAcceptorDefaultBase.hpp"
// Tempus
#include "Tempus_config.hpp"
#include "Tempus_SolutionHistory.hpp"

namespace Tempus {

/** \brief TimeStepControl manages the time step size.
 *  There several mechanicisms that effect the time step size and
 *  handled with this class:
 *   - Maximum and minimum time
 *   - Maximum and minimum time index
 *   - Maximum and minimum time step size
 *   - Maximum and minimum error
 *   - Maximum and minimum order
 *   - Startup considerations (e.g., ramping)
 *   - Solution and/or diagnostic output
 *  Additional step control can be added through the step control observer,
 *  or inheriting from this class.
 *   - Stability limits (e.g., CFL number)
 */
template<class Scalar>
class TimeStepControl
  : virtual public Teuchos::Describable,
    virtual public Teuchos::ParameterListAcceptor,
    virtual public Teuchos::VerboseObject<Tempus::TimeStepControl<Scalar> >
{
public:

  /** \brief Default constructor. */
  TimeStepControl();

  /** \brief Construct from ParameterList */
  TimeStepControl(Teuchos::RCP<Teuchos::ParameterList> pList);

  /// This is a copy constructor
  TimeStepControl(const TimeStepControl<Scalar>& tsc);

  /// Destructor
  virtual ~TimeStepControl() {};

  /** \brief Determine the time step size.*/
  virtual void getNextTimeStep(
    const Teuchos::RCP<SolutionHistory<Scalar> > & solutionHistory,
    Status & integratorStatus);

  /** \brief Check if time is within minimum and maximum time. */
  virtual bool timeInRange(const Scalar time) const;

  /** \brief Check if time step index is within minimum and maximum index. */
  virtual bool indexInRange(const int iStep) const;

  /// \name Overridden from Teuchos::ParameterListAcceptor
  //@{
    void setParameterList(const Teuchos::RCP<Teuchos::ParameterList> & pl);
    Teuchos::RCP<const Teuchos::ParameterList> getValidParameters() const;
    Teuchos::RCP<Teuchos::ParameterList> getNonconstParameterList();
    Teuchos::RCP<Teuchos::ParameterList> unsetParameterList();
  //@}

  /// \name Overridden from Teuchos::Describable
  //@{
    std::string description() const;
    void describe(Teuchos::FancyOStream          &out,
                  const Teuchos::EVerbosityLevel verbLevel) const;
  //@}

  Scalar timeMin_;        ///< Minimum simulation time
  Scalar timeMax_;        ///< Maximum simulation time
  Scalar dtMin_;          ///< Minimum time step
  Scalar dtInit_;         ///< Initial time step
  Scalar dtMax_;          ///< Maximum time step
  int    iStepMin_;       ///< Minimum time step index
  int    iStepMax_;       ///< Maximum time step index
  Scalar errorMaxAbs_;    ///< Maximum absolute error
  Scalar errorMaxRel_;    ///< Maximum relative error
  int orderMin_;          ///< Minimum time integration order
  int orderInit_;         ///< Initial time integration order
  int orderMax_;          ///< Maximum time integration order

  std::string stepType_;  ///< Integrator step type for step control

  std::vector<int>    outputIndices_;  ///< Vector of output indices.
  std::vector<Scalar> outputTimes_;    ///< Vector of output times.

  int nFailuresMax_;            ///< Maximum number of stepper failures
  int nConsecutiveFailuresMax_; ///< Maximum number of consecutive stepper failures

  Teuchos::RCP<Teuchos::ParameterList> pList_;

  bool outputAdjustedDt_; ///< Flag indicating that dt was adjusted for output.
  Scalar dtAfterOutput_;  ///< dt to reinstate after output step.

};
} // namespace Tempus

#endif // Tempus_TimeStepControl_decl_hpp
