// SPDX-FileCopyrightText: 2026 AJ Ashton <aj@ajashton.ca>
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

namespace tilepeek {

// Registers the bundled Breeze icon themes with QIcon and keeps them in sync
// with the OS light/dark color scheme. Must be called after QGuiApplication
// is constructed.
void installBundledIconThemes();

} // namespace tilepeek
