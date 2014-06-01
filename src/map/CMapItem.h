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

#ifndef CMAPITEM_H
#define CMAPITEM_H

#include <QListWidgetItem>
#include <QMutex>

class IMap;

class CMapItem : public QListWidgetItem
{
    public:
        CMapItem(QListWidget * parent);
        virtual ~CMapItem();

        /**
           @brief As the drawing thread is using the list widget to iterate of all maps to draw, all access has to be synchronised.
         */
        static QMutex mutexActiveMaps;

    private:
        friend class CMap;
        /**
           @brief A MD5 hash over the first 1024 bytes of the map file, to identify the map
         */
        QString         key;
        /**
           @brief List of map files forming that particular map
         */
        QStringList     filenames;
        /**
           @brief List of loaded map objects when map is activated.
         */
        QList<IMap*>    files;

};

#endif //CMAPITEM_H

