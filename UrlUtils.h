#include <QUrl>
#include <QString>

class UrlUtils
{
  /*
   * Do not add class scope variables. This class exists solely to provide convince rappers for some deprecated functions.
   */
 public:
     static void addQueryItem(QUrl& url, const QString& _item, const QString& _value );
     static QString queryItemValue(const QUrl& url, const QString& _item );
     static void removeQueryItem(QUrl& url, const QString &key);
};