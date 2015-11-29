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

#ifndef PATTERN_H
#define PATTERN_H

#include <QRegExp>
#include <QString>
#include <QList>

/**
 * Regular Expression adapted to the needs of kio-locate.
 */
class LocateRegExp
{
public:
	/**
	 * Constructor
	 * @param pattern the pattern to start with
	 * @param ignoreCase specifies, if the search should be case sensitive
	 */
	explicit LocateRegExp(const QString& pattern, bool ignoreCase = false);
	LocateRegExp();

	virtual ~LocateRegExp();

	/**
	 * Determines whether a file name is matching this regular expression.
	 * @param file the filename to match
	 */
	virtual bool isMatching(const QString& file) const;

	/**
	 * @return The position of the last match.
	 */
	virtual int getMatchPosition() const;

	/**
	 * @return The length of the last match.
	 */
	virtual int getMatchedLength() const;

	/**
	 * Set the pattern.
	 * @param pattern the pattern to search for. It may be prepended by an
	 * exclamation mark, to invert its meaning.
	 */
	virtual void setPattern(const QString& pattern);

	/**
	 * Get the pattern.
	 * @return search pattern
	 */
	virtual QString getPattern() const;

 private:
	bool m_negated;
	bool m_ignoreCase;
	QRegExp m_regExp;
	QString m_pattern;
};

/**
 * List of regular expressions
 */
class LocateRegExpList: public QList<LocateRegExp>
{
 public:
	virtual ~LocateRegExpList();

	/**
	 * Converts a stringlist into a regexplist.
	 * @param list the stringlist to convert
	 */
	LocateRegExpList& operator = (const QStringList& list);

	/**
	 * Determines whether a file name is matching at least one regular
	 * expression in the list.
	 * @param file the filename to match
	 */
	virtual bool isMatchingOne(const QString& file) const;

	/**
	 * Determines whether a file name is matching all regular expressions
	 * in the list.
	 * @param file the filename to match
	 */
	virtual bool isMatchingAll(const QString& file) const;
};

#endif
