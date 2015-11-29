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

#ifndef LOCATER_H
#define LOCATER_H

#include <QObject>
#include <QStringList>
#include <KProcess>


/**
 * Interface to the locate command.
 *
 * Usage is very simple:
 * - Calling locate searches for the given pattern.
 * - You can then collect the found files by connecting to the signal
 *   found.
 * - When finished the signal finished is emitted.
 */
class Locater : public QObject
{
	Q_OBJECT
 public:
	/**
	 * Constructor
	 */
	explicit Locater(QObject *parent = 0, const char *name = 0);

	virtual ~Locater();

	/**
	 * Starts the search.
	 * @param pattern the pattern to search for
	 * @param ignoreCase whether to ignore case or not
	 * @param regExp whether to treat pattern as a regular expression or not
	 * @return true if locate could be started
	 */
	bool locate(const QString& pattern, bool ignoreCase = false, bool regExp = false);

	/**
	 * Set parameters for using locate.
	 * @param binary the binary to use (default: automatically chosen from
     * slocate, rlocate and locate)
	 * @param additionalArguments additional arguments to use
	 */
	void setupLocate(const QString& binary = "", const QString& additionalArguments = "");

	void stop();

    QString binary() { return m_binary; }
    bool binaryExists() { return m_binaryExists; }

 signals:
	/**
	 * Emitted whenever some new files are found.
	 * @param items a list of the new filenames
	 */
	void found(const QStringList& items);

	/**
	 * Emitted when the search is finished.
	 */
	void finished();

 private slots:
	void gotOutput();
	void finished(int, QProcess::ExitStatus);

 private:
	KProcess m_process;
	QString m_binary;
	QString m_additionalArguments;
    bool m_binaryExists;
};

#endif
