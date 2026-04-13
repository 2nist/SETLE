#pragma once

#include <juce_core/juce_core.h>

struct ParamDef
{
    const char* id { nullptr };
    const char* name { nullptr };
    float minValue { 0.0f };
    float maxValue { 1.0f };
    float defaultValue { 0.0f };
};

class ModuleProcessor
{
public:
    virtual ~ModuleProcessor() = default;

    virtual const char* getModuleId() const noexcept = 0;
    virtual const char* getModuleName() const noexcept = 0;

    virtual const ParamDef* getParamDefs() const noexcept { return nullptr; }
    virtual int getNumParams() const noexcept { return 0; }

    virtual void setParam(juce::StringRef, float) noexcept {}
    virtual float getParam(juce::StringRef) const noexcept { return 0.0f; }

    virtual void getState(juce::MemoryBlock&) const {}
    virtual bool setState(const void*, int) { return true; }
};
