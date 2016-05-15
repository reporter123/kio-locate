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

#include <unistd.h>
#include "locater.h"
#include <QDebug>
#include <QStandardPaths>
#include <KApplication>


Locater::Locater(QObject *parent, const char *name)
	: QObject(parent)
{
	setObjectName(name);

	qDebug() << "Locater::Locater" << endl;

	m_process.setOutputChannelMode(KProcess::OnlyStdoutChannel);
	//m_process.setReadChannel(QProcess::StandardOutput);
	connect(&m_process, SIGNAL(finished(int, QProcess::ExitStatus)),
			 this, SLOT(finished(int, QProcess::ExitStatus)));
	connect(&m_process, SIGNAL(readyReadStandardOutput()),
			 this, SLOT(gotOutput()));

	setupLocate();
}


Locater::~Locater()
{
	qDebug() << "Locater::~Locater" << endl;
}


void Locater::setupLocate(const QString& binary, const QString& additionalArguments)
{
	qDebug() << "Locater::setupLocate(" << binary << ", " << additionalArguments << ")" << endl;

    // Automatically choose the correct binary if not specified.
    if (binary.isEmpty()) {
        if (!QStandardPaths::findExecutable("slocate").isEmpty()) {
            m_binary = "slocate";
        } else if (!QStandardPaths::findExecutable("rlocate").isEmpty()) {
            m_binary = "rlocate";
        } else {
            m_binary = "locate";
        }
        qDebug() << "Using binary:" << m_binary << endl;
    } else {
        m_binary = binary;
    }
	m_additionalArguments = additionalArguments;
    m_binaryExists = !QStandardPaths::findExecutable(m_binary).isEmpty();
}


bool Locater::locate(const QString& pattern, bool ignoreCase, bool regExp)
{
	qDebug() << "Locater::locate(" << pattern << "," << ignoreCase << "," << regExp << ")" << endl;

	//m_process.reset();
	m_process << m_binary;
	if (!m_additionalArguments.isEmpty()) {
		m_process << m_additionalArguments;
	}
	if (ignoreCase) {
		// m_process << "--ignore-case";
		m_process << "-i";
	}
	if (regExp) {
		m_process << "-r";
	}
	m_process << pattern;

	//KIO jobs are asyncrinous so just use KProcess::execute here.	
	return (m_process.execute() == QProcess::NormalExit);
}

void Locater::stop()
{
	qDebug() << "Locater::stop()" << endl;

	m_process.kill();
	emit finished();
}


void Locater::gotOutput()
{
	qDebug() << "Locater::gotOutput" << endl;

	QStringList items;

	while (m_process.canReadLine()) {
		QString line = m_process.readLine().trimmed();
		//qDebug() << "OUTPUT>> '" << line << "'" << endl;

		items << line;
	}

	emit found(items);
}


void Locater::finished(int, QProcess::ExitStatus)
{
	qDebug() << "Locater::finished" << endl;

	emit finished();
}


#include "locater.moc"
