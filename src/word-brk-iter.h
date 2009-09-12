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
#ifndef WORD_BRK_ITER_H
#define WORD_BRK_ITER_H

#include <unicode/ubrk.h>

#ifdef __cplusplus
extern "C" {
#endif

    /**
 * Open a new UBreakIterator for locating text boundaries using specified breaking rules.
 * The rule syntax is ... (TBD)
 * @param rules A set of rules specifying the text breaking conventions.
 * @param rulesLength The number of characters in rules, or -1 if null-terminated.
 * @param type The type of break iterator.
 * @param parseErr   Receives position and context information for any syntax errors
 *                   detected while parsing the rules.
 * @param status A UErrorCode to receive any errors.
 * @return A UBreakIterator for the specified rules.
 * @see ubrk_open
 */
    UBreakIterator * createBreakIterator(const UChar *rules,
        int32_t rulesLength, UBreakIteratorType type,
        UParseError *parseErr, UErrorCode *status);

#ifdef __cplusplus
}
#endif

#endif
