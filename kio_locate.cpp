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

#include "kio_locate.h"
#include "klocateconfig.h"
#include "ui_klocateconfigwidget.h"
#include "ui_klocateconfigfilterwidget.h"
#include "ui_klocateconfiglocatewidget.h"

#include <algorithm>
#include <stdlib.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>

#include <KApplication>
#include <KComponentData>
#include <KConfigDialog>
#include <KDebug>
#include <KIconLoader>
#include <KLocale>
#include <KUrl>
#include <KUser>
#include <QFile>
#include <kde_file.h>

using namespace KIO;


static const char* queryQuery = "q"; // krazy:exclude=doublequote_chars
static const char* queryDirectory = "directory";
static const char* queryCase = "case";
static const char* queryRegExp = "regexp";

static const char* iconNames[] = {
    "folder", "folder-green", "folder-grey", "folder-orange",
    "folder-red", "folder-violet", "folder-yellow"
};


/////////////////////////////////////////////////////////////////////
// HELPERS

/**
 * Determines if a string contains unescaped wildcard characters.
 * Currently, wildcard characters are: * + ? [ ]
 * For older versions of Konqueror: + behaves identical to *
 * @param s the string to inspect
 */
static bool hasWildcards(const QString& s)
{
    for (int i = 0; i < s.length(); ++i) {
        if ((s[i] == '*' || s[i] == '+' || s[i] == '?' || s[i] == '[' || s[i] == ']') && (i < 1 || s[i-1] != '\\'))
            return true;
    }
    return false;
}


/**
 * Converts a string containing shell wildcard characters (globbing characters)
 * to a regular expression. This function takes care of escaped wildcards and
 * any regexp special characters which happen to be in the string.
 * @param s the string to convert
 * @return the converted string
 */
static QString convertWildcardsToRegExp(QString s)
{
    bool in_set = false;

    // No support for regexp searches.
    // (Konqueror makes passing chars like "/" almost impossible anyway.)
    // Note that this converts actual wildcards to escaped wildcards (\wildcard),
    // and escaped wildcards to 'triple'-escaped wildcards (\\\wildcard).
    s = QRegExp::escape(s);

    // Walk through the string, converting \wildcard to regexp and
    // \\\wildcard back to \wildcard.
    for (int i = 1; i < s.length(); ++i) {
        //kDebug() << s.left(i+1) << endl;
        if (i < 3 || s[i-3] != '\\' || s[i-2] != '\\') {
            // If it was an unescaped character (now possibly escaped once)
            if (s[i-1] == '\\') {
                // If it actually was escaped once
                if (!in_set) {
                    // If it's NOT inside a character set, we need to convert *?[
                    if (s[i] == '*' || s[i] == '+') {
                        s = s.left(i-1) + "[^/]*" + s.mid(i+1);
                        i += 3; // 3 == strlen("[^/]*")-strlen("\\*")
                    } else if (s[i] == '?') {
                        s = s.left(i-1) + "[^/]" + s.mid(i+1);
                        i += 2; // 2 == strlen("[^/]")-strlen("\\*")
                    } else if (s[i] == '[') {
                        s = s.left(i-1) + s.mid(i);
                        --i;
                        in_set = true;
                    }
                } else {
                    // If it's inside a character set, we need to convert ]^
                    // and to unescape everything else.
                    if (s[i] == ']') {
                        s = s.left(i-1) + s.mid(i);
                        --i;
                        in_set = false;
                    } else if (s[i] == '^' && i >= 2 && s[i-2] == '[') {
                        s = s.left(i-1) + s.mid(i);
                        --i;
                    } else {
                        s = s.left(i-1) + s.mid(i);
                    }
                }
            }
        } else {
            // If it's an escaped character, remove the extra escape characters.
            s = s.left(i-3) + s.mid(i-1);
            i -= 2; // 2 == strlen("\\\\")-strlen("")
        }
    }
    return s;
}


/**
 * Determines if path includes a trailing slash.
 * @param path the path to inspect
 */
static bool hasTrailingSlash(const QString& path)
{
    int n = path.length();
    return ((n > 0) && (path[n-1] == '/'));
}


/**
 * Strips a trailing slash / from a path.
 * @param path the path to strip the slash off
 */
static QString stripTrailingSlash(const QString& path)
{
    int n = path.length();
    if ((n > 0) && (path[n-1] == '/')) {
        return path.left(n-1);
    }
    return path;
}


/**
 * Add a trailing slash / to a path if there is none yet.
 * @param path the path to append the slash to
 */
static QString addTrailingSlash(const QString& path)
{
    int n = path.length();
    if ((n > 0) && (path[n-1] == '/')) {
        return path;
    }
    return path + '/';
}


static const UDSEntry pathToUDSEntry(const QString& path, const QString& display,
    const QString& url = QString(), const QString& icon = QString())
{
    UDSEntry entry;
    entry.insert(KIO::UDSEntry::UDS_NAME, display);

    if (!path.isEmpty()) {
        KDE_struct_stat info;
        KDE_lstat(path.toLocal8Bit(), &info);

        entry.insert(KIO::UDSEntry::UDS_SIZE, info.st_size);
        entry.insert(KIO::UDSEntry::UDS_ACCESS, info.st_mode);
        entry.insert(KIO::UDSEntry::UDS_MODIFICATION_TIME, info.st_mtime);
        entry.insert(KIO::UDSEntry::UDS_ACCESS_TIME, info.st_atime);
        entry.insert(KIO::UDSEntry::UDS_CREATION_TIME, info.st_ctime);

        struct passwd * user = getpwuid(info.st_uid);
        struct group * group = getgrgid(info.st_gid);
        entry.insert(KIO::UDSEntry::UDS_USER, ((user != NULL) ? user->pw_name : "???" ));
        entry.insert(KIO::UDSEntry::UDS_GROUP, ((group != NULL) ? group->gr_name : "???" ));

        if (url.isEmpty()) {
            // List an existing file.
            entry.insert(KIO::UDSEntry::UDS_URL, "file:" + path);

            mode_t type = info.st_mode;
            if (S_ISLNK(type)) {
                QString slink;
                char buff[1000];
                int n = readlink(path.toLocal8Bit(), buff, 1000);
                if (n != -1) {
                    buff[n] = 0;
                    slink = buff;
                }
                entry.insert(KIO::UDSEntry::UDS_LINK_DEST, slink);
            } else {
                type &= S_IFMT;
            }
            entry.insert(KIO::UDSEntry::UDS_FILE_TYPE, type);

            if (path.contains("/.")) {
                entry.insert(KIO::UDSEntry::UDS_HIDDEN, 1);
            }
        } else {
            // List a locate link.
            entry.insert(KIO::UDSEntry::UDS_URL, url);
            entry.insert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
        }
    } else {
        entry.insert(KIO::UDSEntry::UDS_URL, url);
    }

    if (!icon.isEmpty()) {
        entry.insert(KIO::UDSEntry::UDS_ICON_NAME, icon);
    }

    return entry;
}


/////////////////////////////////////////////////////////////////////
// INITIALIZATION

LocateProtocol::LocateProtocol(const QByteArray &pool_socket, const QByteArray &app_socket)
    : SlaveBase("kio_locate", pool_socket, app_socket)
{
    kDebug() << "LocateProtocol::LocateProtocol()" << endl;

    connect(&m_locater, SIGNAL(found(const QStringList&)),
             this, SLOT(processLocateOutput(const QStringList&)));
    connect(&m_locater, SIGNAL(finished()),
             this, SLOT(locateFinished()));

    m_baseDir = NULL;
    m_curDir = NULL;
}


LocateProtocol::~LocateProtocol()
{
    kDebug() << "LocateProtocol::~LocateProtocol()" << endl;

    delete m_baseDir;
}


const LocateRegExp& LocateProtocol::getRegExp() const
{
    return m_locateRegExp;
}


int LocateProtocol::getCollapseDirectoryThreshold() const
{
    return m_config.m_collapseDirectoryThreshold;
}


/////////////////////////////////////////////////////////////////////
// KIO STUFF

void LocateProtocol::setUrl(const QUrl& url)
{
    if (url.scheme() != "locater") {
        QString pattern = url.toString();
        pattern = pattern.mid(url.scheme().length() + 1);

        KUrl newUrl;
        newUrl.setProtocol("locater");

        kDebug() << "Pattern: " << pattern;

        if (pattern.isEmpty() || pattern == "/") {
            // Give help.
            newUrl.setPath("help");
        } else if (hasTrailingSlash(pattern)) {
            // Detect auto-completion from within konqueror and "stop"
            // this search.
            newUrl.setPath("autosearch");
            newUrl.addQueryItem(queryQuery, pattern);
        } else if (url.scheme() == "rlocate") {
            // Standard regexp search.
            newUrl.setPath("search");
            newUrl.addQueryItem(queryQuery, pattern);
            newUrl.addQueryItem(queryRegExp, "1");
        } else {
            // Standard wildcard search.
            newUrl.setPath("search");
            newUrl.addQueryItem(queryQuery, pattern);
        }
        m_url = newUrl;

        kDebug() << "Redirect: " << m_url << endl;
    } else {
        m_url = url;
    }
    // Perhaps this will be unnecessary most times, but who knows...
    updateConfig();
}

void LocateProtocol::get(const QUrl& url)
{
    kDebug() << "LocateProtocol::get(" << url << ")" << endl;

    setUrl(url);

    if (isSearchRequest()) {
        if (m_locater.binaryExists()) {
            error(KIO::ERR_IS_DIRECTORY, QString());
        } else {
            QString html = i18n(
                "<h1>\"%1\" could not be started.</h1><p>Please note that "
                "kio-locate can't be used on its own. You need an additional "
                "program for doing searches. Typically this is the command "
                "line tool <i>locate</i> that can be found in many distributions "
                "by default. You can check if the correct tool is used by "
                "looking at the <a href=\"locater:config\">setting</a> \"Locate "
                "Binary\".</p><p>Besides the mentioned tool <i>locate</i>, "
                "kio-locate can use any tool that uses the same syntax. In "
                "particular, it was reported to work with <i>slocate</i> and "
                "<i>rlocate</i></p>.", m_locater.binary());
            outputHtml(html);
        }
    } else if (isConfigRequest()) {
        configRequest();
    } else if (isHelpRequest()) {
        helpRequest();
    } else {
        // What's this?
        error(KIO::ERR_DOES_NOT_EXIST, QString());
    }
}


void LocateProtocol::stat(const QUrl& url)
{
    kDebug() << "LocateProtocol::stat(" << url << ")" << endl ;

    setUrl(url);
    //triggers infine launch loop in dolphin
    if(url == QString("locate:/") || url == QString("locate:")){
      error(KIO::ERR_DOES_NOT_EXIST, QString());
      return;
    }
    
    if (isSearchRequest() || isConfigRequest() || isHelpRequest()) {
	
        bool isDir = isSearchRequest() && m_locater.binaryExists();
        /// \todo Is UDS_NAME used for anything in stat? If so we should
        /// at least strip of the protocol part.
        UDSEntry entry;
        entry.insert(KIO::UDSEntry::UDS_NAME, url.toString());
        entry.insert(KIO::UDSEntry::UDS_FILE_TYPE, isDir ? S_IFDIR : S_IFREG);
        statEntry(entry);
        finished();
        /// \todo Somehow locate: and locate:/ is thought to be a directory
        /// by konqueror anyway. How to change this?
    } else {
        // What's this?
        error(KIO::ERR_DOES_NOT_EXIST, QString());
    }
}


void LocateProtocol::listDir(const QUrl& url)
{
    kDebug() << "LocateProtocol::listDir(" << url << ")" << endl ;

    setUrl(url);

    if (isSearchRequest()) {
        searchRequest();
    } else if (isConfigRequest() || isHelpRequest()) {
        error(KIO::ERR_IS_FILE, QString());
    } else {
        // What's this?
        error(KIO::ERR_DOES_NOT_EXIST, QString());
    }
}


void LocateProtocol::mimetype(const QUrl& url)
{
    kDebug() << "LocateProtocol::mimetype(" << url << ")" << endl ;

    setUrl(url);

    if (isSearchRequest()) {
        if (m_locater.binaryExists()) {
            mimeType("inode/directory");
        } else {
            mimeType("text/html");
        }
    } else if (isConfigRequest() || isHelpRequest()) {
        mimeType("text/html");
    }
    finished();
}


void LocateProtocol::outputHtml(const QString& body)
{
    mimeType("text/html");

    QString theData = "<html><body>" + body + "</body></html>";
    data(theData.toLocal8Bit());
    finished();
}


/////////////////////////////////////////////////////////////////////
// SEARCHING

bool LocateProtocol::isSearchRequest()
{
    return m_url.path() == "search";
}


void LocateProtocol::searchRequest()
{
    // Reset old values.
    m_caseSensitivity = caseAuto;
    m_useRegExp = false;
    m_locatePattern.clear();
    m_locateDirectory.clear();
    m_regExps.clear();
    m_pendingPath.clear();

    delete m_baseDir;
    m_baseDir = NULL;
    m_curDir = NULL;

    updateConfig();

    QString query = m_url.queryItem(queryQuery);
    m_locateDirectory = addTrailingSlash(m_url.queryItem(queryDirectory));

    QString caseSensitivity = m_url.queryItem(queryCase);
    if (caseSensitivity == "sensitive") {
        m_caseSensitivity = caseSensitive;
    } else if (caseSensitivity == "insensitive") {
        m_caseSensitivity = caseInsensitive;
    }

    QString useRegExp = m_url.queryItem(queryRegExp);
    if (!useRegExp.isEmpty() && useRegExp != "0") {
        m_useRegExp = true;
    }

    // Split query into components. The first component is the query
    // for locate. The others are filtering regular expressions. They are
    // delimited by (not escaped) whitespace.
    // If the last component starts with two backslahes \\, then the search
    // is only to be done within the directory following them.
    query = query.simplified();
    int s = 0;
    int n = query.length();
    bool regexp = false;
    QString display;
    for (int i = 0; i <= n; i++) {
        if ((i == n) || ((query[i] == ' ') && (i > 0)
                   && (query[i-1] != '\\') && (i-s > 0))) {
            QString temp = query.mid(s, i-s);
            QString part = partToPattern(temp, s==0);
            if (s == 0) {
                // We don't want to show the escaped regexpified string to
                // the user, so we store the string we get in for later display.
                display = temp;
                // This is the same check as in partToPattern.
                // ie. regexp is used if locate pattern contains wildcards,
                // or regexp searching was enabled.
                regexp = hasWildcards(temp);
                m_locatePattern = part;
            } else {
                // For each regular expression determine if it should be
                // case sensitive.
                m_regExps += LocateRegExp(part, !isCaseSensitive(part));
            }
            s = i+1;
        }
    }

    kDebug() << "Pattern: " << m_locatePattern << endl;
    kDebug() << "Directory: " << m_locateDirectory << endl;

    // We set up the regexp used to see whether the match was in the
    // directory part or the filename part of a path.
    m_locateRegExp = LocateRegExp(convertWildcardsToRegExp(m_locatePattern), !isCaseSensitive(m_locatePattern));

    // Now perform the search...
    infoMessage(i18n("Locating %1 ...", display));

    bool started = m_locater.locate(m_locatePattern, !isCaseSensitive(m_locatePattern), regexp);

    if (!started) {
        kDebug() << "Locate could not be found." << endl;
        finished();
    }
}


bool LocateProtocol::isCaseSensitive(const QString& text)
{
    if (m_caseSensitivity == caseSensitive) {
        return true;
    } else if (m_caseSensitivity == caseInsensitive) {
        return false;
    } else if (m_config.m_caseSensitivity == caseSensitive) {
        return true;
    } else if (m_config.m_caseSensitivity == caseInsensitive) {
        return false;
    } else {
        return text != text.toLower();
    }
}


void LocateProtocol::addHit(const QString& path, int subItems)
{
    // kDebug() << "LocateProtocol::addHit( " << path << ", " << subItems << " )" << endl;
    if (QFile::exists(path)) {
        if (subItems > 0) {
            m_entries += pathToUDSEntry(path, pathToDisplay(path, subItems), makeLocaterUrl(path), iconNames[m_config.m_collapsedIcon]);
        } else {
            m_entries += pathToUDSEntry(path, pathToDisplay(path));
        }
    }
}


void LocateProtocol::addPreviousLocateOutput()
{
    if (m_baseDir == NULL) {
        return;
    }
    // m_baseDir->debugTrace();
    if (m_locateDirectory == "/") {
        // Allow toplevel directories to collapse (e.g. when locating "/usr/").
        m_baseDir->prepareListing(this, 0);
    } else {
        m_baseDir->prepareListing(this, m_locateDirectory.length());
    }
    m_baseDir->listItems(this);
    delete m_baseDir;
    m_baseDir = NULL;
    m_curDir = NULL;

    listEntries(m_entries);
    m_entries.clear();
}


void LocateProtocol::processPath(const QString &path, const QString &nextPath)
{
    if (nextPath.isNull()) {
        // We need to know the next path, so we remember this path for later processing.
        m_pendingPath = path;
    } else if (!nextPath.startsWith(path + '/')) {
        if (isMatching(path)) {
            if ((m_baseDir != NULL) && !path.startsWith(m_baseDir->m_path)) {
                addPreviousLocateOutput();
            }
            // Add path to current directory.
            if (m_baseDir == NULL) {
                int p = path.indexOf('/', 1);
                QString base = path;
                if (p >= 0) {
                    base = path.left(p+1);
                }
                m_baseDir = new LocateDirectory(NULL, base);
                m_curDir = m_baseDir;
            }
            m_curDir = m_curDir->addPath(path);
        }
    }
}


void LocateProtocol::processLocateOutput(const QStringList& items)
{
    // I don't know if this really necessary, but if we were signaled, we'll
    // better stop.
    if (wasKilled()) {
        m_locater.stop();
        return;
    }
    // Go through what we have found.
    QStringList::ConstIterator it = items.begin();
    if (!m_pendingPath.isNull()) {
        processPath(m_pendingPath, *it);
        m_pendingPath.clear();
    }
    for (; it != items.end();) {
        QString path = *it;
        ++it;
        processPath(path, it != items.end() ? *it : QString());
    }
}


void LocateProtocol::locateFinished()
{
    // Add any pending items.
    if (!m_pendingPath.isNull()) {
        processPath(m_pendingPath, "");
        m_pendingPath.clear();
    }
    addPreviousLocateOutput();

    kDebug() << "LocateProtocol::locateFinished" << endl;
    infoMessage(i18nc("Locate processing finished.", "Finished."));
    finished();
}


QString LocateProtocol::partToPattern(const QString& part, bool forLocate)
{
    kDebug() << "BEG part: " << part << endl;
    QString pattern = part;
    // Unescape whitespace.
    pattern.replace("\\ ", " ");
    // Unquote quoted pattern.
    int n = pattern.length(), index;
    if ((n > 1) && (pattern[0] == '"') && (pattern[n-1] == '"')) {
        pattern = pattern.mid(1, n-2);
    }

    // We can't do regular expression matching on the locate pattern,
    // the regular expression format used by locate is incompatible
    // with the format used by QRegExp.
    if (!m_useRegExp || forLocate) {
        // Escape regexp characters for filtering pattern, and for locate,
        // but the latter only if it is actually necessary to pass a regexp to locate.
        // (ie. the pattern contains wildcards.)
        if (!forLocate || hasWildcards(pattern)) {
            pattern = convertWildcardsToRegExp(pattern);
        } else {
            // Special case for locate pattern without wildcards:
            // Unescape all escaped wildcards.
            pattern.replace("\\*", "*");
            pattern.replace("\\+", "+");
            pattern.replace("\\?", "?");
            pattern.replace("\\[", "[");
            pattern.replace("\\]", "]");
        }
    }

    // Special treatment for the pattern used for locate:
    if (forLocate) {
        // Replace ~/ and ~user/ at the beginning (as the shell does)
        if ((pattern.length() > 0) && (pattern[0] == '~')) {
            index = pattern.indexOf('/');
            if (index >= 0) {
                QString name = pattern.mid(1, index-1);
                QString homeDir;
                if (name.isEmpty()) {
                    homeDir = KUser(KUser::UseRealUserID).homeDir();
                } else {
                    homeDir = KUser(name).homeDir();
                }
                if (!homeDir.isEmpty()) {
                    pattern.replace(0, index, homeDir);
                }
            }
        }
        pattern.replace("\\~", "~");
    }
    kDebug() << "END part: " << pattern << endl;
    return pattern;
}


bool LocateProtocol::isMatching(const QString& path)
{
    // The file has to belong to our directory.
    if (!path.startsWith(m_locateDirectory)) {
        return false;
    }
    // And it has to match at least one regular expression in the whitelist.
    if (!m_config.m_whiteList.isMatchingOne(path)) {
        return false;
    }
    // And it may not match any regular expression in the blacklist.
    if (m_config.m_blackList.isMatchingOne(path)) {
        return false;
    }
    // And it has to match against all regular expressions specified in the URL.
    if (!m_regExps.isMatchingAll(path)) {
        return false;
    }
    // And it must not solely match m_locateDirectory.
    return m_locateRegExp.isMatching(path.mid(m_locateDirectory.length()));
}


QString LocateProtocol::pathToDisplay(const QString& path, int subItems)
{
    // Split off the directory part. If it is not just the minimal '/'.
    QString display = path;
    if ((m_locateDirectory != "/") && display.startsWith(m_locateDirectory)) {
        display = display.mid(m_locateDirectory.length());
    }
    if (subItems > 0) {
        // Can't use m_collapsedDisplay.arg(subItems).arg(display); here
        // because user might forget to type %1 or %2, or type it twice.
        // In both cases the result of arg() is undefined.
        QString output = m_config.m_collapsedDisplay, temp;
        temp.setNum(subItems);
        output.replace("%1", temp);
        output.replace("%2", display);
        display = output;
    }
    return display;
}


QString LocateProtocol::makeLocaterUrl(const QString& directory)
{
    KUrl url(m_url);
    url.removeQueryItem(queryDirectory);
    url.addQueryItem(queryDirectory, directory);
    return url.url();
}


/////////////////////////////////////////////////////////////////////
// CONFIG

bool LocateProtocol::isConfigRequest()
{
    return m_url.path() == "config";
}


void LocateProtocol::configRequest()
{
    // This flag is used to show either a "successful" or an "unchanged" message
    // in configFinished().
    m_configUpdated = false;

    // Don't show it twice. During my tests I never got there however.
    if(KConfigDialog::showDialog("settings"))
        return;

    KConfigDialog *dialog = new KConfigDialog(0, "settings", KLocateConfig::self());
    dialog->setFaceType(KPageDialog::List);
    dialog->setWindowTitle(i18n("Configure - kio-locate"));
    dialog->setWindowIcon(SmallIcon("edit-find"));

    Ui::KLocateConfigWidget w1;
    Ui::KLocateConfigFilterWidget w2;
    Ui::KLocateConfigLocateWidget w3;
    QWidget* page1 = new QWidget;
    QWidget* page2 = new QWidget;
    QWidget* page3 = new QWidget;
    w1.setupUi(page1);
    w2.setupUi(page2);
    w3.setupUi(page3);

    for (uint i = 0; i < sizeof(iconNames)/sizeof(char*); i++) {
        w1.kcfg_collapsedIcon->setItemIcon(i, KIcon(iconNames[i]));
    }

    dialog->addPage(page1, i18nc("General settings", "General"), "configure");
    dialog->addPage(page2, i18nc("Filter settings", "Filters"), "view-filter");
    dialog->addPage(page3, i18nc("Locate tool settings", "Locate"), "edit-find");

    // React on user's actions.
    connect(dialog, SIGNAL(settingsChanged(const QString&)), this, SLOT(updateConfig()));
    connect(dialog, SIGNAL(finished()), this, SLOT(configFinished()));

    dialog->exec();

    delete dialog;
}


void LocateProtocol::configFinished()
{
    kDebug() << "LocateProtocol::configFinished" << endl;

    QString html;
    if (m_configUpdated) {
        html = i18n("Configuration successfully updated.");
    } else {
        html = i18n("Configuration unchanged.");
    }
    outputHtml("<h1>" + html + "</h1>");
}


void LocateProtocol::updateConfig()
{
    // It's not needed to update the config if it's still up to date.
    kDebug() << "LocateProtocol::updateConfig" << endl;

    KLocateConfig::self()->readConfig();
    m_config.m_caseSensitivity = (LocateCaseSensitivity) KLocateConfig::caseSensitivity();
    m_config.m_collapseDirectoryThreshold = KLocateConfig::collapseDirectoryThreshold();
    m_config.m_collapsedDisplay = KLocateConfig::collapsedDisplay();
    m_config.m_collapsedIcon = (LocateCollapsedIcon) KLocateConfig::collapsedIcon();
    m_config.m_whiteList = KLocateConfig::whiteList();
    m_config.m_blackList = KLocateConfig::blackList();

    m_locater.setupLocate(KLocateConfig::locateBinary(),
                          KLocateConfig::locateAdditionalArguments());

    m_configUpdated = true;
}


/////////////////////////////////////////////////////////////////////
// HELP

bool LocateProtocol::isHelpRequest()
{
    return m_url.path() == "help";
}


void LocateProtocol::helpRequest()
{
    // Redirect the user to our help documents.
    redirection(KUrl("help:/kioslave/kio-locate/"));
    finished();
}


/////////////////////////////////////////////////////////////////////
// SEARCH STRUCTURES

LocateDirectory::LocateDirectory(LocateDirectory *parent, const QString& path)
{
    m_parent = parent;
    m_path = path;
    m_itemsCount = 0;
}

LocateDirectory::~LocateDirectory()
{
    qDeleteAll(m_childs);
}

LocateDirectory *LocateDirectory::addPath(const QString& path)
{
    if (path.startsWith(m_path)) {
        QString relPath = path.mid(m_path.length());
        int p = relPath.lastIndexOf('/');
        if (p >= 0) {
            LocateDirectory *child = getSubDirectory(relPath.left(p));
            child->addItem(relPath.mid(p+1));
            return child;
        }
        addItem(relPath);
        return this;
    }
    if (m_parent != NULL) {
        return m_parent->addPath(path);
    }
    // This should not happen
    return this;
}


LocateDirectory *LocateDirectory::getSubDirectory(const QString& relPath)
{
    QString base = relPath;
    int p = relPath.indexOf('/');
    if (p >= 0) {
        base = relPath.left(p);
    }
    LocateDirectory *child = m_childs.value(base);
    if (child == NULL) {
        child = new LocateDirectory(this, addTrailingSlash(m_path + base));
        m_childs.insert(base, child);
    }
    if (p >= 0) {
        return child->getSubDirectory(relPath.mid(p+1));
    }
    return child;
}


void LocateDirectory::addItem(const QString& path)
{
    m_items += LocateItem(m_path + path, 0);
    m_itemsCount++;
}


int LocateDirectory::countMatchingItems(const LocateProtocol* protocol, int skip)
{
    int count = 0;
    LocateItems::ConstIterator item = m_items.begin();
    for (; item != m_items.end(); ++item) {
        if ((*item).m_subItems) {
            count += (*item).m_subItems;
        } else if (protocol->getRegExp().isMatching((*item).m_path.mid(skip))) {
            ++count;
        }
    }
    return count;
}


void LocateDirectory::prepareListing(const LocateProtocol* protocol, int skip)
{
    int n = m_path.length(), newSkip = n;
    if (skip > newSkip) newSkip = skip;

    // Recursively walk through all children
    LocateDirectoriesIterator child(m_childs);
    while (child.hasNext()) {
        child.next().value()->prepareListing(protocol, newSkip);
    }

    // Set m_fullCount to the total number of children matching the pattern
    m_fullCount = countMatchingItems(protocol, newSkip);

    // Collapse if directory part matches
    LocateDirectory* parent = m_parent;
    if (parent == NULL) {
        parent = this;
    }
    if (n > skip && protocol->getRegExp().isMatching(m_path.mid(skip))) {
        // Directory part matches
        m_childs.clear();
        m_items.clear();
        m_itemsCount = 0;
        parent->m_items += LocateItem(m_path, m_fullCount);
        ++parent->m_itemsCount;
        if (m_fullCount != 0) {
            parent->m_items += LocateItem(m_path, 0);
            ++parent->m_itemsCount;
        }
    }

    // Collapse if we got too many hits
    int maxHits = protocol->getCollapseDirectoryThreshold();
    if (n > skip && maxHits != 0 && m_itemsCount > maxHits) {
        if (m_parent != NULL) {
            m_parent->m_items += LocateItem(m_path, m_fullCount);
            ++m_parent->m_itemsCount;
        } else {
            m_items.clear();
            m_items += LocateItem(m_path, m_fullCount);
            ++m_itemsCount;
        }
    } else {
        // Propagate items to parent
        // (only root LocateDirectory runs listItems)
        if (m_parent != NULL) {
            m_parent->m_items += m_items;
            m_parent->m_itemsCount += m_itemsCount;
        }
    }
}


void LocateDirectory::listItems(LocateProtocol *protocol)
{
    LocateItems::ConstIterator item = m_items.begin();
    for (; item != m_items.end(); ++item) {
        protocol->addHit(stripTrailingSlash((*item).m_path), (*item).m_subItems);
    }
}


void LocateDirectory::debugTrace(int level)
{
    QString ws;
    ws.fill(' ', level);
    kDebug() << ws << m_path << endl;
    LocateItems::ConstIterator item = m_items.begin();
    for (; item != m_items.end(); ++item) {
        kDebug() << ws << "+ " << (*item).m_path << endl;
    }
    LocateDirectoriesIterator child(m_childs);
    while (child.hasNext()) {
        child.next().value()->debugTrace(level+2);
    }
}


LocateItem::LocateItem()
{
}


LocateItem::LocateItem(const QString& path, int subItems)
{
    m_path = path;
    m_subItems = subItems;
}


/////////////////////////////////////////////////////////////////////
// INVOCATION

extern "C"
{
    int KDE_EXPORT kdemain(int argc, char **argv)
    {
        // We use KApplication instead of KInstance here, because we use a
        // config dialog and such gui stuff.
        QApplication app(argc, argv);
        KComponentData componentData("kio_locate");

        kDebug() << "*** Starting kio_locate " << endl;

        if (argc != 4) {
            kDebug() << "Usage: kio_locate  protocol domain-socket1 domain-socket2" << endl;
            exit(-1);
        }

        LocateProtocol slave(argv[2], argv[3]);
        slave.dispatchLoop();

        kDebug() << "*** kio_locate Done" << endl;
        return 0;
    }
}


#include "kio_locate.moc"
