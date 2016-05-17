/***************************************************************************
 *   kio-locate: KDE I/O Slave for the locate command                      *
 *                                                                         *
 *   Copyright (C) 2010 by Bernd Brandstetter                              *
 *   kde-bbrand@kabelmail.de
 *
 *   Ported to KDE4
 *                                                                         *
 *   Copyright (C) 2005 by Tobi Vollebregt                                 *
 *   tobivollebregt@gmail.com                                              *
 *                                                                         *
 *   Thanks to Google's Summer Of Code Program!                            *
 *                                                                         *
 *   Copyright (C) 2004 by Armin Straub                                    *
 *   linux@arminstraub.de                                                  *
 *                                                                         *
 *   This program was initially written by Michael Schuerig.               *
 *   Although I have completely rewritten it, most ideas are adopted       *
 *   from his original work.                                               *
 *                                                                         *
 *   Copyright (C) 2002 by Michael Schuerig                                *
 *   michael@schuerig.de                                                   *
 *                                                                         *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

/**
 * \mainpage KDE I/O Slave for the locate command
 *
 * \section intro_sec What is kio-locate?
 * kio-locate is a KDE I/O Slave for the locate command.
 * <p>This means that you can use kio-locate by simply typing in konquerors
 * address box. You can e.g. type "locate:index.html" to find all files that
 * contain "index.html" in their name.
 * <p>There's even more: You can use kio-locate in all KDE applications, that
 * accept URLs.
 *
 * \todo If a directory matches then don't add its children.\ At least make
 * this configurable.
 *
 * \todo Check if locate's databases are outdated and warn about that.
 *
 * \todo Provide a means of updating the locate database (configurable
 * of course).
 */

#ifndef KIO_LOCATE_H
#define KIO_LOCATE_H

#include <QString>
#include <QStringList>
#include <QHash>
#include <QList>

#include <kio/global.h>
#include <kio/slavebase.h>

#include <kicon.h>

#include "locater.h"
#include "pattern.h"
#include "UrlUtils.h"
#define KDE_EXPORT   __attribute__ ((visibility("default")))

class QByteArray;
class QUrl;
class LocateItem;
class LocateDirectory;

typedef QList<LocateItem> LocateItems;
typedef QHash<QString, LocateDirectory*> LocateDirectories;
typedef QHashIterator<QString, LocateDirectory*> LocateDirectoriesIterator;

enum LocateCaseSensitivity { caseAuto, caseSensitive, caseInsensitive };
enum LocateCollapsedIcon { iconBlue, iconGreen, iconGrey, iconOrange, iconRed, iconViolet, iconYellow };


/**
 * Implementation of the kioslave for the locate protocol.
 *
 * Enables you to enter "locate:pattern" wherever an URL can be used
 * in KDE.
 */
class LocateProtocol : public QObject, public KIO::SlaveBase
{
	Q_OBJECT
 public:
	/**
	 * Constructor
	 */
	LocateProtocol(const QByteArray &pool_socket, const QByteArray &app_socket);

	virtual ~LocateProtocol();

	/**
	 * Returns the mimetype "inode/directory".
	 * @param url the url to work on
	 */
	virtual void mimetype(const QUrl& url);

	/**
	 * Raises an error so that eyerone notes we are dealing with
	 * directories only.
	 * @param url the url to work on
	 */
	virtual void get(const QUrl& url);

	/**
	 * Marks the url as a directory.
	 * @param url the url to work on
	 */
	virtual void stat(const QUrl& url);

	/**
	 * Searches for the pattern specified in the url.
	 * Every file found is listed.
	 * @param url the url to work on
	 */
	virtual void listDir(const QUrl& url);

	/**
	 * Actually report a hit.
	 * If subItems > 0 then this hit is a "directory subsearch".
	 * @param path the path of the hit
	 * @param subItems the number of hits beneath this one
	 * (or 0 for a regular hit)
	 */
	virtual void addHit(const QString& path, int subItems = 0);

	const LocateRegExp& getRegExp() const;
	int getCollapseDirectoryThreshold() const;

 private slots:
	void processLocateOutput(const QStringList& items);
	void locateFinished();
	void configFinished();
	void updateConfig();

 private:
	Locater m_locater;
	QUrl m_url;

	QString m_locatePattern;
	LocateRegExp m_locateRegExp;    // Equals m_locatePattern, but regexp.
	QString m_locateDirectory;      // Includes a trailing slash.
	LocateRegExpList m_regExps;     // List of extra filtering regexps.

	LocateCaseSensitivity m_caseSensitivity; // For current search.
	bool m_useRegExp;

    // Options
	struct
	{
		LocateCaseSensitivity m_caseSensitivity; // Default case sensitivity.
		int m_collapseDirectoryThreshold; // Maximum number of hits in a directory
		                                  // before a directory search is created
		QString m_collapsedDisplay;       // Format string used for collapsed directories.
		LocateCollapsedIcon m_collapsedIcon; // Icon used for collapsed directories.
		LocateRegExpList m_whiteList;     // Path must match at least one regexp in this list.
		LocateRegExpList m_blackList;     // Path may not match any regexp in this list.
	} m_config;

	bool m_configUpdated;  // Used in config methods to check if config was cancelled or okay'ed.
	QString m_pendingPath; // Must be processed as soon as new output becomes available.

	LocateDirectory *m_baseDir; // The toplevel directory, e.g. "/usr/".
	LocateDirectory *m_curDir;  // The current directory (while locating).

	KIO::UDSEntryList m_entries; // Used to cache a lot of hits and list them all at once.

	QString partToPattern(const QString& part, bool forLocate);
	bool isMatching(const QString& file);
	QString pathToDisplay(const QString& path, int subItems = 0);
	void addPreviousLocateOutput();
	void processPath(const QString &path, const QString &nextPath);

	bool isSearchRequest();
	bool isConfigRequest();
	bool isHelpRequest();

	void searchRequest();
	void configRequest();
	void helpRequest();

	bool isCaseSensitive(const QString& text);

	/**
	 * Composes a locater:... url for the current search parameters
	 * bound to a given directory.
	 * @param directory the directory in which we should be searched
	 * @return the url that envokes the specified search
	 */
	QString makeLocaterUrl(const QString& dir);

	/**
	 * This function has to check whether we are accessed via the standard
	 * locater protocol. If this is not the case we have to redirect
	 * this access to an access via the locater protocol.
     * There are other ways, that I have tried to achieve this:
	 * - Use a search provider. This works perfectly for konqueror but
	 *   not for any other KDE application.
	 * - Really use redirection. But this turned out not to be a good
	 *   solution. Seemed not to work from OpenDialogs ...
	 * - So no we just internally redirect... Works, isn't it?
	 */
	void setUrl(const QUrl& url);

    void outputHtml(const QString& body);
};


/**
 * Internally used class to represent a hit as kio-locate will
 * report.
 *
 * This may either be a path as given by locate or a directory
 * search (if the number of subItems is set to a positive number).
 */
class LocateItem
{
 public:
	LocateItem();
	LocateItem(const QString& path, int subItems);

 public:
	QString m_path;
	int m_subItems;
};


/**
 * Internally used class to represent a directory while kio-locate
 * gathers data from locate.
 *
 * Each directory has a list of files found in just that directory
 * and a list of all subdirectories.
 */
class LocateDirectory
{
 public:
	LocateDirectory(LocateDirectory *parent, const QString& path);
	~LocateDirectory();

	LocateDirectory *addPath(const QString& path);
	void prepareListing(const LocateProtocol* protocol, int skip);
	void listItems(LocateProtocol *protocol);
	void debugTrace(int level = 0);

 public:
	QString m_path;                 // Including trailing slash.
	LocateDirectory *m_parent;
	LocateDirectories m_childs;
	LocateItems m_items;
	int m_itemsCount;
	int m_fullCount;

	LocateDirectory *getSubDirectory(const QString& relPath);
	void addItem(const QString& path);
	int countMatchingItems(const LocateProtocol *protocol, int skip);
};

#endif // KIO_LOCATE_H


