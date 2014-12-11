/**********************************************************************************************
    Copyright (C) 2014 Oliver Eichler oliver.eichler@gmx.de

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

**********************************************************************************************/

#ifndef CMAINWINDOW_H
#define CMAINWINDOW_H

#include <QMainWindow>
#include <QPointer>
#include "ui_IMainWindow.h"

class CQlgtDb;

class CMainWindow : public QMainWindow, private Ui::IMainWindow
{
    Q_OBJECT
    public:
        CMainWindow();
        virtual ~CMainWindow();

        void stdOut(const QString& str);
        void stdErr(const QString& str);

    private slots:
        void slotSelectSource();
        void slotSelectTarget();


    private:
        QPointer<CQlgtDb> dbQlgt;
};

#endif //CMAINWINDOW_H

