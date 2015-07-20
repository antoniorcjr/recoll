/* Copyright (C) 2012 J.F.Dockes
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
#ifndef _STRMATCHER_H_INCLUDED_
#define _STRMATCHER_H_INCLUDED_

#include <string>

// Encapsulating simple wildcard/regexp string matching.

// Matcher class. Interface to either wildcard or regexp yes/no matcher
class StrMatcher {
public:
    StrMatcher(const std::string& exp) 
    : m_sexp(exp)
    {
    }
    virtual ~StrMatcher() {};
    virtual bool match(const std::string &val) const = 0;
    virtual std::string::size_type baseprefixlen() const = 0;
    virtual bool setExp(const std::string& newexp) 
    {
	m_sexp = newexp;
	return true;
    }
    virtual bool ok() const
    {
	return true;
    }
    virtual const std::string& exp()
    {
	return m_sexp;
    }
    virtual StrMatcher *clone() = 0;
    const string& getreason() 
    {
	return m_reason;
    }
protected:
    std::string m_sexp;
    std::string m_reason;
};

class StrWildMatcher : public StrMatcher {
public:
    StrWildMatcher(const std::string& exp)
    : StrMatcher(exp)
    {
    }
    virtual ~StrWildMatcher()
    {
    }
    virtual bool match(const std::string& val) const;
    virtual std::string::size_type baseprefixlen() const;
    virtual StrWildMatcher *clone()
    {
	return new StrWildMatcher(m_sexp);
    }
};

class StrRegexpMatcher : public StrMatcher {
public:
    StrRegexpMatcher(const std::string& exp);
    virtual bool setExp(const std::string& newexp);
    virtual ~StrRegexpMatcher();
    virtual bool match(const std::string& val) const;
    virtual std::string::size_type baseprefixlen() const;
    virtual bool ok() const;
    virtual StrRegexpMatcher *clone()
    {
	return new StrRegexpMatcher(m_sexp);
    }
    const string& getreason() 
    {
	return m_reason;
    }
private:
    void *m_compiled;
    bool m_errcode;
};

#endif /* _STRMATCHER_H_INCLUDED_ */
