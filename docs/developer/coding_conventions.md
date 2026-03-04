# WavePSF Coding Conventions

## File Naming
- Lowercase filenames without separation
- Examples: `mainwindow.h`, `psfcalculator.cpp`, `iwavefrontgenerator.h`

## C++ Code Style
- **Indentation**: Always use tabs instead of spaces
- **Class names**: PascalCase (`PSFCalculator`, `MainWindow`)
- **Function/variable names**: camelCase (`calculatePSF`, `zernikeCoeffs`)
- **Constants**: UPPER_CASE (`MAX_ITERATIONS`, `DEFAULT_SIZE`)
- **Private members**: When referencing private members always use `this->`. (reasoning: auto complete feature of the IDE can be used and it becomes clear, even without reading the code or knowing the context, that class members are used)

## Variable types
- Use size_t when it directly represents a size in memory or will be involved in buffer math

## C++ Code Style - Qt Specific

### Signal and Slot Naming
**Slots (Action-oriented):**
- Named like regular methods describing what they do
- Examples: `setParams`, `selectSystemStyle`, `updateDisplayMode`, `saveSettings`

**Signals (Event-oriented):**
- Named to indicate what happened
- Examples: `styleChanged`, `parameterUpdated`, `fileLoaded`, `connectionLost`

### Range-based For Loops
- Always use `qAsConst()` when iterating over Qt containers in range-based for loops:
```cpp
// Correct - prevents container detachment
for (const QString& item : qAsConst(stringList)) {
    // process item
}

// Avoid - can cause container detachment warning
for (const QString& item : stringList) {
    // process item
}
```


## Header Guards
todo

## Comments
todo

## Project Structure
- Place headers in same directory as source files (not separate include/ folder)
- Group related classes in subdirectories: `core/psf/`, `gui/`, etc.
- Keep main.cpp in src/ root

## Error Handling
todo

## Git commit messages
todo