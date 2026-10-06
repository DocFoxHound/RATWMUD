#pragma once
// The economy orchestrator's state as JSON (RatwOrchestrator.cpp; saved with the society by RatwWire.cpp).
#include "RatwJsonDoc.h"
#include "RatwOrchestrator.h"

namespace ratw::orchestra
{
bool readDials(const json::Value& doc, Dials& dials, std::string& problem);
json::Value stateJson(const State& state);
State readState(const json::Value& value);
json::Value briefJson(const Brief& brief, bool full);
} // namespace ratw::orchestra
