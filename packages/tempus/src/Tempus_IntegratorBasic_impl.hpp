#ifndef Tempus_IntegratorBasic_impl_hpp
#define Tempus_IntegratorBasic_impl_hpp

#include "Teuchos_VerboseObjectParameterListHelpers.hpp"
#include "Teuchos_TimeMonitor.hpp"
#include "Tempus_StepperFactory.hpp"
#include "Tempus_TimeStepControl.hpp"
#include <ctime>


namespace Tempus {

template<class Scalar>
IntegratorBasic<Scalar>::IntegratorBasic(
  Teuchos::RCP<Teuchos::ParameterList>                inputPL,
  const Teuchos::RCP<Thyra::ModelEvaluator<Scalar> >& model)
     : tempusPL_(inputPL), integratorStatus_(WORKING)
{
  using Teuchos::RCP;
  using Teuchos::ParameterList;

  // Get the name of the integrator to build.
  std::string integratorName_ =tempusPL_->get<std::string>("Integrator Name");

  this->setParameterList(Teuchos::sublist(tempusPL_, integratorName_, true));
  this->setStepper(model);
  this->setTimeStepControl();
  this->parseScreenOutput();
  this->setSolutionHistory();
  this->setObserver();
  this->initialize();

  if (integratorTimer_ == Teuchos::null)
    integratorTimer_ = rcp(new Teuchos::Time("Integrator Timer"));
  if (stepperTimer_ == Teuchos::null)
    stepperTimer_    = rcp(new Teuchos::Time("Stepper Timer"));

  if (Teuchos::as<int>(this->getVerbLevel()) >=
      Teuchos::as<int>(Teuchos::VERB_HIGH)) {
    RCP<Teuchos::FancyOStream> out = this->getOStream();
    Teuchos::OSTab ostab(out,1,"IntegratorBasic::IntegratorBasic");
    *out << this->description() << std::endl;
  }
}


template<class Scalar>
void IntegratorBasic<Scalar>::setStepper(
  const Teuchos::RCP<Thyra::ModelEvaluator<Scalar> >& model)
{
  using Teuchos::RCP;
  using Teuchos::ParameterList;

  if (stepper_ == Teuchos::null) {
    // Construct from Integrator ParameterList
    RCP<StepperFactory<Scalar> > sf =Teuchos::rcp(new StepperFactory<Scalar>());
    std::string stepperName = integratorPL_->get<std::string>("Stepper Name");

    RCP<ParameterList> stepperPL = Teuchos::sublist(tempusPL_,stepperName,true);
    stepper_ = sf->createStepper(stepperPL, model);
  } else {
    stepper_->setModel(model);
  }
}


template<class Scalar>
void IntegratorBasic<Scalar>::setStepper(
  Teuchos::RCP<Stepper<Scalar> > newStepper)
{
  using Teuchos::RCP;
  using Teuchos::ParameterList;

  // Make integratorPL_ consistent with new stepper.
  RCP<ParameterList> newStepperPL = newStepper->getNonconstParameterList();
  integratorPL_->set("Stepper Name", newStepperPL->name());
  integratorPL_->set(newStepperPL->name(), newStepperPL);

  stepper_ = Teuchos::null;
  stepper_ = newStepper;
}


template<class Scalar>
void IntegratorBasic<Scalar>::setSolutionHistory(
  Teuchos::RCP<SolutionHistory<Scalar> > sh)
{
  using Teuchos::RCP;
  using Teuchos::ParameterList;

  if (sh == Teuchos::null) {
    // Construct from Integrator ParameterList
    RCP<ParameterList> shPL =
      Teuchos::sublist(integratorPL_, "Solution History", true);
    solutionHistory_ = rcp(new SolutionHistory<Scalar>(shPL));

    // Create IC SolutionState
    // Create meta data
    RCP<SolutionStateMetaData<Scalar> > md =
                                     rcp(new SolutionStateMetaData<Scalar> ());
    md->setTime (timeStepControl_->timeMin_);
    md->setIStep(timeStepControl_->iStepMin_);
    md->setDt   (timeStepControl_->dtInit_);
    int orderTmp = timeStepControl_->orderInit_;
    if (orderTmp == 0) orderTmp = stepper_->getOrderMin();
    md->setOrder(orderTmp);
    md->setSolutionStatus(Status::PASSED);  // ICs are considered passing.

    // Create initial condition solution state
    typedef Thyra::ModelEvaluatorBase MEB;
    Thyra::ModelEvaluatorBase::InArgs<Scalar> inArgsIC =
      stepper_->getModel()->getNominalValues();
    RCP<Thyra::VectorBase<Scalar> > x = inArgsIC.get_x()->clone_v();
    RCP<Thyra::VectorBase<Scalar> > xdot;
    if (inArgsIC.supports(MEB::IN_ARG_x_dot)) {
      xdot = inArgsIC.get_x_dot()->clone_v();
    } else {
      xdot = x->clone_v();
    }
    RCP<Thyra::VectorBase<Scalar> > xdotdot = Teuchos::null;
    RCP<SolutionState<Scalar> > newState = rcp(new SolutionState<Scalar>(
      md, x, xdot, xdotdot, stepper_->getDefaultStepperState()));

    solutionHistory_->addState(newState);

  } else {

    TEUCHOS_TEST_FOR_EXCEPTION( sh->getNumStates() < 1,
      std::out_of_range,
         "Error - setSolutionHistory requires at least one SolutionState.\n"
      << "        Supplied SolutionHistory has only " << sh->getNumStates()
      << " SolutionStates.\n");

    // Make integratorPL_ consistent with new SolutionHistory.
    RCP<ParameterList> shPL = sh->getNonconstParameterList();
    integratorPL_->set("Solution History", shPL->name());
    integratorPL_->set(shPL->name(), shPL);

    solutionHistory_ = Teuchos::null;
    solutionHistory_ = sh;
  }
}


template<class Scalar>
void IntegratorBasic<Scalar>::setTimeStepControl(
  Teuchos::RCP<TimeStepControl<Scalar> > tsc)
{
  using Teuchos::RCP;
  using Teuchos::ParameterList;

  if (tsc == Teuchos::null) {
    // Construct from Integrator ParameterList
    RCP<ParameterList> tscPL =
      Teuchos::sublist(integratorPL_,"Time Step Control",true);
    timeStepControl_ = rcp(new TimeStepControl<Scalar>(tscPL));

    if (timeStepControl_->orderMin_ == 0)
      timeStepControl_->orderMin_ = stepper_->getOrderMin();
    if (timeStepControl_->orderMax_ == 0)
      timeStepControl_->orderMax_ = stepper_->getOrderMax();

  } else {
    // Make integratorPL_ consistent with new TimeStepControl.
    RCP<ParameterList> tscPL = tsc->getNonconstParameterList();
    integratorPL_->set("Time Step Control", tscPL->name());
    integratorPL_->set(tscPL->name(), tscPL);

    timeStepControl_ = Teuchos::null;
    timeStepControl_ = tsc;
  }
}


template<class Scalar>
void IntegratorBasic<Scalar>::setObserver(
  Teuchos::RCP<IntegratorObserver<Scalar> > obs)
{
  if (obs == Teuchos::null) {
    // Create default IntegratorObserver
    integratorObserver_ =
      Teuchos::rcp(new IntegratorObserver<Scalar>(solutionHistory_,
                                                  timeStepControl_));
  } else {
    integratorObserver_ = obs;
  }
}


template<class Scalar>
void IntegratorBasic<Scalar>::initialize()
{
}


template<class Scalar>
std::string IntegratorBasic<Scalar>::description() const
{
  std::string name = "Tempus::IntegratorBasic";
  return(name);
}


template<class Scalar>
void IntegratorBasic<Scalar>::describe(
  Teuchos::FancyOStream          &out,
  const Teuchos::EVerbosityLevel verbLevel) const
{
  out << description() << "::describe" << std::endl;
  out << "solutionHistory= " << solutionHistory_->description()<<std::endl;
  out << "timeStepControl= " << timeStepControl_->description()<<std::endl;
  out << "stepper        = " << stepper_        ->description()<<std::endl;

  if (Teuchos::as<int>(verbLevel) >=
              Teuchos::as<int>(Teuchos::VERB_HIGH)) {
    out << "solutionHistory= " << std::endl;
    solutionHistory_->describe(out,verbLevel);
    out << "timeStepControl= " << std::endl;
    timeStepControl_->describe(out,verbLevel);
    out << "stepper        = " << std::endl;
    stepper_        ->describe(out,verbLevel);
  }
}


template <class Scalar>
bool IntegratorBasic<Scalar>::advanceTime(const Scalar timeFinal)
{
  if (timeStepControl_->timeInRange(timeFinal))
    timeStepControl_->timeMax_ = timeFinal;
  bool itgrStatus = advanceTime();
  return itgrStatus;
}


template <class Scalar>
void IntegratorBasic<Scalar>::startIntegrator()
{
  std::time_t begin = std::time(nullptr);
  integratorTimer_->start();
  Teuchos::RCP<Teuchos::FancyOStream> out = this->getOStream();
  Teuchos::OSTab ostab(out,0,"ScreenOutput");
  *out << "\nTempus - IntegratorBasic\n"
       << std::asctime(std::localtime(&begin)) << "\n"
       << "  Stepper = " << stepper_->description() << "\n"
       << "  Simulation Time Range  [" << timeStepControl_->timeMin_
       << ", " << timeStepControl_->timeMax_ << "]\n"
       << "  Simulation Index Range [" << timeStepControl_->iStepMin_
       << ", " << timeStepControl_->iStepMax_ << "]\n"
       << "============================================================================\n"
       << "  Step       Time         dt  Abs Error  Rel Error  Order  nFail  dCompTime"
       << std::endl;
  integratorStatus_ = WORKING;
}


template <class Scalar>
bool IntegratorBasic<Scalar>::advanceTime()
{
  TEMPUS_FUNC_TIME_MONITOR("Tempus::IntegratorBasic::advanceTime()");
  {
    startIntegrator();
    integratorObserver_->observeStartIntegrator();

    while (integratorStatus_ == WORKING and
          timeStepControl_->timeInRange (solutionHistory_->getCurrentTime()) and
          timeStepControl_->indexInRange(solutionHistory_->getCurrentIndex())){

      stepperTimer_->reset();
      stepperTimer_->start();
      solutionHistory_->initWorkingState();

      startTimeStep();
      integratorObserver_->observeStartTimeStep();

      timeStepControl_->getNextTimeStep(solutionHistory_, integratorStatus_);
      integratorObserver_->observeNextTimeStep(integratorStatus_);

      if (integratorStatus_ == FAILED) break;

      integratorObserver_->observeBeforeTakeStep();

      stepper_->takeStep(solutionHistory_);

      integratorObserver_->observeAfterTakeStep();

      stepperTimer_->stop();
      acceptTimeStep();
      integratorObserver_->observeAcceptedTimeStep(integratorStatus_);
    }

    endIntegrator();
    integratorObserver_->observeEndIntegrator(integratorStatus_);
  }

  return (integratorStatus_ == Status::PASSED);
}


template <class Scalar>
void IntegratorBasic<Scalar>::startTimeStep()
{
  Teuchos::RCP<SolutionStateMetaData<Scalar> > wsmd =
    solutionHistory_->getWorkingState()->metaData_;

  // Check if we need to dump screen output this step
  std::vector<int>::const_iterator it =
    std::find(outputScreenIndices_.begin(),
              outputScreenIndices_.end(),
              wsmd->getIStep()+1);
  if (it == outputScreenIndices_.end())
    wsmd->setOutputScreen(false);
  else
    wsmd->setOutputScreen(true);
}


template <class Scalar>
void IntegratorBasic<Scalar>::acceptTimeStep()
{
  using Teuchos::RCP;
  RCP<SolutionStateMetaData<Scalar> > wsmd =
    solutionHistory_->getWorkingState()->metaData_;

       // Stepper failure
  if ( solutionHistory_->getWorkingState()->getSolutionStatus() == FAILED or
       solutionHistory_->getWorkingState()->getStepperStatus() == FAILED or
       // Constant time step failure
       ((timeStepControl_->stepType_ == "Constant") and
       (wsmd->getDt() != timeStepControl_->dtInit_))
     )
  {
    wsmd->setNFailures(wsmd->getNFailures()+1);
    wsmd->setNConsecutiveFailures(wsmd->getNConsecutiveFailures()+1);
    wsmd->setSolutionStatus(FAILED);
  }

  // Too many failures
  if (wsmd->getNFailures() >= timeStepControl_->nFailuresMax_) {
    RCP<Teuchos::FancyOStream> out = this->getOStream();
    Teuchos::OSTab ostab(out,1,"continueIntegration");
    *out << "Failure - Stepper has failed more than the maximum allowed.\n"
         << "  (nFailures = "<<wsmd->getNFailures()<< ") >= (nFailuresMax = "
         <<timeStepControl_->nFailuresMax_<<")" << std::endl;
    integratorStatus_ = FAILED;
    return;
  }
  if (wsmd->getNConsecutiveFailures()
      >= timeStepControl_->nConsecutiveFailuresMax_){
    RCP<Teuchos::FancyOStream> out = this->getOStream();
    Teuchos::OSTab ostab(out,1,"continueIntegration");
    *out << "Failure - Stepper has failed more than the maximum "
         << "consecutive allowed.\n"
         << "  (nConsecutiveFailures = "<<wsmd->getNConsecutiveFailures()
         << ") >= (nConsecutiveFailuresMax = "
         <<timeStepControl_->nConsecutiveFailuresMax_
         << ")" << std::endl;
    integratorStatus_ = FAILED;
    return;
  }

  // =======================================================================
  // Made it here! Accept this time step

  solutionHistory_->promoteWorkingState();

  RCP<SolutionStateMetaData<Scalar> > csmd =
    solutionHistory_->getCurrentState()->metaData_;

  csmd->setNFailures(std::max(csmd->getNFailures()-1,0));
  csmd->setNConsecutiveFailures(0);

  // Output and screen output
  if (csmd->getOutput() == true) {
    // Dump solution!
  }

  if (csmd->getOutputScreen() == true) {
    const double steppertime = stepperTimer_->totalElapsedTime();
    stepperTimer_->reset();
    RCP<Teuchos::FancyOStream> out = this->getOStream();
    Teuchos::OSTab ostab(out,0,"ScreenOutput");
    *out
    <<std::scientific<<std::setw( 6)<<std::setprecision(3)<<csmd->getIStep()
                     <<std::setw(11)<<std::setprecision(3)<<csmd->getTime()
                     <<std::setw(11)<<std::setprecision(3)<<csmd->getDt()
                     <<std::setw(11)<<std::setprecision(3)<<csmd->getErrorAbs()
                     <<std::setw(11)<<std::setprecision(3)<<csmd->getErrorRel()
    <<std::fixed     <<std::setw( 7)<<std::setprecision(1)<<csmd->getOrder()
    <<std::scientific<<std::setw( 7)<<std::setprecision(3)<<csmd->getNFailures()
                     <<std::setw(11)<<std::setprecision(3)<<steppertime
    <<std::endl;
  }
}


template <class Scalar>
void IntegratorBasic<Scalar>::endIntegrator()
{
  std::string exitStatus;
  if (solutionHistory_->getCurrentState()->getSolutionStatus() ==
      Status::FAILED or integratorStatus_ == Status::FAILED) {
    exitStatus = "Time integration FAILURE!";
  } else {
    integratorStatus_ = Status::PASSED;
    exitStatus = "Time integration complete.";
  }

  integratorTimer_->stop();
  const double runtime = integratorTimer_->totalElapsedTime();
  std::time_t end = std::time(nullptr);
  Teuchos::RCP<Teuchos::FancyOStream> out = this->getOStream();
  Teuchos::OSTab ostab(out,0,"ScreenOutput");
  *out << "============================================================================\n"
       << "  Total runtime = " << runtime << " sec = "
       << runtime/60.0 << " min\n"
       << std::asctime(std::localtime(&end))
       << exitStatus << "\n"
       << std::endl;
}


template <class Scalar>
void IntegratorBasic<Scalar>::parseScreenOutput()
{
  // This has been delayed until timeStepControl has been constructed.

  // Parse output indices
  outputScreenIndices_.clear();
  std::string str =
    integratorPL_->get<std::string>("Screen Output Index List", "");
  std::string delimiters(",");
  std::string::size_type lastPos = str.find_first_not_of(delimiters, 0);
  std::string::size_type pos     = str.find_first_of(delimiters, lastPos);
  while ((pos != std::string::npos) || (lastPos != std::string::npos)) {
    std::string token = str.substr(lastPos,pos-lastPos);
    outputScreenIndices_.push_back(int(std::stoi(token)));
    if(pos==std::string::npos)
      break;

    lastPos = str.find_first_not_of(delimiters, pos);
    pos = str.find_first_of(delimiters, lastPos);
  }

  int outputScreenIndexInterval =
    integratorPL_->get<int>("Screen Output Index Interval", 1000000);
  int outputScreen_i   = timeStepControl_->iStepMin_;
  const int finalIStep = timeStepControl_->iStepMax_;
  while (outputScreen_i <= finalIStep) {
    outputScreenIndices_.push_back(outputScreen_i);
    outputScreen_i += outputScreenIndexInterval;
  }

  // order output indices
  std::sort(outputScreenIndices_.begin(),outputScreenIndices_.end());

  return;
}


template <class Scalar>
void IntegratorBasic<Scalar>::setParameterList(
  const Teuchos::RCP<Teuchos::ParameterList> & tmpPL)
{
  if (tmpPL != Teuchos::null) integratorPL_ = tmpPL;
  integratorPL_->validateParametersAndSetDefaults(*this->getValidParameters());

  Teuchos::readVerboseObjectSublist(&*integratorPL_,this);

  std::string integratorType =
    integratorPL_->get<std::string>("Integrator Type");
  TEUCHOS_TEST_FOR_EXCEPTION( integratorType != "Integrator Basic",
    std::logic_error,
    "Error - Inconsistent Integrator Type for IntegratorBasic\n"
    << "    Integrator Type = " << integratorType << "\n");

  return;
}


/** \brief Create valid IntegratorBasic ParameterList.
 */
template<class Scalar>
Teuchos::RCP<const Teuchos::ParameterList>
IntegratorBasic<Scalar>::getValidParameters() const
{
  static Teuchos::RCP<Teuchos::ParameterList> validPL;

  if (is_null(validPL)) {

    Teuchos::RCP<Teuchos::ParameterList> pl = Teuchos::parameterList();
    Teuchos::setupVerboseObjectSublist(&*pl);

    std::ostringstream tmp;
    tmp << "'Integrator Type' must be 'Integrator Basic'.";
    pl->set("Integrator Type", "Integrator Basic", tmp.str());

    tmp.clear();
    tmp << "Screen Output Index List.  Required to be in TimeStepControl range "
        << "['Minimum Time Step Index', 'Maximum Time Step Index']";
    pl->set("Screen Output Index List", "", tmp.str());
    pl->set("Screen Output Index Interval", 1000000,
      "Screen Output Index Interval (e.g., every 100 time steps");

    pl->set("Stepper Name", "",
      "'Stepper Name' selects the Stepper block to construct (Required).");

    // Solution History
    pl->sublist("Solution History",false,"solutionHistory_docs")
        .disableRecursiveValidation();

    // Time Step Control
    pl->sublist("Time Step Control",false,"solutionHistory_docs")
        .disableRecursiveValidation();

    validPL = pl;
  }
  return validPL;
}


template <class Scalar>
Teuchos::RCP<Teuchos::ParameterList>
IntegratorBasic<Scalar>::getNonconstParameterList()
{
  return(integratorPL_);
}


template <class Scalar>
Teuchos::RCP<Teuchos::ParameterList>
IntegratorBasic<Scalar>::unsetParameterList()
{
  Teuchos::RCP<Teuchos::ParameterList> temp_param_list = integratorPL_;
  integratorPL_ = Teuchos::null;
  return(temp_param_list);
}

/// Non-member constructor
template<class Scalar>
Teuchos::RCP<Tempus::IntegratorBasic<Scalar> > integratorBasic(
  Teuchos::RCP<Teuchos::ParameterList>                     pList,
  const Teuchos::RCP<Thyra::ModelEvaluator<Scalar> >&      model)
{
  Teuchos::RCP<Tempus::IntegratorBasic<Scalar> > integrator =
    Teuchos::rcp(new Tempus::IntegratorBasic<Scalar>(pList, model));
  return(integrator);
}

} // namespace Tempus
#endif // Tempus_IntegratorBasic_impl_hpp
