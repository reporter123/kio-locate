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

#include "pattern.h"
#include <QStringList>
#include <KDebug>


LocateRegExp::LocateRegExp(const QString& pattern, bool ignoreCase)
{
	m_ignoreCase = ignoreCase;
	setPattern(pattern);
}


LocateRegExp::LocateRegExp()
{
}


LocateRegExp::~LocateRegExp()
{
}


bool LocateRegExp::isMatching(const QString& file) const
{
	bool matching = m_regExp.indexIn(file) >= 0;
	if (m_negated) {
		matching = !matching;
	}
	return matching;
}


int LocateRegExp::getMatchPosition() const
{
	return m_regExp.pos();
}


int LocateRegExp::getMatchedLength() const
{
	return m_regExp.matchedLength();
}


void LocateRegExp::setPattern(const QString& pattern)
{
	m_negated = false;
	m_pattern = pattern;
	if ((m_pattern.length() > 0) && (m_pattern[0] == '!')) {
		m_negated = true;
		m_pattern = m_pattern.mid(1, m_pattern.length()-1);
	}
	m_regExp = QRegExp(m_pattern, m_ignoreCase ? Qt::CaseInsensitive : Qt::CaseSensitive);
}


QString LocateRegExp::getPattern() const
{
	return m_pattern;
}


LocateRegExpList::~LocateRegExpList()
{
}


LocateRegExpList& LocateRegExpList::operator=(const QStringList& list)
{
	clear();
	QStringList::ConstIterator it = list.begin();
	for (; it != list.end(); ++it) {
		append(LocateRegExp((*it), (*it) == (*it).toLower()));
	}
	return *this;
}


bool LocateRegExpList::isMatchingOne(const QString& file) const
{
	bool matches = false;
	LocateRegExpList::ConstIterator it = begin();
	for (; !matches && (it != end()); ++it) {
		matches = (*it).isMatching(file);
	}
	return matches;
}


bool LocateRegExpList::isMatchingAll(const QString& file) const
{
	bool matches = true;
	LocateRegExpList::ConstIterator it = begin();
	for (; matches && (it != end()); ++it) {
		matches = (*it).isMatching(file);
	}
	return matches;
}
