/**********************************************************************************************
    Copyright (C) 2021 Henri Hornburg <pingurus@t-online.de>

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

#include "gis/Poi.h"
#include "poi/CRawPoi.h"

#include <QRegularExpression>

CRawPoi::CRawPoi(const QStringList &data, const QPointF &coordinates, const quint64 &key, const QString& category)
    : category(category), coordinates(coordinates), data(data), key(key)
{
    for(const QRegularExpression& regex : {QRegularExpression("name:" + QLocale::system().name() + "=(.+)", QRegularExpression::UseUnicodePropertiesOption),
                                           QRegularExpression("name:en=(.+)", QRegularExpression::UseUnicodePropertiesOption),
                                           QRegularExpression("name=(.+)", QRegularExpression::UseUnicodePropertiesOption),
                                           QRegularExpression("name:\\w\\w=(.+)", QRegularExpression::UseUnicodePropertiesOption),
                                           QRegularExpression("brand=(.+)", QRegularExpression::UseUnicodePropertiesOption),
                                           QRegularExpression("operator=(.+)", QRegularExpression::UseUnicodePropertiesOption)})
    {
        const QStringList& matches = data.filter(regex);
        if (!matches.isEmpty())
        {
            name = regex.match(matches[0]).captured(1).trimmed();
            break;
        }
    }
}

const QString &CRawPoi::getCategory() const
{
    return category;
}

const QPointF &CRawPoi::getCoordinates() const
{
    return coordinates;
}

const QStringList &CRawPoi::getData() const
{
    return data;
}

const quint64 &CRawPoi::getKey() const
{
    return key;
}

const QString &CRawPoi::getName(bool replaceEmptyByCategory) const
{
    if(replaceEmptyByCategory && name.isEmpty())
    {
        return category;
    }
    return name;
}

poi_t CRawPoi::toPoi() const
{
    poi_t poi;
    poi.pos = coordinates;
    poi.name = getName();

    const QRegularExpression emailRegex ("(contact:email)=(.+)", QRegularExpression::UseUnicodePropertiesOption);
    const QRegularExpression phoneRegex ("(contact:phone|phone|contact:mobile)=(.+)", QRegularExpression::UseUnicodePropertiesOption);
    const QRegularExpression websiteRegex ("(contact:website|website|url)=(.+)", QRegularExpression::UseUnicodePropertiesOption);
    const QRegularExpression osmRegex ("(osm_id)=(P|W|R)/(.+)", QRegularExpression::UseUnicodePropertiesOption);
    const QRegularExpression wikipediaRegex ("(.*:)?(wikipedia)(:[A-z]{2})?=(.+)", QRegularExpression::UseUnicodePropertiesOption);
    const QRegularExpression wikidataRegex ("(.*:)?(wikidata)=(.+)", QRegularExpression::UseUnicodePropertiesOption);
    poi.desc = "";
    bool firstLine = true;
    for(QString line : data)
    {
        if(firstLine)
        {
            firstLine = false;
        }
        else
        {
            poi.desc += "<br>\n";
        }

        QRegularExpressionMatch emailMatch = emailRegex.match(line);
        if(emailMatch.hasMatch())
        {
            poi.desc += emailMatch.captured(1).trimmed() + "=<a href=\"mailto:" + emailMatch.captured(2).trimmed() + "\">" + emailMatch.captured(2).trimmed() + "</a>";
            continue;
        }
        QRegularExpressionMatch phoneMatch = phoneRegex.match(line);
        if(phoneMatch.hasMatch())
        {
            poi.desc += phoneMatch.captured(1).trimmed() + "=<a href=\"tel:" + phoneMatch.captured(2).trimmed() + "\">" + phoneMatch.captured(2).trimmed() + "</a>";
            continue;
        }
        QRegularExpressionMatch websiteMatch = websiteRegex.match(line);
        if(websiteMatch.hasMatch())
        {
            poi.desc += websiteMatch.captured(1).trimmed() + "=<a href=\"" + websiteMatch.captured(2).trimmed() + "\">" + websiteMatch.captured(2).trimmed() + "</a>";
            continue;
        }
        QRegularExpressionMatch osmMatch = osmRegex.match(line);
        if(osmMatch.hasMatch())
        {
            poi.desc += osmMatch.captured(1).trimmed() + "=<a href=\"https://www.openstreetmap.org/";
            if(osmMatch.captured(2) == "P")
            {
                poi.desc += "node/";
            }
            else if(osmMatch.captured(2) == "W")
            {
                poi.desc += "way/";
            }
            else
            {
                poi.desc += "relation/";
            }
            poi.desc += osmMatch.captured(3).trimmed() + "\">" + osmMatch.captured(3).trimmed() + "</a>";
            continue;
        }
        QRegularExpressionMatch wikipediaMatch = wikipediaRegex.match(line);
        if(wikipediaMatch.hasMatch())
        {
            poi.desc += wikipediaMatch.captured(1) + wikipediaMatch.captured(2) + wikipediaMatch.captured(3);
            if(wikipediaMatch.captured(3).isEmpty())
            {
                poi.desc += "=<a href=\"https://wikipedia.org/wiki/";
            }
            else
            {
                poi.desc += "=<a href=\"https://" + wikipediaMatch.captured(3).remove(0, 1) + ".wikipedia.org/wiki/";
            }
            poi.desc += wikipediaMatch.captured(4).trimmed() + "\">" + wikipediaMatch.captured(4).trimmed() + "</a>";
            continue;
        }
        QRegularExpressionMatch wikidataMatch = wikidataRegex.match(line);
        if(wikidataMatch.hasMatch())
        {
            poi.desc += wikidataMatch.captured(1) + wikidataMatch.captured(2) + "=<a href=\"https://wikidata.org/wiki/" + wikidataMatch.captured(3).trimmed() + "\">" + wikidataMatch.captured(3).trimmed() + "</a>";
            continue;
        }
        poi.desc += line;
    }
    return poi;
}

