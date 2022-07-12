/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2021 <tsujan2000@gmail.com>
 *
 * FeatherPad is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * FeatherPad is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * @license GPL-3.0+ <https://spdx.org/licenses/GPL-3.0+.html>
 */

#include "highlighter.h"

static const QRegularExpression tclBracedVariable ("(?<!\\\\)(\\\\{2})*\\K\\$\\{[^\\}]+\\}");

bool Highlighter::isEscapedTclQuote (const QString &text, const int pos,
                                     const int start, bool isStartQuote)
{
    if (start < 0 || pos < start) return false;
    int i = 0;
    while (pos - i > start && text.at (pos - i - 1) == '\\')
        ++i;
    if (i % 2 != 0) return true;
    /* check if the start quote in inside a variable of the form ${...} */
    if (isStartQuote && pos > start + 1)
    {
        QRegularExpressionMatch match;
        int index = text.lastIndexOf (tclBracedVariable, pos, &match);
        if (index >= start && index < pos
            && index + match.capturedLength() > pos
            && !isTclQuoted (text, index, start))
        {
            return true;
        }
    }
    return false;
}
/*************************/
bool Highlighter::isTclQuoted (const QString &text, const int index, const int start)
{
    if (start < 0 || index < start) return false;
    int pos = start - 1;
    bool res = false;
    int N;
    if (pos == -1)
    {
        if (previousBlockState() != doubleQuoteState)
            N = 0;
        else
        {
            N = 1;
            res = true;
        }
    }
    else N = 0;
    int nxtPos;
    while ((nxtPos = text.indexOf (quoteMark, pos + 1)) >= 0)
    {
        if (format (nxtPos) == commentFormat || format (nxtPos) == urlFormat)
        {
            pos = nxtPos;
            continue;
        }
        ++N;
        if (N % 2 != 0 && index < nxtPos) // we don't need to check if the quote is escaped
            return false;
        if ((N % 2 == 0 && isEscapedTclQuote (text, nxtPos, pos + 1, false)) // end quote
            || (N % 2 != 0 && (isEscapedTclQuote (text, nxtPos, pos + 1, true)))) // start quote
        {
            --N;
            pos = nxtPos;
            continue;
        }
        if (index < nxtPos)
        {
            if (N % 2 == 0) res = true;
            else res = false;
            break;
        }
        if (N % 2 == 0) res = false;
        else res = true;
        pos = nxtPos;
    }
    return res;
}
/*************************/
// Check whether a character is inside a Tcl variable of the form ${...},
// supposing that such variables can contain any character other than "}".
// The variable "quotesAreFormatted" determines whether all quotes before
// "start" are already formatted or not.
bool Highlighter::insideTclBracedVariable (const QString &text, const int pos, const int start,
                                           bool quotesAreFormatted)
{
    if (start < 0 || pos < start + 2 || pos >= text.length()) return false;
    QRegularExpressionMatch match;
    int indx = text.lastIndexOf (tclBracedVariable, pos, &match);
    return indx >= start
           && indx < pos - 1 // "pos" is after "${"
           && indx + match.capturedLength() > pos + 1 // "pos" is before "}"
           && (quotesAreFormatted ? format (indx) != quoteFormat && format (indx) != urlInsideQuoteFormat
                                  : !isTclQuoted (text, indx, start));
}
/*************************/
void Highlighter::multiLineTclQuote (const QString &text)
{
    int index = 0;
    int prevState = previousBlockState();
    if (index > 0 || prevState != doubleQuoteState)
    {
        index = text.indexOf (quoteMark, index);
        while (isEscapedTclQuote (text, index, 0, true))
            index = text.indexOf (quoteMark, index + 1);
        if (format (index) == commentFormat) return;
    }

    QRegularExpressionMatch quoteMatch;
    while (index >= 0)
    {
        int endIndex;
        if (index == 0 && prevState == doubleQuoteState)
            endIndex = text.indexOf (quoteMark, 0, &quoteMatch);
        else
            endIndex = text.indexOf (quoteMark, index + 1, &quoteMatch);

        while (isEscapedTclQuote (text, endIndex, index, false))
            endIndex = text.indexOf (quoteMark, endIndex + 1, &quoteMatch);

        int quoteLength;
        if (endIndex == -1)
        {
            setCurrentBlockState (doubleQuoteState);
            quoteLength = text.length() - index;
        }
        else
            quoteLength = endIndex - index + quoteMatch.capturedLength();

        setFormat (index, quoteLength, quoteFormat);

#if (QT_VERSION < QT_VERSION_CHECK(6,0,0))
        QString str = text.mid (index, quoteLength);
#else
        QString str = text.sliced (index, quoteLength);
#endif
        int urlIndex = 0;
        QRegularExpressionMatch urlMatch;
        while ((urlIndex = str.indexOf (urlPattern, urlIndex, &urlMatch)) > -1)
        {
            setFormat (urlIndex + index, urlMatch.capturedLength(), urlInsideQuoteFormat);
            urlIndex += urlMatch.capturedLength();
        }

        int indx = index + quoteLength;
        index = text.indexOf (quoteMark, indx);
        while (isEscapedTclQuote (text, index, indx, true))
            index = text.indexOf (quoteMark, index + 1);
        if (format (index) == commentFormat) return;
    }
}
/*************************/
void Highlighter::highlightTclBlock (const QString &text)
{
    TextBlockData *data = new TextBlockData;
    setCurrentBlockUserData (data);
    setCurrentBlockState (0);

    int index;
    QTextCharFormat fi;

    singleLineComment (text, 0);
    multiLineTclQuote (text);
    int bn = currentBlock().blockNumber();
    if (bn >= startCursor.blockNumber() && bn <= endCursor.blockNumber())
    {
        data->setHighlighted();
        QRegularExpressionMatch match;
        for (const HighlightingRule &rule : qAsConst (highlightingRules))
        {
            if (rule.format == commentFormat)
                continue;

            index = text.indexOf (rule.pattern, 0, &match);
            if (rule.format != whiteSpaceFormat)
            {
                fi = format (index);
                while (index >= 0
                       && (fi == quoteFormat || fi == urlInsideQuoteFormat
                           || fi == commentFormat || fi == urlFormat
                           || fi == altQuoteFormat)) // backslash should be ignored inside ${...}
                {
                    index = text.indexOf (rule.pattern, index + match.capturedLength(), &match);
                    fi = format (index);
                }
            }

            while (index >= 0)
            {
                int length = match.capturedLength();
                setFormat (index, length, rule.format);
                index = text.indexOf (rule.pattern, index + length, &match);

                if (rule.format != whiteSpaceFormat)
                {
                    fi = format (index);
                    while (index >= 0
                           && (fi == quoteFormat || fi == urlInsideQuoteFormat
                               || fi == commentFormat || fi == urlFormat
                               || fi == altQuoteFormat))
                    {
                        index = text.indexOf (rule.pattern, index + match.capturedLength(), &match);
                        fi = format (index);
                    }
                }
            }
        }
    }

    /*********************************************
     * Parentheses, Braces and Brackets Matching *
     *********************************************/

    /* left parenthesis */
    index = text.indexOf ('(');
    fi = format (index);
    while (index >= 0
           && (fi == quoteFormat || fi == urlInsideQuoteFormat
               || fi == commentFormat || fi == urlFormat
               || insideTclBracedVariable (text, index, 0, true) // like ${...(...}
               || isEscapedChar (text, index)))
    {
        index = text.indexOf ('(', index + 1);
        fi = format (index);
    }
    while (index >= 0)
    {
        ParenthesisInfo *info = new ParenthesisInfo;
        info->character = '(';
        info->position = index;
        data->insertInfo (info);

        index = text.indexOf ('(', index + 1);
        fi = format (index);
        while (index >= 0
               && (fi == quoteFormat || fi == urlInsideQuoteFormat
                   || fi == commentFormat || fi == urlFormat
                   || insideTclBracedVariable (text, index, 0, true)
                   || isEscapedChar (text, index)))
        {
            index = text.indexOf ('(', index + 1);
            fi = format (index);
        }
    }

    /* right parenthesis */
    index = text.indexOf (')');
    fi = format (index);
    while (index >= 0
           && (fi == quoteFormat || fi == urlInsideQuoteFormat
               || fi == commentFormat || fi == urlFormat
               || insideTclBracedVariable (text, index, 0, true) // like ${...)...}
               || isEscapedChar (text, index)))
    {
        index = text.indexOf (')', index + 1);
        fi = format (index);
    }
    while (index >= 0)
    {
        ParenthesisInfo *info = new ParenthesisInfo;
        info->character = ')';
        info->position = index;
        data->insertInfo (info);

        index = text.indexOf (')', index +1);
        fi = format (index);
        while (index >= 0
               && (fi == quoteFormat || fi == urlInsideQuoteFormat
                   || fi == commentFormat || fi == urlFormat
                   || insideTclBracedVariable (text, index, 0, true)
                   || isEscapedChar (text, index)))
        {
            index = text.indexOf (')', index + 1);
            fi = format (index);
        }
    }

    /* left brace */
    index = text.indexOf ('{');
    fi = format (index);
    while (index >= 0
           && (fi == quoteFormat || fi == urlInsideQuoteFormat
               || fi == commentFormat || fi == urlFormat
               || insideTclBracedVariable (text, index, 0, true) // like ${...{...}
               || isEscapedChar (text, index)))
    {
        index = text.indexOf ('{', index + 1);
        fi = format (index);
    }
    while (index >= 0)
    {
        BraceInfo *info = new BraceInfo;
        info->character = '{';
        info->position = index;
        data->insertInfo (info);

        index = text.indexOf ('{', index + 1);
        fi = format (index);
        while (index >= 0
               && (fi == quoteFormat || fi == urlInsideQuoteFormat
                   || fi == commentFormat || fi == urlFormat
                   || insideTclBracedVariable (text, index, 0, true)
                   || isEscapedChar (text, index)))
        {
            index = text.indexOf ('{', index + 1);
            fi = format (index);
        }
    }

    /* right brace */
    index = text.indexOf ('}');
    fi = format (index);
    while (index >= 0
           && (fi == quoteFormat || fi == urlInsideQuoteFormat
               || fi == commentFormat || fi == urlFormat
               || (isEscapedChar (text, index)
                   && !insideTclBracedVariable (text, index - 1, 0, true)))) // not like ${...\}
    {
        index = text.indexOf ('}', index + 1);
        fi = format (index);
    }
    while (index >= 0)
    {
        BraceInfo *info = new BraceInfo;
        info->character = '}';
        info->position = index;
        data->insertInfo (info);

        index = text.indexOf ('}', index +1);
        fi = format (index);
        while (index >= 0
               && (fi == quoteFormat || fi == urlInsideQuoteFormat
                   || fi == commentFormat || fi == urlFormat
                   || (isEscapedChar (text, index)
                       && !insideTclBracedVariable (text, index - 1, 0, true))))
        {
            index = text.indexOf ('}', index + 1);
            fi = format (index);
        }
    }

    /* left bracket */
    index = text.indexOf ('[');
    fi = format (index);
    while (index >= 0
           && (fi == quoteFormat || fi == urlInsideQuoteFormat
               || fi == commentFormat || fi == urlFormat
               || insideTclBracedVariable (text, index, 0, true) // like ${...[...}
               || isEscapedChar (text, index)))
    {
        index = text.indexOf ('[', index + 1);
        fi = format (index);
    }
    while (index >= 0)
    {
        BracketInfo *info = new BracketInfo;
        info->character = '[';
        info->position = index;
        data->insertInfo (info);

        index = text.indexOf ('[', index + 1);
        fi = format (index);
        while (index >= 0
               && (fi == quoteFormat || fi == urlInsideQuoteFormat
                   || fi == commentFormat || fi == urlFormat
                   || insideTclBracedVariable (text, index, 0, true)
                   || isEscapedChar (text, index)))
        {
            index = text.indexOf ('[', index + 1);
            fi = format (index);
        }
    }

    /* right bracket */
    index = text.indexOf (']');
    fi = format (index);
    while (index >= 0
           && (fi == quoteFormat || fi == urlInsideQuoteFormat
               || fi == commentFormat || fi == urlFormat
               || insideTclBracedVariable (text, index, 0, true) // like ${...]...}
               || isEscapedChar (text, index)))
    {
        index = text.indexOf (']', index + 1);
        fi = format (index);
    }
    while (index >= 0)
    {
        BracketInfo *info = new BracketInfo;
        info->character = ']';
        info->position = index;
        data->insertInfo (info);

        index = text.indexOf (']', index +1);
        fi = format (index);
        while (index >= 0
               && (fi == quoteFormat || fi == urlInsideQuoteFormat
                   || fi == commentFormat || fi == urlFormat
                   || insideTclBracedVariable (text, index, 0, true)
                   || isEscapedChar (text, index)))
        {
            index = text.indexOf (']', index + 1);
            fi = format (index);
        }
    }

    setCurrentBlockUserData (data);
}
