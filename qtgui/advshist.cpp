/* Copyright (C) 2005 J.F.Dockes 
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "autoconfig.h"

#include "advshist.h"
#include "guiutils.h"
#include "debuglog.h"

#include <QtXml/QXmlDefaultHandler>

using namespace std;
using namespace Rcl;

class SDHXMLHandler : public QXmlDefaultHandler {
public:
    SDHXMLHandler()
    {
	resetTemps();
    }
    bool startElement(const QString & /* namespaceURI */,
		      const QString & /* localName */,
		      const QString &qName,
		      const QXmlAttributes &attributes);
    bool endElement(const QString & /* namespaceURI */,
		    const QString & /* localName */,
		    const QString &qName);
    bool characters(const QString &str)
    {
	currentText += str;
	return true;
    }

    // The object we set up
    RefCntr<SearchData> sd;

private:
    void resetTemps() 
    {
	currentText = whatclause = "";
	text.clear();
	field.clear();
	slack = 0;
	d = m = y = di.d1 = di.m1 = di.y1 = di.d2 = di.m2 = di.y2 = 0;
	hasdates = false;
	exclude = false;
    }

    // Temporary data while parsing.
    QString currentText;
    QString whatclause;
    string field, text;
    int slack;
    int d, m, y;
    DateInterval di;
    bool hasdates;
    bool exclude;
};

bool SDHXMLHandler::startElement(const QString & /* namespaceURI */,
				const QString & /* localName */,
				const QString &qName,
				const QXmlAttributes &)
{
    LOGDEB2(("SDHXMLHandler::startElement: name [%s]\n", 
	     (const char *)qName.toUtf8()));
    if (qName == "SD") {
	resetTemps();
	// A new search descriptor. Allocate data structure
	sd = RefCntr<SearchData>(new SearchData);
	if (sd.isNull()) {
	    LOGERR(("SDHXMLHandler::startElement: out of memory\n"));
	    return false;
	}
    }	
    return true;
}

bool SDHXMLHandler::endElement(const QString & /* namespaceURI */,
			      const QString & /* localName */,
			      const QString &qName)
{
    LOGDEB2(("SDHXMLHandler::endElement: name [%s]\n", 
	     (const char *)qName.toUtf8()));

    if (qName == "CLT") {
	if (currentText == "OR") {
	    sd->setTp(SCLT_OR);
	}
    } else if (qName == "CT") {
	whatclause = currentText.trimmed();
    } else if (qName == "NEG") {
	exclude = true;
    } else if (qName == "F") {
	field = base64_decode(qs2utf8s(currentText.trimmed()));
    } else if (qName == "T") {
	text = base64_decode(qs2utf8s(currentText.trimmed()));
    } else if (qName == "S") {
	slack = atoi((const char *)currentText.toUtf8());
    } else if (qName == "C") {
	SearchDataClause *c;
	if (whatclause == "AND" || whatclause.isEmpty()) {
	    c = new SearchDataClauseSimple(SCLT_AND, text, field);
	    c->setexclude(exclude);
	} else if (whatclause == "OR") {
	    c = new SearchDataClauseSimple(SCLT_OR, text, field);
	    c->setexclude(exclude);
	} else if (whatclause == "EX") {
	    // Compat with old hist. We don't generete EX (SCLT_EXCL) anymore
	    // it's replaced with OR + exclude flag
	    c = new SearchDataClauseSimple(SCLT_OR, text, field);
	    c->setexclude(true);
	} else if (whatclause == "FN") {
	    c = new SearchDataClauseFilename(text);
	    c->setexclude(exclude);
	} else if (whatclause == "PH") {
	    c = new SearchDataClauseDist(SCLT_PHRASE, text, slack, field);
	    c->setexclude(exclude);
	} else if (whatclause == "NE") {
	    c = new SearchDataClauseDist(SCLT_NEAR, text, slack, field);
	    c->setexclude(exclude);
	} else {
	    LOGERR(("Bad clause type [%s]\n", qs2utf8s(whatclause).c_str()));
	    return false;
	}
	sd->addClause(c);
	whatclause = "";
	text.clear();
	field.clear();
	slack = 0;
	exclude = false;
    } else if (qName == "D") {
	d = atoi((const char *)currentText.toUtf8());
    } else if (qName == "M") {
	m = atoi((const char *)currentText.toUtf8());
    } else if (qName == "Y") {
	y = atoi((const char *)currentText.toUtf8());
    } else if (qName == "DMI") {
	di.d1 = d;
	di.m1 = m;
	di.y1 = y;
	hasdates = true;
    } else if (qName == "DMA") {
	di.d2 = d;
	di.m2 = m;
	di.y2 = y;
	hasdates = true;
    } else if (qName == "MIS") {
	sd->setMinSize(atoll((const char *)currentText.toUtf8()));
    } else if (qName == "MAS") {
	sd->setMaxSize(atoll((const char *)currentText.toUtf8()));
    } else if (qName == "ST") {
	string types = (const char *)currentText.toUtf8();
	vector<string> vt;
	stringToTokens(types, vt);
	for (unsigned int i = 0; i < vt.size(); i++) 
	    sd->addFiletype(vt[i]);
    } else if (qName == "IT") {
	string types(qs2utf8s(currentText));
	vector<string> vt;
	stringToTokens(types, vt);
	for (unsigned int i = 0; i < vt.size(); i++) 
	    sd->remFiletype(vt[i]);
    } else if (qName == "YD") {
	string d;
	base64_decode(qs2utf8s(currentText.trimmed()), d);
	sd->addClause(new SearchDataClausePath(d));
    } else if (qName == "ND") {
	string d;
	base64_decode(qs2utf8s(currentText.trimmed()), d);
	sd->addClause(new SearchDataClausePath(d, true));
    } else if (qName == "SD") {
	// Closing current search descriptor. Finishing touches...
	if (hasdates)
	    sd->setDateSpan(&di);
	resetTemps();
    } 
    currentText.clear();
    return true;
}


AdvSearchHist::AdvSearchHist()
{
    read();
    m_current = -1;
}

AdvSearchHist::~AdvSearchHist()
{
    for (vector<RefCntr<SearchData> >::iterator it = m_entries.begin();
	 it != m_entries.end(); it++) {
	it->release();
    }
}

RefCntr<Rcl::SearchData> AdvSearchHist::getolder()
{
    m_current++;
    if (m_current >= int(m_entries.size())) {
	m_current--;
	return RefCntr<Rcl::SearchData>();
    }
    return m_entries[m_current];
}

RefCntr<Rcl::SearchData> AdvSearchHist::getnewer()
{
    if (m_current == -1 || m_current == 0 || m_entries.empty())
	return RefCntr<Rcl::SearchData>();
    return m_entries[--m_current];
}

bool AdvSearchHist::push(RefCntr<SearchData> sd)
{
    m_entries.insert(m_entries.begin(), sd);
    if (m_current != -1)
	m_current++;

    string xml = sd->asXML();
    g_dynconf->enterString(advSearchHistSk, xml, 100);
    return true;
}

bool AdvSearchHist::read()
{
    if (!g_dynconf)
	return false;
    list<string> lxml = g_dynconf->getStringList(advSearchHistSk);
    
    for (list<string>::const_iterator it = lxml.begin(); it != lxml.end();
	 it++) {
	SDHXMLHandler handler;
	QXmlSimpleReader reader;
	reader.setContentHandler(&handler);
	reader.setErrorHandler(&handler);
	QXmlInputSource xmlInputSource;
	xmlInputSource.setData(QString::fromUtf8(it->c_str()));
	if (!reader.parse(xmlInputSource)) {
	    LOGERR(("AdvSearchHist::read: parse failed for [%s]\n", 
		    it->c_str()));
	    return false;
	}
	m_entries.push_back(handler.sd);
    }
    return true;
}

void AdvSearchHist::clear()
{
    g_dynconf->eraseAll(advSearchHistSk);
}
