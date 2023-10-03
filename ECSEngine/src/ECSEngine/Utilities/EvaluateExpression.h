#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"

namespace ECSEngine {

	enum ECS_EVALUATE_EXPRESSION_OPERATORS : unsigned char {
		ECS_EVALUATE_EXPRESSION_ADD,
		ECS_EVALUATE_EXPRESSION_MINUS,
		ECS_EVALUATE_EXPRESSION_MULTIPLY,
		ECS_EVALUATE_EXPRESSION_DIVIDE,
		ECS_EVALUATE_EXPRESSION_MODULO,
		ECS_EVALUATE_EXPRESSION_NOT,
		ECS_EVALUATE_EXPRESSION_AND,
		ECS_EVALUATE_EXPRESSION_OR,
		ECS_EVALUATE_EXPRESSION_XOR,
		ECS_EVALUATE_EXPRESSION_SHIFT_LEFT,
		ECS_EVALUATE_EXPRESSION_SHIFT_RIGHT,
		ECS_EVALUATE_EXPRESSION_LOGICAL_AND,
		ECS_EVALUATE_EXPRESSION_LOGICAL_OR,
		ECS_EVALUATE_EXPRESSION_LOGICAL_EQUALITY,
		ECS_EVALUATE_EXPRESSION_LOGICAL_INEQUALITY,
		ECS_EVALUATE_EXPRESSION_LOGICAL_NOT,
		ECS_EVALUATE_EXPRESSION_LESS,
		ECS_EVALUATE_EXPRESSION_LESS_EQUAL,
		ECS_EVALUATE_EXPRESSION_GREATER,
		ECS_EVALUATE_EXPRESSION_GREATER_EQUAL,
		ECS_EVALUATE_EXPRESSION_OPERATOR_COUNT
	};

	struct EvaluateExpressionOperator {
		ECS_EVALUATE_EXPRESSION_OPERATORS operator_index;
		unsigned int index;
	};

	struct EvaluateExpressionNumber {
		double value;
		unsigned int index;
	};

	// logical operators except not it returns only the operator as single
	ECSENGINE_API ECS_EVALUATE_EXPRESSION_OPERATORS GetEvaluateExpressionOperator(char character);

	// logical operators except not it returns only the operator as single
	ECSENGINE_API ECS_EVALUATE_EXPRESSION_OPERATORS GetEvaluateExpressionOperator(wchar_t character);

	// It reports correctly the logical operators. It increments the index if a double operator is found
	ECSENGINE_API ECS_EVALUATE_EXPRESSION_OPERATORS GetEvaluateExpressionOperator(char character, char next_character, unsigned int& index);

	// It reports correctly the logical operators. It increments the index if a double operator is found
	ECSENGINE_API ECS_EVALUATE_EXPRESSION_OPERATORS GetEvaluateExpressionOperator(wchar_t character, wchar_t next_character, unsigned int& index);

	// Fills in the operators and their indices. For double operators (like logical + shifts) it reporst the second character
	ECSENGINE_API void GetEvaluateExpressionOperators(Stream<char> characters, CapacityStream<EvaluateExpressionOperator>& operators);

	// Fills in the operators and their indices. For double operators (like logical + shifts) it reporst the second character
	ECSENGINE_API void GetEvaluateExpressionOperators(Stream<wchar_t> characters, CapacityStream<EvaluateExpressionOperator>& operators);

	// Fills in a buffer with the order in which the operators must be evaluated
	ECSENGINE_API void GetEvaluateExpressionOperatorOrder(Stream<EvaluateExpressionOperator> operators, CapacityStream<unsigned int>& order);

	// If the character at the offset is not a value character, it incremenets the offset and returns DBL_MAX
	// Else it parses the value and incremenets the offset right after the value has ended
	ECSENGINE_API double EvaluateExpressionValue(Stream<char> characters, unsigned int& offset);

	// If the character at the offset is not a value character, it incremenets the offset and returns DBL_MAX
	// Else it parses the value and incremenets the offset right after the value has ended
	ECSENGINE_API double EvaluateExpressionValue(Stream<wchar_t> characters, unsigned int& offset);

	ECSENGINE_API void GetEvaluateExpressionNumbers(Stream<char> characters, CapacityStream<EvaluateExpressionNumber>& numbers);

	ECSENGINE_API void GetEvaluateExpressionNumbers(Stream<wchar_t> characters, CapacityStream<EvaluateExpressionNumber>& numbers);

	// For single operators, the right needs to be specified, the left can be missing
	ECSENGINE_API double EvaluateOperator(ECS_EVALUATE_EXPRESSION_OPERATORS operator_, double left, double right);

	// Returns the value of the constant expression
	ECSENGINE_API double EvaluateExpression(Stream<char> characters);

	// Returns the value of the constant expression
	ECSENGINE_API double EvaluateExpression(Stream<wchar_t> characters);

}