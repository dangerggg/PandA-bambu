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
 *              Copyright (C) 2004-2020 Politecnico di Milano
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
 * @file memory.cpp
 * @brief Datastructure to describe functions allocation in high-level synthesis
 *
 *
 * @author Fabrizio Ferrandi <fabrizio.ferrandi@polimi.it>
 * $Revision$
 * $Date$
 * Last modified by $Author$
 *
 */
#include "functions.hpp"
#include "exceptions.hpp"
#include "string_manipulation.hpp" //STR

functions::functions() = default;

functions::~functions() = default;

void functions::map_shared_function(unsigned int funID_scope, const std::string& fun)
{
   shared_functions[funID_scope].insert(fun);
   THROW_ASSERT(proxied_functions.find(fun) == proxied_functions.end(), "function already mapped in a different scope: " + fun + "->" + STR(proxied_functions.at(fun)));
   proxied_functions[fun] = funID_scope;
}

const CustomOrderedSet<std::string>& functions::get_shared_functions(unsigned int funID_scope) const
{
   return shared_functions.at(funID_scope);
}

bool functions::has_shared_functions(unsigned int funID_scope) const
{
   return shared_functions.find(funID_scope) != shared_functions.end();
}

bool functions::is_a_shared_function(unsigned int funID_scope, const std::string& fun) const
{
   return (has_shared_functions(funID_scope) && shared_functions.at(funID_scope).find(fun) != shared_functions.at(funID_scope).end());
}

void functions::add_shared_function_proxy(unsigned int funID_scope, const std::string& fun)
{
   shared_function_proxy[funID_scope].insert(fun);
}

const CustomOrderedSet<std::string>& functions::get_proxied_shared_functions(unsigned int funID_scope) const
{
   THROW_ASSERT(has_proxied_shared_functions(funID_scope), "No proxy functions for " + STR(funID_scope));
   return shared_function_proxy.at(funID_scope);
}

bool functions::has_proxied_shared_functions(unsigned int funID_scope) const
{
   return shared_function_proxy.find(funID_scope) != shared_function_proxy.end();
}

bool functions::is_a_proxied_shared_function(unsigned int funID_scope, const std::string& fun) const
{
   return (has_proxied_shared_functions(funID_scope) && shared_function_proxy.at(funID_scope).find(fun) != shared_function_proxy.at(funID_scope).end());
}

bool functions::is_a_proxied_function(const std::string& fun) const
{
   return proxied_functions.find(fun) != proxied_functions.end();
}

unsigned int functions::get_proxy_mapping(const std::string& fun) const
{
   THROW_ASSERT(proxied_functions.find(fun) != proxied_functions.end(), "this is not a proxy function");
   return proxied_functions.at(fun);
}
