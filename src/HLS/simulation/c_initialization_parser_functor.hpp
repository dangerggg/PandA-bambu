/*
 *
 *                   _/_/_/    _/_/   _/    _/ _/_/_/    _/_/
 *                  _/   _/ _/    _/ _/_/  _/ _/   _/ _/    _/
 *                 _/_/_/  _/_/_/_/ _/  _/_/ _/   _/ _/_/_/_/
 *                _/      _/    _/ _/    _/ _/   _/ _/    _/
 *               _/      _/    _/ _/    _/ _/_/_/  _/    _/
 *
 *             ***********************************************
 *                              PandA Project
 *                     URL: http://panda.dei.polimi.it
 *                       Politecnico di Milano - DEIB
 *                        System Architectures Group
 *             ***********************************************
 *              Copyright (c) 2018-2020 Politecnico di Milano
 *
 *   This file is part of the PandA framework.
 *
 *   The PandA framework is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
/**
 * @file c_initialization_parser_functor.hpp
 * @brief Specification of the abstract functor used during parsing of C initialization string
 *
 * @author Marco Lattuada <marco.lattuada@polimi.it>
 *
 */
#ifndef C_INITIALIZATION_PARSER_FUNCTOR_HPP
#define C_INITIALIZATION_PARSER_FUNCTOR_HPP

/// STD include
#include <string>

/// utility include
#include "refcount.hpp"

/**
 * Abstract functor used during parsing of C initialization string
 */
class CInitializationParserFunctor
{
 protected:
   /// The debug level
   int debug_level;

 public:
   /**
    * Destructor
    */
   virtual ~CInitializationParserFunctor();

   /**
    * Check that all the necessary information was present in the initialization string
    */
   virtual void CheckEnd() = 0;

   /**
    * Start the initialization of a new aggregated data structure
    */
   virtual void GoDown() = 0;

   /**
    * Consume an element of an aggregated data structure
    */
   virtual void GoNext() = 0;

   /**
    * Ends the initialization of the current aggregated  data structure
    */
   virtual void GoUp() = 0;

   /**
    * Process an element
    * @param content is the string assocated with the string
    */
   virtual void Process(const std::string& content) = 0;
};
typedef refcount<CInitializationParserFunctor> CInitializationParserFunctorRef;
#endif
