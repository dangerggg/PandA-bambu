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
 *              Copyright (c) 2004-2017 Politecnico di Milano
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
 * @file host_profiling.cpp
 * @brief Analysis step performing profiling of loops, paths or both
 *
 * @author Christian Pilato <pilato@elet.polimi.it>
 * @author Marco Lattuada <marco.lattuada@polimi.it>
 *
*/

///Autoheader include
#include "config_HAVE_POLIXML_BUILT.hpp"

///Header include
#include "host_profiling.hpp"

///Behavior include
#include "behavioral_helper.hpp"
#include "application_manager.hpp"
#include "function_behavior.hpp"
#include "loop.hpp"
#include "loops.hpp"
#include "profiling_information.hpp"

///Constants include
#include "host_profiling_xml.hpp"

///Frontend include
#include "Parameter.hpp"

///Graph include
#include "basic_block.hpp"
#include "graph.hpp"

///STD include
#include <unistd.h>
#include <cerrno>

///STL include
#include <unordered_map>

///Utility include
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/convenience.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/cast.hpp>
#include "utility.hpp"

HostProfiling_Method operator&(const HostProfiling_Method first, const HostProfiling_Method second)
{
   return static_cast<HostProfiling_Method>(static_cast<int>(first) | static_cast<int>(second));
}

HostProfiling::HostProfiling(const application_managerRef _AppM, const DesignFlowManagerConstRef _design_flow_manager, const ParameterConstRef _parameters) :
   ApplicationFrontendFlowStep(_AppM, HOST_PROFILING, _design_flow_manager, _parameters)
{
   debug_level = parameters->get_class_debug_level(GET_CLASS(*this), DEBUG_LEVEL_NONE);
}

HostProfiling::~HostProfiling()
{
}

const std::unordered_set<std::pair<FrontendFlowStepType, FrontendFlowStep::FunctionRelationship> > HostProfiling::ComputeFrontendRelationships(const DesignFlowStep::RelationshipType relationship_type) const
{
   std::unordered_set<std::pair<FrontendFlowStepType, FunctionRelationship> > relationships;
   switch(relationship_type)
   {
      case(DEPENDENCE_RELATIONSHIP) :
      {
         const HostProfiling_Method profiling_method = parameters->getOption<HostProfiling_Method>(OPT_profiling_method);
         THROW_ASSERT(profiling_method != HostProfiling_Method::PM_NONE, "Host profiilng required but algorithm has not been selected");
         if(static_cast<int>(profiling_method) & static_cast<int>(HostProfiling_Method::PM_BBP))
         {
            relationships.insert(std::pair<FrontendFlowStepType, FunctionRelationship>(BASIC_BLOCKS_PROFILING, WHOLE_APPLICATION));
         }
#if HAVE_ZEBU_BUILT
         if(static_cast<int>(profiling_method) & static_cast<int>(HostProfiling_Method::PM_PATH_PROBABILITY))
         {
            relationships.insert(std::pair<FrontendFlowStepType, FunctionRelationship>(PROBABILITY_PATH, WHOLE_APPLICATION));
         }
#endif
         if(static_cast<int>(profiling_method) & static_cast<int>(HostProfiling_Method::PM_HPP))
         {
            relationships.insert(std::pair<FrontendFlowStepType, FunctionRelationship>(HPP_PROFILING, WHOLE_APPLICATION));
         }
         if(static_cast<int>(profiling_method) & static_cast<int>(HostProfiling_Method::PM_TP))
         {
            relationships.insert(std::pair<FrontendFlowStepType, FunctionRelationship>(TP_PROFILING, WHOLE_APPLICATION));
         }
         if(static_cast<int>(profiling_method) & static_cast<int>(HostProfiling_Method::PM_MAX_LOOP_ITERATIONS))
         {
            relationships.insert(std::pair<FrontendFlowStepType, FunctionRelationship>(LOOPS_PROFILING, WHOLE_APPLICATION));
         }
         if(static_cast<int>(profiling_method) & static_cast<int>(HostProfiling_Method::PM_XML_FILE))
         {
            relationships.insert(std::pair<FrontendFlowStepType, FunctionRelationship>(READ_PROFILING_DATA, WHOLE_APPLICATION));
         }
         break;
      }
      case(INVALIDATION_RELATIONSHIP) :
      case(PRECEDENCE_RELATIONSHIP) :
      {
         break;
      }
      default:
      {
         THROW_UNREACHABLE("");
      }
   }
   return relationships;
}

DesignFlowStep_Status HostProfiling::Exec()
{
   return DesignFlowStep_Status::EMPTY;
}

void HostProfiling::normalize(const application_managerRef AppM, const std::unordered_map<unsigned int, std::unordered_map<unsigned int, unsigned long long> > & loop_instances, const ParameterConstRef parameters)
{
#ifndef NDEBUG
   const int debug_level = parameters->get_class_debug_level("HostProfiling");
#endif
   INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "-->Normalizing loop iteration");
   //Iterating over all functions with body
   for (const auto f : AppM->get_functions_with_body())
   {
      //Normalizing basic block execution time
      //First computing number of execution of the function
      //number of function execution
      const FunctionBehaviorRef FB = AppM->GetFunctionBehavior(f);
      INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "-->Function: " + FB->CGetBehavioralHelper()->get_function_name());

      //Normalizing loop number of iteration and frequency
      const std::list<LoopConstRef> & loops = FB->CGetLoops()->GetList();
      std::list<LoopConstRef>::const_iterator loop, loop_end = loops.end();
      for(loop = loops.begin(); loop != loop_end; loop++)
      {
         unsigned int loop_id = (*loop)->GetId();
         ///FIXME: zero loop
         if(loop_id == 0)
            continue;
         long double avg_number = 0.0;
         long double abs_execution = 0.0;
         PathProfilingInformation & path_profiling = FB->profiling_information->path_profiling;
         if(path_profiling.find(loop_id) == path_profiling.end())
            continue;
         INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "-->Loop: " + boost::lexical_cast<std::string>(loop_id));
         for(std::map<std::set<unsigned int>, long double>::const_iterator k = path_profiling.find(loop_id)->second.begin(); k != path_profiling.find(loop_id)->second.end(); k++)
         {
            abs_execution += k->second;
         }
         if(abs_execution != 0.0)
         {
            if(loop_instances.find(f) == loop_instances.end())
            {
               THROW_ERROR_CODE(PROFILING_EC, "Function " + FB->CGetBehavioralHelper()->get_function_name() + " exited abnormally");
            }
            THROW_ASSERT(loop_instances.at(f).find(loop_id) != loop_instances.at(f).end(), "Loop " + boost::lexical_cast<std::string>(f) + " is no executed");
            THROW_ASSERT(loop_instances.at(f).at(loop_id) != 0, "Loop " + boost::lexical_cast<std::string>(loop_id) + " of function " + boost::lexical_cast<std::string>(f) + " is executed but does not exist an external path with its header");
            avg_number = abs_execution / loop_instances.at(f).at(loop_id);
         }
         INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "-->Avg. Number Executions: " + boost::lexical_cast<std::string>(avg_number));
         FB->profiling_information->avg_iterations[(*loop)->GetId()] = avg_number;
         FB->profiling_information->abs_iterations[(*loop)->GetId()] = static_cast<unsigned long long int>(llroundl(abs_execution));
         INDENT_DBG_MEX(DEBUG_LEVEL_PEDANTIC, debug_level, "Factor: " + boost::lexical_cast<std::string>(abs_execution));
         for(std::map<std::set<unsigned int>, long double>::iterator k = path_profiling.at(loop_id).begin(); k != path_profiling.at(loop_id).end(); k++)
         {
            INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "-->New path");
            INDENT_DBG_MEX(DEBUG_LEVEL_PEDANTIC, debug_level, "---Absolute path: " + boost::lexical_cast<std::string>(k->second));
            k->second /= abs_execution;
            INDENT_DBG_MEX(DEBUG_LEVEL_PEDANTIC, debug_level, "---Relative path: " + boost::lexical_cast<std::string>(k->second));
            INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--");
         }
         INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--");
         INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--");
      }
      if(parameters->getOption<bool>(OPT_print_dot))
      {
         FB->CGetLoops()->WriteDot("LF.dot", FB->CGetProfilingInformation());
      }
      INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--");
   }
   INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--Normalized loop iteration");
}
