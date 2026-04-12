#include "DrumPatternLibrary.h"

namespace setle::gridroll
{

void DrumPatternLibrary::loadFromManifest(const juce::File& manifestFile, const juce::File& patternsRootDir)
{
    patterns.clear();

    if (!manifestFile.existsAsFile())
        return;

    const auto text = manifestFile.loadFileAsString();
    const auto parsed = juce::JSON::parse(text);
    juce::Array<juce::var>* array = nullptr;
    if (parsed.isArray())
    {
        array = parsed.getArray();
    }
    else if (auto* root = parsed.getDynamicObject(); root != nullptr)
    {
        auto patternsVar = root->getProperty("patterns");
        if (patternsVar.isArray())
            array = patternsVar.getArray();
    }

    if (array == nullptr)
        return;

    for (const auto& item : *array)
    {
        const auto* obj = item.getDynamicObject();
        if (obj == nullptr)
            continue;

        DrumPattern pattern;
        pattern.id = obj->getProperty("id").toString();
        pattern.name = obj->getProperty("name").toString();
        pattern.category = obj->getProperty("category").toString();
        if (pattern.category.isEmpty())
            pattern.category = obj->getProperty("role").toString();
        pattern.style = obj->getProperty("style").toString();
        pattern.feel = obj->getProperty("feel").toString();
        pattern.timeSignature = obj->getProperty("time_signature").toString();

        auto relative = obj->getProperty("path").toString();
        if (relative.isEmpty())
            relative = obj->getProperty("file").toString();
        if (relative.isEmpty())
            relative = obj->getProperty("asset").toString();

        if (!relative.isEmpty())
        {
            relative = relative.replace("exported_patterns/", "");
            pattern.midiFile = patternsRootDir.getChildFile(relative);
        }

        if (pattern.id.isNotEmpty() && pattern.midiFile.existsAsFile())
            patterns.push_back(std::move(pattern));
    }
}

std::vector<DrumPattern> DrumPatternLibrary::getPatternsForCategory(const juce::String& category) const
{
    std::vector<DrumPattern> out;
    for (const auto& pattern : patterns)
        if (pattern.category.equalsIgnoreCase(category))
            out.push_back(pattern);

    return out;
}

} // namespace setle::gridroll
