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

#include "gis/rte/router/IRouter.h"

IRouter::IRouter(bool fastRouting, QWidget *parent)
    : QWidget(parent)
    , fastRouting(fastRouting)
{
}

IRouter::~IRouter()
{
}

int IRouter::calcRoute(const QVector<QPointF> &points, QVector<QPolygonF> &coords, QVector<qreal> &costs)
{
    //Dummy implementation for cases where there is no gain
    for(int i = 0; i < points.length()-1; i++)
    {
        QPolygonF poly;
        qreal cost;
        if(calcRoute(points[i], points[i+1], poly, &cost) == -1)
        {
            return -1;
        }
        coords.append(poly);
        costs.append(costs);
    }
    return coords.length();
}

