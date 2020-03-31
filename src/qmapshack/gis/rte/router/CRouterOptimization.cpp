/**********************************************************************************************
    Copyright (C) 2019 Henri Hornburg hrnbg@t-online.de

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

#include "CRouterOptimization.h"
#include <gis/rte/router/CRouterSetup.h>
#include <GeoMath.h>
#include <QDebug>
#include <QElapsedTimer>
#include <QRandomGenerator>

CRouterOptimization::CRouterOptimization()
{
    routerOptions = CRouterSetup::self().getOptions();
}

int CRouterOptimization::optimize(SGisLine &line)
{
    /********************************************
    * DEBUG
    * ******************************************/
    QElapsedTimer timer;
    QElapsedTimer totalTimer;
    totalTimer.start();
    qreal timeEveryIterationTest = 0;
    qreal timePreprocessing = 0;
    qreal timeRouter = 0;
    qreal timeDisplay = 0;
    int successfulTries = 0;
    int totalTries = 0;
    /********************************************
    * ******************************************/

    checkRouter();
    if(!CRouterSetup::self().hasFastRouting())
    {
        return -1;
    }
    if(line.length() < 4)
    {
        return 0; //There is nothing to optimize
    }
    timer.start();
    qreal bestCosts = getRealRouteCosts(line);
    qDebug() << "Time needed for first routing: " + QString::number(timer.elapsed());
    if(bestCosts < 0)
    {
        return -1;
    }
    qreal lastWorkingOrderCosts = bestCosts;
    SGisLine lastWorkingOrder = line;


    //Preporcessing in case the user supplies some very bad order. Only uses air distance * factor and already known distances
    timer.start();
    SGisLine newWorkingOrder;
    qreal gain = createNextBestOrder(lastWorkingOrder, newWorkingOrder);
    while(gain < 0)
    {
        lastWorkingOrder = newWorkingOrder;
        gain = createNextBestOrder(lastWorkingOrder, newWorkingOrder);
    }
    timePreprocessing = timer.elapsed();
    timer.start();
    lastWorkingOrderCosts = getRealRouteCosts(lastWorkingOrder);
    qDebug() << "Time needed for preporcessing routing: " + QString::number(timer.elapsed());
    //Test if this was useful at all
    if(bestCosts < lastWorkingOrderCosts)
    {
        lastWorkingOrder = line;
        lastWorkingOrderCosts = bestCosts;
        qDebug() << "Preprocessing was useless";
    }

    int numOfRestarts = 0;
    // The number of needed starting permutations is somewhat arbitrary,
    // but you'd likely need more to find the global optimum if there are more possibilities
    while(numOfRestarts < line.length())
    {
        SGisLine newWorkingOrder;
        timer.start();
        qreal bestInsertionGain = createNextBestOrder(lastWorkingOrder, newWorkingOrder);
        timeEveryIterationTest += timer.elapsed();

        if(bestInsertionGain < 0)
        {
            totalTries++;
            timer.start();
            qreal newWorkingOrderCosts = getRealRouteCosts(newWorkingOrder);
            timeRouter += timer.elapsed();

            if(newWorkingOrderCosts < 0)
            {
                return -1;
            }

            if(newWorkingOrderCosts < lastWorkingOrderCosts)
            {
                successfulTries++;
                lastWorkingOrder = newWorkingOrder;
                lastWorkingOrderCosts = newWorkingOrderCosts;

                if(newWorkingOrderCosts < bestCosts)
                {
                    bestCosts = newWorkingOrderCosts;
                    line = newWorkingOrder;

                    // Do this every time, since changes to the line are displayed and the data is available anyways.
                    // Gives a nice animation on screen if the subpoints are displayed aswell.
                    timer.start();
                    fillSubPts(line);
                    timeDisplay += timer.elapsed();
                }
            }
        }
        else
        {
            numOfRestarts += 1;
            // QRandomgenerator::bounded(highest) is exclusive.
            // However, since we don't want to move start and end, so we need two numbers less andshift them one up
            int index1 = QRandomGenerator::global()->bounded(lastWorkingOrder.length()-2)+1;
            int index2 = QRandomGenerator::global()->bounded(lastWorkingOrder.length()-2)+1;
            while(index1 == index2)
            {
                index2 = QRandomGenerator::global()->bounded(lastWorkingOrder.length()-2)+1;
            }
            std::swap(lastWorkingOrder[index1], lastWorkingOrder[index2]);
            lastWorkingOrderCosts = getRealRouteCosts(lastWorkingOrder);
        }
    }

    qDebug()<<"Total time needed: " + QString::number(totalTimer.elapsed());
    qDebug()<<"Time spent in preprocessing: " + QString::number(timePreprocessing);
    qDebug()<<"Time spent in createNextBestOrder testing every iteration: " + QString::number(timeEveryIterationTest);
    qDebug()<<"Time spent waiting for routing: " + QString::number(timeRouter);
    qDebug()<<"Best costs achieved: " + QString::number(bestCosts);
    qDebug()<<"Percentage of suggestions okay: " + QString::number((qreal)successfulTries/totalTries);
    qDebug()<<"Number of router requests: " + QString::number(totalNumOfRoutes);
    qDebug()<<"Time spent adding subpoints: " + QString::number(timeDisplay);

    //Do this a last time, since it happens that the route is already optimal.
    //Return the return value as this is the last point the code may fail for some odd reason
    return fillSubPts(line);
}

qreal CRouterOptimization::createNextBestOrder(const SGisLine &oldOrder, SGisLine &newOrder)
{
    qreal bestInsertionGain = 0;
    int bestBaseIndex = -1;
    int bestInsertedItemIndex = -1;

    // lastWorkingOrder.length()-2, since we can't use the last two items as base
    for(int baseIndex = 0; baseIndex < oldOrder.length() -2; baseIndex++)
    {
        // Keep start and end fixed
        for(int insertedItemIndex = 1; insertedItemIndex < oldOrder.length() -1; insertedItemIndex++)
        {
            if(baseIndex == insertedItemIndex || baseIndex == insertedItemIndex-1)
            {
                //TODO handle if it starts to cycle or repeat to find the same solution
                continue;
            }

            qreal insertionGain = bestKnownDistance(oldOrder[baseIndex], oldOrder[insertedItemIndex])
                                  + bestKnownDistance(oldOrder[insertedItemIndex], oldOrder[baseIndex +1])
                                  - bestKnownDistance(oldOrder[baseIndex], oldOrder[baseIndex+1])
                                  + bestKnownDistance(oldOrder[insertedItemIndex-1], oldOrder[insertedItemIndex+1])
                                  - bestKnownDistance(oldOrder[insertedItemIndex-1], oldOrder[insertedItemIndex])
                                  - bestKnownDistance(oldOrder[insertedItemIndex], oldOrder[insertedItemIndex+1]);

            if(insertionGain < bestInsertionGain)
            {
                bestBaseIndex = baseIndex;
                bestInsertionGain = insertionGain;
                bestInsertedItemIndex = insertedItemIndex;
            }
        }
    }

    //Change and return even if no improvement happens for possible later algorithm changes
    newOrder = SGisLine(oldOrder);
    if(bestBaseIndex >= 0 && bestInsertedItemIndex >= 0)
    {
        // If the index of the inserted item was smaller than that of the base item,
        // moving it will cause the index of the base to decrease. Thus, we don't add 1 to place it after the base
        if(bestInsertedItemIndex < bestBaseIndex)
        {
            newOrder.move(bestInsertedItemIndex, bestBaseIndex);
        }
        else
        {
            newOrder.move(bestInsertedItemIndex, bestBaseIndex+1);
        }
    }
    return bestInsertionGain;
}

qreal CRouterOptimization::getRealRouteCosts(const SGisLine &line)
{
    qreal costs = 0;
    for(int i = 0; i < line.length()-1; i++)
    {
        const routing_cache_item_t* route = getRoute(line[i].coord, line[i+1].coord);
        if(route == nullptr)
        {
            return -1;
        }
        costs += route->costs;
    }
    return costs;
}

qreal CRouterOptimization::bestKnownDistance(const IGisLine::point_t& start, const IGisLine::point_t& end)
{
    //10 digits after the decimal point in exponential format should be by far enough
    QString start_key=QString::number(start.coord.x(), 'e', 10) + QString::number(start.coord.y(), 'e', 10);
    QString end_key=QString::number(end.coord.x(), 'e', 10) + QString::number(end.coord.y(), 'e', 10);

    if(!routingCache.contains(start_key))
    {
        routingCache[start_key] = QMap<QString, routing_cache_item_t>();
    }

    if(routingCache[start_key].contains(end_key))
    {
        return routingCache[start_key][end_key].costs;
    }
    else
    {
        //Multiply it with the average of the minimum occuring factor and the average factor, to get a reasonable compromise of optimization speed and optimality of results
        return GPS_Math_DistanceQuick(start.coord.x(), end.coord.x(), start.coord.y(), end.coord.y()) * (totalAirToCosts/totalNumOfRoutes + minAirToCostFactor)/2;
    }
}

const CRouterOptimization::routing_cache_item_t* CRouterOptimization::getRoute(const QPointF& start, const QPointF& end)
{
    QString start_key=QString::number(start.x(), 'e', 10) + QString::number(start.y(), 'e', 10);
    QString end_key=QString::number(end.x(), 'e', 10) + QString::number(end.y(), 'e', 10);

    if(!routingCache.contains(start_key))
    {
        routingCache[start_key] = QMap<QString, routing_cache_item_t>();
    }

    if(!routingCache[start_key].contains(end_key))
    {
        routing_cache_item_t cacheItem;
        int response = CRouterSetup::self().calcRoute(start, end, cacheItem.route, &cacheItem.costs);
        if(response < 0)
        {
            return nullptr;
        }
        routingCache[start_key][end_key] = cacheItem;

        qreal airToCostFactor = cacheItem.costs/GPS_Math_DistanceQuick(start.x(), end.x(), start.y(), end.y());
        if( airToCostFactor < minAirToCostFactor || minAirToCostFactor < 0)
        {
            minAirToCostFactor = airToCostFactor;
        }
        totalAirToCosts += airToCostFactor;
        totalNumOfRoutes++;
    }
    return &routingCache[start_key][end_key];
}

int CRouterOptimization::fillSubPts(SGisLine &line)
{
    for(int i = 0; i < line.length()-1; i++)
    {
        line[i].subpts.clear();
        const routing_cache_item_t* route= getRoute(line[i].coord, line[i+1].coord);
        if(route == nullptr)
        {
            return -1;
        }
        for(QPointF point : route->route)
        {
            line[i].subpts<<IGisLine::subpt_t(point);
        }
    }
    return 0;
}

void CRouterOptimization::checkRouter()
{
    if(routerOptions != CRouterSetup::self().getOptions())
    {
        routingCache = QMap<QString, QMap<QString, routing_cache_item_t> >();
        routerOptions = CRouterSetup::self().getOptions();
    }
}
