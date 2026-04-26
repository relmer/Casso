#include "Pch.h"

#include "ExpressionEvaluator.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExpressionEvaluator::Evaluate  (stub)
//
////////////////////////////////////////////////////////////////////////////////

ExprResult ExpressionEvaluator::Evaluate (const std::string & expr, const ExprContext & ctx)
{
    ExprResult result = {};
    result.success       = false;
    result.value         = 0;
    result.hasUnresolved = false;
    result.error         = "Expression evaluator not yet implemented";
    return result;
}
