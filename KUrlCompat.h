#include <QUrl>
#include <QString>

class KUrlCompat : public QUrl
{
  /*
   * Do not add class scope variables. This class exists solely to provide convince rappers for deprecated functions.
   */
 public:
     void addQueryItem( const QString& _item, const QString& _value );
     QString queryItemValue( const QString& _item ) const;
};