#pragma once
#include "dashboard_spec/Document.h"
namespace dashboard_spec {
class Validator;
// Public entry always uses the validator's bounded document/schema pipeline.
Error SemanticPass(const Document &document, const Validator &validator);
class SemanticPassAccess {
    friend class Validator;
    static Error Run(const Document &document);
};
} // namespace dashboard_spec
