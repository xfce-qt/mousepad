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

void Highlighter::reSTMainFormatting (int start, const QString &text)
{
    if (start < 0) return;
    TextBlockData *data = static_cast<TextBlockData *>(currentBlock().userData());
    if (data == nullptr) return;

    data->setHighlighted(); // completely highlighted
    QTextCharFormat fi;
    QRegularExpressionMatch match;
    for (const HighlightingRule &rule : qAsConst (highlightingRules))
    {
        int index = text.indexOf (rule.pattern, start, &match);
        if (rule.format != whiteSpaceFormat)
        {
            fi = format (index);
            while (index >= 0 && fi != mainFormat)
            {
                index = text.indexOf (rule.pattern, index + match.capturedLength(), &match);
                fi = format (index);
            }
        }
        while (index >= 0)
        {
            /* get the overwritten format if existent */
            QTextCharFormat prevFormat = format (index + match.capturedLength() - 1);

            setFormat (index, match.capturedLength(), rule.format);
            if (rule.pattern == QRegularExpression (":[\\w\\-+]+:`[^`]*`"))
            { // format the reference start too
                QTextCharFormat boldFormat = neutralFormat;
                boldFormat.setFontWeight (QFont::Bold);
                setFormat (index, text.indexOf (":`", index) - index + 1, boldFormat);
            }
            index += match.capturedLength();

            if (rule.format != whiteSpaceFormat && prevFormat != mainFormat)
            { // if a format is overwriiten by this rule, reformat from here
                setFormat (index, text.length() - index, mainFormat);
                reSTMainFormatting (index, text);
                break;
            }

            index = text.indexOf (rule.pattern, index, &match);
            if (rule.format != whiteSpaceFormat)
            {
                fi = format (index);
                while (index >= 0 && fi != mainFormat)
                {
                    index = text.indexOf (rule.pattern, index + match.capturedLength(), &match);
                    fi = format (index);
                }
            }
        }
    }
}
/*************************/
void Highlighter::highlightReSTBlock (const QString &text)
{
    int index;
    TextBlockData *data = new TextBlockData;
    setCurrentBlockUserData (data);
    setCurrentBlockState (0);

    /*******************************
    * reST Code and Comment Blocks *
    ********************************/
    QRegularExpressionMatch match;
    QTextBlock prevBlock = currentBlock().previous();
    static const QRegularExpression codeBlockStart1 ("^\\.{2} code-block::");
    static const QRegularExpression codeBlockStart2 ("^(?!\\s*\\.{2}\\s+).*::$");
    static const QRegularExpression restComment ("^\\s*\\.{2}"
                                                 "(?!("
                                                     /* not a label or substitution (".. _X:" or ".. |X| Y::") */
                                                     " _[\\w\\s\\-+]*:(?!\\S)"
                                                     "|"
                                                     " \\|[\\w\\s]+\\|\\s+\\w+::(?!\\S)"
                                                     "|"
                                                     /* not a footnote (".. [#X]") */
                                                     " (\\[(\\w|\\s|-|\\+|\\*)+\\]|\\[#(\\w|\\s|-|\\+)*\\])\\s+"
                                                     "|"
                                                     /* not ".. X::" */
                                                     " (\\w|-)+::(?!\\S)"
                                                 "))"
                                                 "\\s+.*");
    /* definitely, the start of a code block */
    if (text.indexOf (codeBlockStart1, 0, &match) == 0)
    { // also overwrites commentFormat
        /* the ".. code-block::" part will be formatted later */
        setFormat (match.capturedLength(), text.count() - match.capturedLength(), codeBlockFormat);
        setCurrentBlockState (codeBlockState);
    }
    /* perhaps the start of a code block */
    else if (text.indexOf (codeBlockStart2) == 0)
    {
        bool isCommented (false);
        if (previousBlockState() >= endState || previousBlockState() < -1)
        {
            int spaces = text.indexOf (QRegularExpression ("\\S"));
            if (spaces > 0)
            {
                if (TextBlockData *prevData = static_cast<TextBlockData *>(prevBlock.userData()))
                {
                    QString prevLabel = prevData->labelInfo();
                    if (prevLabel.startsWith ("c") && text.startsWith (prevLabel.right(prevLabel.count() - 1)))
                    { // not a code block start but a comment
                        isCommented = true;
                        setFormat (0, text.count(), commentFormat);
                        setCurrentBlockState (previousBlockState());
                        data->insertInfo (prevLabel);
                    }
                }
            }
        }
        /* definitely, the start of a code block */
        if (!isCommented)
        {
            QTextCharFormat blockFormat = codeBlockFormat;
            blockFormat.setFontWeight (QFont::Bold);
            setFormat (text.count() - 2,  2, blockFormat);
            setCurrentBlockState (codeBlockState);
        }
    }
    /* perhaps a comment */
    else if (text.indexOf (restComment) == 0)
    {
        bool isCodeLine (false);
        QString prevLabel;
        if (previousBlockState() == codeBlockState
            || previousBlockState() >= endState || previousBlockState() < -1)
        {
            if (TextBlockData *prevData = static_cast<TextBlockData *>(prevBlock.userData()))
            {
                prevLabel = prevData->labelInfo();
                if (prevLabel.isEmpty()
                    || (!prevLabel.startsWith ("c") && text.startsWith (prevLabel)))
                { // not a commnt but a code line
                    isCodeLine = true;
                    if (prevLabel.isEmpty())
                    { // the code block was started or kept in the previous line
                        int spaces = text.indexOf (QRegularExpression ("\\S"));
                        if (spaces == -1) // spaces only keep the code block
                            setCurrentBlockState (codeBlockState);
                        else
                        { // a code line
                            setFormat (0, text.count(), codeBlockFormat);
                            if (spaces == 0) // a line without indent only keeps the code block
                                setCurrentBlockState (codeBlockState);
                            else
                            {
                                /* remember the starting spaces */
                                QString spaceStr = text.left (spaces);
                                data->insertInfo (spaceStr);
                                /* also, the state should depend on the starting spaces */
                                int n = static_cast<int>(qHash (spaceStr));
                                int state = 2 * (n + (n >= 0 ? endState/2 + 1 : 0));
                                setCurrentBlockState (state);
                            }
                        }
                    }
                    else
                    {
                        setFormat (0, text.count(), codeBlockFormat);
                        setCurrentBlockState (previousBlockState());
                        data->insertInfo (prevLabel);
                    }
                }
            }
        }
        /* definitely a comment */
        if (!isCodeLine)
        {
            setFormat (0, text.count(), commentFormat);
            if ((previousBlockState() >= endState || previousBlockState() < -1)
                && prevLabel.startsWith ("c") && text.startsWith (prevLabel.right(prevLabel.count() - 1)))
            {
                setCurrentBlockState (previousBlockState());
                data->insertInfo (prevLabel);
            }
            else
            {
                /* remember the starting spaces (which consists of 3 spaces at least)
                    but add a "c" to its beginning to distinguish it from a code block */
                QString spaceStr;
                int spaces = text.indexOf (QRegularExpression ("\\S"));
                if (spaces == -1)
                    spaceStr = "c   ";
                else
                    spaceStr = "c" + text.left (spaces) + "   ";
                data->insertInfo (spaceStr);
                /* also, the state should depend on the starting spaces */
                int n = static_cast<int>(qHash (spaceStr));
                int state = 2 * (n + (n >= 0 ? endState/2 + 1 : 0)); // always an even number but maybe negative
                setCurrentBlockState (state);
            }
        }
    }
    /* now, everything depends on the previous block */
    else if (prevBlock.isValid())
    {
        if (previousBlockState() == codeBlockState)
        { // the code block was started or kept in the previous line
            int spaces = text.indexOf (QRegularExpression ("\\S"));
            if (text.isEmpty() || spaces == -1) // spaces only keep the code block
                setCurrentBlockState (codeBlockState);
            else
            { // a code line
                setFormat (0, text.count(), codeBlockFormat);
                if (spaces == 0) // a line without indent only keeps the code block
                    setCurrentBlockState (codeBlockState);
                else
                {
                    /* remember the starting spaces */
                    QString spaceStr = text.left (spaces);
                    data->insertInfo (spaceStr);
                    /* also, the state should depend on the starting spaces */
                    int n = static_cast<int>(qHash (spaceStr));
                    int state = 2 * (n + (n >= 0 ? endState/2 + 1 : 0));
                    setCurrentBlockState (state);
                }
            }
        }
        else if (previousBlockState() >= endState || previousBlockState() < -1)
        {
            if (TextBlockData *prevData = static_cast<TextBlockData *>(prevBlock.userData()))
            {
                QString prevLabel = prevData->labelInfo();
                if (text.isEmpty()
                    || ((prevLabel.startsWith ("c") && text.startsWith (prevLabel.right(prevLabel.count() - 1)))
                        || text.startsWith (prevLabel)))
                {
                    setCurrentBlockState (previousBlockState());
                    data->insertInfo (prevLabel);
                    if (prevLabel.startsWith ("c")) // a comment continues
                        setFormat (0, text.count(), commentFormat);
                    else
                        setFormat (0, text.count(), codeBlockFormat);
                }
            }
        }
    }

    /***********************
    * reST Main Formatting *
    ************************/
    int bn = currentBlock().blockNumber();
    if (bn >= startCursor.blockNumber() && bn <= endCursor.blockNumber())
        reSTMainFormatting (0, text);

    /*********************************************
     * Parentheses, Braces and Brackets Matching *
     *********************************************/

    /* left parenthesis */
    QTextCharFormat fi;
    index = text.indexOf ('(');
    fi = format (index);
    while (index >= 0
           && (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat
               || fi == commentFormat || fi == urlFormat
               || fi == regexFormat))
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
               && (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat
                   || fi == commentFormat || fi == urlFormat
                   || fi == regexFormat))
        {
            index = text.indexOf ('(', index + 1);
            fi = format (index);
        }
    }

    /* right parenthesis */
    index = text.indexOf (')');
    fi = format (index);
    while (index >= 0
           && (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat
               || fi == commentFormat || fi == urlFormat
               || fi == regexFormat))
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
               && (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat
                   || fi == commentFormat || fi == urlFormat
                   || fi == regexFormat))
        {
            index = text.indexOf (')', index + 1);
            fi = format (index);
        }
    }

    /* left brace */
    index = text.indexOf ('{');
    fi = format (index);
    while (index >= 0
           && (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat
               || fi == commentFormat || fi == urlFormat
               || fi == regexFormat))
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
               && (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat
                   || fi == commentFormat || fi == urlFormat
                   || fi == regexFormat))
        {
            index = text.indexOf ('{', index + 1);
            fi = format (index);
        }
    }

    /* right brace */
    index = text.indexOf ('}');
    fi = format (index);
    while (index >= 0
           && (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat
               || fi == commentFormat || fi == urlFormat
               || fi == regexFormat))
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
               && (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat
                   || fi == commentFormat || fi == urlFormat
                   || fi == regexFormat))
        {
            index = text.indexOf ('}', index + 1);
            fi = format (index);
        }
    }

    /* left bracket */
    index = text.indexOf ('[');
    fi = format (index);
    while (index >= 0
           && (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat
               || fi == commentFormat || fi == urlFormat
               || fi == regexFormat))
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
               && (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat
                   || fi == commentFormat || fi == urlFormat
                   || fi == regexFormat))
        {
            index = text.indexOf ('[', index + 1);
            fi = format (index);
        }
    }

    /* right bracket */
    index = text.indexOf (']');
    fi = format (index);
    while (index >= 0
           && (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat
               || fi == commentFormat || fi == urlFormat
               || fi == regexFormat))
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
               && (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat
                   || fi == commentFormat || fi == urlFormat
                   || fi == regexFormat))
        {
            index = text.indexOf (']', index + 1);
            fi = format (index);
        }
    }

    setCurrentBlockUserData (data);
}
