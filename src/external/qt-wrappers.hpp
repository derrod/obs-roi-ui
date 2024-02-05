/******************************************************************************
    Copyright (C) 2023 by Lain Bailey <lain@obsproject.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#pragma once

#include <QApplication>
#include <QMessageBox>
#include <QWidget>
#include <QWindow>
#include <QThread>
#include <obs.hpp>

#include <functional>
#include <memory>
#include <vector>

#define QT_UTF8(str) QString::fromUtf8(str, -1)
#define QT_TO_UTF8(str) str.toUtf8().constData()
#define MAX_LABEL_LENGTH 80

class QDataStream;
class QComboBox;
class QWidget;
class QLayout;
class QString;
struct gs_window;
class QLabel;
class QToolBar;

bool QTToGSWindow(QWindow *window, gs_window &gswindow);
void setThemeID(QWidget *widget, const QString &themeID);
