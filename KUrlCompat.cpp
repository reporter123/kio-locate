#include <QString>
#include <QUrl>

#include "KUrlCompat.h"

void KUrlCompat::addQueryItem( const QString& _item, const QString& _value )
{
    QString item = _item + QLatin1Char('=');
    QString value = QString::fromLatin1(QUrl::toPercentEncoding(_value));
  
    QString strQueryEncoded = QString::fromUtf8(encodedQuery());
    if (!strQueryEncoded.isEmpty())
       strQueryEncoded += QLatin1Char('&');
    strQueryEncoded += item + value;
    setEncodedQuery( strQueryEncoded.toUtf8() );
}

QString KUrlCompat::queryItemValue( const QString& _item ) const
{
    QString strQueryEncoded = encodedQueryItemValue(_item.toUtf8());
    strQueryEncoded.replace( ::QLatin1Char('+'), QLatin1Char(' ') ); // + in queries means space.
    return QUrl::fromPercentEncoding( strQueryEncoded.toUtf8() );
}