// Copyright 2002 - 2008, 2010, 2011 National Technology Engineering
// Solutions of Sandia, LLC (NTESS). Under the terms of Contract
// DE-NA0003525 with NTESS, the U.S. Government retains certain rights
// in this software.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
// 
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
// 
//     * Neither the name of NTESS nor the names of its contributors
//       may be used to endorse or promote products derived from this
//       software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// 


#include "stk_util/util/FArray.hpp"
#include <sstream>    // for operator<<, basic_ostream, ostringstream, basic_ostream<>::__ostrea...
#include <stdexcept>  // for range_error
#include <string>     // for char_traits, basic_string

namespace sierra {

// Force inclusion of array_dimension_error by linker
FArrayBootstrap::~FArrayBootstrap()
{
}

namespace {

const char *ordinal[] = {"first", "second", "third", "fourth", "fifth", "sixth", "seventh", "eighth"};

} // namespace <unnamed>


void
array_dimension_error(
  const std::type_info &	type,
  unsigned			dimension,
  unsigned			value,
  unsigned			upper)
{
  std::ostringstream os ;
  os << type.name() << " ";
  if (dimension >= sizeof(ordinal)/sizeof(ordinal[0]))
    os << dimension;
  else
    os << ordinal[dimension];
  os << " dimension value " << value
     << " is out of bounds (0:" << upper - 1 << ")";

  throw std::range_error(os.str().c_str());
}

} // namespace sierra
