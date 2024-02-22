#include "ecspch.h"
#include "EvaluateExpression.h"
#include "BasicTypes.h"
#include "StringUtilities.h"

namespace ECSEngine {

	// --------------------------------------------------------------------------------------------------

	template<typename CharacterType>
	ECS_EVALUATE_EXPRESSION_OPERATORS GetEvaluateExpressionOperatorImpl(CharacterType character) {
		if (character == Character<CharacterType>('+')) {
			return ECS_EVALUATE_EXPRESSION_ADD;
		}
		else if (character == Character<CharacterType>('-')) {
			return ECS_EVALUATE_EXPRESSION_MINUS;
		}
		else if (character == Character<CharacterType>('*')) {
			return ECS_EVALUATE_EXPRESSION_MULTIPLY;
		}
		else if (character == Character<CharacterType>('/')) {
			return ECS_EVALUATE_EXPRESSION_DIVIDE;
		}
		else if (character == Character<CharacterType>('%')) {
			return ECS_EVALUATE_EXPRESSION_MODULO;
		}
		else if (character == Character<CharacterType>('~')) {
			return ECS_EVALUATE_EXPRESSION_NOT;
		}
		else if (character == Character<CharacterType>('|')) {
			return ECS_EVALUATE_EXPRESSION_OR;
		}
		else if (character == Character<CharacterType>('&')) {
			return ECS_EVALUATE_EXPRESSION_AND;
		}
		else if (character == Character<CharacterType>('^')) {
			return ECS_EVALUATE_EXPRESSION_XOR;
		}
		else if (character == Character<CharacterType>('<')) {
			return ECS_EVALUATE_EXPRESSION_LESS;
		}
		else if (character == Character<CharacterType>('>')) {
			return ECS_EVALUATE_EXPRESSION_GREATER;
		}
		else if (character == Character<CharacterType>('=')) {
			return ECS_EVALUATE_EXPRESSION_LOGICAL_EQUALITY;
		}
		else if (character == Character<CharacterType>('!')) {
			return ECS_EVALUATE_EXPRESSION_LOGICAL_NOT;
		}

		return ECS_EVALUATE_EXPRESSION_OPERATOR_COUNT;
	}

	// --------------------------------------------------------------------------------------------------

	ECS_EVALUATE_EXPRESSION_OPERATORS GetEvaluateExpressionOperator(char character)
	{
		return GetEvaluateExpressionOperatorImpl(character);
	}

	// --------------------------------------------------------------------------------------------------

	ECS_EVALUATE_EXPRESSION_OPERATORS GetEvaluateExpressionOperator(wchar_t character)
	{
		return GetEvaluateExpressionOperatorImpl(character);
	}

	// --------------------------------------------------------------------------------------------------

	template<typename CharacterType>
	ECS_EVALUATE_EXPRESSION_OPERATORS GetEvaluateExpressionOperatorImpl(CharacterType character, CharacterType next_character, unsigned int& index) {
		ECS_EVALUATE_EXPRESSION_OPERATORS current = GetEvaluateExpressionOperator(character);
		ECS_EVALUATE_EXPRESSION_OPERATORS next = GetEvaluateExpressionOperator(next_character);

		if (current == next) {
			switch (current) {
			case ECS_EVALUATE_EXPRESSION_AND:
				index++;
				return ECS_EVALUATE_EXPRESSION_LOGICAL_AND;
			case ECS_EVALUATE_EXPRESSION_OR:
				index++;
				return ECS_EVALUATE_EXPRESSION_LOGICAL_OR;
			case ECS_EVALUATE_EXPRESSION_LOGICAL_EQUALITY:
				index++;
				return ECS_EVALUATE_EXPRESSION_LOGICAL_EQUALITY;
				index++;
			case ECS_EVALUATE_EXPRESSION_LESS:
				index++;
				return ECS_EVALUATE_EXPRESSION_SHIFT_LEFT;
			case ECS_EVALUATE_EXPRESSION_GREATER:
				index++;
				return ECS_EVALUATE_EXPRESSION_SHIFT_RIGHT;
			default:
				// Return a failure if both characters are the same but not of the types with
				// double characters or they are both not an operator
				return ECS_EVALUATE_EXPRESSION_OPERATOR_COUNT;
			}
		}

		// Check for logical not equality
		if (current == ECS_EVALUATE_EXPRESSION_LOGICAL_NOT && next == ECS_EVALUATE_EXPRESSION_LOGICAL_EQUALITY) {
			index++;
			return ECS_EVALUATE_EXPRESSION_LOGICAL_INEQUALITY;
		}

		// Check for less equal and greater equal
		if (current == ECS_EVALUATE_EXPRESSION_LESS && next == ECS_EVALUATE_EXPRESSION_LOGICAL_EQUALITY) {
			index++;
			return ECS_EVALUATE_EXPRESSION_LESS_EQUAL;
		}

		if (current == ECS_EVALUATE_EXPRESSION_GREATER && next == ECS_EVALUATE_EXPRESSION_LOGICAL_EQUALITY) {
			index++;
			return ECS_EVALUATE_EXPRESSION_GREATER_EQUAL;
		}

		// Else just return the current
		return current;
	}

	// --------------------------------------------------------------------------------------------------

	ECS_EVALUATE_EXPRESSION_OPERATORS GetEvaluateExpressionOperator(char character, char next_character, unsigned int& index)
	{
		return GetEvaluateExpressionOperatorImpl(character, next_character, index);
	}

	// --------------------------------------------------------------------------------------------------

	ECS_EVALUATE_EXPRESSION_OPERATORS GetEvaluateExpressionOperator(wchar_t character, wchar_t next_character, unsigned int& index)
	{
		return GetEvaluateExpressionOperatorImpl(character, next_character, index);
	}

	// --------------------------------------------------------------------------------------------------

	template<typename CharacterType>
	void GetEvaluateExpressionOperatorsImpl(Stream<CharacterType> characters, CapacityStream<EvaluateExpressionOperator>& operators) {
		for (unsigned int index = 0; index < characters.size; index++) {
			CharacterType character = characters[index];
			CharacterType next_character = index < characters.size - 1 ? characters[index + 1] : Character<CharacterType>('\0');

			ECS_EVALUATE_EXPRESSION_OPERATORS operator_ = GetEvaluateExpressionOperator(character, next_character, index);
			if (operator_ != ECS_EVALUATE_EXPRESSION_OPERATOR_COUNT) {
				// We have a match
				operators.Add({ operator_, index });
			}
		}
	}

	// --------------------------------------------------------------------------------------------------

	void GetEvaluateExpressionOperators(Stream<char> characters, CapacityStream<EvaluateExpressionOperator>& operators)
	{
		GetEvaluateExpressionOperatorsImpl<char>(characters, operators);
	}

	// --------------------------------------------------------------------------------------------------

	void GetEvaluateExpressionOperators(Stream<wchar_t> characters, CapacityStream<EvaluateExpressionOperator>& operators)
	{
		GetEvaluateExpressionOperatorsImpl<wchar_t>(characters, operators);
	}

	// --------------------------------------------------------------------------------------------------

	ECS_EVALUATE_EXPRESSION_OPERATORS ORDER0[] = {
		ECS_EVALUATE_EXPRESSION_LOGICAL_NOT,
		ECS_EVALUATE_EXPRESSION_NOT
	};

	ECS_EVALUATE_EXPRESSION_OPERATORS ORDER1[] = {
		ECS_EVALUATE_EXPRESSION_MULTIPLY,
		ECS_EVALUATE_EXPRESSION_DIVIDE,
		ECS_EVALUATE_EXPRESSION_MODULO
	};

	ECS_EVALUATE_EXPRESSION_OPERATORS ORDER2[] = {
		ECS_EVALUATE_EXPRESSION_ADD,
		ECS_EVALUATE_EXPRESSION_MINUS
	};

	ECS_EVALUATE_EXPRESSION_OPERATORS ORDER3[] = {
		ECS_EVALUATE_EXPRESSION_SHIFT_LEFT,
		ECS_EVALUATE_EXPRESSION_SHIFT_RIGHT
	};

	ECS_EVALUATE_EXPRESSION_OPERATORS ORDER4[] = {
		ECS_EVALUATE_EXPRESSION_LESS,
		ECS_EVALUATE_EXPRESSION_LESS_EQUAL,
		ECS_EVALUATE_EXPRESSION_GREATER,
		ECS_EVALUATE_EXPRESSION_GREATER_EQUAL
	};

	ECS_EVALUATE_EXPRESSION_OPERATORS ORDER5[] = {
		ECS_EVALUATE_EXPRESSION_LOGICAL_EQUALITY,
		ECS_EVALUATE_EXPRESSION_LOGICAL_INEQUALITY
	};

	ECS_EVALUATE_EXPRESSION_OPERATORS ORDER6[] = {
		ECS_EVALUATE_EXPRESSION_AND
	};

	ECS_EVALUATE_EXPRESSION_OPERATORS ORDER7[] = {
		ECS_EVALUATE_EXPRESSION_XOR
	};

	ECS_EVALUATE_EXPRESSION_OPERATORS ORDER8[] = {
		ECS_EVALUATE_EXPRESSION_OR
	};

	ECS_EVALUATE_EXPRESSION_OPERATORS ORDER9[] = {
		ECS_EVALUATE_EXPRESSION_LOGICAL_AND
	};

	ECS_EVALUATE_EXPRESSION_OPERATORS ORDER10[] = {
		ECS_EVALUATE_EXPRESSION_LOGICAL_OR
	};

	Stream<ECS_EVALUATE_EXPRESSION_OPERATORS> OPERATOR_ORDER[] = {
		{ ORDER0, ECS_COUNTOF(ORDER0) },
		{ ORDER1, ECS_COUNTOF(ORDER1) },
		{ ORDER2, ECS_COUNTOF(ORDER2) },
		{ ORDER3, ECS_COUNTOF(ORDER3) },
		{ ORDER4, ECS_COUNTOF(ORDER4) },
		{ ORDER5, ECS_COUNTOF(ORDER5) },
		{ ORDER6, ECS_COUNTOF(ORDER6) },
		{ ORDER7, ECS_COUNTOF(ORDER7) },
		{ ORDER8, ECS_COUNTOF(ORDER8) },
		{ ORDER9, ECS_COUNTOF(ORDER9) },
		{ ORDER10, ECS_COUNTOF(ORDER10) }
	};

	void GetEvaluateExpressionOperatorOrder(Stream<EvaluateExpressionOperator> operators, CapacityStream<unsigned int>& order)
	{
		for (size_t precedence = 0; precedence < ECS_COUNTOF(OPERATOR_ORDER); precedence++) {
			for (size_t index = 0; index < operators.size; index++) {
				for (size_t op_index = 0; op_index < OPERATOR_ORDER[precedence].size; op_index++) {
					if (operators[index].operator_index == OPERATOR_ORDER[precedence][op_index]) {
						order.AddAssert(index);
					}
				}
			}
		}
	}

	// --------------------------------------------------------------------------------------------------

	template<typename CharacterType>
	double EvaluateExpressionValueImpl(Stream<CharacterType> characters, unsigned int& offset) {
		// Verify numbers
		unsigned int first_offset = offset;
		CharacterType first = characters[offset];
		if ((first >= Character<CharacterType>('0') && first <= Character<CharacterType>('9')) || first == Character<CharacterType>('.')) {
			// It is a number
			// keep going
			offset++;
			CharacterType last = characters[offset];
			while (offset < characters.size && (last >= Character<CharacterType>('0') && last <= Character<CharacterType>('9')) ||
				last == Character<CharacterType>('.')) {
				offset++;
				last = characters[offset];
			}

			return ConvertCharactersToDouble(Stream<CharacterType>(characters.buffer + first_offset, offset - first_offset));
		}

		// Verify the boolean true and false
		CharacterType true_chars[] = {
			Character<CharacterType>('t'),
			Character<CharacterType>('r'),
			Character<CharacterType>('u'),
			Character<CharacterType>('e')
		};

		CharacterType false_chars[] = {
			Character<CharacterType>('f'),
			Character<CharacterType>('a'),
			Character<CharacterType>('l'),
			Character<CharacterType>('s'),
			Character<CharacterType>('e')
		};

		if (memcmp(true_chars, characters.buffer + offset, sizeof(true_chars)) == 0) {
			offset += 4;
			return 1.0;
		}
		if (memcmp(false_chars, characters.buffer + offset, sizeof(false_chars)) == 0) {
			offset += 5;
			return 0.0;
		}

		return DBL_MAX;
	}

	double EvaluateExpressionValue(Stream<char> characters, unsigned int& offset)
	{
		return EvaluateExpressionValueImpl<char>(characters, offset);
	}

	// --------------------------------------------------------------------------------------------------

	double EvaluateExpressionValue(Stream<wchar_t> characters, unsigned int& offset)
	{
		return EvaluateExpressionValueImpl<wchar_t>(characters, offset);
	}

	// --------------------------------------------------------------------------------------------------

	template<typename CharacterType>
	static double EvaluateExpressionImpl(Stream<CharacterType> characters) {
		// Start by looking at braces

		// Get the pairs of braces
		ECS_STACK_ADDITION_STREAM(unsigned int, opened_braces, 512);
		ECS_STACK_ADDITION_STREAM(unsigned int, closed_braces, 512);
		// Add an invisible pair of braces, in order to make processing easier
		opened_braces.Add((unsigned int)0);
		FindToken(characters, Character<CharacterType>('('), opened_braces_addition);
		FindToken(characters, Character<CharacterType>(')'), closed_braces_addition);
		closed_braces.Add(characters.size + 1);

		auto get_matching_brace = [=](size_t closed_index) {
			size_t subindex = 0;
			if (closed_index < closed_braces.size - 1) {
				for (; subindex < opened_braces.size - 1; subindex++) {
					if (opened_braces[subindex] < closed_braces[closed_index] && opened_braces[subindex + 1] > closed_braces[closed_index]) {
						break;
					}
				}
			}
			else {
				subindex = 0;
			}

			return subindex;
		};

		// If the closed_braces count is different from opened braces count, fail with DBL_MAX
		if (opened_braces.size != closed_braces.size) {
			return DBL_MAX;
		}

		// Increase the indices by one for the braces in order to keep order between the first invisible
		// pair of braces and the rest
		for (size_t index = 1; index < opened_braces.size; index++) {
			opened_braces[index]++;
			closed_braces[index]++;
		}
		// This one gets incremented when it shouldn't
		closed_braces[closed_braces.size - 1]--;

		ECS_STACK_CAPACITY_STREAM(EvaluateExpressionNumber, variables, 512);
		ECS_STACK_CAPACITY_STREAM(EvaluateExpressionOperator, operators, 512);
		ECS_STACK_CAPACITY_STREAM(unsigned int, operator_order, 512);

		// In case of success, insert the value of a pair evaluation into the 
		// scope variables
		auto insert_brace_value = [&](
			Stream<EvaluateExpressionNumber> scope_variables,
			Stream<EvaluateExpressionOperator> scope_operators,
			unsigned int index,
			double value
			) {
				// Remove all the scope variables and the operators
				size_t scope_offset = scope_variables.buffer - variables.buffer;
				variables.Remove(scope_offset, scope_variables.size);
				size_t operator_offset = scope_operators.buffer - operators.buffer;
				operators.Remove(operator_offset, scope_operators.size);

				size_t insert_index = 0;
				for (; insert_index < variables.size; insert_index++) {
					if (variables[insert_index].index > index) {
						break;
					}
				}
				variables.Insert(insert_index, { value, index });
		};

		// Get all the variables and scope operators
		GetEvaluateExpressionOperators(characters, operators);
		GetEvaluateExpressionNumbers(characters, variables);

		for (size_t index = 0; index < closed_braces.size; index++) {
			operator_order.size = 0;

			size_t opened_brace_index = get_matching_brace(index);
			unsigned int opened_brace_offset = opened_braces[opened_brace_index];
			unsigned int end_brace_offset = closed_braces[index];

			// For all comparisons we must use greater or equal because the parenthese position
			// is offseted by 1

			// Determine the operators and the variables in this scope
			size_t start_operators = 0;
			for (; start_operators < operators.size; start_operators++) {
				if (operators[start_operators].index >= opened_brace_offset) {
					break;
				}
			}
			size_t end_operators = start_operators;
			for (; end_operators < operators.size; end_operators++) {
				if (operators[end_operators].index >= end_brace_offset) {
					break;
				}
			}
			end_operators--;

			size_t start_variables = 0;
			for (; start_variables < variables.size; start_variables++) {
				if (variables[start_variables].index >= opened_brace_offset) {
					break;
				}
			}

			size_t end_variables = start_variables;
			for (; end_variables < variables.size; end_variables++) {
				if (variables[end_variables].index >= end_brace_offset) {
					break;
				}
			}
			end_variables--;

			Stream<EvaluateExpressionOperator> scope_operators = { operators.buffer + start_operators, end_operators - start_operators + 1 };
			Stream<EvaluateExpressionNumber> scope_variables = { variables.buffer + start_variables, end_variables - start_variables + 1 };
			size_t scope_variable_initial_count = scope_variables.size;
			size_t scope_operator_initial_count = scope_operators.size;

			// Use the index of the opened brace for insertion

			// Treat the case (-value) separately
			if (scope_operators.size == 1 && scope_variables.size == 1) {
				if (scope_operators[0].operator_index == ECS_EVALUATE_EXPRESSION_LOGICAL_NOT) {
					insert_brace_value(scope_variables, scope_operators, opened_brace_offset, !(bool)(scope_variables[0].value));
				}
				else if (scope_operators[0].operator_index == ECS_EVALUATE_EXPRESSION_MINUS) {
					insert_brace_value(scope_variables, scope_operators, opened_brace_offset, -scope_variables[0].value);
				}
				else {
					// Not a correct operator for a single value
					return DBL_MAX;
				}
			}
			else {
				// Get the precedence
				GetEvaluateExpressionOperatorOrder(scope_operators, operator_order);
				// Go through the operators, first the *, / and %
				for (size_t operator_index = 0; operator_index < scope_operators.size; operator_index++) {
					// Get the left variable
					unsigned int operator_offset = scope_operators[operator_order[operator_index]].index;
					ECS_EVALUATE_EXPRESSION_OPERATORS op_type = scope_operators[operator_order[operator_index]].operator_index;

					size_t right_index = 0;
					for (; right_index < scope_variables.size; right_index++) {
						if (right_index > 0) {
							if (scope_variables[right_index].index > operator_offset && scope_variables[right_index - 1].index < operator_offset) {
								break;
							}
						}
						else {
							if (scope_variables[right_index].index > operator_offset) {
								break;
							}
						}
					}

					if (right_index == scope_variables.size) {
						// No right value, fail
						return DBL_MAX;
					}

					// If single operator, evaluate now
					if (op_type == ECS_EVALUATE_EXPRESSION_LOGICAL_NOT || op_type == ECS_EVALUATE_EXPRESSION_NOT) {
						scope_variables[right_index].value = EvaluateOperator(op_type, 0.0, scope_variables[right_index].value);
						// We don't need to do anything else, no need to squash down a value
					}
					else {
						if (right_index == 0) {
							// Fail, no left value for double operand operator
							return DBL_MAX;
						}

						size_t left_index = right_index - 1;
						// If the left value is not to the left of the operator fail
						if (scope_variables[left_index].index >= operator_offset) {
							return DBL_MAX;
						}

						// Get the result into the left index and move everything one index lower in order to remove right index
						double result = EvaluateOperator(op_type, scope_variables[left_index].value, scope_variables[right_index].value);
						scope_variables[left_index].value = result;
						scope_variables.Remove(right_index);
					}
				}

				// If after processing all the operators there is more than 1 value, error
				if (scope_variables.size > 1) {
					return DBL_MAX;
				}

				// In order to remove the scope variables appropriately
				scope_variables.size = scope_variable_initial_count;
				scope_operators.size = scope_operator_initial_count;
				insert_brace_value(scope_variables, scope_operators, opened_brace_offset, scope_variables[0].value);
			}

			// Remove the brace from the stream
			opened_braces.Remove(opened_brace_index);
		}

		ECS_ASSERT(variables.size == 1);
		return variables[0].value;
	}

	// --------------------------------------------------------------------------------------------------

	template<typename CharacterType>
	void GetEvaluateExpressionNumbersImpl(Stream<CharacterType> characters, CapacityStream<EvaluateExpressionNumber>& numbers) {
		for (unsigned int index = 0; index < characters.size; index++) {
			// Skip whitespaces
			while (characters[index] == Character<CharacterType>(' ') || characters[index] == Character<CharacterType>('\t')) {
				index++;
			}

			unsigned int start_index = index;
			double value = EvaluateExpressionValue(characters, index);
			if (value != DBL_MAX) {
				numbers.AddAssert({ value, start_index });
			}
		}
	}

	// --------------------------------------------------------------------------------------------------

	void GetEvaluateExpressionNumbers(Stream<char> characters, CapacityStream<EvaluateExpressionNumber>& numbers) {
		GetEvaluateExpressionNumbersImpl(characters, numbers);
	}

	// --------------------------------------------------------------------------------------------------

	void GetEvaluateExpressionNumbers(Stream<wchar_t> characters, CapacityStream<EvaluateExpressionNumber>& numbers) {
		GetEvaluateExpressionNumbersImpl(characters, numbers);
	}

	// --------------------------------------------------------------------------------------------------

	double EvaluateOperator(ECS_EVALUATE_EXPRESSION_OPERATORS operator_, double left, double right)
	{
		switch (operator_)
		{
		case ECS_EVALUATE_EXPRESSION_ADD:
			return left + right;
		case ECS_EVALUATE_EXPRESSION_MINUS:
			return left - right;
		case ECS_EVALUATE_EXPRESSION_MULTIPLY:
			return left * right;
		case ECS_EVALUATE_EXPRESSION_DIVIDE:
			return left / right;
		case ECS_EVALUATE_EXPRESSION_MODULO:
			return (int64_t)left % (int64_t)right;
		case ECS_EVALUATE_EXPRESSION_NOT:
			return ~(int64_t)right;
		case ECS_EVALUATE_EXPRESSION_AND:
			return (int64_t)left & (int64_t)right;
		case ECS_EVALUATE_EXPRESSION_OR:
			return (int64_t)left | (int64_t)right;
		case ECS_EVALUATE_EXPRESSION_XOR:
			return (int64_t)left ^ (int64_t)right;
		case ECS_EVALUATE_EXPRESSION_SHIFT_LEFT:
			return (int64_t)left << (int64_t)right;
		case ECS_EVALUATE_EXPRESSION_SHIFT_RIGHT:
			return (int64_t)left >> (int64_t)right;
		case ECS_EVALUATE_EXPRESSION_LOGICAL_AND:
			return (bool)left && (bool)right;
		case ECS_EVALUATE_EXPRESSION_LOGICAL_OR:
			return (bool)left || (bool)right;
		case ECS_EVALUATE_EXPRESSION_LOGICAL_EQUALITY:
			return left == right;
		case ECS_EVALUATE_EXPRESSION_LOGICAL_INEQUALITY:
			return left != right;
		case ECS_EVALUATE_EXPRESSION_LOGICAL_NOT:
			return !(bool)right;
		case ECS_EVALUATE_EXPRESSION_LESS:
			return left < right;
		case ECS_EVALUATE_EXPRESSION_LESS_EQUAL:
			return left <= right;
		case ECS_EVALUATE_EXPRESSION_GREATER:
			return left > right;
		case ECS_EVALUATE_EXPRESSION_GREATER_EQUAL:
			return left >= right;
		default:
			ECS_ASSERT(false);
		}

		return 0.0;
	}

	// --------------------------------------------------------------------------------------------------

	double EvaluateExpression(Stream<char> characters)
	{
		return EvaluateExpressionImpl<char>(characters);
	}

	// --------------------------------------------------------------------------------------------------

	double EvaluateExpression(Stream<wchar_t> characters) {
		return EvaluateExpressionImpl<wchar_t>(characters);
	}

	// --------------------------------------------------------------------------------------------------

}