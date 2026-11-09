#include "SystemSettings.h"

#include <juce_core/juce_core.h>

#if JUCE_MAC
 #include <objc/message.h>
 #include <objc/runtime.h>
#endif

namespace ans::system
{

#if JUCE_MAC
namespace
{
// [[NSWorkspace sharedWorkspace] <selector>], a BOOL property.
bool workspaceFlag (const char* selector)
{
    auto* workspaceClass = objc_getClass ("NSWorkspace");
    if (workspaceClass == nullptr) return false;
    auto shared = reinterpret_cast<id (*) (id, SEL)> (objc_msgSend) (reinterpret_cast<id> (workspaceClass), sel_registerName ("sharedWorkspace"));
    if (shared == nullptr) return false;
    const auto sel = sel_registerName (selector);
    if (! class_respondsToSelector (object_getClass (shared), sel)) return false;
    return reinterpret_cast<BOOL (*) (id, SEL)> (objc_msgSend) (shared, sel);
}
} // namespace

bool increaseContrast() { return workspaceFlag ("accessibilityDisplayShouldIncreaseContrast"); }
#else
bool increaseContrast() { return false; }
#endif

} // namespace ans::system
