/*
    SystemSettings - what the Mac's Accessibility settings ask of every app
    (decision 0014): Increase Contrast, for stronger colours. Read from
    NSWorkspace through the Objective-C runtime, so the file stays C++;
    elsewhere it is off. Noterator's SystemSettings, the same code.
*/

#pragma once

namespace ans::system
{

bool increaseContrast();

} // namespace ans::system
