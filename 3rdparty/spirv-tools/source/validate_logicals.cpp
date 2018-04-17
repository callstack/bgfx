// Copyright (c) 2017 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Validates correctness of logical SPIR-V instructions.

#include "validate.h"

#include "diagnostic.h"
#include "opcode.h"
#include "val/instruction.h"
#include "val/validation_state.h"

namespace libspirv {

namespace {

// Returns operand word for given instruction and operand index.
// The operand is expected to only have one word.
inline uint32_t GetOperandWord(const spv_parsed_instruction_t* inst,
                               size_t operand_index) {
  assert(operand_index < inst->num_operands);
  const spv_parsed_operand_t& operand = inst->operands[operand_index];
  assert(operand.num_words == 1);
  return inst->words[operand.offset];
}

// Returns the type id of instruction operand at |operand_index|.
// The operand is expected to be an id.
inline uint32_t GetOperandTypeId(ValidationState_t& _,
                                 const spv_parsed_instruction_t* inst,
                                 size_t operand_index) {
  return _.GetTypeId(GetOperandWord(inst, operand_index));
}
}  // namespace

// Validates correctness of logical instructions.
spv_result_t LogicalsPass(ValidationState_t& _,
                          const spv_parsed_instruction_t* inst) {
  const SpvOp opcode = static_cast<SpvOp>(inst->opcode);
  const uint32_t result_type = inst->type_id;

  switch (opcode) {
    case SpvOpAny:
    case SpvOpAll: {
      if (!_.IsBoolScalarType(result_type))
        return _.diag(SPV_ERROR_INVALID_DATA)
               << "Expected bool scalar type as Result Type: "
               << spvOpcodeString(opcode);

      const uint32_t vector_type = GetOperandTypeId(_, inst, 2);
      if (!vector_type || !_.IsBoolVectorType(vector_type))
        return _.diag(SPV_ERROR_INVALID_DATA)
               << "Expected operand to be vector bool: "
               << spvOpcodeString(opcode);

      break;
    }

    case SpvOpIsNan:
    case SpvOpIsInf:
    case SpvOpIsFinite:
    case SpvOpIsNormal:
    case SpvOpSignBitSet: {
      if (!_.IsBoolScalarType(result_type) && !_.IsBoolVectorType(result_type))
        return _.diag(SPV_ERROR_INVALID_DATA)
               << "Expected bool scalar or vector type as Result Type: "
               << spvOpcodeString(opcode);

      const uint32_t operand_type = GetOperandTypeId(_, inst, 2);
      if (!operand_type || (!_.IsFloatScalarType(operand_type) &&
                            !_.IsFloatVectorType(operand_type)))
        return _.diag(SPV_ERROR_INVALID_DATA)
               << "Expected operand to be scalar or vector float: "
               << spvOpcodeString(opcode);

      if (_.GetDimension(result_type) != _.GetDimension(operand_type))
        return _.diag(SPV_ERROR_INVALID_DATA)
               << "Expected vector sizes of Result Type and the operand to be "
                  "equal: "
               << spvOpcodeString(opcode);

      break;
    }

    case SpvOpFOrdEqual:
    case SpvOpFUnordEqual:
    case SpvOpFOrdNotEqual:
    case SpvOpFUnordNotEqual:
    case SpvOpFOrdLessThan:
    case SpvOpFUnordLessThan:
    case SpvOpFOrdGreaterThan:
    case SpvOpFUnordGreaterThan:
    case SpvOpFOrdLessThanEqual:
    case SpvOpFUnordLessThanEqual:
    case SpvOpFOrdGreaterThanEqual:
    case SpvOpFUnordGreaterThanEqual:
    case SpvOpLessOrGreater:
    case SpvOpOrdered:
    case SpvOpUnordered: {
      if (!_.IsBoolScalarType(result_type) && !_.IsBoolVectorType(result_type))
        return _.diag(SPV_ERROR_INVALID_DATA)
               << "Expected bool scalar or vector type as Result Type: "
               << spvOpcodeString(opcode);

      const uint32_t left_operand_type = GetOperandTypeId(_, inst, 2);
      if (!left_operand_type || (!_.IsFloatScalarType(left_operand_type) &&
                                 !_.IsFloatVectorType(left_operand_type)))
        return _.diag(SPV_ERROR_INVALID_DATA)
               << "Expected operands to be scalar or vector float: "
               << spvOpcodeString(opcode);

      if (_.GetDimension(result_type) != _.GetDimension(left_operand_type))
        return _.diag(SPV_ERROR_INVALID_DATA)
               << "Expected vector sizes of Result Type and the operands to be "
                  "equal: "
               << spvOpcodeString(opcode);

      if (left_operand_type != GetOperandTypeId(_, inst, 3))
        return _.diag(SPV_ERROR_INVALID_DATA)
               << "Expected left and right operands to have the same type: "
               << spvOpcodeString(opcode);

      break;
    }

    case SpvOpLogicalEqual:
    case SpvOpLogicalNotEqual:
    case SpvOpLogicalOr:
    case SpvOpLogicalAnd: {
      if (!_.IsBoolScalarType(result_type) && !_.IsBoolVectorType(result_type))
        return _.diag(SPV_ERROR_INVALID_DATA)
               << "Expected bool scalar or vector type as Result Type: "
               << spvOpcodeString(opcode);

      if (result_type != GetOperandTypeId(_, inst, 2) ||
          result_type != GetOperandTypeId(_, inst, 3))
        return _.diag(SPV_ERROR_INVALID_DATA)
               << "Expected both operands to be of Result Type: "
               << spvOpcodeString(opcode);

      break;
    }

    case SpvOpLogicalNot: {
      if (!_.IsBoolScalarType(result_type) && !_.IsBoolVectorType(result_type))
        return _.diag(SPV_ERROR_INVALID_DATA)
               << "Expected bool scalar or vector type as Result Type: "
               << spvOpcodeString(opcode);

      if (result_type != GetOperandTypeId(_, inst, 2))
        return _.diag(SPV_ERROR_INVALID_DATA)
               << "Expected operand to be of Result Type: "
               << spvOpcodeString(opcode);

      break;
    }

    case SpvOpSelect: {
      uint32_t dimension = 1;
      {
        const Instruction* type_inst = _.FindDef(result_type);
        assert(type_inst);

        const SpvOp type_opcode = type_inst->opcode();
        switch (type_opcode) {
          case SpvOpTypePointer: {
            if (!_.features().variable_pointers &&
                !_.features().variable_pointers_storage_buffer)
              return _.diag(SPV_ERROR_INVALID_DATA)
                     << "Using pointers with OpSelect requires capability "
                     << "VariablePointers or VariablePointersStorageBuffer";
            break;
          }

          case SpvOpTypeVector: {
            dimension = type_inst->word(3);
            break;
          }

          case SpvOpTypeBool:
          case SpvOpTypeInt:
          case SpvOpTypeFloat: {
            break;
          }

          default: {
            return _.diag(SPV_ERROR_INVALID_DATA)
                   << "Expected scalar or vector type as Result Type: "
                   << spvOpcodeString(opcode);
          }
        }
      }

      const uint32_t condition_type = GetOperandTypeId(_, inst, 2);
      const uint32_t left_type = GetOperandTypeId(_, inst, 3);
      const uint32_t right_type = GetOperandTypeId(_, inst, 4);

      if (!condition_type || (!_.IsBoolScalarType(condition_type) &&
                              !_.IsBoolVectorType(condition_type)))
        return _.diag(SPV_ERROR_INVALID_DATA)
               << "Expected bool scalar or vector type as condition: "
               << spvOpcodeString(opcode);

      if (_.GetDimension(condition_type) != dimension)
        return _.diag(SPV_ERROR_INVALID_DATA)
               << "Expected vector sizes of Result Type and the condition to be"
               << " equal: " << spvOpcodeString(opcode);

      if (result_type != left_type || result_type != right_type)
        return _.diag(SPV_ERROR_INVALID_DATA)
               << "Expected both objects to be of Result Type: "
               << spvOpcodeString(opcode);

      break;
    }

    case SpvOpIEqual:
    case SpvOpINotEqual:
    case SpvOpUGreaterThan:
    case SpvOpUGreaterThanEqual:
    case SpvOpULessThan:
    case SpvOpULessThanEqual:
    case SpvOpSGreaterThan:
    case SpvOpSGreaterThanEqual:
    case SpvOpSLessThan:
    case SpvOpSLessThanEqual: {
      if (!_.IsBoolScalarType(result_type) && !_.IsBoolVectorType(result_type))
        return _.diag(SPV_ERROR_INVALID_DATA)
               << "Expected bool scalar or vector type as Result Type: "
               << spvOpcodeString(opcode);

      const uint32_t left_type = GetOperandTypeId(_, inst, 2);
      const uint32_t right_type = GetOperandTypeId(_, inst, 3);

      if (!left_type ||
          (!_.IsIntScalarType(left_type) && !_.IsIntVectorType(left_type)))
        return _.diag(SPV_ERROR_INVALID_DATA)
               << "Expected operands to be scalar or vector int: "
               << spvOpcodeString(opcode);

      if (_.GetDimension(result_type) != _.GetDimension(left_type))
        return _.diag(SPV_ERROR_INVALID_DATA)
               << "Expected vector sizes of Result Type and the operands to be"
               << " equal: " << spvOpcodeString(opcode);

      if (!right_type ||
          (!_.IsIntScalarType(right_type) && !_.IsIntVectorType(right_type)))
        return _.diag(SPV_ERROR_INVALID_DATA)
               << "Expected operands to be scalar or vector int: "
               << spvOpcodeString(opcode);

      if (_.GetDimension(result_type) != _.GetDimension(right_type))
        return _.diag(SPV_ERROR_INVALID_DATA)
               << "Expected vector sizes of Result Type and the operands to be"
               << " equal: " << spvOpcodeString(opcode);

      if (_.GetBitWidth(left_type) != _.GetBitWidth(right_type))
        return _.diag(SPV_ERROR_INVALID_DATA)
               << "Expected both operands to have the same component bit "
                  "width: "
               << spvOpcodeString(opcode);

      break;
    }

    default:
      break;
  }

  return SPV_SUCCESS;
}

}  // namespace libspirv