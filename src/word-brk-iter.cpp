/***************************************************************************
 *   Copyright (C) 2009 by Keith Stribley                                  *
 *   devel@thanlwinsoft.org                                                *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Library General Public License as       *
 *   published by the Free Software Foundation; either version 2 of the    *
 *   License, or (at your option) any later version.                       *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this program; if not, write to the                 *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
#include <unicode/ubrk.h>
#include <unicode/brkiter.h>
#include <unicode/rbbi.h>

#include "word-brk-iter.h"

/**
* MyRbbi is rather a hack to get around the fact that a break iterator
* gets into an infinite loop unless break type is set. fBreakType is
* protected, so this is the only way to set it.
*/
class MyRbbi : public RuleBasedBreakIterator
{
   public:
        MyRbbi(const UnicodeString &rules, UBreakIteratorType type, UParseError &parseError, UErrorCode &status)
            : RuleBasedBreakIterator(rules, parseError, status)
        {
            fBreakType = type;
        }

};

UBreakIterator * createBreakIterator(const UChar *rules, int32_t rulesLength, UBreakIteratorType type, UParseError *parseErr, UErrorCode *status)
{
    UnicodeString usRules(rules, rulesLength);
    BreakIterator * brkIter = new MyRbbi(usRules, type, *parseErr, *status);
    if (brkIter && U_FAILURE(*status))
    {
        delete brkIter;
        brkIter = NULL;
    }
    return reinterpret_cast<UBreakIterator*>(brkIter);
}
