/* Copyright (C) 2007 J.F.Dockes
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
#ifndef _confgui_h_included_
#define _confgui_h_included_
/**
 * This file defines a number of simple classes (virtual base: ConfParamW) 
 * which let the user input configuration parameters. 
 *
 * Subclasses are defined for entering different kind of data, ie a string, 
 * a file name, an integer, etc.
 *
 * Each configuration gui object is linked to the configuration data through
 * a "link" object which knows the details of interacting with the actual
 * configuration data, like the parameter name, the actual config object, 
 * the method to call etc.
 * 
 * The link object is set when the input widget is created and cannot be 
 * changed.
 *
 * The widgets are typically linked to a temporary configuration object, which
 * is then copied to the actual configuration if the data is accepted, or
 * destroyed and recreated as a copy if Cancel is pressed (you have to 
 * delete/recreate the widgets in this case as the links are no longer valid).
 */
#include <string>
#include <limits.h>

#include <qglobal.h>
#include <qstring.h>
#include <qwidget.h>

#include "refcntr.h"

using std::string;

class QHBoxLayout;
class QLineEdit;
class QListWidget;
class QSpinBox;
class QComboBox;
class QCheckBox;
class QPushButton;

namespace confgui {

    // A class to isolate the gui widget from the config storage mechanism
    class ConfLinkRep {
    public:
	virtual ~ConfLinkRep() {}
	virtual bool set(const string& val) = 0;
	virtual bool get(string& val) = 0;
    };
    typedef RefCntr<ConfLinkRep> ConfLink;

    // Useful to store/manage data which has no direct representation in
    // the config, ie list of subkey directories
    class ConfLinkNullRep : public ConfLinkRep {
    public:
	virtual ~ConfLinkNullRep() {}
	virtual bool set(const string&)
	{
	    return true;
	}
	virtual bool get(string& val) {val = ""; return true;}
    };

    // A widget to let the user change one configuration
    // parameter. Subclassed for specific parameter types. Basically
    // has a label and some kind of entry widget
    class ConfParamW : public QWidget {
	Q_OBJECT
    public:
	ConfParamW(QWidget *parent, ConfLink cflink)
	    : QWidget(parent), m_cflink(cflink), m_fsencoding(false)
	{
	}
	virtual void loadValue() = 0;
        virtual void setFsEncoding(bool onoff) {m_fsencoding = onoff;}
    protected:
	ConfLink     m_cflink;
	QHBoxLayout *m_hl;
        // File names are encoded as local8bit in the config files. Other
        // are encoded as utf-8
        bool         m_fsencoding;
	virtual bool createCommon(const QString& lbltxt,
				  const QString& tltptxt);
    public slots:
        virtual void setEnabled(bool) = 0;
    protected slots:
        void setValue(const QString& newvalue);
        void setValue(int newvalue);
        void setValue(bool newvalue);
    };

    // Widgets for setting the different types of configuration parameters:

    // Int
    class ConfParamIntW : public ConfParamW {
	Q_OBJECT
    public:
        // The default value is only used if none exists in the sample
        // configuration file. Defaults are normally set in there.
	ConfParamIntW(QWidget *parent, ConfLink cflink, 
		      const QString& lbltxt,
		      const QString& tltptxt,
		      int minvalue = INT_MIN, 
		      int maxvalue = INT_MAX,
                      int defaultvalue = 0);
	virtual void loadValue();
    public slots:
        virtual void setEnabled(bool i) {if(m_sb) ((QWidget*)m_sb)->setEnabled(i);}
    protected:
	QSpinBox *m_sb;
        int       m_defaultvalue;
    };

    // Arbitrary string
    class ConfParamStrW : public ConfParamW {
	Q_OBJECT
    public:
	ConfParamStrW(QWidget *parent, ConfLink cflink, 
		      const QString& lbltxt,
		      const QString& tltptxt);
	virtual void loadValue();
    public slots:
        virtual void setEnabled(bool i) {if(m_le) ((QWidget*)m_le)->setEnabled(i);}
    protected:
	QLineEdit *m_le;
    };

    // Constrained string: choose from list
    class ConfParamCStrW : public ConfParamW {
	Q_OBJECT
    public:
	ConfParamCStrW(QWidget *parent, ConfLink cflink, 
		      const QString& lbltxt,
		      const QString& tltptxt, const QStringList& sl);
	virtual void loadValue();
    public slots:
        virtual void setEnabled(bool i) {if(m_cmb) ((QWidget*)m_cmb)->setEnabled(i);}
    protected:
	QComboBox *m_cmb;
    };

    // Boolean
    class ConfParamBoolW : public ConfParamW {
	Q_OBJECT
    public:
	ConfParamBoolW(QWidget *parent, ConfLink cflink, 
		      const QString& lbltxt,
		      const QString& tltptxt);
	virtual void loadValue();
    public slots:
        virtual void setEnabled(bool i) {if(m_cb) ((QWidget*)m_cb)->setEnabled(i);}
    public:
	QCheckBox *m_cb;
    };

    // File name
    class ConfParamFNW : public ConfParamW {
	Q_OBJECT
    public:
	ConfParamFNW(QWidget *parent, ConfLink cflink, 
		      const QString& lbltxt,
		     const QString& tltptxt, bool isdir = false);
	virtual void loadValue();
    protected slots:
	void showBrowserDialog();
    public slots:
        virtual void setEnabled(bool i) 
        {
            if(m_le) ((QWidget*)m_le)->setEnabled(i);
            if(m_pb) ((QWidget*)m_pb)->setEnabled(i);
        }
    protected:
	QLineEdit *m_le;
        QPushButton *m_pb;
	bool       m_isdir;
    };

    // String list
    class ConfParamSLW : public ConfParamW {
	Q_OBJECT
    public:
	ConfParamSLW(QWidget *parent, ConfLink cflink, 
		      const QString& lbltxt,
		      const QString& tltptxt);
	virtual void loadValue();
	QListWidget *getListBox() {return m_lb;}
	
    public slots:
        virtual void setEnabled(bool i) {if(m_lb) ((QWidget*)m_lb)->setEnabled(i);}
    protected slots:
	virtual void showInputDialog();
	void deleteSelected();
    signals:
        void entryDeleted(QString);
    protected:
	QListWidget *m_lb;
	void listToConf();
    };

    // Dir name list
    class ConfParamDNLW : public ConfParamSLW {
	Q_OBJECT
    public:
	ConfParamDNLW(QWidget *parent, ConfLink cflink, 
		      const QString& lbltxt,
		      const QString& tltptxt)
	    : ConfParamSLW(parent, cflink, lbltxt, tltptxt)
	    {
                m_fsencoding = true;
	    }
    protected slots:
	virtual void showInputDialog();
    };

    // Constrained string list (chose from predefined)
    class ConfParamCSLW : public ConfParamSLW {
	Q_OBJECT
    public:
	ConfParamCSLW(QWidget *parent, ConfLink cflink, 
		      const QString& lbltxt,
		      const QString& tltptxt,
		      const QStringList& sl)
	    : ConfParamSLW(parent, cflink, lbltxt, tltptxt), m_sl(sl)
	    {
	    }
    protected slots:
	virtual void showInputDialog();
    protected:
	const QStringList m_sl;
    };

    extern void setSzPol(QWidget *w, QSizePolicy::Policy hpol, 
			 QSizePolicy::Policy vpol,
			 int hstretch, int vstretch);
}

#endif /* _confgui_h_included_ */
