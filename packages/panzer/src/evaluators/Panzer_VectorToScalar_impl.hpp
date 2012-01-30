#ifndef PANZER_VECTOR_TO_SCALAR_IMPL_HPP
#define PANZER_VECTOR_TO_SCALAR_IMPL_HPP

#include <string>

namespace panzer {

//**********************************************************************
PHX_EVALUATOR_CTOR(VectorToScalar,p)
{
  Teuchos::RCP<PHX::DataLayout> scalar_dl = 
    p.get< Teuchos::RCP<PHX::DataLayout> >("Data Layout Scalar");

  Teuchos::RCP<PHX::DataLayout> vector_dl = 
    p.get< Teuchos::RCP<PHX::DataLayout> >("Data Layout Vector");

  const std::vector<std::string>& scalar_names = 
    *(p.get< Teuchos::RCP<const std::vector<std::string> > >("Scalar Names"));

  scalar_fields.resize(scalar_names.size());
  for (std::size_t i=0; i < scalar_names.size(); ++i)
    scalar_fields[i] = 
      PHX::MDField<ScalarT,Cell,Point>(scalar_names[i], scalar_dl);

  vector_field = 
    PHX::MDField<ScalarT,Cell,Point,Dim>(p.get<std::string>
					 ("Vector Name"), vector_dl);

  this->addDependentField(vector_field);
  
  for (std::size_t i=0; i < scalar_fields.size(); ++i)
    this->addEvaluatedField(scalar_fields[i]);
  
  std::string n = "VectorToScalar: " + vector_field.fieldTag().name();
  this->setName(n);
}

//**********************************************************************
PHX_POST_REGISTRATION_SETUP(VectorToScalar,worksets,fm)
{
  for (std::size_t i=0; i < scalar_fields.size(); ++i)
    this->utils.setFieldData(scalar_fields[i],fm);

  this->utils.setFieldData(vector_field,fm);
}

//**********************************************************************
PHX_EVALUATE_FIELDS(VectorToScalar,workset)
{ 

  typedef typename PHX::MDField<ScalarT,Cell,Point>::size_type size_type;

  // Loop over cells
  for (std::size_t cell = 0; cell < workset.num_cells; ++cell) {

    // Loop over points
    for (size_type pt = 0; pt < vector_field.dimension(1); ++pt) {
      
      // Loop over scalars
      for (std::size_t sc = 0; sc < scalar_fields.size(); ++sc) {
      
	scalar_fields[sc](cell,pt) = vector_field(cell,pt,sc);

      }
    }
  }
}

//**********************************************************************

}

#endif
