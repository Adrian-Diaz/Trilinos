// @HEADER
// ****************************************************************************
//                Tempus: Copyright (2017) Sandia Corporation
//
// Distributed under BSD 3-clause license (See accompanying file Copyright.txt)
// ****************************************************************************
// @HEADER

#include "Teuchos_UnitTestHarness.hpp"
#include "Teuchos_ParameterList.hpp"

#include "Tempus_StepperFactory.hpp"
#include "Tempus_UtilsUnitTest.hpp"

namespace Tempus_Unit_Test {

using Teuchos::RCP;
using Teuchos::rcp;
using Teuchos::rcp_const_cast;
using Teuchos::ParameterList;

// Comment out any of the following tests to exclude from build/run.
#define INITIALIZE


#ifdef INITIALIZE
// ************************************************************
// ************************************************************
TEUCHOS_UNIT_TEST(BDF2UnitTest, initialize)
{
  // Default construction.
  auto stepper = rcp(new Tempus::StepperBDF2<double>());
  TEST_ASSERT(!(stepper->isInitialized()));

  auto model = rcp(new Tempus_Test::SinCosModel<double>());
  auto obs   = rcp(new Tempus::StepperBDF2Observer<double>());

  StepperInitializeBasic(model, stepper, obs, out, success);

  // Start up Stepper
  stepper->setStartUpStepper();      TEST_ASSERT(!(stepper->isInitialized()));
  stepper->initialize();             TEST_ASSERT(stepper->isInitialized());
}
#endif // INITIALIZE

} // namespace Tempus_Test
