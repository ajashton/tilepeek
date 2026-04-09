# Instructions for Coding Agents

We are building TilePeek, a cross-platform desktop application for inspecting tiled map data in MBTiles and PMTiles format.

Performance and responsiveness are top priority for the user experience. Other heavyweight and web-based options exist for this task, but we want users to be confident that double-clicking a MBTiles or PMTiles file in their file mananger will instantly show them data.

## Coding style

- Follow the C++23 standard, making use of new features where they fit well
- Use Qt6 with QtWidgets for all GUI components
- Use other Qt6 components where sensible and where the standard library does not meet our needs
- Ask before adding any new dependencies
- Aim for a moderate number of unit tests
  - Prioritize tests that capture tricky or hard-to-observe conditions/calculations
  - Prioritize testing parts of the code that are likely to be affected by future changes
  - Most real-world bug fixes should be accompanied by new or updated unit tests
