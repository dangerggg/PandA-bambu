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
 *              Copyright (C) 2018-2020 Politecnico di Milano
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
 * @file interface_infer.cpp
 * @brief Restructure the top level function to adhere the specified interface.
 *
 * @author Fabrizio Ferrandi <fabrizio.ferrandi@polimi.it>
 *
 */

/// Header include
#include "interface_infer.hpp"

///. include
#include "Parameter.hpp"

/// design_flows includes
#include "design_flow_graph.hpp"
#include "design_flow_manager.hpp"

/// design_flows/technology includes
#include "technology_flow_step.hpp"
#include "technology_flow_step_factory.hpp"

/// behavior include
#include "application_manager.hpp"
#include "behavioral_helper.hpp"
#include "call_graph.hpp"
#include "call_graph_manager.hpp"
#include "function_behavior.hpp"
#include "hls_manager.hpp"
#include "hls_target.hpp"

/// parser/treegcc include
#include "token_interface.hpp"

/// tree includes
#include "dbgPrintHelper.hpp"      // for DEBUG_LEVEL_
#include "string_manipulation.hpp" // for GET_CLASS
#include "tree_basic_block.hpp"
#include "tree_helper.hpp"
#include "tree_manager.hpp"
#include "tree_manipulation.hpp"
#include "tree_node.hpp"
#include "tree_reindex.hpp"

/// XML includes used for writing and reading the configuration file
#include "polixml.hpp"
#include "xml_dom_parser.hpp"
#include "xml_helper.hpp"

#include "area_model.hpp"
#include "library_manager.hpp"
#include "technology_manager.hpp"
#include "technology_node.hpp"
#include "time_model.hpp"

#include "structural_manager.hpp"
#include "structural_objects.hpp"

#include "constant_strings.hpp"
#include "copyrights_strings.hpp"

#include "language_writer.hpp"

#define EPSILON 0.000000001
#define ENCODE_FDNAME(argName_string, MODE, interfaceType) (argName_string + STR_CST_interface_parameter_keyword + (MODE) + interfaceType)

const CustomUnorderedSet<std::pair<FrontendFlowStepType, FrontendFlowStep::FunctionRelationship>> interface_infer::ComputeFrontendRelationships(const DesignFlowStep::RelationshipType relationship_type) const
{
   CustomUnorderedSet<std::pair<FrontendFlowStepType, FunctionRelationship>> relationships;
   switch(relationship_type)
   {
      case(DEPENDENCE_RELATIONSHIP):
      {
         relationships.insert(std::make_pair(IR_LOWERING, SAME_FUNCTION));
         relationships.insert(std::make_pair(USE_COUNTING, SAME_FUNCTION));
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

void interface_infer::ComputeRelationships(DesignFlowStepSet& relationship, const DesignFlowStep::RelationshipType relationship_type)
{
   switch(relationship_type)
   {
      case(PRECEDENCE_RELATIONSHIP):
      {
         break;
      }
      case DEPENDENCE_RELATIONSHIP:
      {
         const DesignFlowGraphConstRef design_flow_graph = design_flow_manager.lock()->CGetDesignFlowGraph();
         const auto* technology_flow_step_factory = GetPointer<const TechnologyFlowStepFactory>(design_flow_manager.lock()->CGetDesignFlowStepFactory("Technology"));
         const std::string technology_flow_signature = TechnologyFlowStep::ComputeSignature(TechnologyFlowStep_Type::LOAD_TECHNOLOGY);
         const vertex technology_flow_step = design_flow_manager.lock()->GetDesignFlowStep(technology_flow_signature);
         const DesignFlowStepRef technology_design_flow_step =
             technology_flow_step ? design_flow_graph->CGetDesignFlowStepInfo(technology_flow_step)->design_flow_step : technology_flow_step_factory->CreateTechnologyFlowStep(TechnologyFlowStep_Type::LOAD_TECHNOLOGY);
         relationship.insert(technology_design_flow_step);
         break;
      }
      case INVALIDATION_RELATIONSHIP:
      {
         break;
      }
      default:
         THROW_UNREACHABLE("");
   }
   FunctionFrontendFlowStep::ComputeRelationships(relationship, relationship_type);
}

interface_infer::interface_infer(const application_managerRef _AppM, unsigned int _function_id, const DesignFlowManagerConstRef _design_flow_manager, const ParameterConstRef _parameters)
    : FunctionFrontendFlowStep(_AppM, _function_id, INTERFACE_INFER, _design_flow_manager, _parameters)
{
   debug_level = parameters->get_class_debug_level(GET_CLASS(*this), DEBUG_LEVEL_NONE);
}

interface_infer::~interface_infer() = default;

void interface_infer::Computepar2ssa(statement_list* sl, std::map<unsigned, unsigned>& par2ssa)
{
   for(auto block : sl->list_of_bloc)
   {
      for(const auto& stmt : block.second->CGetStmtList())
      {
         TreeNodeMap<size_t> used_ssa = tree_helper::ComputeSsaUses(stmt);
         for(const auto& s : used_ssa)
         {
            const tree_nodeRef ssa_tn = GET_NODE(s.first);
            const auto* ssa = GetPointer<const ssa_name>(ssa_tn);
            if(ssa->var != nullptr and GET_NODE(ssa->var)->get_kind() == parm_decl_K)
            {
               auto par_id = GET_INDEX_NODE(ssa->var);
               if(par2ssa.find(par_id) != par2ssa.end())
               {
                  THROW_ASSERT(par2ssa.find(par_id)->second == GET_INDEX_NODE(s.first), "unexpected condition");
               }
               else
                  par2ssa[par_id] = GET_INDEX_NODE(s.first);
            }
         }
      }
   }
}

void interface_infer::classifyArgRecurse(CustomOrderedSet<unsigned>& Visited, ssa_name* argSSA, unsigned int destBB, statement_list* sl, bool& canBeMovedToBB2, bool& isRead, bool& isWrite, bool& unkwown_pattern, std::list<tree_nodeRef>& writeStmt,
                                         std::list<tree_nodeRef>& readStmt)
{
   tree_nodeRef readType;
   for(auto par_use : argSSA->CGetUseStmts())
   {
      auto use_stmt = GET_NODE(par_use.first);
      if(Visited.find(GET_INDEX_NODE(par_use.first)) != Visited.end())
         continue;
      Visited.insert(GET_INDEX_NODE(par_use.first));
      std::cerr << "STMT: " << use_stmt->ToString() << std::endl;
      auto gn = GetPointer<gimple_node>(use_stmt);
      if(auto ga = GetPointer<gimple_assign>(use_stmt))
      {
         if(GET_NODE(ga->op0)->get_kind() == mem_ref_K)
         {
            if(GET_NODE(ga->op1)->get_kind() == mem_ref_K)
            {
               unkwown_pattern = true;
               THROW_WARNING("Pattern currently not supported: *x=*y; " + use_stmt->ToString());
            }
            else
            {
               THROW_ASSERT(GET_NODE(ga->op1)->get_kind() == ssa_name_K || GetPointer<cst_node>(GET_NODE(ga->op1)), "unexpected condition");
               if(GetPointer<cst_node>(GET_NODE(ga->op1)) || GetPointer<ssa_name>(GET_NODE(ga->op1)) != argSSA)
               {
                  isWrite = true;
                  writeStmt.push_back(par_use.first);
               }
            }
         }
         else if(GET_NODE(ga->op1)->get_kind() == mem_ref_K)
         {
            if(ga->vdef)
            {
               unkwown_pattern = true;
               canBeMovedToBB2 = false;
               THROW_WARNING("Pattern currently not supported: use of a volatile load " + use_stmt->ToString());
            }
            else
            {
               if(sl->list_of_bloc.find(gn->bb_index)->second->loop_id != 0)
                  canBeMovedToBB2 = false;
               isRead = true;
               if(readType && GET_INDEX_NODE(GetPointer<mem_ref>(GET_NODE(ga->op1))->type) != GET_INDEX_NODE(readType))
                  canBeMovedToBB2 = false; /// reading different objects
               readType = GetPointer<mem_ref>(GET_NODE(ga->op1))->type;
               readStmt.push_back(par_use.first);
               if(ga->bb_index == destBB)
                  canBeMovedToBB2 = false;
            }
         }
         else if(GET_NODE(ga->op1)->get_kind() == call_expr_K)
         {
            unkwown_pattern = true;
            canBeMovedToBB2 = false;
            THROW_WARNING("Pattern currently not supported: parameter passed as a parameter to another function " + use_stmt->ToString());
         }
         else if(GET_NODE(ga->op1)->get_kind() == nop_expr_K || GET_NODE(ga->op1)->get_kind() == view_convert_expr_K || GET_NODE(ga->op1)->get_kind() == ssa_name_K || GET_NODE(ga->op1)->get_kind() == pointer_plus_expr_K ||
                 GET_NODE(ga->op1)->get_kind() == cond_expr_K)
         {
            canBeMovedToBB2 = false;
            auto op0SSA = GetPointer<ssa_name>(GET_NODE(ga->op0));
            THROW_ASSERT(argSSA, "unexpected condition");
            classifyArgRecurse(Visited, op0SSA, destBB, sl, canBeMovedToBB2, isRead, isWrite, unkwown_pattern, writeStmt, readStmt);
         }
         else
            THROW_ERROR("Pattern currently not supported: parameter used in a non-supported statement " + use_stmt->ToString() + ":" + GET_NODE(ga->op1)->get_kind_text());
      }
      else if(auto gc = GetPointer<gimple_call>(use_stmt))
      {
         canBeMovedToBB2 = false;
         // look for the actual vs formal parameter binding
#if HAVE_ASSERTS
         bool found = false;
#endif
         unsigned par_index = 0;
         for(auto par : gc->args)
         {
            auto par_node = GET_NODE(par);
            auto ssaPar = GetPointer<ssa_name>(par_node);
            if(ssaPar)
            {
               if(argSSA == ssaPar)
               {
#if HAVE_ASSERTS
                  found = true;
#endif
                  break;
               }
            }
            else
               THROW_ERROR("unexpected pattern: " + par_node->ToString());
            ++par_index;
         }
         THROW_ASSERT(found, "found: " + argSSA->ToString() + " index=" + STR(par_index));
         THROW_ASSERT(gc->fn, "unexpected condition");
         auto fn_node = GET_NODE(gc->fn);
         if(fn_node->get_kind() == addr_expr_K)
         {
            auto ae = GetPointer<addr_expr>(fn_node);
            auto ae_op = GET_NODE(ae->op);
            if(ae_op->get_kind() == function_decl_K)
            {
               auto fd2 = GetPointer<function_decl>(ae_op);
               THROW_ASSERT(fd2->body, "unexpected condition");
               std::map<unsigned, unsigned> par2ssa;
               auto sl2 = GetPointer<statement_list>(GET_NODE(fd2->body));
               Computepar2ssa(sl2, par2ssa);
               unsigned par2_index = 0;
               auto arg_id = 0u;
               std::string pdName_string2;
               for(auto arg : fd2->list_of_args)
               {
                  if(par2_index == par_index)
                  {
                     arg_id = GET_INDEX_NODE(arg);
                     auto pd = GetPointer<parm_decl>(GET_NODE(arg));
                     auto pdName = GET_NODE(pd->name);
                     THROW_ASSERT(GetPointer<identifier_node>(pdName), "unexpected condition");
                     pdName_string2 = GetPointer<identifier_node>(pdName)->strg;
                     break;
                  }
                  ++par2_index;
               }
               THROW_ASSERT(arg_id, "unexpected condition");
               if(par2ssa.find(arg_id) != par2ssa.end())
               {
                  /// propagate design interfaces
                  auto HLSMgr = GetPointer<HLS_manager>(AppM);
                  auto par_node = GET_NODE(argSSA->var);
                  auto pd = GetPointer<parm_decl>(par_node);
                  auto pdName = GET_NODE(pd->name);
                  THROW_ASSERT(GetPointer<identifier_node>(pdName), "unexpected condition");
                  const std::string& pdName_string = GetPointer<identifier_node>(pdName)->strg;
                  auto fun_node = GET_NODE(pd->scpe);
                  THROW_ASSERT(fun_node && fun_node->get_kind() == function_decl_K, "unexpected condition");
                  auto fd = GetPointer<function_decl>(fun_node);
                  std::string fname;
                  tree_helper::get_mangled_fname(fd, fname);
                  if(HLSMgr->design_interface_arraysize.find(fname) != HLSMgr->design_interface_arraysize.end() && HLSMgr->design_interface_arraysize.find(fname)->second.find(pdName_string) != HLSMgr->design_interface_arraysize.find(fname)->second.end())
                  {
                     std::string fname2;
                     tree_helper::get_mangled_fname(fd2, fname2);
                     std::cerr << "fname2=" << fname2 << " pdName_string2=" << pdName_string2 << "\n";
                     std::cerr << "fname=" << fname << " pdName_string=" << pdName_string << "\n";
                     HLSMgr->design_interface_arraysize[fname2][pdName_string2] = HLSMgr->design_interface_arraysize.find(fname)->second.find(pdName_string)->second;
                  }
                  const auto TM = AppM->get_tree_manager();
                  auto argSSANode = TM->GetTreeReindex(par2ssa.find(arg_id)->second);
                  auto argSSA2 = GetPointer<ssa_name>(GET_NODE(argSSANode));
                  THROW_ASSERT(argSSA, "unexpected condition");
                  classifyArgRecurse(Visited, argSSA2, destBB, sl2, canBeMovedToBB2, isRead, isWrite, unkwown_pattern, writeStmt, readStmt);
                  std::cerr << "Sub-function done\n";
               }
            }
            else
               THROW_ERROR("unexpected pattern: " + ae_op->ToString());
         }
         else if(fn_node)
            THROW_ERROR("unexpected pattern: " + fn_node->ToString());
         else
            THROW_ERROR("unexpected pattern");
      }
      else
      {
         unkwown_pattern = true;
         canBeMovedToBB2 = false;
         THROW_WARNING("USE PATTERN unexpected" + use_stmt->ToString());
      }
   }
}

void interface_infer::classifyArg(statement_list* sl, tree_nodeRef argSSANode, bool& canBeMovedToBB2, bool& isRead, bool& isWrite, bool& unkwown_pattern, std::list<tree_nodeRef>& writeStmt, std::list<tree_nodeRef>& readStmt)
{
   unsigned int destBB = bloc::ENTRY_BLOCK_ID;
   for(auto bb_succ : sl->list_of_bloc[bloc::ENTRY_BLOCK_ID]->list_of_succ)
   {
      if(bb_succ == bloc::EXIT_BLOCK_ID)
         continue;
      if(destBB == bloc::ENTRY_BLOCK_ID)
         destBB = bb_succ;
      else
         THROW_ERROR("unexpected pattern");
   }
   THROW_ASSERT(destBB != bloc::ENTRY_BLOCK_ID, "unexpected condition");
   auto argSSA = GetPointer<ssa_name>(GET_NODE(argSSANode));
   THROW_ASSERT(argSSA, "unexpected condition");
   CustomOrderedSet<unsigned> Visited;
   classifyArgRecurse(Visited, argSSA, destBB, sl, canBeMovedToBB2, isRead, isWrite, unkwown_pattern, writeStmt, readStmt);
}

void interface_infer::create_Read_function(tree_nodeRef refStmt, const std::string& argName_string, tree_nodeRef origStmt, unsigned int destBB, const std::string& fdName, tree_nodeRef argSSANode, tree_nodeRef aType, tree_nodeRef readType,
                                           const std::list<tree_nodeRef>& usedStmt_defs, const tree_manipulationRef tree_man, const tree_managerRef TM, bool commonRWSignature)
{
   THROW_ASSERT(refStmt, "expected a ref statement");
   auto gn = GetPointer<gimple_node>(GET_NODE(refStmt));
   THROW_ASSERT(gn, "expected a gimple_node");
   THROW_ASSERT(gn->scpe, "expected a scope");
   THROW_ASSERT(GET_NODE(gn->scpe)->get_kind() == function_decl_K, "expected a function_decl");
   auto fd = GetPointer<function_decl>(GET_NODE(gn->scpe));
   THROW_ASSERT(fd->body, "expected a body");
   auto* sl = GetPointer<statement_list>(GET_NODE(fd->body));
   std::string fname;
   tree_helper::get_mangled_fname(fd, fname);
   /// create the function_decl
   std::vector<tree_nodeRef> argsT;
   tree_nodeRef bit_size_type;
   tree_nodeRef boolean_type;
   if(commonRWSignature)
   {
      boolean_type = tree_man->create_boolean_type();
      bit_size_type = tree_man->create_bit_size_type();
      argsT.push_back(bit_size_type);
      argsT.push_back(bit_size_type);
      argsT.push_back(readType);
   }
   argsT.push_back(aType);
   const std::string srcp = fd->include_name + ":" + STR(fd->line_number) + ":" + STR(fd->column_number);
   auto function_decl_node = tree_man->create_function_decl(fdName, fd->scpe, argsT, readType, srcp, false);

   std::vector<tree_nodeRef> args;
   if(commonRWSignature)
   {
      const auto sel_value_id = TM->new_tree_node_id();
      const auto sel_value = tree_man->CreateIntegerCst(boolean_type, 0, sel_value_id);
      args.push_back(sel_value);
      const auto size_value_id = TM->new_tree_node_id();
      const auto size_value = tree_man->CreateIntegerCst(bit_size_type, tree_helper::Size(readType), size_value_id);
      args.push_back(size_value);
      const auto data_value_id = TM->new_tree_node_id();
      tree_nodeRef data_value;
      if(GET_NODE(readType)->get_kind() == integer_type_K || GET_NODE(readType)->get_kind() == enumeral_type_K || GET_NODE(readType)->get_kind() == pointer_type_K || GET_NODE(readType)->get_kind() == reference_type_K)
         data_value = tree_man->CreateIntegerCst(readType, 0, data_value_id);
      else if(GET_NODE(readType)->get_kind() == real_type_K)
         data_value = tree_man->CreateRealCst(readType, 0.l, data_value_id);
      else
         THROW_ERROR("unexpected data type");
      args.push_back(data_value);
   }
   if(origStmt)
   {
      THROW_ASSERT(GET_NODE(origStmt)->get_kind() == gimple_assign_K, "unexpected condition");
      auto ga = GetPointer<gimple_assign>(GET_NODE(origStmt));
      THROW_ASSERT(GET_NODE(ga->op1)->get_kind() == mem_ref_K, "unexpected condition");
      auto mr = GetPointer<mem_ref>(GET_NODE(ga->op1));
      args.push_back(mr->op0);
   }
   else
      args.push_back(argSSANode);
   auto call_expr_node = tree_man->CreateCallExpr(function_decl_node, args, srcp);
   auto new_assignment = tree_man->CreateGimpleAssign(readType, tree_nodeRef(), tree_nodeRef(), call_expr_node, destBB, srcp); /// TO BE IMPROVED
   tree_nodeRef temp_ssa_var = GetPointer<gimple_assign>(GET_NODE(new_assignment))->op0;
   for(auto defSSA : usedStmt_defs)
   {
      auto ssaDefVar = GetPointer<ssa_name>(GET_NODE(defSSA));
      THROW_ASSERT(ssaDefVar, "unexpected condition");
      std::list<tree_nodeRef> varUses;
      for(auto used : ssaDefVar->CGetUseStmts())
      {
         varUses.push_back(used.first);
      }
      for(auto used : varUses)
      {
         TM->ReplaceTreeNode(used, defSSA, temp_ssa_var);
      }
   }
   if(origStmt)
      sl->list_of_bloc[destBB]->PushBefore(new_assignment, origStmt);
   else
      sl->list_of_bloc[destBB]->PushBack(new_assignment);
   GetPointer<HLS_manager>(AppM)->design_interface_loads[fname][destBB][argName_string].push_back(GET_INDEX_NODE(new_assignment));
   BehavioralHelperRef helper = BehavioralHelperRef(new BehavioralHelper(AppM, GET_INDEX_NODE(function_decl_node), false, parameters));
   FunctionBehaviorRef FB = FunctionBehaviorRef(new FunctionBehavior(AppM, helper, parameters));
   AppM->GetCallGraphManager()->AddFunctionAndCallPoint(GET_INDEX_NODE(gn->scpe), GET_INDEX_NODE(function_decl_node), new_assignment->index, FB, FunctionEdgeInfo::CallType::direct_call);
}

void interface_infer::create_Write_function(const std::string& argName_string, tree_nodeRef origStmt, const std::string& fdName, tree_nodeRef writeValue, tree_nodeRef aType, tree_nodeRef writeType, const tree_manipulationRef tree_man,
                                            const tree_managerRef TM, bool commonRWSignature)
{
   THROW_ASSERT(origStmt, "expected a ref statement");
   auto gn = GetPointer<gimple_node>(GET_NODE(origStmt));
   THROW_ASSERT(gn, "expected a gimple_node");
   unsigned int destBB = gn->bb_index;
   THROW_ASSERT(gn->scpe, "expected a scope");
   THROW_ASSERT(GET_NODE(gn->scpe)->get_kind() == function_decl_K, "expected a function_decl");
   auto fd = GetPointer<function_decl>(GET_NODE(gn->scpe));
   THROW_ASSERT(fd->body, "expected a body");
   auto* sl = GetPointer<statement_list>(GET_NODE(fd->body));
   std::string fname;
   tree_helper::get_mangled_fname(fd, fname);
   tree_nodeRef boolean_type;
   const auto size_value_id = TM->new_tree_node_id();
   const auto bit_size_type = tree_man->create_bit_size_type();
   const auto size_value = tree_man->CreateIntegerCst(bit_size_type, tree_helper::Size(writeType), size_value_id);

   /// create the function_decl
   std::vector<tree_nodeRef> argsT;
   if(commonRWSignature)
   {
      boolean_type = tree_man->create_boolean_type();
      argsT.push_back(boolean_type);
   }
   argsT.push_back(bit_size_type);
   if(tree_helper::is_int(TM, GET_INDEX_NODE(writeValue)))
   {
      argsT.push_back(tree_man->CreateUnsigned(tree_helper::CGetType(GET_NODE(writeValue))));
   }
   else
      argsT.push_back(writeType);
   argsT.push_back(aType);
   const std::string srcp = fd->include_name + ":" + STR(fd->line_number) + ":" + STR(fd->column_number);
   auto function_decl_node = tree_man->create_function_decl(fdName, fd->scpe, argsT, tree_man->create_void_type(), srcp, false);

   std::vector<tree_nodeRef> args;
   if(commonRWSignature)
   {
      const auto sel_value_id = TM->new_tree_node_id();
      const auto sel_value = tree_man->CreateIntegerCst(boolean_type, 1, sel_value_id);
      args.push_back(sel_value);
   }
   args.push_back(size_value);
   if(tree_helper::is_int(TM, GET_INDEX_NODE(writeValue)))
   {
      const auto ga_nop = tree_man->CreateNopExpr(writeValue, tree_man->CreateUnsigned(tree_helper::CGetType(GET_NODE(writeValue))), tree_nodeRef(), tree_nodeRef());
      sl->list_of_bloc[destBB]->PushBefore(ga_nop, origStmt);
      args.push_back(GetPointer<gimple_assign>(GET_NODE(ga_nop))->op0);
   }
   else
      args.push_back(writeValue);
   THROW_ASSERT(GET_NODE(origStmt)->get_kind() == gimple_assign_K, "unexpected condition");
   auto ga = GetPointer<gimple_assign>(GET_NODE(origStmt));
   THROW_ASSERT(GET_NODE(ga->op0)->get_kind() == mem_ref_K, "unexpected condition");
   auto mr = GetPointer<mem_ref>(GET_NODE(ga->op0));
   args.push_back(mr->op0);

   auto new_writecall = tree_man->create_gimple_call(function_decl_node, args, srcp, destBB);

   sl->list_of_bloc[destBB]->PushBefore(new_writecall, origStmt);
   GetPointer<HLS_manager>(AppM)->design_interface_stores[fname][destBB][argName_string].push_back(GET_INDEX_NODE(new_writecall));
   addGimpleNOPxVirtual(origStmt, TM);
   BehavioralHelperRef helper = BehavioralHelperRef(new BehavioralHelper(AppM, GET_INDEX_NODE(function_decl_node), false, parameters));
   FunctionBehaviorRef FB = FunctionBehaviorRef(new FunctionBehavior(AppM, helper, parameters));
   AppM->GetCallGraphManager()->AddFunctionAndCallPoint(GET_INDEX_NODE(gn->scpe), GET_INDEX_NODE(function_decl_node), new_writecall->index, FB, FunctionEdgeInfo::CallType::direct_call);
}

void interface_infer::create_resource_Read_simple(const std::vector<std::string>& operations, const std::string& argName_string, const std::string& interfaceType, unsigned int inputBitWidth, bool IO_port, unsigned n_resources)
{
   const std::string ResourceName = ENCODE_FDNAME(argName_string, "_Read_", interfaceType);
   auto HLSMgr = GetPointer<HLS_manager>(AppM);
   auto HLS_T = HLSMgr->get_HLS_target();
   auto TechMan = HLS_T->get_technology_manager();
   if(!TechMan->is_library_manager(INTERFACE_LIBRARY) || !TechMan->get_library_manager(INTERFACE_LIBRARY)->is_fu(ResourceName))
   {
      INDENT_DBG_MEX(DEBUG_LEVEL_PEDANTIC, debug_level, "-->Creating interface resource: " + INTERFACE_LIBRARY + ":" + ResourceName);
      structural_objectRef interface_top;
      structural_managerRef CM = structural_managerRef(new structural_manager(parameters));
      structural_type_descriptorRef module_type = structural_type_descriptorRef(new structural_type_descriptor(ResourceName));
      CM->set_top_info(ResourceName, module_type);
      interface_top = CM->get_circ();
      /// add description and license
      GetPointer<module>(interface_top)->set_description("Interface module for function: " + ResourceName);
      GetPointer<module>(interface_top)->set_copyright(GENERATED_COPYRIGHT);
      GetPointer<module>(interface_top)->set_authors("Component automatically generated by bambu");
      GetPointer<module>(interface_top)->set_license(GENERATED_LICENSE);
      bool isMultipleResource = interfaceType == "acknowledge" || interfaceType == "valid" || interfaceType == "handshake" || interfaceType == "fifo";
      if(isMultipleResource)
         GetPointer<module>(interface_top)->set_multi_unit_multiplicity(n_resources);

      unsigned int address_bitsize = HLSMgr->get_address_bitsize();
      structural_type_descriptorRef word_bool_type = structural_type_descriptorRef(new structural_type_descriptor("bool", address_bitsize));
      structural_type_descriptorRef Intype = structural_type_descriptorRef(new structural_type_descriptor("bool", inputBitWidth));
      structural_type_descriptorRef bool_type = structural_type_descriptorRef(new structural_type_descriptor("bool", 0));
      structural_type_descriptorRef rwtype = structural_type_descriptorRef(new structural_type_descriptor("bool", 1));
      if(interfaceType == "valid" || interfaceType == "handshake" || interfaceType == "fifo")
      {
         CM->add_port(CLOCK_PORT_NAME, port_o::IN, interface_top, bool_type);
         CM->add_port(RESET_PORT_NAME, port_o::IN, interface_top, bool_type);
      }
      if(isMultipleResource)
      {
         CM->add_port_vector(START_PORT_NAME, port_o::IN, n_resources, interface_top, bool_type);
      }
      structural_objectRef addrPort;
      if(isMultipleResource)
         addrPort = CM->add_port_vector("in1", port_o::IN, n_resources, interface_top, word_bool_type);
      else
         addrPort = CM->add_port("in1", port_o::IN, interface_top, word_bool_type); // this port has a fixed name
      GetPointer<port_o>(addrPort)->set_is_addr_bus(true);
      GetPointer<port_o>(addrPort)->set_is_var_args(true); /// required to activate the module generation
      if(interfaceType == "valid" || interfaceType == "handshake" || interfaceType == "fifo")
      {
         auto inPort_o_vld = CM->add_port_vector(DONE_PORT_NAME, port_o::OUT, n_resources, interface_top, bool_type);
      }
      if(isMultipleResource)
         CM->add_port_vector("out1", port_o::OUT, n_resources, interface_top, rwtype);
      else
         CM->add_port("out1", port_o::OUT, interface_top, rwtype);

      auto inPort = CM->add_port("_" + argName_string + (interfaceType == "fifo" ? "_dout" : (IO_port ? "_i" : "")), port_o::IN, interface_top, Intype);
      GetPointer<port_o>(inPort)->set_port_interface(port_o::port_interface::PI_RNONE);
      if(interfaceType == "acknowledge" || interfaceType == "handshake")
      {
         auto inPort_o_ack = CM->add_port("_" + argName_string + (IO_port ? "_i" : "") + "_ack", port_o::OUT, interface_top, bool_type);
         GetPointer<port_o>(inPort_o_ack)->set_port_interface(port_o::port_interface::PI_RACK);
      }
      if(interfaceType == "valid" || interfaceType == "handshake")
      {
         auto inPort_o_vld = CM->add_port("_" + argName_string + (IO_port ? "_i" : "") + "_vld", port_o::IN, interface_top, bool_type);
         GetPointer<port_o>(inPort_o_vld)->set_port_interface(port_o::port_interface::PI_RVALID);
      }
      if(interfaceType == "fifo")
      {
         auto inPort_empty_n = CM->add_port("_" + argName_string + "_empty_n", port_o::IN, interface_top, bool_type);
         GetPointer<port_o>(inPort_empty_n)->set_port_interface(port_o::port_interface::PI_EMPTY_N);
         auto inPort_read = CM->add_port("_" + argName_string + "_read", port_o::OUT, interface_top, bool_type);
         GetPointer<port_o>(inPort_read)->set_port_interface(port_o::port_interface::PI_READ);
      }

      CM->add_NP_functionality(interface_top, NP_functionality::LIBRARY, "out1");
      CM->add_NP_functionality(interface_top, NP_functionality::VERILOG_GENERATOR, "Read_" + interfaceType + ".cpp");
      TechMan->add_resource(INTERFACE_LIBRARY, ResourceName, CM);
      for(auto fdName : operations)
         TechMan->add_operation(INTERFACE_LIBRARY, ResourceName, fdName);
      auto* fu = GetPointer<functional_unit>(TechMan->get_fu(ResourceName, INTERFACE_LIBRARY));
      const target_deviceRef device = HLS_T->get_target_device();
      fu->area_m = area_model::create_model(device->get_type(), parameters);
      fu->area_m->set_area_value(0);
      if(!isMultipleResource)
         fu->logical_type = functional_unit::COMBINATIONAL;

      for(auto fdName : operations)
      {
         auto* op = GetPointer<operation>(fu->get_operation(fdName));
         op->time_m = time_model::create_model(device->get_type(), parameters);
         if(interfaceType == "valid" || interfaceType == "handshake" || interfaceType == "fifo")
         {
            op->bounded = false;
            op->time_m->set_execution_time(HLS_T->get_technology_manager()->CGetSetupHoldTime() + EPSILON, 0);
         }
         else
         {
            op->bounded = true;
            op->time_m->set_execution_time(EPSILON, 0);
            op->time_m->set_stage_period(0.0);
            op->time_m->set_synthesis_dependent(true);
         }
      }
      if(isMultipleResource)
         HLSMgr->design_interface_constraints[function_id][INTERFACE_LIBRARY][ResourceName] = n_resources;
      /// otherwise no constraints are required for this resource
      INDENT_DBG_MEX(DEBUG_LEVEL_PEDANTIC, debug_level, "<--Interface resource created: ");
   }
}

void interface_infer::create_resource_Write_simple(const std::vector<std::string>& operations, const std::string& argName_string, const std::string& interfaceType, unsigned int inputBitWidth, bool IO_port, bool isDiffSize, unsigned n_resources)
{
   const std::string ResourceName = ENCODE_FDNAME(argName_string, "_Write_", interfaceType);
   auto HLSMgr = GetPointer<HLS_manager>(AppM);
   auto HLS_T = HLSMgr->get_HLS_target();
   auto TechMan = HLS_T->get_technology_manager();
   if(!TechMan->is_library_manager(INTERFACE_LIBRARY) || !TechMan->get_library_manager(INTERFACE_LIBRARY)->is_fu(ResourceName))
   {
      INDENT_DBG_MEX(DEBUG_LEVEL_PEDANTIC, debug_level, "-->Creating interface resource: " + INTERFACE_LIBRARY + ":" + ResourceName);
      structural_objectRef interface_top;
      structural_managerRef CM = structural_managerRef(new structural_manager(parameters));
      structural_type_descriptorRef module_type = structural_type_descriptorRef(new structural_type_descriptor(ResourceName));
      CM->set_top_info(ResourceName, module_type);
      interface_top = CM->get_circ();
      /// add description and license
      GetPointer<module>(interface_top)->set_description("Interface module for function: " + ResourceName);
      GetPointer<module>(interface_top)->set_copyright(GENERATED_COPYRIGHT);
      GetPointer<module>(interface_top)->set_authors("Component automatically generated by bambu");
      GetPointer<module>(interface_top)->set_license(GENERATED_LICENSE);
      bool isAVH = interfaceType == "acknowledge" || interfaceType == "valid" || interfaceType == "handshake" || interfaceType == "fifo" || interfaceType == "none_registered";
      bool isMultipleResource = isDiffSize || isAVH;

      if(isMultipleResource)
         GetPointer<module>(interface_top)->set_multi_unit_multiplicity(n_resources);

      unsigned int address_bitsize = HLSMgr->get_address_bitsize();
      structural_type_descriptorRef word_bool_type = structural_type_descriptorRef(new structural_type_descriptor("bool", address_bitsize));
      structural_type_descriptorRef Intype = structural_type_descriptorRef(new structural_type_descriptor("bool", inputBitWidth));
      structural_type_descriptorRef rwsize = structural_type_descriptorRef(new structural_type_descriptor("bool", 1));
      structural_type_descriptorRef rwtype = structural_type_descriptorRef(new structural_type_descriptor("bool", 1));
      structural_type_descriptorRef bool_type = structural_type_descriptorRef(new structural_type_descriptor("bool", 0));
      if(interfaceType == "none_registered" || interfaceType == "acknowledge" || interfaceType == "handshake" || interfaceType == "fifo")
      {
         CM->add_port(CLOCK_PORT_NAME, port_o::IN, interface_top, bool_type);
         CM->add_port(RESET_PORT_NAME, port_o::IN, interface_top, bool_type);
      }
      if(isMultipleResource)
      {
         CM->add_port_vector(START_PORT_NAME, port_o::IN, n_resources, interface_top, bool_type);
      }
      structural_objectRef sizePort, writePort, addrPort;
      if(isMultipleResource)
      {
         sizePort = CM->add_port_vector("in1", port_o::IN, n_resources, interface_top, rwsize);
         writePort = CM->add_port_vector("in2", port_o::IN, n_resources, interface_top, rwtype);
         addrPort = CM->add_port_vector("in3", port_o::IN, n_resources, interface_top, word_bool_type);
      }
      else
      {
         sizePort = CM->add_port("in1", port_o::IN, interface_top, rwsize);
         writePort = CM->add_port("in2", port_o::IN, interface_top, rwtype);
         addrPort = CM->add_port("in3", port_o::IN, interface_top, word_bool_type);
      }
      GetPointer<port_o>(addrPort)->set_is_addr_bus(true);
      GetPointer<port_o>(addrPort)->set_is_var_args(true); /// required to activate the module generation
      if(interfaceType == "none_registered" || interfaceType == "acknowledge" || interfaceType == "handshake" || interfaceType == "fifo")
      {
         CM->add_port_vector(DONE_PORT_NAME, port_o::OUT, n_resources, interface_top, bool_type);
      }

      auto inPort_o = CM->add_port("_" + argName_string + (interfaceType == "fifo" ? "_din" : (IO_port ? "_o" : "")), port_o::OUT, interface_top, Intype);
      GetPointer<port_o>(inPort_o)->set_port_interface(port_o::port_interface::PI_WNONE);
      if(interfaceType == "acknowledge" || interfaceType == "handshake")
      {
         auto inPort_o_ack = CM->add_port("_" + argName_string + (IO_port ? "_o" : "") + "_ack", port_o::IN, interface_top, bool_type);
         GetPointer<port_o>(inPort_o_ack)->set_port_interface(port_o::port_interface::PI_WACK);
      }
      if(interfaceType == "valid" || interfaceType == "handshake")
      {
         auto inPort_o_vld = CM->add_port("_" + argName_string + (IO_port ? "_o" : "") + "_vld", port_o::OUT, interface_top, bool_type);
         GetPointer<port_o>(inPort_o_vld)->set_port_interface(port_o::port_interface::PI_WVALID);
      }
      if(interfaceType == "fifo")
      {
         auto inPort_full_n = CM->add_port("_" + argName_string + "_full_n", port_o::IN, interface_top, bool_type);
         GetPointer<port_o>(inPort_full_n)->set_port_interface(port_o::port_interface::PI_FULL_N);
         auto inPort_read = CM->add_port("_" + argName_string + "_write", port_o::OUT, interface_top, bool_type);
         GetPointer<port_o>(inPort_read)->set_port_interface(port_o::port_interface::PI_WRITE);
      }

      CM->add_NP_functionality(interface_top, NP_functionality::LIBRARY, "in1 in2");
      const auto writer = static_cast<HDLWriter_Language>(parameters->getOption<unsigned int>(OPT_writer_language));
      if((interfaceType == "none" || interfaceType == "none_registered") && !(isDiffSize && !isAVH) && writer == HDLWriter_Language::VHDL)
         CM->add_NP_functionality(interface_top, NP_functionality::VHDL_GENERATOR, "Write_" + interfaceType + ((isDiffSize && !isAVH) ? "DS" : "") + "_VHDL.cpp");
      else
         CM->add_NP_functionality(interface_top, NP_functionality::VERILOG_GENERATOR, "Write_" + interfaceType + ((isDiffSize && !isAVH) ? "DS" : "") + ".cpp");
      TechMan->add_resource(INTERFACE_LIBRARY, ResourceName, CM);
      for(auto fdName : operations)
         TechMan->add_operation(INTERFACE_LIBRARY, ResourceName, fdName);
      auto* fu = GetPointer<functional_unit>(TechMan->get_fu(ResourceName, INTERFACE_LIBRARY));
      const target_deviceRef device = HLS_T->get_target_device();
      fu->area_m = area_model::create_model(device->get_type(), parameters);
      fu->area_m->set_area_value(0);
      if(!isMultipleResource)
         fu->logical_type = functional_unit::COMBINATIONAL;

      for(auto fdName : operations)
      {
         auto* op = GetPointer<operation>(fu->get_operation(fdName));
         op->time_m = time_model::create_model(device->get_type(), parameters);
         if(interfaceType == "acknowledge" || interfaceType == "handshake" || interfaceType == "fifo")
         {
            op->bounded = false;
            op->time_m->set_execution_time(HLS_T->get_technology_manager()->CGetSetupHoldTime() + EPSILON, 0);
         }
         else if(interfaceType == "none_registered")
         {
            op->bounded = true;
            op->time_m->set_execution_time(EPSILON, 2);
            op->time_m->set_stage_period(HLS_T->get_technology_manager()->CGetSetupHoldTime() + EPSILON);
            const ControlStep ii_cs(1);
            op->time_m->set_initiation_time(ii_cs);
         }
         else
         {
            op->bounded = true;
            op->time_m->set_execution_time(EPSILON, 0);
            op->time_m->set_stage_period(0.0);
         }
         op->time_m->set_synthesis_dependent(true);
      }
      /// add constraint on resource
      HLSMgr->design_interface_constraints[function_id][INTERFACE_LIBRARY][ResourceName] = n_resources;
      INDENT_DBG_MEX(DEBUG_LEVEL_PEDANTIC, debug_level, "<--Interface resource created: ");
   }
}

void interface_infer::create_resource_array(const std::vector<std::string>& operationsR, const std::vector<std::string>& operationsW, const std::string& argName_string, const std::string& interfaceType, unsigned int inputBitWidth, unsigned int arraySize,
                                            unsigned n_resources, unsigned alignment)
{
   auto n_channels = parameters->getOption<unsigned int>(OPT_channels_number);
   bool isDP = inputBitWidth <= 64 && n_resources == 1 && n_channels == 2;
   auto NResources = isDP ? 2 : n_resources;
   auto read_write_string = (isDP ? std::string("ReadWriteDP_") : std::string("ReadWrite_"));
   const std::string ResourceName = ENCODE_FDNAME(argName_string, "_" + read_write_string, interfaceType);
   auto HLSMgr = GetPointer<HLS_manager>(AppM);
   auto HLS_T = HLSMgr->get_HLS_target();
   auto TechMan = HLS_T->get_technology_manager();
   if(!TechMan->is_library_manager(INTERFACE_LIBRARY) || !TechMan->get_library_manager(INTERFACE_LIBRARY)->is_fu(ResourceName))
   {
      INDENT_DBG_MEX(DEBUG_LEVEL_PEDANTIC, debug_level, "-->Creating interface resource: " + INTERFACE_LIBRARY + ":" + ResourceName);
      structural_objectRef interface_top;
      structural_managerRef CM = structural_managerRef(new structural_manager(parameters));
      structural_type_descriptorRef module_type = structural_type_descriptorRef(new structural_type_descriptor(ResourceName));
      CM->set_top_info(ResourceName, module_type);
      interface_top = CM->get_circ();
      /// add description and license
      GetPointer<module>(interface_top)->set_description("Interface module for function: " + ResourceName);
      GetPointer<module>(interface_top)->set_copyright(GENERATED_COPYRIGHT);
      GetPointer<module>(interface_top)->set_authors("Component automatically generated by bambu");
      GetPointer<module>(interface_top)->set_license(GENERATED_LICENSE);
      GetPointer<module>(interface_top)->set_multi_unit_multiplicity(NResources);

      auto nbitAddres = 32u - static_cast<unsigned>(__builtin_clz(arraySize * alignment - 1));
      unsigned int address_bitsize = HLSMgr->get_address_bitsize();
      structural_type_descriptorRef word_bool_type = structural_type_descriptorRef(new structural_type_descriptor("bool", address_bitsize));
      auto nbit = 32u - static_cast<unsigned>(__builtin_clz(arraySize - 1));
      structural_type_descriptorRef address_interface_type = structural_type_descriptorRef(new structural_type_descriptor("bool", nbit));
      structural_type_descriptorRef Intype = structural_type_descriptorRef(new structural_type_descriptor("bool", inputBitWidth));
      structural_type_descriptorRef size1 = structural_type_descriptorRef(new structural_type_descriptor("bool", 1));
      structural_type_descriptorRef rwsize = structural_type_descriptorRef(new structural_type_descriptor("bool", 1));
      structural_type_descriptorRef bool_type = structural_type_descriptorRef(new structural_type_descriptor("bool", 0));
      CM->add_port(CLOCK_PORT_NAME, port_o::IN, interface_top, bool_type);
      CM->add_port(RESET_PORT_NAME, port_o::IN, interface_top, bool_type);
      CM->add_port_vector(START_PORT_NAME, port_o::IN, NResources, interface_top, bool_type);

      auto selPort = CM->add_port_vector("in1", port_o::IN, NResources, interface_top, size1);
      auto sizePort = CM->add_port_vector("in2", port_o::IN, NResources, interface_top, size1);
      auto dataPort = CM->add_port_vector("in3", port_o::IN, NResources, interface_top, size1);
      auto addrPort = CM->add_port_vector("in4", port_o::IN, NResources, interface_top, word_bool_type);
      GetPointer<port_o>(dataPort)->set_port_alignment(nbitAddres);

      GetPointer<port_o>(addrPort)->set_is_addr_bus(true);
      GetPointer<port_o>(addrPort)->set_is_var_args(true); /// required to activate the module generation

      CM->add_port_vector("out1", port_o::OUT, NResources, interface_top, rwsize);

      auto inPort_address = CM->add_port("_" + argName_string + "_address0", port_o::OUT, interface_top, address_interface_type);
      GetPointer<port_o>(inPort_address)->set_port_interface(port_o::port_interface::PI_ADDRESS);
      GetPointer<port_o>(inPort_address)->set_port_alignment(alignment);
      if(isDP)
      {
         auto inPort_address1 = CM->add_port("_" + argName_string + "_address1", port_o::OUT, interface_top, address_interface_type);
         GetPointer<port_o>(inPort_address1)->set_port_interface(port_o::port_interface::PI_ADDRESS);
         GetPointer<port_o>(inPort_address1)->set_port_alignment(alignment);
      }

      auto inPort_ce = CM->add_port("_" + argName_string + "_ce0", port_o::OUT, interface_top, bool_type);
      GetPointer<port_o>(inPort_ce)->set_port_interface(port_o::port_interface::PI_CHIPENABLE);
      if(isDP)
      {
         auto inPort_ce1 = CM->add_port("_" + argName_string + "_ce1", port_o::OUT, interface_top, bool_type);
         GetPointer<port_o>(inPort_ce1)->set_port_interface(port_o::port_interface::PI_CHIPENABLE);
      }

      if(!operationsW.empty())
      {
         auto inPort_we = CM->add_port("_" + argName_string + "_we0", port_o::OUT, interface_top, bool_type);
         GetPointer<port_o>(inPort_we)->set_port_interface(port_o::port_interface::PI_WRITEENABLE);
         if(isDP)
         {
            auto inPort_we1 = CM->add_port("_" + argName_string + "_we1", port_o::OUT, interface_top, bool_type);
            GetPointer<port_o>(inPort_we1)->set_port_interface(port_o::port_interface::PI_WRITEENABLE);
         }
      }
      if(!operationsR.empty())
      {
         auto inPort_din = CM->add_port("_" + argName_string + "_q0", port_o::IN, interface_top, Intype);
         GetPointer<port_o>(inPort_din)->set_port_interface(port_o::port_interface::PI_DIN);
         if(isDP)
         {
            auto inPort_din1 = CM->add_port("_" + argName_string + "_q1", port_o::IN, interface_top, Intype);
            GetPointer<port_o>(inPort_din1)->set_port_interface(port_o::port_interface::PI_DIN);
         }
      }
      if(!operationsW.empty())
      {
         auto inPort_dout = CM->add_port("_" + argName_string + "_d0", port_o::OUT, interface_top, Intype);
         GetPointer<port_o>(inPort_dout)->set_port_interface(port_o::port_interface::PI_DOUT);
         if(isDP)
         {
            auto inPort_dout1 = CM->add_port("_" + argName_string + "_d1", port_o::OUT, interface_top, Intype);
            GetPointer<port_o>(inPort_dout1)->set_port_interface(port_o::port_interface::PI_DOUT);
         }
      }

      CM->add_NP_functionality(interface_top, NP_functionality::LIBRARY, "in1 in2 in3 out1");
      CM->add_NP_functionality(interface_top, NP_functionality::VERILOG_GENERATOR, read_write_string + interfaceType + ".cpp");
      TechMan->add_resource(INTERFACE_LIBRARY, ResourceName, CM);
      for(auto fdName : operationsR)
         TechMan->add_operation(INTERFACE_LIBRARY, ResourceName, fdName);
      for(auto fdName : operationsW)
         TechMan->add_operation(INTERFACE_LIBRARY, ResourceName, fdName);
      auto* fu = GetPointer<functional_unit>(TechMan->get_fu(ResourceName, INTERFACE_LIBRARY));
      const target_deviceRef device = HLS_T->get_target_device();
      fu->area_m = area_model::create_model(device->get_type(), parameters);
      fu->area_m->set_area_value(0);

      technology_nodeRef bram_f_unit = TechMan->get_fu(isDP ? ARRAY_1D_STD_BRAM_NN_SDS : ARRAY_1D_STD_BRAM_SDS, LIBRARY_STD_FU);
      auto* bram_fu = GetPointer<functional_unit>(bram_f_unit);
      technology_nodeRef load_op_node = bram_fu->get_operation("LOAD");
      auto* load_op = GetPointer<operation>(load_op_node);
      auto load_delay = load_op->time_m->get_execution_time();
      auto load_cycles = load_op->time_m->get_cycles();
      auto load_ii = load_op->time_m->get_initiation_time();
      auto load_sp = load_op->time_m->get_stage_period();
      for(auto fdName : operationsR)
      {
         auto* op = GetPointer<operation>(fu->get_operation(fdName));
         op->time_m = time_model::create_model(device->get_type(), parameters);
         op->bounded = true;
         op->time_m->set_execution_time(load_delay, load_cycles);
         op->time_m->set_initiation_time(load_ii);
         op->time_m->set_stage_period(load_sp);
         op->time_m->set_synthesis_dependent(true);
      }
      technology_nodeRef store_op_node = bram_fu->get_operation("STORE");
      auto* store_op = GetPointer<operation>(store_op_node);
      auto store_delay = store_op->time_m->get_execution_time();
      auto store_cycles = store_op->time_m->get_cycles();
      auto store_ii = store_op->time_m->get_initiation_time();
      auto store_sp = store_op->time_m->get_stage_period();
      for(auto fdName : operationsW)
      {
         auto* op = GetPointer<operation>(fu->get_operation(fdName));
         op->time_m = time_model::create_model(device->get_type(), parameters);
         op->bounded = true;
         op->time_m->set_execution_time(store_delay, store_cycles);
         op->time_m->set_initiation_time(store_ii);
         op->time_m->set_stage_period(store_sp);
         op->time_m->set_synthesis_dependent(true);
      }
      /// add constraint on resource
      HLSMgr->design_interface_constraints[function_id][INTERFACE_LIBRARY][ResourceName] = NResources;
      INDENT_DBG_MEX(DEBUG_LEVEL_PEDANTIC, debug_level, "<--Interface resource created: ");
   }
}

void interface_infer::ComputeResourcesAlignment(unsigned& n_resources, unsigned& alignment, unsigned int inputBitWidth, bool is_acType, bool is_signed, bool is_fixed)
{
   n_resources = 1;
   if(inputBitWidth > 64 && inputBitWidth <= 128)
   {
      n_resources = 2;
   }
   else if(inputBitWidth > 128)
   {
      n_resources = inputBitWidth / 32 + (inputBitWidth % 32 ? 1 : 0);
      if(!is_signed && inputBitWidth % 32 == 0 && !is_fixed)
         ++n_resources;
   }
   /// compute alignment
   alignment = 1;
   if(inputBitWidth <= 8)
   {
      alignment = !is_acType ? 1 : 4;
   }
   else if(inputBitWidth <= 16)
   {
      alignment = !is_acType ? 2 : 4;
   }
   else if(inputBitWidth <= 32)
   {
      alignment = 4;
   }
   else if(inputBitWidth <= 64)
   {
      alignment = 8;
   }
   else if(inputBitWidth <= 128)
   {
      alignment = 16;
   }
   else
   {
      alignment = (inputBitWidth / 32) + 4 * (inputBitWidth % 32 ? 1 : 0);
      if(!is_signed && inputBitWidth % 32 == 0 && !is_fixed)
         alignment += 4;
   }
}

void interface_infer::create_resource(const std::vector<std::string>& operationsR, const std::vector<std::string>& operationsW, const std::string& argName_string, const std::string& interfaceType, unsigned int inputBitWidth, bool isDiffSize,
                                      const std::string& fname, unsigned n_resources, unsigned alignment)
{
   if(interfaceType == "none" || interfaceType == "none_registered" || interfaceType == "acknowledge" || interfaceType == "valid" || interfaceType == "ovalid" || interfaceType == "handshake" || interfaceType == "fifo")
   {
      THROW_ASSERT(!operationsR.empty() || !operationsW.empty(), "unexpected condition");
      bool IO_P = !operationsR.empty() && !operationsW.empty();
      if(!operationsR.empty())
      {
         create_resource_Read_simple(operationsR, argName_string, (interfaceType == "ovalid" ? "none" : interfaceType), inputBitWidth, IO_P, n_resources);
      }
      if(!operationsW.empty())
      {
         create_resource_Write_simple(operationsW, argName_string, (interfaceType == "ovalid" ? "valid" : interfaceType), inputBitWidth, IO_P, isDiffSize, n_resources);
      }
   }
   else if(interfaceType == "array")
   {
      auto HLSMgr = GetPointer<HLS_manager>(AppM);
      THROW_ASSERT(HLSMgr->design_interface_arraysize.find(fname) != HLSMgr->design_interface_arraysize.end() && HLSMgr->design_interface_arraysize.find(fname)->second.find(argName_string) != HLSMgr->design_interface_arraysize.find(fname)->second.end(),
                   "unexpected condition");
      auto arraySizeSTR = HLSMgr->design_interface_arraysize.find(fname)->second.find(argName_string)->second;
      auto arraySize = boost::lexical_cast<unsigned>(arraySizeSTR);
      if(arraySize == 0)
         THROW_ERROR("array size equal to zero");
      create_resource_array(operationsR, operationsW, argName_string, interfaceType, inputBitWidth, arraySize, n_resources, alignment);
   }
   else
      THROW_ERROR("interface not supported: " + interfaceType);
}

void interface_infer::addGimpleNOPxVirtual(tree_nodeRef origStmt, const tree_managerRef TM)
{
   auto gn = GetPointer<gimple_node>(GET_NODE(origStmt));
   THROW_ASSERT(gn, "expected a gimple_node");
   THROW_ASSERT(gn->scpe, "expected a scope");
   THROW_ASSERT(GET_NODE(gn->scpe)->get_kind() == function_decl_K, "expected a function_decl");
   auto fd = GetPointer<function_decl>(GET_NODE(gn->scpe));
   THROW_ASSERT(fd->body, "expected a body");
   auto* sl = GetPointer<statement_list>(GET_NODE(fd->body));

   auto origGN = GetPointer<gimple_node>(GET_NODE(origStmt));
   std::map<TreeVocabularyTokenTypes_TokenEnum, std::string> gimple_nop_schema;
   gimple_nop_schema[TOK(TOK_SRCP)] = "<built-in>:0:0";
   const auto gimple_node_id = TM->new_tree_node_id();
   TM->create_tree_node(gimple_node_id, gimple_nop_K, gimple_nop_schema);
   auto gimple_nop_Node = TM->GetTreeReindex(gimple_node_id);
   auto newGN = GetPointer<gimple_node>(GET_NODE(gimple_nop_Node));
   newGN->memdef = origGN->memdef;
   newGN->memuse = origGN->memuse;
   for(auto vUse : origGN->vuses)
   {
      auto sn = GetPointer<ssa_name>(GET_NODE(vUse));
      newGN->AddVuse(vUse);
      sn->AddUseStmt(gimple_nop_Node);
   }
   for(auto vOver : origGN->vovers)
   {
      if(writeVdef.find(GET_INDEX_NODE(vOver)) == writeVdef.end())
         newGN->AddVover(vOver);
   }
   if(origGN->vdef)
   {
      auto snDef = GetPointer<ssa_name>(GET_NODE(origGN->vdef));
      newGN->vdef = origGN->vdef;
      snDef->SetDefStmt(gimple_nop_Node);
      writeVdef.insert(GET_INDEX_NODE(origGN->vdef));
   }
   sl->list_of_bloc[origGN->bb_index]->PushBefore(gimple_nop_Node, origStmt);
   sl->list_of_bloc[origGN->bb_index]->RemoveStmt(origStmt);
}

DesignFlowStep_Status interface_infer::InternalExec()
{
   if(GetPointer<const HLS_manager>(AppM))
   {
      const auto top_functions = AppM->CGetCallGraphManager()->GetRootFunctions();
      bool is_top = top_functions.find(function_id) != top_functions.end();
      if(is_top)
      {
         writeVdef.clear();
         auto HLSMgr = GetPointer<HLS_manager>(AppM);
         /// load xml interface specification file
         for(auto source_file : AppM->input_files)
         {
            const std::string output_temporary_directory = parameters->getOption<std::string>(OPT_output_temporary_directory);
            std::string leaf_name = source_file.second == "-" ? "stdin-" : GetLeafFileName(source_file.second);
            auto XMLfilename = output_temporary_directory + "/" + leaf_name + ".interface.xml";
            if((boost::filesystem::exists(boost::filesystem::path(XMLfilename))))
            {
               INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "-->parsing " + XMLfilename);
               XMLDomParser parser(XMLfilename);
               parser.Exec();
               if(parser)
               {
                  // Walk the tree:
                  const xml_element* node = parser.get_document()->get_root_node(); // deleted by DomParser.
                  for(const auto& iter : node->get_children())
                  {
                     const auto* Enode = GetPointer<const xml_element>(iter);
                     if(!Enode)
                        continue;
                     if(Enode->get_name() == "function")
                     {
                        std::string fname;
                        for(auto attr : Enode->get_attributes())
                        {
                           std::string key = attr->get_name();
                           std::string value = attr->get_value();
                           if(key == "id")
                              fname = value;
                        }
                        if(fname == "")
                           THROW_ERROR("malformed interface file");
                        for(const auto& iterArg : Enode->get_children())
                        {
                           const auto* EnodeArg = GetPointer<const xml_element>(iterArg);
                           if(!EnodeArg)
                              continue;
                           if(EnodeArg->get_name() == "arg")
                           {
                              std::string argName;
                              std::string interfaceType;
                              std::string interfaceSize;
                              std::string interfaceTypename;
                              std::string interfaceTypenameInclude;
                              for(auto attrArg : EnodeArg->get_attributes())
                              {
                                 std::string key = attrArg->get_name();
                                 std::string value = attrArg->get_value();
                                 if(key == "id")
                                    argName = value;
                                 if(key == "interface_type")
                                    interfaceType = value;
                                 if(key == "size")
                                    interfaceSize = value;
                                 if(key == "interface_typename")
                                 {
                                    interfaceTypename = value;
                                    xml_node::convert_escaped(interfaceTypename);
                                 }
                                 if(key == "interface_typename_include")
                                    interfaceTypenameInclude = value;
                              }
                              if(argName == "")
                                 THROW_ERROR("malformed interface file");
                              if(interfaceType == "")
                                 THROW_ERROR("malformed interface file");
                              std::cerr << "|" << argName << "|" << interfaceType << "|\n";
                              HLSMgr->design_interface[fname][argName] = interfaceType;
                              if(interfaceType == "array")
                                 HLSMgr->design_interface_arraysize[fname][argName] = interfaceSize;
                              HLSMgr->design_interface_typename[fname][argName] = interfaceTypename;
                              HLSMgr->design_interface_typename_signature[fname].push_back(interfaceTypename);
                              if((interfaceTypename.find("ap_int<") != std::string::npos || interfaceTypename.find("ap_uint<") != std::string::npos) && interfaceTypenameInclude.find("ac_int.h") != std::string::npos)
                                 boost::replace_all(interfaceTypenameInclude, "ac_int.h", "ap_int.h");
                              if((interfaceTypename.find("ap_fixed<") != std::string::npos || interfaceTypename.find("ap_ufixed<") != std::string::npos) && interfaceTypenameInclude.find("ac_fixed.h") != std::string::npos)
                                 boost::replace_all(interfaceTypenameInclude, "ac_fixed.h", "ap_fixed.h");
                              HLSMgr->design_interface_typenameinclude[fname][argName] = interfaceTypenameInclude;
                           }
                        }
                     }
                  }
               }
               INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--parsed file " + XMLfilename);
            }
         }

         bool modified = false;
         auto& DesignInterface = HLSMgr->design_interface;
         auto& DesignInterfaceTypename = HLSMgr->design_interface_typename;
         const auto TM = AppM->get_tree_manager();
         auto fnode = TM->get_tree_node_const(function_id);
         auto fd = GetPointer<function_decl>(fnode);
         std::string fname;
         tree_helper::get_mangled_fname(fd, fname);
         if(DesignInterface.find(fname) != DesignInterface.end())
         {
            const tree_manipulationRef tree_man = tree_manipulationRef(new tree_manipulation(TM, parameters));
            /// pre-process the list of statements to bind parm_decl and ssa variables
            std::map<unsigned, unsigned> par2ssa;
            auto* sl = GetPointer<statement_list>(GET_NODE(fd->body));
            Computepar2ssa(sl, par2ssa);

            INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "-->Analyzing function " + fname);
            auto& DesignInterfaceArgs = DesignInterface.find(fname)->second;
            auto& DesignInterfaceTypenameArgs = DesignInterfaceTypename.find(fname)->second;
            for(auto arg : fd->list_of_args)
            {
               auto arg_id = GET_INDEX_NODE(arg);
               INDENT_DBG_MEX(DEBUG_LEVEL_PEDANTIC, debug_level, "---parm_decl id: " + STR(arg_id));
               auto a = GetPointer<parm_decl>(GET_NODE(arg));
               auto aType = a->type;
               auto argName = GET_NODE(a->name);
               THROW_ASSERT(GetPointer<identifier_node>(argName), "unexpected condition");
               const std::string& argName_string = GetPointer<identifier_node>(argName)->strg;
               INDENT_DBG_MEX(DEBUG_LEVEL_PEDANTIC, debug_level, "---parm_decl name: " + argName_string);
               THROW_ASSERT(DesignInterfaceArgs.find(argName_string) != DesignInterfaceArgs.end(), "Not matched parameter name: " + argName_string);
               auto interfaceType = DesignInterfaceArgs.find(argName_string)->second;
               auto interfaceTypename = DesignInterfaceTypenameArgs.find(argName_string)->second;
               if(interfaceType != "default")
               {
                  if(par2ssa.find(arg_id) == par2ssa.end())
                  {
                     THROW_WARNING("parameter not used by any statement");
                     if(tree_helper::is_a_pointer(TM, GET_INDEX_NODE(aType)))
                        DesignInterfaceArgs[argName_string] = "none";
                     else
                        THROW_ERROR("parameter not used: specified interface does not make sense - " + interfaceType);
                     continue;
                  }
                  if(interfaceType == "bus") /// TO BE FIXED
                  {
                     DesignInterfaceArgs[argName_string] = "default";
                     continue;
                  }
                  if(tree_helper::is_a_pointer(TM, GET_INDEX_NODE(aType)))
                  {
                     std::cerr << "is a pointer\n";
                     std::cerr << "list of statement that use this parameter\n";
                     auto inputBitWidth = tree_helper::size(TM, tree_helper::get_pointed_type(TM, GET_INDEX_NODE(aType)));
                     bool is_signed;
                     bool is_fixed;
                     auto acTypeBw = ac_type_bitwidth(interfaceTypename, is_signed, is_fixed);
                     bool is_acType = false;
                     if(acTypeBw)
                     {
                        is_acType = true;
                        inputBitWidth = acTypeBw;
                     }
                     unsigned n_resources;
                     unsigned alignment;
                     ComputeResourcesAlignment(n_resources, alignment, inputBitWidth, is_acType, is_signed, is_fixed);
                     THROW_ASSERT(inputBitWidth, "unexpected condition");

                     auto argSSANode = TM->GetTreeReindex(par2ssa.find(arg_id)->second);
                     bool canBeMovedToBB2 = true;
                     bool isRead = false;
                     bool isWrite = false;
                     bool unkwown_pattern = false;
                     std::list<tree_nodeRef> writeStmt;
                     std::list<tree_nodeRef> readStmt;
                     bool commonRWSignature = interfaceType == "array";

                     classifyArg(sl, argSSANode, canBeMovedToBB2, isRead, isWrite, unkwown_pattern, writeStmt, readStmt);

                     if(unkwown_pattern)
                        THROW_ERROR("unknown pattern identified");
                     if(writeStmt.empty() && readStmt.empty())
                        THROW_ERROR("parameter " + argName_string + " cannot have interface " + interfaceType + " (no load or write is associated with it)");

                     if(canBeMovedToBB2 && isRead)
                        std::cerr << "YES can be moved\n";
                     if(isRead && isWrite)
                     {
                        std::cerr << "IO arg\n";
                        if(interfaceType == "ptrdefault")
                        {
                           if(parameters->IsParameter("none-ptrdefault") && parameters->GetParameter<int>("none-ptrdefault") == 1)
                           {
                              DesignInterfaceArgs[argName_string] = "none";
                              interfaceType = "none";
                           }
                           else if(parameters->IsParameter("none-registered-ptrdefault") && parameters->GetParameter<int>("none-registered-ptrdefault") == 1)
                           {
                              DesignInterfaceArgs[argName_string] = "none_registered";
                              interfaceType = "none_registered";
                           }
                           else
                           {
                              DesignInterfaceArgs[argName_string] = "ovalid";
                              interfaceType = "ovalid";
                           }
                        }
                        else if(interfaceType == "fifo")
                        {
                           THROW_ERROR("parameter " + argName_string + " cannot have interface " + interfaceType + " because it is read only");
                        }
                     }
                     else if(isRead)
                     {
                        std::cerr << "I arg\n";
                        if(interfaceType == "ptrdefault")
                        {
                           DesignInterfaceArgs[argName_string] = "none";
                           interfaceType = "none";
                        }
                        else if(interfaceType == "ovalid")
                        {
                           THROW_ERROR("parameter " + argName_string + " cannot have interface " + interfaceType + " because it is read only");
                        }
                     }
                     else if(isWrite)
                     {
                        std::cerr << "O arg\n";
                        if(interfaceType == "ptrdefault")
                        {
                           if(parameters->IsParameter("none-ptrdefault") && parameters->GetParameter<int>("none-ptrdefault") == 1)
                           {
                              DesignInterfaceArgs[argName_string] = "none";
                              interfaceType = "none";
                           }
                           else if(parameters->IsParameter("none-registered-ptrdefault") && parameters->GetParameter<int>("none-registered-ptrdefault") == 1)
                           {
                              DesignInterfaceArgs[argName_string] = "none_registered";
                              interfaceType = "none_registered";
                           }
                           else
                           {
                              DesignInterfaceArgs[argName_string] = "valid";
                              interfaceType = "valid";
                           }
                        }
                     }
                     else
                        THROW_ERROR("pattern not yet supported: unused arg");
                     if(canBeMovedToBB2 && isRead && !isWrite)
                     {
                        unsigned int destBB = bloc::ENTRY_BLOCK_ID;
                        for(auto bb_succ : sl->list_of_bloc[bloc::ENTRY_BLOCK_ID]->list_of_succ)
                        {
                           if(bb_succ == bloc::EXIT_BLOCK_ID)
                              continue;
                           if(destBB == bloc::ENTRY_BLOCK_ID)
                              destBB = bb_succ;
                           else
                              THROW_ERROR("unexpected pattern");
                        }
                        THROW_ASSERT(destBB != bloc::ENTRY_BLOCK_ID, "unexpected condition");
                        std::string fdName = ENCODE_FDNAME(argName_string, "_Read_", interfaceType);
                        std::vector<std::string> operationsR, operationsW;
                        operationsR.push_back(fdName);
                        std::list<tree_nodeRef> usedStmt_defs;
                        tree_nodeRef readType;
                        for(auto rs : readStmt)
                        {
                           auto rs_node = GET_NODE(rs);
                           auto rs_ga = GetPointer<gimple_assign>(rs_node);
                           usedStmt_defs.push_back(rs_ga->op0);
                           if(!readType)
                              readType = GetPointer<mem_ref>(GET_NODE(rs_ga->op1))->type;
                        }
                        create_Read_function(readStmt.front(), argName_string, tree_nodeRef(), destBB, fdName, argSSANode, aType, readType, usedStmt_defs, tree_man, TM, commonRWSignature);
                        for(auto rs : readStmt)
                           addGimpleNOPxVirtual(rs, TM);
                        create_resource(operationsR, operationsW, argName_string, interfaceType, inputBitWidth, false, fname, n_resources, alignment);
                        modified = true;
                     }
                     else if(isRead && !isWrite)
                     {
                        std::string fdName = ENCODE_FDNAME(argName_string, "_Read_", interfaceType);
                        std::vector<std::string> operationsR, operationsW;
                        unsigned int loadIdIndex = 0;
                        for(auto rs : readStmt)
                        {
                           std::list<tree_nodeRef> usedStmt_defs;
                           auto rs_node = GET_NODE(rs);
                           auto rs_ga = GetPointer<gimple_assign>(rs_node);
                           usedStmt_defs.push_back(rs_ga->op0);
                           std::string instanceFname = fdName + STR(loadIdIndex);
                           operationsR.push_back(instanceFname);
                           create_Read_function(rs, argName_string, rs, rs_ga->bb_index, instanceFname, argSSANode, aType, GetPointer<mem_ref>(GET_NODE(rs_ga->op1))->type, usedStmt_defs, tree_man, TM, commonRWSignature);
                           addGimpleNOPxVirtual(rs, TM);
                           usedStmt_defs.clear();
                           ++loadIdIndex;
                        }
                        create_resource(operationsR, operationsW, argName_string, interfaceType, inputBitWidth, false, fname, n_resources, alignment);
                        modified = true;
                     }
                     else if(canBeMovedToBB2 && isRead && isWrite)
                     {
                        unsigned int destBB = bloc::ENTRY_BLOCK_ID;
                        for(auto bb_succ : sl->list_of_bloc[bloc::ENTRY_BLOCK_ID]->list_of_succ)
                        {
                           if(bb_succ == bloc::EXIT_BLOCK_ID)
                              continue;
                           if(destBB == bloc::ENTRY_BLOCK_ID)
                              destBB = bb_succ;
                           else
                              THROW_ERROR("unexpected pattern");
                        }
                        THROW_ASSERT(destBB != bloc::ENTRY_BLOCK_ID, "unexpected condition");
                        std::string fdName = ENCODE_FDNAME(argName_string, "_Read_", (interfaceType == "ovalid" ? "none" : interfaceType));
                        std::vector<std::string> operationsR, operationsW;
                        operationsR.push_back(fdName);
                        std::list<tree_nodeRef> usedStmt_defs;
                        tree_nodeRef readType;
                        for(auto rs : readStmt)
                        {
                           auto rs_node = GET_NODE(rs);
                           auto rs_ga = GetPointer<gimple_assign>(rs_node);
                           usedStmt_defs.push_back(rs_ga->op0);
                           if(!readType)
                              readType = GetPointer<mem_ref>(GET_NODE(rs_ga->op1))->type;
                        }
                        create_Read_function(readStmt.front(), argName_string, tree_nodeRef(), destBB, fdName, argSSANode, aType, readType, usedStmt_defs, tree_man, TM, commonRWSignature);
                        for(auto rs : readStmt)
                           addGimpleNOPxVirtual(rs, TM);
                        unsigned int IdIndex = 0;
                        fdName = ENCODE_FDNAME(argName_string, "_Write_", (interfaceType == "ovalid" ? "valid" : interfaceType));
                        bool isDiffSize = false;
                        unsigned WrittenSize = 0;
                        for(auto ws : writeStmt)
                        {
                           auto ws_node = GET_NODE(ws);
                           auto ws_ga = GetPointer<gimple_assign>(ws_node);
                           if(WrittenSize == 0)
                           {
                              WrittenSize = tree_helper::Size(ws_ga->op1);
                              if(WrittenSize < inputBitWidth)
                                 isDiffSize = true;
                           }
                           else if(WrittenSize != tree_helper::Size(ws_ga->op1) || WrittenSize < inputBitWidth)
                              isDiffSize = true;
                           std::string instanceFname = fdName + STR(IdIndex);
                           operationsW.push_back(instanceFname);
                           create_Write_function(argName_string, ws, instanceFname, ws_ga->op1, aType, GetPointer<mem_ref>(GET_NODE(ws_ga->op0))->type, tree_man, TM, commonRWSignature);
                           ++IdIndex;
                        }
                        create_resource(operationsR, operationsW, argName_string, interfaceType, inputBitWidth, isDiffSize, fname, n_resources, alignment);
                        modified = true;
                     }
                     else if(isRead && isWrite)
                     {
                        std::string fdName = ENCODE_FDNAME(argName_string, "_Read_", (interfaceType == "ovalid" ? "none" : interfaceType));
                        std::vector<std::string> operationsR, operationsW;
                        unsigned int IdIndex = 0;
                        std::list<tree_nodeRef> usedStmt_defs;
                        for(auto rs : readStmt)
                        {
                           auto rs_node = GET_NODE(rs);
                           auto rs_ga = GetPointer<gimple_assign>(rs_node);
                           usedStmt_defs.push_back(rs_ga->op0);
                           std::string instanceFname = fdName + STR(IdIndex);
                           operationsR.push_back(instanceFname);
                           create_Read_function(rs, argName_string, rs, rs_ga->bb_index, instanceFname, argSSANode, aType, GetPointer<mem_ref>(GET_NODE(rs_ga->op1))->type, usedStmt_defs, tree_man, TM, commonRWSignature);
                           addGimpleNOPxVirtual(rs, TM);
                           usedStmt_defs.clear();
                           ++IdIndex;
                        }
                        IdIndex = 0;
                        fdName = ENCODE_FDNAME(argName_string, "_Write_", (interfaceType == "ovalid" ? "valid" : interfaceType));
                        bool isDiffSize = false;
                        unsigned WrittenSize = 0;
                        for(auto ws : writeStmt)
                        {
                           auto ws_node = GET_NODE(ws);
                           auto ws_ga = GetPointer<gimple_assign>(ws_node);
                           if(WrittenSize == 0)
                           {
                              WrittenSize = tree_helper::Size(ws_ga->op1);
                              if(WrittenSize < inputBitWidth)
                                 isDiffSize = true;
                           }
                           else if(WrittenSize != tree_helper::Size(ws_ga->op1) || WrittenSize < inputBitWidth)
                              isDiffSize = true;
                           std::string instanceFname = fdName + STR(IdIndex);
                           operationsW.push_back(instanceFname);
                           create_Write_function(argName_string, ws, instanceFname, ws_ga->op1, aType, GetPointer<mem_ref>(GET_NODE(ws_ga->op0))->type, tree_man, TM, commonRWSignature);
                           ++IdIndex;
                        }
                        create_resource(operationsR, operationsW, argName_string, interfaceType, inputBitWidth, isDiffSize, fname, n_resources, alignment);
                        modified = true;
                     }
                     else if(!isRead && isWrite)
                     {
                        std::vector<std::string> operationsR, operationsW;
                        unsigned int IdIndex = 0;
                        std::string fdName = ENCODE_FDNAME(argName_string, "_Write_", (interfaceType == "ovalid" ? "valid" : interfaceType));
                        bool isDiffSize = false;
                        unsigned WrittenSize = 0;
                        for(auto ws : writeStmt)
                        {
                           auto ws_node = GET_NODE(ws);
                           auto ws_ga = GetPointer<gimple_assign>(ws_node);
                           if(WrittenSize == 0)
                           {
                              WrittenSize = tree_helper::Size(ws_ga->op1);
                              if(WrittenSize < inputBitWidth)
                                 isDiffSize = true;
                           }
                           else if(WrittenSize != tree_helper::Size(ws_ga->op1) || WrittenSize < inputBitWidth)
                              isDiffSize = true;
                           std::string instanceFname = fdName + STR(IdIndex);
                           operationsW.push_back(instanceFname);
                           create_Write_function(argName_string, ws, instanceFname, ws_ga->op1, aType, GetPointer<mem_ref>(GET_NODE(ws_ga->op0))->type, tree_man, TM, commonRWSignature);
                           ++IdIndex;
                        }
                        create_resource(operationsR, operationsW, argName_string, interfaceType, inputBitWidth, isDiffSize, fname, n_resources, alignment);
                        modified = true;
                     }
                     else
                        THROW_ERROR("pattern not yet supported");
                  }
                  else if(interfaceType == "none")
                  {
                     THROW_ERROR("unexpected interface (" + interfaceType + ") for parameter " + argName_string);
                  }
                  else
                  {
                     THROW_ERROR("not yet supported interface (" + interfaceType + ") for parameter " + argName_string);
                  }
               }
            }

            INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--Analyzed function " + fname);
         }
         if(modified)
            function_behavior->UpdateBBVersion();
         return modified ? DesignFlowStep_Status::SUCCESS : DesignFlowStep_Status::UNCHANGED;
      }
   }
   return DesignFlowStep_Status::UNCHANGED;
}

void interface_infer::Initialize()
{
   FunctionFrontendFlowStep::Initialize();
}

bool interface_infer::HasToBeExecuted() const
{
   return bb_version == 0;
}
