#ifndef Tempus_Types_hpp
#define Tempus_Types_hpp

namespace Tempus {


/** \brief Status for the Integrator, the Stepper and the SolutionState */
enum Status {
  PASSED,
  FAILED,
  WORKING
};


/** \brief Convert Status to string. */
inline
const char* toString(const Status status)
{
  switch(status) {
    case PASSED:
      return "PASSED";
    case FAILED:
      return "FAILED";
    case WORKING:
      return "WORKING";
    default:
      TEUCHOS_TEST_FOR_EXCEPT("Invalid Status!");
  }
  return 0; // Should never get here!
}


} // namespace Tempus
#endif // Tempus_Types_hpp
