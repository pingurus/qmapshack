/**********************************************************************************************
    Copyright (C) 2014 Oliver Eichler oliver.eichler@gmx.de
    Copyright (C) 2017 Norbert Truchsess norbert.truchsess@t-online.de
    Copyright (C) 2020 Henri Hornburg hrnbg@t-online.de

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

#include "canvas/CCanvas.h"
#include "CMainWindow.h"
#include "gis/CGisWorkspace.h"
#include "gis/rte/CGisItemRte.h"
#include "gis/rte/router/brouter/CRouterBRouterInfo.h"
#include "gis/rte/router/brouter/CRouterBRouterLocal.h"
#include "gis/rte/router/brouter/CRouterBRouterSetup.h"
#include "gis/rte/router/brouter/CRouterBRouterSetupWizard.h"
#include "gis/rte/router/CRouterBRouter.h"
#include "gis/wpt/CGisItemWpt.h"
#include "GeoMath.h"
#include "helpers/CProgressDialog.h"
#include "helpers/CSettings.h"
#include <QtNetwork>
#include <QtWidgets>

CRouterBRouter* CRouterBRouter::pSelf;

CRouterBRouter::CRouterBRouter(QWidget *parent)
    : IRouter(false, parent)
{
    pSelf = this;
    setupUi(this);

    labelBRouterWarning->hide();

    setup = new CRouterBRouterSetup(this);
    setup->load();

    connect(toolSetup, &QToolButton::clicked, this, &CRouterBRouter::slotToolSetupClicked);
    connect(toolProfileInfo, &QToolButton::clicked, this, &CRouterBRouter::slotToolProfileInfoClicked);
    connect(setup, &CRouterBRouterSetup::sigDisplayOnlineProfileFinished, this, &CRouterBRouter::slotDisplayProfileInfo);
    connect(setup, &CRouterBRouterSetup::sigError, this, &CRouterBRouter::slotDisplayError);

    comboAlternative->addItem(tr("original"), "0");
    comboAlternative->addItem(tr("first alternative"), "1");
    comboAlternative->addItem(tr("second alternative"), "2");
    comboAlternative->addItem(tr("third alternative"), "3");

    networkAccessManager = new QNetworkAccessManager(this);
    connect(networkAccessManager, &QNetworkAccessManager::finished, this, &CRouterBRouter::slotRequestFinished);
    connect(setup, &CRouterBRouterSetup::sigVersionChanged, this, &CRouterBRouter::slotVersionChanged);

    timerCloseStatusMsg = new QTimer(this);
    timerCloseStatusMsg->setSingleShot(true);
    timerCloseStatusMsg->setInterval(5000);
    connect(timerCloseStatusMsg, &QTimer::timeout, this, &CRouterBRouter::slotCloseStatusMsg);

    routerSetup = dynamic_cast<CRouterSetup*>(parent);

    connect(toolConsole,       &QToolButton::clicked, this, &CRouterBRouter::slotToggleConsole);
    connect(toolToggleBRouter, &QToolButton::clicked, this, &CRouterBRouter::slotToggleBRouter);
    connect(pushBRouterError,  &QPushButton::clicked, this, &CRouterBRouter::slotClearError);

    textBRouterOutput->setVisible(false);
    textBRouterError->setVisible(false);
    pushBRouterError->setVisible(false);

    localBRouter = new CRouterBRouterLocal(*this);

    updateDialog();

    SETTINGS;

    cfg.beginGroup("Route/brouter");
    comboProfile->setCurrentIndex(cfg.value("profile", 0).toInt());
    checkFastRecalc->setChecked(cfg.value("fastRecalc", false).toBool() && (setup->installMode == CRouterBRouterSetup::eModeLocal));
    comboAlternative->setCurrentIndex(cfg.value("alternative", 0).toInt());
    cfg.endGroup();
}

CRouterBRouter::~CRouterBRouter()
{
    isShutdown = true;
    if (!localBRouter->isBRouterNotRunning())
    {
        localBRouter->stopBRouter();
    }
    SETTINGS;
    cfg.beginGroup("Route/brouter");
    cfg.setValue("profile", comboProfile->currentIndex());
    cfg.setValue("alternative", comboAlternative->currentIndex());
    cfg.setValue("fastRecalc", checkFastRecalc->isChecked());
    cfg.endGroup();
}

void CRouterBRouter::slotToolSetupClicked()
{
    localBRouter->stopBRouter();
    CRouterBRouterSetupWizard setupWizard;
    setupWizard.exec();
    slotClearError();
    setup->load();
    getBRouterVersion();
    updateDialog();
}

void CRouterBRouter::slotToolProfileInfoClicked() const
{
    const int index = comboProfile->currentIndex();
    if (index > -1)
    {
        setup->displayProfileAsync(setup->getProfiles().at(index));
    }
}

void CRouterBRouter::slotDisplayError(const QString &error, const QString &details) const
{
    textBRouterError->setText(error);
    if (!details.isEmpty())
    {
        textBRouterError->append(details);
    }
    QTextCursor cursor = textBRouterError->textCursor();
    cursor.movePosition(QTextCursor::Start);
    textBRouterError->setTextCursor(cursor);
    textBRouterError->setVisible(true);
    textBRouterOutput->setVisible(false);
    pushBRouterError->setVisible(true);
}

void CRouterBRouter::slotClearError()
{
    textBRouterError->clear();
    textBRouterError->setVisible(false);
    pushBRouterError->setVisible(false);
    localBRouter->clearBRouterError();
}

void CRouterBRouter::slotDisplayProfileInfo(const QString &profile, const QString &content)
{
    slotClearError();
    CRouterBRouterInfo info;
    info.setLabel(profile);
    info.setInfo(content);
    info.exec();
}

void CRouterBRouter::setupLocalDir(QString localDir)
{
    if (setup->isLocalBRouterDefaultDir())
    {
        setup->localDir = localDir;
        setup->save();
    }
}

void CRouterBRouter::updateDialog() const
{
    if (setup->installMode == CRouterBRouterSetup::eModeLocal)
    {
        routerSetup->setRouterTitle(CRouterSetup::RouterBRouter, tr("BRouter (offline)"));
        labelCopyrightBRouter->setVisible(true);
        labelCopyrightBRouterWeb->setVisible(false);
    }
    else
    {
        Q_ASSERT(setup->installMode == CRouterBRouterSetup::eModeOnline);
        routerSetup->setRouterTitle(CRouterSetup::RouterBRouter, tr("BRouter (online)"));
        labelCopyrightBRouter->setVisible(false);
        labelCopyrightBRouterWeb->setVisible(true);
    }
    comboProfile->clear();
    bool hasItems = false;
    for(const QString& profile : setup->getProfiles())
    {
        comboProfile->addItem(profile, profile);
        hasItems = true;
    }
    comboProfile->setEnabled(hasItems);
    toolProfileInfo->setEnabled(hasItems);
    comboAlternative->setEnabled(hasItems);
    updateBRouterStatus();
}

void CRouterBRouter::slotCloseStatusMsg() const
{
    timerCloseStatusMsg->stop();
    CCanvas * canvas = CMainWindow::self().getVisibleCanvas();
    if(canvas)
    {
        canvas->slotTriggerCompleteUpdate(CCanvas::eRedrawGis);
        canvas->reportStatus("BRouter", "");
    }
}

QString CRouterBRouter::getOptions()
{
    return QString(tr("profile: %1, alternative: %2")
                   .arg(comboProfile->currentData().toString())
                   .arg(comboAlternative->currentData().toInt()+1));
}

void CRouterBRouter::routerSelected()
{
    getBRouterVersion();
}

bool CRouterBRouter::hasFastRouting()
{
    return setup->installMode == CRouterBRouterSetup::eModeLocal && checkFastRecalc->isChecked();
}

QNetworkRequest CRouterBRouter::getRequest(const QVector<QPointF> &routePoints, const QList<IGisItem*> &nogos, CRouterBRouter::brouter_formats_e format) const
{
    QString lonLats;

    for(const QPointF &pt : routePoints)
    {
        if (!lonLats.isEmpty())
        {
            lonLats.append("|");
        }
        lonLats.append(QString("%1,%2").arg(pt.x(), 0, 'f', 6).arg(pt.y(), 0, 'f', 6));
    }

    QString nogoStr;
    QString nogoPolygons;
    QString nogoPolylines;

    for(IGisItem* const & item : nogos)
    {
        switch(item->type())
        {
        case IGisItem::eTypeWpt:
        {
            CGisItemWpt * wpt = static_cast<CGisItemWpt*>(item);
            const qreal& rad = wpt->getProximity();
            if (rad != NOFLOAT && rad > 0.)
            {
                const QPointF& pos = wpt->getPosition();
                if (!nogoStr.isEmpty())
                {
                    nogoStr.append("|");
                }
                nogoStr.append(QString("%1,%2,%3").arg(pos.x(), 0, 'f', 6).arg(pos.y(), 0, 'f', 6).arg(rad, 0, 'f', 0));
            }
            break;
        }

        case IGisItem::eTypeOvl:
        case IGisItem::eTypeRte:
        case IGisItem::eTypeTrk:
        {
            IGisLine* line = dynamic_cast<IGisLine*>(item);
            Q_ASSERT(line != nullptr);
            QPolygonF polygon;
            line->getPolylineDegFromData(polygon);
            QString nogoPoints;
            for (const QPointF& point : polygon)
            {
                if (!nogoPoints.isEmpty())
                {
                    nogoPoints.append(",");
                }
                nogoPoints.append(QString("%1,%2").arg(point.x(), 0, 'f', 6).arg(point.y(), 0, 'f', 6));
            }
            if (item->type() == IGisItem::eTypeOvl)
            {
                if (!nogoPolygons.isEmpty())
                {
                    nogoPolygons.append("|");
                }
                nogoPolygons.append(QString("%1").arg(nogoPoints));
            }
            else
            {
                if (!nogoPolylines.isEmpty())
                {
                    nogoPolylines.append("|");
                }
                nogoPolylines.append(QString("%1").arg(nogoPoints));
            }
            break;
        }

        default:
        {
            Q_ASSERT(false);
        }
        }
    }

    QUrlQuery urlQuery;
    urlQuery.addQueryItem("lonlats", lonLats.toLatin1());
    if (!nogoStr.isEmpty())
    {
        urlQuery.addQueryItem("nogos", nogoStr.toLatin1());
    }
    if (!nogoPolygons.isEmpty())
    {
        urlQuery.addQueryItem("polygons", nogoPolygons.toLatin1());
    }
    if (!nogoPolylines.isEmpty())
    {
        urlQuery.addQueryItem("polylines", nogoPolylines.toLatin1());
    }
    urlQuery.addQueryItem("profile", comboProfile->currentData().toString());
    urlQuery.addQueryItem("alternativeidx", comboAlternative->currentData().toString());
    if(format == eBrouterFormatGeoJson)
    {
        urlQuery.addQueryItem("format", "geojson");
        urlQuery.addQueryItem("exportWaypoints", "1");
    }
    else
    {
        urlQuery.addQueryItem("format", "gpx");
    }
    QUrl url = setup->getServiceUrl();
    url.setQuery(urlQuery);

    return QNetworkRequest(url);
}

int CRouterBRouter::calcRoute(const QPointF& p1, const QPointF& p2, QPolygonF& coords, qreal *costs)
{
    if(!hasFastRouting())
    {
        return -1;
    }

    const QVector<QPointF> points = {p1*RAD_TO_DEG, p2*RAD_TO_DEG};

    QList<IGisItem*> nogos;
    CGisWorkspace::self().getNogoAreas(nogos);

    return synchronousRequest(points, nogos, coords, costs);
}

int CRouterBRouter::calcRoute(const QVector<QPointF>& points, QVector<QPolygonF>& coords, QVector<qreal>&costs)
{
    if(!hasFastRouting())
    {
        return -1;
    }

    QVector<QPointF> points_deg;
    for(const QPointF& point: points)
    {
        points_deg.append(point*RAD_TO_DEG);
    }

    QList<IGisItem*> nogos;
    CGisWorkspace::self().getNogoAreas(nogos);

    return synchronousRequest(points_deg, nogos, coords, costs);
}

int CRouterBRouter::synchronousRequest(const QVector<QPointF> &points, const QList<IGisItem *> &nogos, QByteArray &response, CRouterBRouter::brouter_formats_e format)
{
    if (!mutex.tryLock())
    {
        // skip further on-the-fly-requests as long a previous request is still running
        return -1;
    }

    if (setup->installMode == CRouterBRouterSetup::eModeLocal && localBRouter->isBRouterNotRunning())
    {
        localBRouter->startBRouter();
    }

    synchronous = true;

    QNetworkReply * reply = networkAccessManager->get(getRequest(points, nogos, format));

    reply->setProperty("options", getOptions());
    reply->setProperty("time", QDateTime::currentDateTimeUtc().toMSecsSinceEpoch());

    CProgressDialog progress(tr("Calculate route with %1").arg(getOptions()), 0, NOINT, nullptr);

    QEventLoop eventLoop;
    connect(&progress, &CProgressDialog::rejected, reply, &QNetworkReply::abort);
    connect(reply, &QNetworkReply::finished, &eventLoop, &QEventLoop::quit);
    //Processing userinputevents in local eventloop would cause a SEGV when clicking 'abort' of calling LineOp
    eventLoop.exec(QEventLoop::ExcludeUserInputEvents);

    const QNetworkReply::NetworkError& netErr = reply->error();
    if (netErr == QNetworkReply::RemoteHostClosedError && nogos.size() > 1 && !isMinimumVersion(1, 4, 10))
    {
        throw tr("this version of BRouter does not support more then 1 nogo-area");
    }
    else if(netErr != QNetworkReply::NoError)
    {
        throw reply->errorString();
    }
    else
    {
        slotClearError();

        response = reply->readAll();

        if(response.isEmpty())
        {
            throw tr("response is empty");
        }
    }


    reply->deleteLater();
    slotCloseStatusMsg();
    mutex.unlock();

    return 0;
}

int CRouterBRouter::synchronousRequest(const QVector<QPointF> &points, const QList<IGisItem *> &nogos, QPolygonF &coords, qreal* costs)
{
    QByteArray res;
    if(synchronousRequest(points, nogos, res) == -1)
    {
        return -1;
    }

    try
    {
        QDomDocument xml;
        xml.setContent(res);

        const QDomElement &xmlGpx = xml.documentElement();
        if(xmlGpx.isNull() || xmlGpx.tagName() != "gpx")
        {
            throw QString(res);
        }
        else
        {
            setup->parseBRouterVersion(xmlGpx.attribute("creator"));

            // read the shape
            const QDomNodeList &xmlLatLng = xmlGpx.firstChildElement("trk")
                                            .firstChildElement("trkseg")
                                            .elementsByTagName("trkpt");
            for(int n = 0; n < xmlLatLng.size(); n++)
            {
                const QDomElement &elem   = xmlLatLng.item(n).toElement();
                coords << QPointF();
                QPointF &point = coords.last();
                point.setX(elem.attribute("lon").toFloat()*DEG_TO_RAD);
                point.setY(elem.attribute("lat").toFloat()*DEG_TO_RAD);
            }

            //find costs of route (copied and adapted from CGisItemRte::setResultFromBrouter)
            if(costs != nullptr)
            {
                const QDomNodeList &nodes = xml.childNodes();
                for (int i = 0; i < nodes.count(); i++)
                {
                    const QDomNode &node = nodes.at(i);
                    if (node.isComment())
                    {
                        const QString &commentTxt = node.toComment().data();
                        // ' track-length = 180864 filtered ascend = 428 plain-ascend = -172 cost=270249 '
                        const QRegExp rxAscDes("(\\s*track-length\\s*=\\s*)(-?\\d+)(\\s*)(filtered ascend\\s*=\\s*-?\\d+)(\\s*)(plain-ascend\\s*=\\s*-?\\d+)(\\s*)(cost\\s*=\\s*)(-?\\d+)(\\s*)");
                        int pos = rxAscDes.indexIn(commentTxt);
                        if (pos > -1)
                        {
                            bool ok;
                            *costs = rxAscDes.cap(9).toDouble(&ok);
                            if(!ok)
                            {
                                *costs = -1;
                            }
                        }
                        break;
                    }
                }
            }
        }
    }
    catch(const QString& msg)
    {
        coords.clear();
        if(!msg.isEmpty())
        {
            mutex.unlock();
            throw tr("Bad response from server: %1").arg(msg);
        }
    }

    return coords.size();
}
int CRouterBRouter::synchronousRequest(const QVector<QPointF> &points, const QList<IGisItem *> &nogos, QVector<QPolygonF> &coords, QVector<qreal> &costs)
{
    QByteArray res;
    if(synchronousRequest(points, nogos, res, eBrouterFormatGeoJson) == -1)
    {
        return -1;
    }

    try
    {
        const QRegExp rxCosts("\\[\"(\\d+)\", \"(\\d+)\", \"\\d+\", \"(\\d+)\", \"(\\d+)\", \"(\\d+)\", \"(\\d+)\", \"(\\d+)\", \"(\\d+)\",");
        QVector<std::tuple<QPointF, qreal, QPolygonF, qreal> > segments;
        int pos = 0;
        while ((pos = rxCosts.indexIn(res, pos)) != -1)
        {
            pos += rxCosts.matchedLength();
            qreal lon = rxCosts.cap(1).toDouble()/1000000;
            qreal lat = rxCosts.cap(2).toDouble()/1000000;
            qreal dist = rxCosts.cap(3).toDouble();
            qreal costPerKm = rxCosts.cap(4).toDouble();
            qreal elevCost = rxCosts.cap(5).toDouble();
            qreal turnCost = rxCosts.cap(6).toDouble();
            qreal nodeCost = rxCosts.cap(7).toDouble();
            qreal initialCost = rxCosts.cap(8).toDouble();
            segments.append(std::tuple<QPointF, qreal, QPolygonF, qreal>(QPointF(lon*DEG_TO_RAD, lat*DEG_TO_RAD),
                                                                         dist/1000*costPerKm+elevCost+turnCost+nodeCost+initialCost,
                                                                         QPolygonF(),
                                                                         dist));
        }

        const QRegExp rxPoints("\\[(\\d+\\.\\d+), (\\d+\\.\\d+), \\d+\\.\\d+\\]");
        QVector<QPointF> subPoints;
        pos = 0;
        while((pos = rxPoints.indexIn(res, pos)) != -1)
        {
            pos += rxPoints.matchedLength();
            subPoints.append(QPointF(rxPoints.cap(1).toDouble()*DEG_TO_RAD, rxPoints.cap(2).toDouble()*DEG_TO_RAD));
        }

        if(subPoints.isEmpty() || segments.isEmpty())
        {
            throw QString(res);
        }

        {//Add scope to prevent human mistakes due to segmentIndex variable being defined
            int segmentIndex = 0;
            //Attach subPoints to their segments
            for(int i = 0; i < subPoints.length(); i++)
            {
                std::get<2>(segments[segmentIndex]).append(subPoints[i]);
                //At dead ends for some reason the coordinates of the segment don't match any of the subpoints
                //The coordinate at the segment marks the end of this segment
                if((std::abs(std::get<0>(segments[segmentIndex]).x() - subPoints[i].x()) < 0.000001 * DEG_TO_RAD &&
                    std::abs(std::get<0>(segments[segmentIndex]).y() - subPoints[i].y()) < 0.000001 * DEG_TO_RAD) ||
                   (i < subPoints.length()-1 && i > 1 && subPoints[i-1] == subPoints [i+1]))
                {
                    segmentIndex++;
                }
            }
        }

        int lastMinDistSegEndIndex = -1;
        int lastminDistSubPointIndex = -1;
        //Find Polygons and costs for partial routes
        for(int i = 0; i < points.length()-1; i++)
        {
            qreal minDist = -1;
            int minDistSegEndIndex = 0;
            for(int segmentIndex = lastMinDistSegEndIndex + 1; segmentIndex < segments.length(); segmentIndex++)
            {
                qreal dist = GPS_Math_DistanceQuick(points[i+1].x()*DEG_TO_RAD, points[i+1].y()*DEG_TO_RAD, std::get<0>(segments[segmentIndex]).x(), std::get<0>(segments[segmentIndex]).y());
                if(minDist < 0 || dist < minDist)
                {
                    minDist = dist;
                    minDistSegEndIndex = segmentIndex;
                }
            }

            //The current minimal distance is that to the endpoint of the chosen segment
            int minDistSubPointIndex = std::get<2>(segments[minDistSegEndIndex]).length() - 1;
            {
                int newMinDistSegEndIndex = minDistSegEndIndex;
                for(int adjacent = 0; adjacent < 2; adjacent++)
                {
                    if(minDistSegEndIndex+adjacent < segments.length())
                    {
                        for(int subPointIndex = 0; subPointIndex < std::get<2>(segments[minDistSegEndIndex+adjacent]).length(); subPointIndex++)
                        {
                            QPointF subPoint = std::get<2>(segments[minDistSegEndIndex+adjacent])[subPointIndex];
                            qreal dist = GPS_Math_DistanceQuick(points[i+1].x()*DEG_TO_RAD, points[i+1].y()*DEG_TO_RAD,
                                                                subPoint.x(), subPoint.y());
                            if(dist < minDist)
                            {
                                minDist = dist;
                                newMinDistSegEndIndex = minDistSegEndIndex+adjacent;
                                minDistSubPointIndex = subPointIndex;
                            }
                        }
                    }
                }
                minDistSegEndIndex = newMinDistSegEndIndex;
            }

            qreal cost = 0;
            QPolygonF poly;

            if(lastMinDistSegEndIndex >= 0 &&  lastminDistSubPointIndex+1 < std::get<2>(segments[lastMinDistSegEndIndex]).length())
            {
                QPointF lastPoint = std::get<2>(segments[lastMinDistSegEndIndex])[lastminDistSubPointIndex+1];
                qreal dist = 0;
                for(int subPointIndex = lastminDistSubPointIndex+1; subPointIndex < std::get<2>(segments[lastMinDistSegEndIndex]).length(); subPointIndex++)
                {
                    QPointF thisPoint = std::get<2>(segments[lastMinDistSegEndIndex])[subPointIndex];
                    dist+=GPS_Math_DistanceQuick(thisPoint.x(), thisPoint.y(), lastPoint.x(), lastPoint.y());
                    poly.append(thisPoint);
                    lastPoint=thisPoint;
                }
                cost += std::get<1>(segments[lastMinDistSegEndIndex])*dist/std::get<3>(segments[lastMinDistSegEndIndex]);
            }

            for(int segmentIndex = lastMinDistSegEndIndex+1; segmentIndex < minDistSegEndIndex; segmentIndex++)
            {
                cost += std::get<1>(segments[segmentIndex]);
                poly.append(std::get<2>(segments[segmentIndex]));
            }

            QPointF lastPoint = std::get<2>(segments[minDistSegEndIndex])[0];
            qreal dist = 0;
            for(int subPointIndex = 0; subPointIndex <= minDistSubPointIndex; subPointIndex++)
            {
                QPointF thisPoint = std::get<2>(segments[minDistSegEndIndex])[subPointIndex];
                dist+=GPS_Math_DistanceQuick(thisPoint.x(), thisPoint.y(), lastPoint.x(), lastPoint.y());
                poly.append(thisPoint);
                lastPoint=thisPoint;
            }
            cost += std::get<1>(segments[minDistSegEndIndex])*dist/std::get<3>(segments[minDistSegEndIndex]);
            lastMinDistSegEndIndex = minDistSegEndIndex;

            if(poly.length() < 2)
            {
                qDebug()<<"Very short path in fetching multiple routes at once";
            }
            //Add the subroute to the vectors provided
            coords.append(poly);
            costs.append(cost);
        }
    }
    catch(const QString& msg)
    {
        coords.clear();
        if(!msg.isEmpty())
        {
            mutex.unlock();
            throw tr("Bad response from server: %1").arg(msg);
        }
    }

    return coords.size();
}
void CRouterBRouter::calcRoute(const IGisItem::key_t& key)
{
    mutex.lock();
    if (setup->installMode == CRouterBRouterSetup::eModeLocal && localBRouter->isBRouterNotRunning())
    {
        localBRouter->startBRouter();
    }
    CGisItemRte *rte = dynamic_cast<CGisItemRte*>(CGisWorkspace::self().getItemByKey(key));
    if(nullptr == rte)
    {
        mutex.unlock();
        return;
    }

    QList<IGisItem*> nogos;
    CGisWorkspace::self().getNogoAreas(nogos);

    rte->reset();

    slotCloseStatusMsg();

    QVector<QPointF> points;
    for(const CGisItemRte::rtept_t &pt : rte->getRoute().pts)
    {
        points << QPointF(pt.lon, pt.lat);
    }

    synchronous = false;

    QNetworkReply * reply = networkAccessManager->get(getRequest(points, nogos));

    reply->setProperty("key.item", key.item);
    reply->setProperty("key.project", key.project);
    reply->setProperty("key.device", key.device);
    reply->setProperty("options", getOptions());
    reply->setProperty("time", QDateTime::currentDateTimeUtc().toMSecsSinceEpoch());

    CCanvas * canvas = CMainWindow::self().getVisibleCanvas();
    if(canvas)
    {
        canvas->slotTriggerCompleteUpdate(CCanvas::eRedrawGis);
        canvas->reportStatus("BRouter", tr("<b>BRouter</b><br/>Routing request sent to server. Please wait..."));
    }

    progress = new CProgressDialog(tr("Calculate route with %1").arg(getOptions()), 0, NOINT, this);

    connect(progress, &CProgressDialog::rejected, reply, &QNetworkReply::abort);
}

void CRouterBRouter::slotRequestFinished(QNetworkReply* reply)
{
    if (synchronous)
    {
        return;
    }

    delete progress;

    try
    {
        const QNetworkReply::NetworkError& netErr = reply->error();
        if (netErr == QNetworkReply::RemoteHostClosedError && reply->property("nogos").toInt() > 1 && !isMinimumVersion(1, 4, 10))
        {
            throw tr("this version of BRouter does not support more then 1 nogo-area");
        }
        else if(netErr != QNetworkReply::NoError)
        {
            throw reply->errorString();
        }

        const QByteArray &res = reply->readAll();
        reply->deleteLater();

        if(res.isEmpty())
        {
            throw tr("response is empty");
        }

        slotClearError();

        QDomDocument xml;
        xml.setContent(res);

        const QDomElement &xmlGpx = xml.documentElement();
        if(xmlGpx.isNull() || xmlGpx.tagName() != "gpx")
        {
            throw QString(res);
        }

        IGisItem::key_t key;
        key.item    = reply->property("key.item").toString();
        key.project = reply->property("key.project").toString();
        key.device  = reply->property("key.device").toString();
        qint64 time = reply->property("time").toLongLong();
        time = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch() - time;

        CGisItemRte * rte = dynamic_cast<CGisItemRte*>(CGisWorkspace::self().getItemByKey(key));
        if(rte != nullptr)
        {
            rte->setResultFromBRouter(xml, reply->property("options").toString() + tr("<br/>Calculation time: %1s").arg(time/1000.0, 0, 'f', 2));
        }
    }
    catch(const QString& msg)
    {
        if(!msg.isEmpty())
        {
            CCanvas * canvas = CMainWindow::self().getVisibleCanvas();
            if(canvas)
            {
                canvas->reportStatus("BRouter", tr("<b>BRouter</b><br/>Bad response from server:<br/>%1").arg(msg));
            }
            timerCloseStatusMsg->start();
            reply->deleteLater();
            mutex.unlock();
            return;
        }
    }

    slotCloseStatusMsg();
    mutex.unlock();
}

void CRouterBRouter::slotToggleConsole() const
{
    textBRouterOutput->setVisible(!textBRouterOutput->isVisible());
    bool showError = localBRouter->isBRouterError() && !textBRouterOutput->isVisible();
    textBRouterError->setVisible(showError);
    pushBRouterError->setVisible(showError);
}

void CRouterBRouter::slotToggleBRouter() const
{
    if (localBRouter->isBRouterNotRunning())
    {
        localBRouter->startBRouter();
    }
    else
    {
        localBRouter->stopBRouter();
    }
}

void CRouterBRouter::getBRouterVersion()
{
    if (setup->installMode == CRouterBRouterSetup::eModeLocal)
    {
        localBRouter->getBRouterVersion();
    }
    else
    {
        setup->loadOnlineVersion();
    }
}

void CRouterBRouter::slotVersionChanged()
{
    if (setup->versionMajor != NOINT && setup->versionMinor != NOINT && setup->versionPatch != NOINT)
    {
        labelBRouter->setToolTip(tr("BRouter (Version %1.%2.%3)")
                                 .arg(setup->versionMajor)
                                 .arg(setup->versionMinor)
                                 .arg(setup->versionPatch));
    }
    else
    {
        labelBRouter->setToolTip("BRouter: (failed to read version)");
    }
}

bool CRouterBRouter::isMinimumVersion(int major, int minor, int patch) const
{
    if (setup->versionMajor == NOINT || setup->versionMinor == NOINT || setup->versionPatch == NOINT)
    {
        return false;
    }
    if (setup->versionMajor > major)
    {
        return true;
    }
    if (setup->versionMajor < major)
    {
        return false;
    }
    if (setup->versionMinor > minor)
    {
        return true;
    }
    if (setup->versionMinor < minor)
    {
        return false;
    }
    if (setup->versionPatch > patch)
    {
        return true;
    }
    if (setup->versionPatch < patch)
    {
        return false;
    }
    return true;
}

void CRouterBRouter::updateBRouterStatus() const
{
    if (isShutdown)
    {
        return;
    }

    labelBRouterWarning->hide();
    if (setup->installMode == CRouterBRouterSetup::eModeLocal)
    {
        localBRouter->updateLocalBRouterStatus();
    }
    else
    {
        Q_ASSERT(setup->installMode == CRouterBRouterSetup::eModeOnline);
        labelStatus->setText(tr("online"));
        toolConsole->setVisible(false);
        toolToggleBRouter->setVisible(false);
        checkFastRecalc->setVisible(false);
        textBRouterOutput->clear();
        textBRouterOutput->setVisible(false);
    }
}
