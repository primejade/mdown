/*	$Id$ */
/*
 * Copyright (c) 2017--2021 Kristaps Dzonsons <kristaps@bsd.lv>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include "config.h"

#if HAVE_SYS_QUEUE
# include <sys/queue.h>
#endif

#include <assert.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mdown.h"
#include "extern.h"

static	const char *const names[MDOWN__MAX] = {
	"MDOWN_ROOT",			/* MDOWN_ROOT */
	"MDOWN_BLOCKCODE",            /* MDOWN_BLOCKCODE */
	"MDOWN_BLOCKQUOTE",           /* MDOWN_BLOCKQUOTE */
	"MDOWN_DEFINITION",		/* MDOWN_DEFINITION */
	"MDOWN_DEFINITION_TITLE",	/* MDOWN_DEFINITION_TITLE */
	"MDOWN_DEFINITION_DATA",	/* MDOWN_DEFINITION_DATA */
	"MDOWN_HEADER",               /* MDOWN_HEADER */
	"MDOWN_HRULE",                /* MDOWN_HRULE */
	"MDOWN_LIST",                 /* MDOWN_LIST */
	"MDOWN_LISTITEM",             /* MDOWN_LISTITEM */
	"MDOWN_PARAGRAPH",            /* MDOWN_PARAGRAPH */
	"MDOWN_TABLE_BLOCK",          /* MDOWN_TABLE_BLOCK */
	"MDOWN_TABLE_HEADER",         /* MDOWN_TABLE_HEADER */
	"MDOWN_TABLE_BODY",           /* MDOWN_TABLE_BODY */
	"MDOWN_TABLE_ROW",            /* MDOWN_TABLE_ROW */
	"MDOWN_TABLE_CELL",           /* MDOWN_TABLE_CELL */
	"MDOWN_FOOTNOTES_BLOCK",      /* MDOWN_FOOTNOTES_BLOCK */
	"MDOWN_FOOTNOTE_DEF",         /* MDOWN_FOOTNOTE_DEF */
	"MDOWN_BLOCKHTML",            /* MDOWN_BLOCKHTML */
	"MDOWN_LINK_AUTO",            /* MDOWN_LINK_AUTO */
	"MDOWN_CODESPAN",             /* MDOWN_CODESPAN */
	"MDOWN_DOUBLE_EMPHASIS",      /* MDOWN_DOUBLE_EMPHASIS */
	"MDOWN_EMPHASIS",             /* MDOWN_EMPHASIS */
	"MDOWN_HIGHLIGHT",            /* MDOWN_HIGHLIGHT */
	"MDOWN_IMAGE",                /* MDOWN_IMAGE */
	"MDOWN_LINEBREAK",            /* MDOWN_LINEBREAK */
	"MDOWN_LINK",                 /* MDOWN_LINK */
	"MDOWN_TRIPLE_EMPHASIS",      /* MDOWN_TRIPLE_EMPHASIS */
	"MDOWN_STRIKETHROUGH",        /* MDOWN_STRIKETHROUGH */
	"MDOWN_SUPERSCRIPT",          /* MDOWN_SUPERSCRIPT */
	"MDOWN_FOOTNOTE_REF",         /* MDOWN_FOOTNOTE_REF */
	"MDOWN_MATH_BLOCK",           /* MDOWN_MATH_BLOCK */
	"MDOWN_RAW_HTML",             /* MDOWN_RAW_HTML */
	"MDOWN_ENTITY",               /* MDOWN_ENTITY */
	"MDOWN_NORMAL_TEXT",          /* MDOWN_NORMAL_TEXT */
	"MDOWN_DOC_HEADER",           /* MDOWN_DOC_HEADER */
	"MDOWN_META",			/* MDOWN_META */
	"MDOWN_DOC_FOOTER",           /* MDOWN_DOC_FOOTER */
};

static int
rndr_indent(struct mdown_buf *ob, size_t indent)
{
	size_t	 i;

	for (i = 0; i < indent; i++)
		if (!HBUF_PUTSL(ob, "  "))
			return 0;
	return 1;
}

static int
rndr_short(struct mdown_buf *ob, const struct mdown_buf *b)
{
	size_t	 i;

	for (i = 0; i < 20 && i < b->size; i++)
		if (b->data[i] == '\n') {
			if (!HBUF_PUTSL(ob, "\\n"))
				return 0;
		} else if (b->data[i] == '\t') {
			if (!HBUF_PUTSL(ob, "\\t"))
				return 0;
		} else if (iscntrl((unsigned char)b->data[i])) {
			if (!hbuf_putc(ob, '?'))
				return 0;
		} else {
			if (!hbuf_putc(ob, b->data[i]))
				return 0;
		}

	if (i < b->size && !HBUF_PUTSL(ob, "..."))
		return 0;
	return 1;
}

static int
rndr(struct mdown_buf *ob,
	const struct mdown_node *root, size_t indent)
{
	const struct mdown_node	*n;
	struct mdown_buf		*tmp;

	if (!rndr_indent(ob, indent))
		return 0;
	if (root->chng == MDOWN_CHNG_INSERT && 
	    !HBUF_PUTSL(ob, "INSERT: "))
		return 0;
	if (root->chng == MDOWN_CHNG_DELETE && 
	    !HBUF_PUTSL(ob, "DELETE: "))
		return 0;
	if (!hbuf_puts(ob, names[root->type]))
		return 0;
	if (!hbuf_putc(ob, '\n'))
		return 0;

	switch (root->type) {
	case MDOWN_PARAGRAPH:
		if (!rndr_indent(ob, indent + 1))
			return 0;
		if (!hbuf_printf(ob, "lines: %zu, blank-after: %d\n", 
		    root->rndr_paragraph.lines,
		    root->rndr_paragraph.beoln))
			return 0;
		break;
	case MDOWN_IMAGE:
		if (!rndr_indent(ob, indent + 1))
			return 0;
		if (!hbuf_printf(ob, "source: "))
			return 0;
		if (!rndr_short(ob, &root->rndr_image.link))
			return 0;
		if (root->rndr_image.dims.size) {
			if (!HBUF_PUTSL(ob, "("))
				return 0;
			if (!rndr_short(ob, &root->rndr_image.dims))
				return 0;
			if (!HBUF_PUTSL(ob, ")"))
				return 0;
		}
		if (!HBUF_PUTSL(ob, "\n"))
			return 0;
		if (root->rndr_image.title.size) {
			if (!rndr_indent(ob, indent + 1))
				return 0;
			if (!hbuf_printf(ob, "title: "))
				return 0;
			if (!rndr_short(ob, &root->rndr_image.title))
				return 0;
			if (!HBUF_PUTSL(ob, "\n"))
				return 0;
		}
		if (root->rndr_image.alt.size) {
			if (!rndr_indent(ob, indent + 1))
				return 0;
			if (!hbuf_printf(ob, "alt: "))
				return 0;
			if (!rndr_short(ob, &root->rndr_image.alt))
				return 0;
			if (!HBUF_PUTSL(ob, "\n"))
				return 0;
		}
		if (root->rndr_image.dims.size) {
			if (!rndr_indent(ob, indent + 1))
				return 0;
			if (!hbuf_printf(ob, "dims: "))
				return 0;
			if (!rndr_short(ob, &root->rndr_image.dims))
				return 0;
			if (!HBUF_PUTSL(ob, "\n"))
				return 0;
		}
		if (root->rndr_image.attr_width.size) {
			if (!rndr_indent(ob, indent + 1))
				return 0;
			if (!hbuf_printf(ob, "width (extended): "))
				return 0;
			if (!rndr_short(ob, &root->rndr_image.attr_width))
				return 0;
			if (!HBUF_PUTSL(ob, "\n"))
				return 0;
		}
		if (root->rndr_image.attr_height.size) {
			if (!rndr_indent(ob, indent + 1))
				return 0;
			if (!hbuf_printf(ob, "height (extended): "))
				return 0;
			if (!rndr_short(ob, &root->rndr_image.attr_height))
				return 0;
			if (!HBUF_PUTSL(ob, "\n"))
				return 0;
		}
		break;
	case MDOWN_HEADER:
		if (!rndr_indent(ob, indent + 1))
			return 0;
		if (!hbuf_printf(ob, "level: %zu\n",
		    root->rndr_header.level))
			return 0;
		break;
	case MDOWN_FOOTNOTE_REF:
		if (!rndr_indent(ob, indent + 1))
			return 0;
		if (!hbuf_printf(ob, "number: %zu\n",
		    root->rndr_footnote_ref.num))
			return 0;
		if (!rndr_indent(ob, indent + 1))
			return 0;
		if (!hbuf_printf(ob, "name: "))
			return 0;
		if (!rndr_short(ob, &root->rndr_footnote_ref.key))
			return 0;
		if (!HBUF_PUTSL(ob, "\n"))
			return 0;
		break;
	case MDOWN_FOOTNOTE_DEF:
		if (!rndr_indent(ob, indent + 1))
			return 0;
		if (!hbuf_printf(ob, "number: %zu\n",
		    root->rndr_footnote_def.num))
			return 0;
		if (!rndr_indent(ob, indent + 1))
			return 0;
		if (!hbuf_printf(ob, "name: "))
			return 0;
		if (!rndr_short(ob, &root->rndr_footnote_def.key))
			return 0;
		if (!HBUF_PUTSL(ob, "\n"))
			return 0;
		break;
	case MDOWN_RAW_HTML:
		if (!rndr_indent(ob, indent + 1))
			return 0;
		if (!hbuf_printf(ob, "data: %zu Bytes: ",
		    root->rndr_raw_html.text.size))
			return 0;
		if (!rndr_short(ob, &root->rndr_raw_html.text))
			return 0;
		if (!HBUF_PUTSL(ob, "\n"))
			return 0;
		break;
	case MDOWN_BLOCKHTML:
		if (!rndr_indent(ob, indent + 1))
			return 0;
		if (!hbuf_printf(ob, "data: %zu Bytes: ",
		    root->rndr_blockhtml.text.size))
			return 0;
		if (!rndr_short(ob, &root->rndr_blockhtml.text))
			return 0;
		if (!HBUF_PUTSL(ob, "\n"))
			return 0;
		break;
	case MDOWN_BLOCKCODE:
		if (!rndr_indent(ob, indent + 1))
			return 0;
		if (!hbuf_printf(ob, "data: %zu Bytes: ",
		    root->rndr_blockcode.text.size))
			return 0;
		if (!rndr_short(ob, &root->rndr_blockcode.text))
			return 0;
		if (!HBUF_PUTSL(ob, "\n"))
			return 0;
		break;
	case MDOWN_DEFINITION:
		if (!rndr_indent(ob, indent + 1))
			return 0;
		if (!hbuf_printf(ob, "scope: %s\n",
		    HLIST_FL_BLOCK & root->rndr_definition.flags ? 
		    "block" : "span"))
			return 0;
		break;
	case MDOWN_TABLE_BLOCK:
		if (!rndr_indent(ob, indent + 1))
			return 0;
		if (!hbuf_printf(ob, "columns: %zu\n", 
		    root->rndr_table.columns))
			return 0;
		break;
	case MDOWN_TABLE_CELL:
		if (!rndr_indent(ob, indent + 1))
			return 0;
		if (!hbuf_printf(ob, "current: %zu\n", 
		    root->rndr_table_cell.col))
			return 0;
		break;
	case MDOWN_LISTITEM:
		if (!rndr_indent(ob, indent + 1))
			return 0;
		if (!hbuf_printf(ob, "scope: %s\n",
		    (root->rndr_listitem.flags & HLIST_FL_BLOCK) ?
		    "block" : "span"))
			return 0;
		if (!(root->rndr_listitem.flags &
		     (HLIST_FL_CHECKED | HLIST_FL_UNCHECKED)))
			break;
		if (!rndr_indent(ob, indent + 1))
			return 0;
		if (!hbuf_printf(ob, "check status: %s\n",
		    (root->rndr_listitem.flags & HLIST_FL_CHECKED) ?
		    "checked" : "unchecked"))
			return 0;
		break;
	case MDOWN_LIST:
		if (!rndr_indent(ob, indent + 1))
			return 0;
		if (!hbuf_printf(ob, "list type: %s\n",
		    HLIST_FL_ORDERED & root->rndr_list.flags ? 
		    "ordered" : "unordered"))
			return 0;
		break;
	case MDOWN_META:
		if (!rndr_indent(ob, indent + 1))
			return 0;
		if (!hbuf_printf(ob, "key: "))
			return 0;
		if (!rndr_short(ob, &root->rndr_meta.key))
			return 0;
		if (!HBUF_PUTSL(ob, "\n"))
			return 0;
		break;
	case MDOWN_MATH_BLOCK:
		if (!rndr_indent(ob, indent + 1))
			return 0;
		if (!hbuf_printf(ob, "blockmode: %s\n",
		    root->rndr_math.blockmode ?
		    "block" : "inline"))
			return 0;
		if (!rndr_indent(ob, indent + 1))
			return 0;
		if (!hbuf_printf(ob, "data: %zu Bytes: ",
		    root->rndr_math.text.size))
			return 0;
		if (!rndr_short(ob, &root->rndr_math.text))
			return 0;
		if (!HBUF_PUTSL(ob, "\n"))
			return 0;
		break;
	case MDOWN_ENTITY:
		if (!rndr_indent(ob, indent + 1))
			return 0;
		if (!hbuf_printf(ob, "value: "))
			return 0;
		if (!rndr_short(ob, &root->rndr_entity.text))
			return 0;
		if (!HBUF_PUTSL(ob, "\n"))
			return 0;
		break;
	case MDOWN_LINK_AUTO:
		if (root->rndr_autolink.link.size) {
			if (!rndr_indent(ob, indent + 1))
				return 0;
			if (!HBUF_PUTSL(ob, "link: "))
				return 0;
			if (!rndr_short(ob, &root->rndr_autolink.link))
				return 0;
			if (!HBUF_PUTSL(ob, "\n"))
				return 0;
		}
		break;
	case MDOWN_LINK:
		if (root->rndr_link.title.size) {
			if (!rndr_indent(ob, indent + 1))
				return 0;
			if (!HBUF_PUTSL(ob, "title: "))
				return 0;
			if (!rndr_short(ob, &root->rndr_link.title))
				return 0;
			if (!HBUF_PUTSL(ob, "\n"))
				return 0;
		}
		if (root->rndr_link.link.size) {
			if (!rndr_indent(ob, indent + 1))
				return 0;
			if (!HBUF_PUTSL(ob, "link: "))
				return 0;
			if (!rndr_short(ob, &root->rndr_link.link))
				return 0;
			if (!HBUF_PUTSL(ob, "\n"))
				return 0;
		}
		break;
	case MDOWN_NORMAL_TEXT:
		if (!rndr_indent(ob, indent + 1))
			return 0;
		if (!hbuf_printf(ob, "data: %zu Bytes: ",
		    root->rndr_normal_text.text.size))
			return 0;
		if (!rndr_short(ob, &root->rndr_normal_text.text))
			return 0;
		if (!HBUF_PUTSL(ob, "\n"))
			return 0;
		break;
	default:
		break;
	}

	if ((tmp = hbuf_new(64)) == NULL)
		return 0;

	TAILQ_FOREACH(n, &root->children, entries)
		if (!rndr(tmp, n, indent + 1)) {
			hbuf_free(tmp);
			return 0;
		}

	hbuf_putb(ob, tmp);
	hbuf_free(tmp);
	return 1;
}

int
mdown_tree_rndr(struct mdown_buf *ob,
	const struct mdown_node *root)
{

	return rndr(ob, root, 0);
}

