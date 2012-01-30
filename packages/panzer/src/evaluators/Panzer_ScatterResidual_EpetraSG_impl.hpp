#ifndef PANZER_SCATTER_RESIDUAL_EPETRA_SG_IMPL_HPP
#define PANZER_SCATTER_RESIDUAL_EPETRA_SG_IMPL_HPP

#include "Panzer_config.hpp"
#ifdef HAVE_STOKHOS

#include "Panzer_SGEpetraLinearObjContainer.hpp"

#include "Epetra_Vector.h"
#include "Epetra_Map.h"
#include "Epetra_CrsMatrix.h"

// **********************************************************************
// Specialization: SGResidual
// **********************************************************************

template<typename Traits,typename LO,typename GO>
panzer::ScatterResidual_Epetra<panzer::Traits::SGResidual, Traits,LO,GO>::
ScatterResidual_Epetra(const Teuchos::RCP<const panzer::UniqueGlobalIndexer<LO,GO> > & indexer,
                       const Teuchos::ParameterList& p)
  : globalIndexer_(indexer) 
{ 
  std::string scatterName = p.get<std::string>("Scatter Name");
  scatterHolder_ = 
    Teuchos::rcp(new PHX::Tag<ScalarT>(scatterName,Teuchos::rcp(new PHX::MDALayout<Dummy>(0))));

  // get names to be evaluated
  const std::vector<std::string>& names = 
    *(p.get< Teuchos::RCP< std::vector<std::string> > >("Dependent Names"));

  // grab map from evaluated names to field names
  fieldMap_ = p.get< Teuchos::RCP< std::map<std::string,std::string> > >("Dependent Map");

  Teuchos::RCP<PHX::DataLayout> dl = 
    p.get< Teuchos::RCP<const panzer::PureBasis> >("Basis")->functional;
  
  // build the vector of fields that this is dependent on
  scatterFields_.resize(names.size());
  for (std::size_t eq = 0; eq < names.size(); ++eq) {
    scatterFields_[eq] = PHX::MDField<ScalarT,Cell,NODE>(names[eq],dl);

    // tell the field manager that we depend on this field
    this->addDependentField(scatterFields_[eq]);
  }

  // this is what this evaluator provides
  this->addEvaluatedField(*scatterHolder_);

  this->setName(scatterName+" Scatter Residual (SGResidual)");
}

// **********************************************************************
template<typename Traits,typename LO,typename GO> 
void panzer::ScatterResidual_Epetra<panzer::Traits::SGResidual, Traits,LO,GO>::
postRegistrationSetup(typename Traits::SetupData d, 
		      PHX::FieldManager<Traits>& fm)
{
  // globalIndexer_ = d.globalIndexer_;

  fieldIds_.resize(scatterFields_.size());
  // load required field numbers for fast use
  for(std::size_t fd=0;fd<scatterFields_.size();++fd) {
    // get field ID from DOF manager
    std::string fieldName = fieldMap_->find(scatterFields_[fd].fieldTag().name())->second;
    fieldIds_[fd] = globalIndexer_->getFieldNum(fieldName);

    // fill field data object
    this->utils.setFieldData(scatterFields_[fd],fm);
  }
}

// **********************************************************************
template<typename Traits,typename LO,typename GO>
void panzer::ScatterResidual_Epetra<panzer::Traits::SGResidual, Traits,LO,GO>::
evaluateFields(typename Traits::EvalData workset)
{ 
   std::vector<GO> GIDs;
   std::vector<int> LIDs;
 
   // for convenience pull out some objects from workset
   std::string blockId = workset.block_id;
   const std::vector<std::size_t> & localCellIds = workset.cell_local_ids;

   Teuchos::RCP<SGEpetraLinearObjContainer> sgEpetraContainer 
         = Teuchos::rcp_dynamic_cast<SGEpetraLinearObjContainer>(workset.ghostedLinContainer);
   Teuchos::RCP<Epetra_Vector> r_template = (*sgEpetraContainer->begin())->f;
   const Epetra_BlockMap & map = r_template->Map();

   // NOTE: A reordering of these loops will likely improve performance
   //       The "getGIDFieldOffsets may be expensive.  However the
   //       "getElementGIDs" can be cheaper. However the lookup for LIDs
   //       may be more expensive!

   // scatter operation for each cell in workset
   for(std::size_t worksetCellIndex=0;worksetCellIndex<localCellIds.size();++worksetCellIndex) {
      std::size_t cellLocalId = localCellIds[worksetCellIndex];

      globalIndexer_->getElementGIDs(cellLocalId,GIDs,blockId); 

      // caculate the local IDs for this element
      LIDs.resize(GIDs.size());
      for(std::size_t i=0;i<GIDs.size();i++)
         LIDs[i] = map.LID(GIDs[i]);

      // loop over each field to be scattered
      for (std::size_t fieldIndex = 0; fieldIndex < scatterFields_.size(); fieldIndex++) {
         int fieldNum = fieldIds_[fieldIndex];
         const std::vector<int> & elmtOffset = globalIndexer_->getGIDFieldOffsets(blockId,fieldNum);
   
         // loop over basis functions
         for(std::size_t basis=0;basis<elmtOffset.size();basis++) {
            int offset = elmtOffset[basis];
            int lid = LIDs[offset];

            const ScalarT & field = (scatterFields_[fieldIndex])(worksetCellIndex,basis);

            // loop over stochastic basis scatter field values to residual vectors
            int stochIndex = 0;
            panzer::SGEpetraLinearObjContainer::iterator itr; 
            for(itr=sgEpetraContainer->begin();itr!=sgEpetraContainer->end();++itr,++stochIndex)
               (*(*itr)->f)[lid] += field.coeff(stochIndex);
         }
      }
   }
}

// **********************************************************************
// Specialization: SGJacobian
// **********************************************************************

template<typename Traits,typename LO,typename GO>
panzer::ScatterResidual_Epetra<panzer::Traits::SGJacobian, Traits,LO,GO>::
ScatterResidual_Epetra(const Teuchos::RCP<const UniqueGlobalIndexer<LO,GO> > & indexer,
                       const Teuchos::ParameterList& p)
   : globalIndexer_(indexer)
{ 
  std::string scatterName = p.get<std::string>("Scatter Name");
  scatterHolder_ = 
    Teuchos::rcp(new PHX::Tag<ScalarT>(scatterName,Teuchos::rcp(new PHX::MDALayout<Dummy>(0))));

  // get names to be evaluated
  const std::vector<std::string>& names = 
    *(p.get< Teuchos::RCP< std::vector<std::string> > >("Dependent Names"));

  // grab map from evaluated names to field names
  fieldMap_ = p.get< Teuchos::RCP< std::map<std::string,std::string> > >("Dependent Map");

  Teuchos::RCP<PHX::DataLayout> dl = 
    p.get< Teuchos::RCP<const panzer::PureBasis> >("Basis")->functional;
  
  // build the vector of fields that this is dependent on
  scatterFields_.resize(names.size());
  for (std::size_t eq = 0; eq < names.size(); ++eq) {
    scatterFields_[eq] = PHX::MDField<ScalarT,Cell,NODE>(names[eq],dl);

    // tell the field manager that we depend on this field
    this->addDependentField(scatterFields_[eq]);
  }

  // this is what this evaluator provides
  this->addEvaluatedField(*scatterHolder_);

  this->setName(scatterName+" Scatter Residual (SGJacobian)");
}

// **********************************************************************
template<typename Traits,typename LO,typename GO> 
void panzer::ScatterResidual_Epetra<panzer::Traits::SGJacobian, Traits,LO,GO>::
postRegistrationSetup(typename Traits::SetupData d,
		      PHX::FieldManager<Traits>& fm)
{
  // globalIndexer_ = d.globalIndexer_;

  fieldIds_.resize(scatterFields_.size());
  // load required field numbers for fast use
  for(std::size_t fd=0;fd<scatterFields_.size();++fd) {
    // get field ID from DOF manager
    std::string fieldName = fieldMap_->find(scatterFields_[fd].fieldTag().name())->second;
    fieldIds_[fd] = globalIndexer_->getFieldNum(fieldName);

    // fill field data object
    this->utils.setFieldData(scatterFields_[fd],fm);
  }
}

// **********************************************************************
template<typename Traits,typename LO,typename GO>
void panzer::ScatterResidual_Epetra<panzer::Traits::SGJacobian, Traits,LO,GO>::
evaluateFields(typename Traits::EvalData workset)
{ 
   std::vector<GO> GIDs;
   std::vector<int> rLIDs, cLIDs;
   std::vector<double> jacRow;
 
   // for convenience pull out some objects from workset
   std::string blockId = workset.block_id;
   const std::vector<std::size_t> & localCellIds = workset.cell_local_ids;

   Teuchos::RCP<SGEpetraLinearObjContainer> sgEpetraContainer 
         = Teuchos::rcp_dynamic_cast<SGEpetraLinearObjContainer>(workset.ghostedLinContainer);
   Teuchos::RCP<Epetra_CrsMatrix> Jac_template = (*sgEpetraContainer->begin())->A;
   const Epetra_BlockMap & rMap = Jac_template->RowMap();
   const Epetra_BlockMap & cMap = Jac_template->ColMap();

   // NOTE: A reordering of these loops will likely improve performance
   //       The "getGIDFieldOffsets" may be expensive.  However the
   //       "getElementGIDs" can be cheaper. However the lookup for LIDs
   //       may be more expensive!

   // scatter operation for each cell in workset
   for(std::size_t worksetCellIndex=0;worksetCellIndex<localCellIds.size();++worksetCellIndex) {
      std::size_t cellLocalId = localCellIds[worksetCellIndex];

      globalIndexer_->getElementGIDs(cellLocalId,GIDs,blockId); 

      // caculate the local IDs for this element
      rLIDs.resize(GIDs.size());
      cLIDs.resize(GIDs.size());
      for(std::size_t i=0;i<GIDs.size();i++) {
         rLIDs[i] = rMap.LID(GIDs[i]);
         cLIDs[i] = cMap.LID(GIDs[i]);
      }

      // loop over each field to be scattered
      for(std::size_t fieldIndex = 0; fieldIndex < scatterFields_.size(); fieldIndex++) {
         int fieldNum = fieldIds_[fieldIndex];
         const std::vector<int> & elmtOffset = globalIndexer_->getGIDFieldOffsets(blockId,fieldNum);
        
         // loop over the basis functions (currently they are nodes)
         for(std::size_t rowBasisNum = 0; rowBasisNum < elmtOffset.size(); rowBasisNum++) {
            const ScalarT & scatterField = (scatterFields_[fieldIndex])(worksetCellIndex,rowBasisNum);
            int rowOffset = elmtOffset[rowBasisNum];
            int row = rLIDs[rowOffset];

            // loop over stochastic basis scatter field values to residual vectors
            int stochIndex = 0;
            panzer::SGEpetraLinearObjContainer::iterator itr; 
            for(itr=sgEpetraContainer->begin();itr!=sgEpetraContainer->end();++itr,++stochIndex) {
               Teuchos::RCP<Epetra_Vector> r = (*itr)->f;
               Teuchos::RCP<Epetra_CrsMatrix> Jac = (*itr)->A;

               // Sum residual
               if(r!=Teuchos::null) {
       	          // r->SumIntoMyValue(row,0,scatterField.val().coeff(stochIndex));
       	          (*r)[row] += scatterField.val().coeff(stochIndex);
               }
       
               // loop over the sensitivity indices: all DOFs on a cell
               jacRow.resize(scatterField.size());
               
               for(int sensIndex=0;sensIndex<scatterField.size();++sensIndex)
                  jacRow[sensIndex] = scatterField.fastAccessDx(sensIndex).coeff(stochIndex);
               // TEUCHOS_ASSERT(scatterField.size()==(int) LIDs.size());
       
               // Sum SGJacobian
               int err = Jac->SumIntoMyValues(row, scatterField.size(), &jacRow[0],&cLIDs[0]);
               TEUCHOS_ASSERT_EQUALITY(err,0);
            }
         } // end rowBasisNum
      } // end fieldIndex
   }
}

// **********************************************************************

#endif

#endif
