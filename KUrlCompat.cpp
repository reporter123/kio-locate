#include <QString>
#include <QUrlQuery>
#include <QUrl>

#include "KUrlCompat.h"

void KUrlCompat::addQueryItem( const QString& _item, const QString& _value )
{
    QString item = _item + QLatin1Char('=');
    QString value = QString::fromLatin1(QUrl::toPercentEncoding(_value));
    
    QUrlQuery 	query(*this);
    query.addQueryItem(item, value);
    setQuery(query);
}

QString KUrlCompat::queryItemValue( const QString& _item ) const
{
    QUrlQuery 	query(*this);
    
    return query.queryItemValue(_item.toUtf8());
}

void KUrlCompat::removeQueryItem(const QString &key)
{
    QUrlQuery 	query(*this);
    
    query.removeQueryItem(key);
    setQuery(query);
}