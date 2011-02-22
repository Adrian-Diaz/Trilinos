
#include <vector>
#include <string>
#include <iostream>
#include "Panzer_InputPhysicsBlock.hpp"
#include "Panzer_InputEquationSet.hpp"

std::ostream& 
panzer::operator<<(std::ostream& os, const panzer::InputPhysicsBlock& i) {
  
  os << "Physics Block: " << i.physics_block_id 
     << ", Num Equation Sets: " << i.eq_sets.size() << std::endl;
  for (std::size_t k=0; k < i.eq_sets.size(); ++k) {
    os << "  Set " << k << std::endl;
    os << "    name = " << i.eq_sets[k].name << std::endl;
    os << "    basis = " << i.eq_sets[k].basis << std::endl;
    os << "    int order = " << i.eq_sets[k].integration_order << std::endl;
    os << "    model = " << i.eq_sets[k].model_id << std::endl;
    os << "    prefix = " << i.eq_sets[k].prefix << std::endl;
    os << "    parameterlist: " << std::endl;

    Teuchos::ParameterList::PrintOptions po;
    po.showTypes(true);
    po.indent(6);
    i.eq_sets[k].params.print(os,po);
  }
  return os;
}
