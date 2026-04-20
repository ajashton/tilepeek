// SPDX-FileCopyrightText: 2026 AJ Ashton <aj@ajashton.ca>
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QDialog>

class QFrame;
class QWidget;

class AboutDialog : public QDialog {
    Q_OBJECT
public:
    explicit AboutDialog(QWidget* parent = nullptr);

private:
    static QFrame* makeDivider(QWidget* parent);
    static QString qtVersionLine();
    static QWidget* buildAboutTab(QWidget* parent);
    static QWidget* buildLicenseTab(QWidget* parent);
    static QWidget* buildSystemInfoTab(QWidget* parent);
};
