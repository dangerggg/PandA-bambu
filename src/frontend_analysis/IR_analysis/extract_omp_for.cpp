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
 *              Copyright (c) 2015-2017 Politecnico di Milano
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
 * @file extract_omp_for.cpp
 * @brief Analysis step extracting openmp for
 *
 * @author Marco Lattuada <marco.lattuada@polimi.it>
 *
 */

/// Header include
#include "extract_omp_for.hpp"

///. include
#include "Parameter.hpp"

/// algorithms/loops_detection includes
#include "loop.hpp"
#include "loops.hpp"

/// behavior include
#include "application_manager.hpp"
#include "basic_block.hpp"
#include "function_behavior.hpp"

/// design_flows include
#include "design_flow_graph.hpp"
#include "design_flow_manager.hpp"

/// tree includes
#include "behavioral_helper.hpp"
#include "tree_basic_block.hpp"
#include "tree_manager.hpp"
#include "tree_node.hpp"
#include "tree_reindex.hpp"

ExtractOmpFor::ExtractOmpFor(const application_managerRef _AppM, unsigned int _function_id, const DesignFlowManagerConstRef _design_flow_manager, const ParameterConstRef _parameters)
    : FunctionFrontendFlowStep(_AppM, _function_id, EXTRACT_OMP_FOR, _design_flow_manager, _parameters)
{
   debug_level = parameters->get_class_debug_level(GET_CLASS(*this), DEBUG_LEVEL_NONE);
}

ExtractOmpFor::~ExtractOmpFor()
{
}

const std::unordered_set<std::pair<FrontendFlowStepType, FrontendFlowStep::FunctionRelationship>> ExtractOmpFor::ComputeFrontendRelationships(const DesignFlowStep::RelationshipType relationship_type) const
{
   const auto TM = AppM->get_tree_manager();
   std::unordered_set<std::pair<FrontendFlowStepType, FunctionRelationship>> relationships;
   switch(relationship_type)
   {
      case(DEPENDENCE_RELATIONSHIP):
      {
         relationships.insert(std::pair<FrontendFlowStepType, FunctionRelationship>(LOOPS_ANALYSIS_BAMBU, SAME_FUNCTION));
         break;
      }
      case(INVALIDATION_RELATIONSHIP):
      {
         break;
      }
      case(PRECEDENCE_RELATIONSHIP):
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

void ExtractOmpFor::Initialize()
{
}

bool ExtractOmpFor::HasToBeExecuted() const
{
   return bb_version == 0;
}

DesignFlowStep_Status ExtractOmpFor::InternalExec()
{
   const auto behavioral_helper = function_behavior->CGetBehavioralHelper();
   const auto basic_block_graph = function_behavior->CGetBBGraph(FunctionBehavior::BB);
   const auto basic_block_graph_info = basic_block_graph->CGetBBGraphInfo();
   const auto TM = AppM->get_tree_manager();
   if(behavioral_helper->GetOmpForDegree() or behavioral_helper->IsOmpBodyLoop())
   {
      return DesignFlowStep_Status::UNCHANGED;
   }
   if(debug_level >= DEBUG_LEVEL_PEDANTIC)
   {
      WriteBBGraphDot("BB_Before_" + GetName() + ".dot");
      PrintTreeManager(true);
   }
   bool changed = false;
   const auto loops = function_behavior->CGetLoops();
   for(const auto loop : loops->GetList())
   {
      INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "-->Analyzing loop " + STR(loop->GetId()));
      if(not(loop->loop_type & DOALL_LOOP))
      {
         INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--Skipped loop " + STR(loop->GetId()));
         continue;
      }
      std::unordered_set<vertex> basic_blocks;
      loop->get_recursively_bb(basic_blocks);
      THROW_ASSERT(basic_blocks.size() >= 2, "Unexpected pattern");
      if(basic_blocks.size() == 2)
      {
         for(const auto basic_block : basic_blocks)
         {
            if(basic_block == loop->GetHeader())
            {
               continue;
            }
            const auto bb_node_info = basic_block_graph->CGetBBNodeInfo(basic_block);
            INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "-->Analyzing BB" + STR(bb_node_info->block->number));
            if(bb_node_info->block->CGetStmtList().size() == 1)
            {
               THROW_ERROR("BB" + STR(bb_node_info->block->number) + " has only one statement");
            }
            else if(bb_node_info->block->CGetStmtList().size() == 2)
            {
               const auto list_of_stmt = bb_node_info->block->CGetStmtList();
               const auto first_node = list_of_stmt.front();
               const auto call = GetPointer<const gimple_call>(GET_NODE(first_node));
               if(not call)
               {
                  THROW_ERROR("First operation of loop body is " + first_node->ToString());
               }
               const auto second_node = list_of_stmt.back();
               const auto ga = GetPointer<const gimple_assign>(GET_NODE(second_node));
               if(not ga)
               {
                  THROW_ERROR("Second operation of loop body is " + second_node->ToString());
               }
               const auto pe = GetPointer<const plus_expr>(GET_NODE(ga->op1));
               if(not pe)
               {
                  THROW_ERROR("Second operation of loop body is " + second_node->ToString());
               }
               const auto induction_variable = GetPointer<const ssa_name>(GET_NODE(pe->op0));
               THROW_ASSERT(loop->main_iv, "");
               if(not induction_variable or induction_variable->index != loop->main_iv)
               {
                  THROW_ERROR("Induction variable is " + TM->get_tree_node_const(loop->main_iv)->ToString() + " - Second operation of loop body is " + second_node->ToString());
               }
               const auto call_op = GetPointer<const addr_expr>(GET_NODE(call->fn));
               if(not call_op)
               {
                  THROW_ERROR("First operation of loop body is " + first_node->ToString());
               }
               auto called_function = GetPointer<function_decl>(GET_NODE(call_op->op));
               if(not called_function)
               {
                  THROW_ERROR("First operation of loop body is " + first_node->ToString());
               }
               called_function->omp_body_loop = true;
            }
            else
            {
               THROW_ERROR("Pattern not supported");
            }
            INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--Analyzed BB" + STR(bb_node_info->block->number));
         }
         if(loops->GetList().size() == 2)
         {
            VertexIterator basic_block, basic_block_end;
            for(boost::tie(basic_block, basic_block_end) = boost::vertices(*basic_block_graph); basic_block != basic_block_end; basic_block++)
            {
               const auto basic_block_info = basic_block_graph->CGetBBNodeInfo(*basic_block);
               if(*basic_block == basic_block_graph_info->entry_vertex or *basic_block == basic_block_graph_info->exit_vertex)
                  continue;
               if(basic_block_info->loop_id)
                  continue;
               if(not basic_block_info->block->CGetStmtList().size())
                  continue;
               for(const auto statement : basic_block_info->block->CGetStmtList())
               {
                  const auto gr = GetPointer<const gimple_return>(GET_NODE(statement));
                  if(gr and not gr->op)
                  {
                     continue;
                  }
                  THROW_ERROR(statement->ToString());
               }
            }
            auto fd = GetPointer<function_decl>(TM->get_tree_node_const(function_id));
            function_behavior->UpdateBBVersion();
            fd->omp_for_wrapper = parameters->getOption<size_t>(OPT_num_threads);
            INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--Skipped loop " + STR(loop->GetId()));
            break;
         }
      }
      THROW_ERROR("To be implemented");
      INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--Analyzed loop " + STR(loop->GetId()));
   }
   return changed ? DesignFlowStep_Status::SUCCESS : DesignFlowStep_Status::UNCHANGED;
}
