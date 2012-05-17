#include "Panzer_GlobalEvaluationDataContainer.hpp"

namespace panzer {

GlobalEvaluationData::~GlobalEvaluationData() {}

/** Add a data object to be used in evaluation loop.
  */
void GlobalEvaluationDataContainer::addDataObject(const std::string & key,
                                                  const Teuchos::RCP<GlobalEvaluationData> & ged)
{
   lookupTable_[key] = ged;
}

/** Does this containe have a match to a certain key.
  */
bool GlobalEvaluationDataContainer::containsDataObject(const std::string & key) const
{
   return lookupTable_.find(key)!=lookupTable_.end();
}

/** Get the data object associated with the key.
  */
Teuchos::RCP<GlobalEvaluationData> GlobalEvaluationDataContainer::getDataObject(const std::string & key) const
{
   boost::unordered_map<std::string,Teuchos::RCP<GlobalEvaluationData> >::const_iterator itr = lookupTable_.find(key); 
   TEST_FOR_EXCEPTION(itr==lookupTable_.end(),std::logic_error,
                      "In GlobalEvaluationDataContainer::getDataObject(key) failed to find the data object specified by \""+key+"\"");

   return itr->second; 
}

}
