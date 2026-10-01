#pragma once

#include <juce_core/juce_core.h>

// Descriptions carried by internal drag & drop operations. When a drag leaves the
// plugin window the editor turns them into a temporary .mid file for the DAW.
namespace drag
{
    inline const juce::String groove = "groove"; // index = groove index in the library
    inline const juce::String clips  = "clips";  // the song lane's current selection
    inline const juce::String song   = "song";   // the whole song

    inline juce::var make (const juce::String& type, int index = -1)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("type", type);
        o->setProperty ("index", index);
        return juce::var (o);
    }

    inline juce::String typeOf (const juce::var& v) { return v.getProperty ("type", {}).toString(); }
    inline int indexOf (const juce::var& v) { return (int) v.getProperty ("index", -1); }
}
