#include <QString>
#include <QUrlQuery>
#include <QUrl>

#include "UrlUtils.h"

void UrlUtils::addQueryItem(QUrl& url, const QString& _item, const QString& _value )
{
    QString item = _item + QLatin1Char('=');
    QString value = QString::fromLatin1(QUrl::toPercentEncoding(_value));
    
    QUrlQuery 	query(url);
    query.addQueryItem(item, value);
    url.setQuery(query);
}

QString UrlUtils::queryItemValue(const QUrl& url, const QString& _item )
{
    QUrlQuery 	query(url);
    
    return query.queryItemValue(_item.toUtf8());
}

void UrlUtils::removeQueryItem(QUrl& url, const QString &key)
{
    QUrlQuery 	query(url);
    
    query.removeQueryItem(key);
    url.setQuery(query);
}