/*	$Id$ */
/*
 * Copyright (c) 2008, Natacha Porté
 * Copyright (c) 2011, Vicent Martí
 * Copyright (c) 2014, Xavier Mendez, Devin Torres and the Hoedown authors
 * Copyright (c) 2016--2021 Kristaps Dzonsons <kristaps@bsd.lv>
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
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "mdown.h"
#include "extern.h"

/*
 * Queue entry for header names.
 * Keep these so we can make sure that headers have a unique "id" for
 * themselves.
 */
struct	hentry {
	char		*str; /* header name (raw) */
	size_t	 	 count; /* references */
	TAILQ_ENTRY(hentry) entries;
};

/*
 * Our internal state object.
 */
struct 	html {
	TAILQ_HEAD(, hentry) 	 headers_used;
	ssize_t			 headers_offs; /* header offset */
	unsigned int 		 flags; /* "oflags" in mdown_opts */
	int			 noescape; /* don't escape text */
};

/*
 * Escape regular text that shouldn't be HTML.
 * Return zero on failure, non-zero on success.
 */
static int
escape_html(struct mdown_buf *ob, const char *source,
	size_t length, const struct html *st)
{

	assert(st->noescape == 0);
	return hesc_html(ob, source, length, 
		st->flags & MDOWN_HTML_OWASP, 0,
		st->flags & MDOWN_HTML_NUM_ENT);
}

/*
 * See escape_html().
 */
static int
escape_htmlb(struct mdown_buf *ob, 
	const struct mdown_buf *in, const struct html *st)
{

	return st->noescape ?
		hbuf_putb(ob, in) :
		escape_html(ob, in->data, in->size, st);
}

/*
 * Escape literal text.
 * Like escape_html() except more restrictive.
 * Return zero on failure, non-zero on success.
 */
static int
escape_literal(struct mdown_buf *ob, 
	const struct mdown_buf *in, const struct html *st)
{

	assert(st->noescape == 0);
	return hesc_html(ob, in->data, in->size, 
		st->flags & MDOWN_HTML_OWASP, 1,
		st->flags & MDOWN_HTML_NUM_ENT);
}

/*
 * Escape an href link.
 * Return zero on failure, non-zero on success.
 */
static int
escape_href(struct mdown_buf *ob, const struct mdown_buf *in,
	const struct html *st)
{

	assert(st->noescape == 0);
	return hesc_href(ob, in->data, in->size);
}

/*
 * Escape an HTML attribute.
 * Return zero on failure, non-zero on success.
 */
static int
escape_attr(struct mdown_buf *ob, const struct mdown_buf *in)
{

	return hesc_attr(ob, in->data, in->size);
}

static int
rndr_autolink(struct mdown_buf *ob, 
	const struct rndr_autolink *parm,
	const struct html *st)
{

	if (parm->link.size == 0)
		return 1;

	if (!HBUF_PUTSL(ob, "<a href=\""))
		return 0;
	if (parm->type == HALINK_EMAIL && !HBUF_PUTSL(ob, "mailto:"))
		return 0;
	if (!escape_href(ob, &parm->link, st))
		return 0;
	if (!HBUF_PUTSL(ob, "\">"))
		return 0;

	/*
	 * Pretty printing: if we get an email address as
	 * an actual URI, e.g. `mailto:foo@bar.com`, we don't
	 * want to print the `mailto:` prefix
	 */

	if (hbuf_strprefix(&parm->link, "mailto:")) {
		if (!escape_html(ob, 
		    parm->link.data + 7, 
		    parm->link.size - 7, st))
			return 0;
	} else {
		if (!escape_htmlb(ob, &parm->link, st))
			return 0;
	}

	return HBUF_PUTSL(ob, "</a>");
}

static int
rndr_blockcode(struct mdown_buf *ob, 
	const struct rndr_blockcode *parm,
	const struct html *st)
{
	if (ob->size && !hbuf_putc(ob, '\n'))
		return 0;

	if (parm->lang.size) {
		if (!HBUF_PUTSL(ob, "<pre><code class=\"language-"))
			return 0;
		if (!escape_href(ob, &parm->lang, st))
			return 0;
		if (!HBUF_PUTSL(ob, "\">"))
			return 0;
	} else {
		if (! HBUF_PUTSL(ob, "<pre><code>"))
			return 0;
	}

	if (!escape_literal(ob, &parm->text, st))
		return 0;
	return HBUF_PUTSL(ob, "</code></pre>\n");
}

static int
rndr_definition_data(struct mdown_buf *ob,
	const struct mdown_buf *content)
{

	if (!HBUF_PUTSL(ob, "<dd>\n"))
		return 0;
	if (!hbuf_putb(ob, content))
		return 0;
	return HBUF_PUTSL(ob, "\n</dd>\n");
}

static int
rndr_definition_title(struct mdown_buf *ob,
	const struct mdown_buf *content)
{
	size_t	 sz;

	if (!HBUF_PUTSL(ob, "<dt>"))
		return 0;
	if ((sz = content->size) > 0) {
		while (sz && content->data[sz - 1] == '\n')
			sz--;
		if (!hbuf_put(ob, content->data, sz))
			return 0;
	}
	return HBUF_PUTSL(ob, "</dt>\n");
}

static int
rndr_definition(struct mdown_buf *ob,
	const struct mdown_buf *content)
{

	if (ob->size && !hbuf_putc(ob, '\n'))
		return 0;
	if (!HBUF_PUTSL(ob, "<dl>\n"))
		return 0;
	if (!hbuf_putb(ob, content))
		return 0;
	return HBUF_PUTSL(ob, "</dl>\n");
}

static int
rndr_blockquote(struct mdown_buf *ob,
	const struct mdown_buf *content)
{

	if (ob->size && !hbuf_putc(ob, '\n'))
		return 0;
	if (!HBUF_PUTSL(ob, "<blockquote>\n"))
		return 0;
	if (!hbuf_putb(ob, content))
		return 0;
	return HBUF_PUTSL(ob, "</blockquote>\n");
}

static int
rndr_codespan(struct mdown_buf *ob,
	const struct rndr_codespan *param, 
	const struct html *st)
{

	if (!HBUF_PUTSL(ob, "<code>"))
		return 0;
	if (!escape_htmlb(ob, &param->text, st))
		return 0;
	return HBUF_PUTSL(ob, "</code>");
}

static int
rndr_strikethrough(struct mdown_buf *ob,
	const struct mdown_buf *content)
{

	if (!HBUF_PUTSL(ob, "<del>"))
		return 0;
	if (!hbuf_putb(ob, content))
		return 0;
	return HBUF_PUTSL(ob, "</del>");
}

static int
rndr_double_emphasis(struct mdown_buf *ob,
	const struct mdown_buf *content)
{

	if (!HBUF_PUTSL(ob, "<strong>"))
		return 0;
	if (!hbuf_putb(ob, content))
		return 0;
	return HBUF_PUTSL(ob, "</strong>");
}

static int
rndr_emphasis(struct mdown_buf *ob,
	const struct mdown_buf *content)
{

	if (!HBUF_PUTSL(ob, "<em>"))
		return 0;
	if (!hbuf_putb(ob, content))
		return 0;
	return HBUF_PUTSL(ob, "</em>");
}

static int
rndr_highlight(struct mdown_buf *ob,
	const struct mdown_buf *content)
{

	if (!HBUF_PUTSL(ob, "<mark>"))
		return 0;
	if (!hbuf_putb(ob, content))
		return 0;
	return HBUF_PUTSL(ob, "</mark>");
}

static int
rndr_linebreak(struct mdown_buf *ob)
{

	return HBUF_PUTSL(ob, "<br/>\n");
}

/*
 * Given the header with non-empty content "header", fill "ob" with the
 * identifier used for the header.
 * This will reference-count the header so we don't have duplicates.
 * Return zero on failure (memory), non-zero on success.
 */
static int
rndr_header_id(struct mdown_buf *ob,
	const struct mdown_buf *header, struct html *st)
{
	struct hentry	*hentry;

	/* 
	 * See if the header was previously already defind. 
	 * Note that in HTML5, the identifier is case sensitive.
	 */

	TAILQ_FOREACH(hentry, &st->headers_used, entries) {
		if (strlen(hentry->str) != header->size)
			continue;
		if (strncmp(hentry->str, 
		    header->data, header->size) == 0)
			break;
	}

	/* Convert to escaped values. */

	if (!escape_href(ob, header, st))
		return 0;

	/*
	 * If we're non-unique, then append a "count" value.
	 * XXX: if we have a header named "foo-2", then two headers
	 * named "foo", we'll inadvertently have a collision.
	 * This is a bit much to keep track of, though...
	 */

	if (hentry != NULL)
		return hbuf_printf(ob, "-%zu", ++hentry->count);

	/* Create new header entry. */

	if ((hentry = calloc(1, sizeof(struct hentry))) == NULL)
		return 0;

	TAILQ_INSERT_TAIL(&st->headers_used, hentry, entries);
	hentry->count = 1;
	hentry->str = strndup(header->data, header->size);
	return hentry->str != NULL;
}

static int
rndr_header(struct mdown_buf *ob,
	const struct mdown_buf *content,
	const struct rndr_header *param, 
	struct html *st)
{
	ssize_t	level;

	level = (ssize_t)param->level + st->headers_offs;
	if (level < 1)
		level = 1;
	else if (level > 6)
		level = 6;

	if (ob->size && !hbuf_putc(ob, '\n'))
		return 0;

	if (content->size && (st->flags & MDOWN_HTML_HEAD_IDS)) {
		if (!hbuf_printf(ob, "<h%zu id=\"", level))
			return 0;
		if (!rndr_header_id(ob, content, st))
			return 0;
		if (!HBUF_PUTSL(ob, "\">"))
			return 0;
	} else {
		if (!hbuf_printf(ob, "<h%zu>", level))
			return 0;
	}

	if (!hbuf_putb(ob, content))
		return 0;
	return hbuf_printf(ob, "</h%zu>\n", level);
}

static int
rndr_link(struct mdown_buf *ob,
	const struct mdown_buf *content,
	const struct rndr_link *param,
	const struct html *st)
{

	if (!HBUF_PUTSL(ob, "<a href=\"") ||
	    !escape_href(ob, &param->link, st))
		return 0;

	if (param->title.size)
		if (!HBUF_PUTSL(ob, "\" title=\"") ||
		    !escape_attr(ob, &param->title))
			return 0;
	if (param->attr_cls.size)
		if (!HBUF_PUTSL(ob, "\" class=\"") ||
		    !escape_attr(ob, &param->attr_cls))
			return 0;
	if (param->attr_id.size)
		if (!HBUF_PUTSL(ob, "\" id=\"") ||
		    !escape_attr(ob, &param->attr_id))
			return 0;

	if (!HBUF_PUTSL(ob, "\">") ||
	    !hbuf_putb(ob, content) ||
	    !HBUF_PUTSL(ob, "</a>"))
		return 0;

	return 1;
}

static int
rndr_list(struct mdown_buf *ob,
	const struct mdown_buf *content,
	const struct rndr_list *param)
{

	if (ob->size && !hbuf_putc(ob, '\n'))
		return 0;
	if (param->flags & HLIST_FL_ORDERED) {
		if (param->start > 1) {
			if (!hbuf_printf(ob, 
			    "<ol start=\"%zu\">\n", param->start))
				return 0;
		} else {
			if (!HBUF_PUTSL(ob, "<ol>\n"))
				return 0;
		}
	} else if (!HBUF_PUTSL(ob, "<ul>\n"))
		return 0;

	if (!hbuf_putb(ob, content))
		return 0;

	return (param->flags & HLIST_FL_ORDERED) ?
		HBUF_PUTSL(ob, "</ol>\n") :
		HBUF_PUTSL(ob, "</ul>\n");
}

static int
rndr_listitem(struct mdown_buf *ob,
	const struct mdown_buf *content,
	const struct mdown_node *n)
{
	size_t	 size;
	int	 blk = 0;

	/*
	 * If we're in block mode (which can be assigned post factum in
	 * the parser), make sure that we have an extra <p> around
	 * non-block content.
	 */

	if (((n->rndr_listitem.flags & HLIST_FL_DEF) &&
	     n->parent != NULL &&
	     n->parent->parent != NULL &&
	     n->parent->parent->type == MDOWN_DEFINITION &&
	     (n->parent->parent->rndr_definition.flags & 
	      HLIST_FL_BLOCK)) ||
	    (!(n->rndr_listitem.flags & HLIST_FL_DEF) &&
	     n->parent != NULL &&
	     n->parent->type == MDOWN_LIST &&
	     (n->parent->rndr_list.flags & HLIST_FL_BLOCK))) {
		if (!(hbuf_strprefix(content, "<ul") ||
		      hbuf_strprefix(content, "<ol") ||
		      hbuf_strprefix(content, "<dl") ||
		      hbuf_strprefix(content, "<div") ||
		      hbuf_strprefix(content, "<table") ||
		      hbuf_strprefix(content, "<blockquote") ||
		      hbuf_strprefix(content, "<pre>") ||
		      hbuf_strprefix(content, "<h") ||
		      hbuf_strprefix(content, "<p>")))
			blk = 1;
	}

	/* Only emit <li> if we're not a <dl> list. */

	if (!(n->rndr_listitem.flags & HLIST_FL_DEF) &&
	    !HBUF_PUTSL(ob, "<li>"))
		return 0;

	if (blk && !HBUF_PUTSL(ob, "<p>"))
		return 0;

	if (n->rndr_listitem.flags &
	    (HLIST_FL_CHECKED|HLIST_FL_UNCHECKED))
		HBUF_PUTSL(ob, "<input type=\"checkbox\" ");
	if (n->rndr_listitem.flags & HLIST_FL_CHECKED)
		HBUF_PUTSL(ob, "checked=\"checked\" ");
	if (n->rndr_listitem.flags &
	    (HLIST_FL_CHECKED|HLIST_FL_UNCHECKED))
		HBUF_PUTSL(ob, "/>");

	/* Cut off any trailing space. */

	if ((size = content->size) > 0) {
		while (size && content->data[size - 1] == '\n')
			size--;
		if (!hbuf_put(ob, content->data, size))
			return 0;
	}

	if (blk && !HBUF_PUTSL(ob, "</p>"))
		return 0;
	if (!(n->rndr_listitem.flags & HLIST_FL_DEF) &&
	    !HBUF_PUTSL(ob, "</li>\n"))
		return 0;

	return 1;
}

static int
rndr_paragraph(struct mdown_buf *ob,
	const struct mdown_buf *content, 
	struct html *st)
{
	size_t	i = 0, org;

	if (content->size == 0)
		return 1;
	while (i < content->size &&
	       isspace((unsigned char)content->data[i])) 
		i++;
	if (i == content->size)
		return 1;

	if (ob->size && !hbuf_putc(ob, '\n'))
		return 0;
	if (!HBUF_PUTSL(ob, "<p>"))
		return 0;

	if (st->flags & MDOWN_HTML_HARD_WRAP) {
		for ( ; i < content->size; i++) {
			org = i;
			while (i < content->size && 
			       content->data[i] != '\n')
				i++;

			if (i > org && !hbuf_put
		   	    (ob, content->data + org, i - org))
				return 0;

			/*
			 * Do not insert a line break if this newline is
			 * the last character on the paragraph.
			 */

			if (i >= content->size - 1)
				break;
			if (!rndr_linebreak(ob))
				return 0;
		}
	} else {
		if (!hbuf_put(ob, 
		    content->data + i, content->size - i))
			return 0;
	}

	return HBUF_PUTSL(ob, "</p>\n");
}

static int
rndr_raw_block(struct mdown_buf *ob,
	const struct rndr_blockhtml *param,
	const struct html *st)
{
	size_t	org, sz;

	if ((st->flags & MDOWN_HTML_SKIP_HTML))
		return 1;
	if ((st->flags & MDOWN_HTML_ESCAPE))
		return escape_htmlb(ob, &param->text, st);

	/* 
	 * FIXME: Do we *really* need to trim the HTML? How does that
	 * make a difference? 
	 */

	sz = param->text.size;
	while (sz > 0 && param->text.data[sz - 1] == '\n')
		sz--;

	org = 0;
	while (org < sz && param->text.data[org] == '\n')
		org++;

	if (org >= sz)
		return 1;

	if (ob->size && !hbuf_putc(ob, '\n'))
		return 0;

	if (!hbuf_put(ob, param->text.data + org, sz - org))
		return 0;
	return hbuf_putc(ob, '\n');
}

static int
rndr_triple_emphasis(struct mdown_buf *ob,
	const struct mdown_buf *content)
{

	if (!HBUF_PUTSL(ob, "<strong><em>"))
		return 0;
	if (!hbuf_putb(ob, content))
		return 0;
	return HBUF_PUTSL(ob, "</em></strong>");
}

static int
rndr_hrule(struct mdown_buf *ob)
{

	if (ob->size && !hbuf_putc(ob, '\n'))
		return 0;
	return hbuf_puts(ob, "<hr/>\n");
}

static int
rndr_image(struct mdown_buf *ob,
	const struct rndr_image *param, 
	const struct html *st)
{
	char		 dimbuf[32];
	unsigned int	 x, y;
	int		 rc = 0;

	/*
	 * Scan in our dimensions, if applicable.
	 * It's unreasonable for them to be over 32 characters, so use
	 * that as a cap to the size.
	 */

	if (param->dims.size && 
	    param->dims.size < sizeof(dimbuf) - 1) {
		memset(dimbuf, 0, sizeof(dimbuf));
		memcpy(dimbuf, param->dims.data, param->dims.size);
		rc = sscanf(dimbuf, "%ux%u", &x, &y);
	}

	/* Require an "alt", even if blank. */

	if (!HBUF_PUTSL(ob, "<img src=\"") ||
	    !escape_href(ob, &param->link, st) ||
	    !HBUF_PUTSL(ob, "\" alt=\"") ||
	    !escape_attr(ob, &param->alt) ||
	    !HBUF_PUTSL(ob, "\""))
		return 0;

	if (param->attr_cls.size)
		if (!HBUF_PUTSL(ob, " class=\"") ||
		    !escape_attr(ob, &param->attr_cls) ||
		    !HBUF_PUTSL(ob, "\""))
			return 0;
	if (param->attr_id.size)
		if (!HBUF_PUTSL(ob, " id=\"") ||
		    !escape_attr(ob, &param->attr_id) ||
		    !HBUF_PUTSL(ob, "\""))
			return 0;

	if (param->attr_width.size || param->attr_height.size) {
		if (!HBUF_PUTSL(ob, " style=\""))
			return 0;
		if (param->attr_width.size)
			if (!HBUF_PUTSL(ob, "width:") ||
			    !escape_attr(ob, &param->attr_width) ||
			    !HBUF_PUTSL(ob, ";"))
				return 0;
		if (param->attr_height.size)
			if (!HBUF_PUTSL(ob, "height:") ||
			    !escape_attr(ob, &param->attr_height) ||
			    !HBUF_PUTSL(ob, ";"))
				return 0;
		if (!HBUF_PUTSL(ob, "\""))
			return 0;
	} else if (param->dims.size && rc > 0) {
		if (!hbuf_printf(ob, " width=\"%u\"", x))
			return 0;
		if (rc > 1 && !hbuf_printf(ob, " height=\"%u\"", y))
			return 0;
	}

	if (param->title.size)
		if (!HBUF_PUTSL(ob, " title=\"") ||
		    !escape_htmlb(ob, &param->title, st) ||
		    !HBUF_PUTSL(ob, "\""))
			return 0;

	return hbuf_puts(ob, " />");
}

static int
rndr_raw_html(struct mdown_buf *ob,
	const struct rndr_raw_html *param,
	const struct html *st)
{

	if (st->flags & MDOWN_HTML_SKIP_HTML)
		return 1;

	return (st->flags & MDOWN_HTML_ESCAPE) ?
		escape_htmlb(ob, &param->text, st) :
		hbuf_putb(ob, &param->text);
}

static int
rndr_table(struct mdown_buf *ob,
	const struct mdown_buf *content)
{

	if (ob->size && !hbuf_putc(ob, '\n'))
		return 0;
	if (!HBUF_PUTSL(ob, "<table>\n"))
		return 0;
	if (!hbuf_putb(ob, content))
		return 0;
	return HBUF_PUTSL(ob, "</table>\n");
}

static int
rndr_table_header(struct mdown_buf *ob,
	const struct mdown_buf *content)
{

	if (ob->size && !hbuf_putc(ob, '\n'))
		return 0;
	if (!HBUF_PUTSL(ob, "<thead>\n"))
		return 0;
	if (!hbuf_putb(ob, content))
		return 0;
	return HBUF_PUTSL(ob, "</thead>\n");
}

static int
rndr_table_body(struct mdown_buf *ob,
	const struct mdown_buf *content)
{

	if (ob->size && !hbuf_putc(ob, '\n'))
		return 0;
	if (!HBUF_PUTSL(ob, "<tbody>\n"))
		return 0;
	if (!hbuf_putb(ob, content))
		return 0;
	return HBUF_PUTSL(ob, "</tbody>\n");
}

static int
rndr_tablerow(struct mdown_buf *ob,
	const struct mdown_buf *content)
{

	if (!HBUF_PUTSL(ob, "<tr>\n"))
		return 0;
	if (!hbuf_putb(ob, content))
		return 0;
	return HBUF_PUTSL(ob, "</tr>\n");
}

static int
rndr_tablecell(struct mdown_buf *ob,
	const struct mdown_buf *content,
	const struct rndr_table_cell *param)
{

	if (param->flags & HTBL_FL_HEADER) {
		if (!HBUF_PUTSL(ob, "<th"))
			return 0;
	} else {
		if (!HBUF_PUTSL(ob, "<td"))
			return 0;
	}

	switch (param->flags & HTBL_FL_ALIGNMASK) {
	case HTBL_FL_ALIGN_CENTER:
		if (!HBUF_PUTSL(ob, " style=\"text-align: center\">"))
			return 0;
		break;
	case HTBL_FL_ALIGN_LEFT:
		if (!HBUF_PUTSL(ob, " style=\"text-align: left\">"))
			return 0;
		break;
	case HTBL_FL_ALIGN_RIGHT:
		if (!HBUF_PUTSL(ob, " style=\"text-align: right\">"))
			return 0;
		break;
	default:
		if (!HBUF_PUTSL(ob, ">"))
			return 0;
		break;
	}

	if (!hbuf_putb(ob, content))
		return 0;

	return (param->flags & HTBL_FL_HEADER) ?
		HBUF_PUTSL(ob, "</th>\n") :
		HBUF_PUTSL(ob, "</td>\n");
}

static int
rndr_superscript(struct mdown_buf *ob,
	const struct mdown_buf *content)
{

	if (!HBUF_PUTSL(ob, "<sup>"))
		return 0;
	if (!hbuf_putb(ob, content))
		return 0;
	return HBUF_PUTSL(ob, "</sup>");
}

static int
rndr_normal_text(struct mdown_buf *ob,
	const struct rndr_normal_text *param,
	const struct html *st)
{

	return escape_htmlb(ob, &param->text, st);
}

static int
rndr_footnotes(struct mdown_buf *ob,
	const struct mdown_buf *content)
{

	if (ob->size && !hbuf_putc(ob, '\n'))
		return 0;
	if (!HBUF_PUTSL(ob, "<div class=\"footnotes\">\n"))
		return 0;
	if (!hbuf_puts(ob, "<hr/>\n"))
		return 0;
	if (!HBUF_PUTSL(ob, "<ol>\n"))
		return 0;
	if (!hbuf_putb(ob, content))
		return 0;
	return HBUF_PUTSL(ob, "\n</ol>\n</div>\n");
}

static int
rndr_footnote_def(struct mdown_buf *ob,
	const struct mdown_buf *content, 
	const struct rndr_footnote_def *param)
{
	size_t	i = 0;
	int	pfound = 0;

	/* Insert anchor at the end of first paragraph block. */

	while ((i + 3) < content->size) {
		if (content->data[i++] != '<') 
			continue;
		if (content->data[i++] != '/') 
			continue;
		if (content->data[i++] != 'p' && 
		    content->data[i] != 'P') 
			continue;
		if (content->data[i] != '>') 
			continue;
		i -= 3;
		pfound = 1;
		break;
	}

	if (!hbuf_printf(ob, "\n<li id=\"fn%zu\">\n", param->num))
		return 0;

	if (pfound) {
		if (!hbuf_put(ob, content->data, i))
			return 0;
		if (!hbuf_printf(ob, "&#160;"
		    "<a href=\"#fnref%zu\" rev=\"footnote\">"
		    "&#8617;"
		    "</a>", param->num))
			return 0;
		if (!hbuf_put(ob, 
		    content->data + i, content->size - i))
			return 0;
	} else {
		if (!hbuf_putb(ob, content))
			return 0;
	}

	return HBUF_PUTSL(ob, "</li>\n");
}

static int
rndr_footnote_ref(struct mdown_buf *ob,
	const struct rndr_footnote_ref *param)
{

	return hbuf_printf(ob, 
		"<sup id=\"fnref%zu\">"
		"<a href=\"#fn%zu\" rel=\"footnote\">"
		"%zu</a></sup>", 
		param->num, param->num, param->num);
}

static int
rndr_math(struct mdown_buf *ob,
	const struct rndr_math *param, 
	const struct html *st)
{

	if (param->blockmode && !HBUF_PUTSL(ob, "\\["))
		return 0;
	else if (!param->blockmode && !HBUF_PUTSL(ob, "\\("))
		return 0;

	if (!escape_htmlb(ob, &param->text, st))
		return 0;

	return param->blockmode ?
		HBUF_PUTSL(ob, "\\]") :
		HBUF_PUTSL(ob, "\\)");
}

static int
rndr_doc_footer(struct mdown_buf *ob, const struct html *st)
{

	if (st->flags & MDOWN_STANDALONE)
		return HBUF_PUTSL(ob, "</body>\n");
	return 1;
}

static int
rndr_root(struct mdown_buf *ob,
	const struct mdown_buf *content,
	const struct html *st)
{

	if ((st->flags & MDOWN_STANDALONE) && 
   	    !HBUF_PUTSL(ob, "<!DOCTYPE html>\n<html>\n"))
		    return 0;
	if (!hbuf_putb(ob, content))
		return 0;
	if (st->flags & MDOWN_STANDALONE)
		return HBUF_PUTSL(ob, "</html>\n");
	return 1;
}

/*
 * Split "val" into multiple strings delimited by two or more whitespace
 * characters, padding the output with "starttag" and "endtag".
 * Return zero on failure, non-zero on success.
 */
static int
rndr_meta_multi(struct mdown_buf *ob, const char *b, int href,
	const char *starttag, const char *endtag)
{
	const char	*start;
	size_t		 sz, i, bsz;

	if (b == NULL)
		return 1;

	bsz = strlen(b);

	for (i = 0; i < bsz; i++) {
		while (i < bsz &&
		       isspace((unsigned char)b[i]))
			i++;
		if (i == bsz)
			continue;
		start = &b[i];

		for (; i < bsz; i++)
			if (i < bsz - 1 &&
			    isspace((unsigned char)b[i]) &&
			    isspace((unsigned char)b[i + 1]))
				break;

		if ((sz = &b[i] - start) == 0)
			continue;

		if (!hbuf_puts(ob, starttag))
			return 0;
		if (!HBUF_PUTSL(ob, "\""))
			return 0;
		if (!href && !hesc_attr(ob, start, sz))
			return 0;
		else if (href && !hesc_href(ob, start, sz))
			return 0;
		if (!HBUF_PUTSL(ob, "\""))
			return 0;
		if (!hbuf_puts(ob, endtag))
			return 0;
		if (!HBUF_PUTSL(ob, "\n"))
			return 0;
	}

	return 1;
}

/*
 * Allocate a meta-data value on the queue "mq".
 * Return zero on failure, non-zero on success.
 */
static int
rndr_meta(struct mdown_buf *ob,
	const struct mdown_buf *content,
	struct mdown_metaq *mq,
	const struct mdown_node *n, struct html *st)
{
	struct mdown_meta	*m;
	ssize_t			 val;
	const char		*ep;

	m = calloc(1, sizeof(struct mdown_meta));
	if (m == NULL)
		return 0;
	TAILQ_INSERT_TAIL(mq, m, entries);

	m->key = strndup(n->rndr_meta.key.data,
		n->rndr_meta.key.size);
	if (m->key == NULL)
		return 0;
	m->value = strndup(content->data, content->size);
	if (m->value == NULL)
		return 0;

	if (strcmp(m->key, "shiftheadinglevelby") == 0) {
		val = (ssize_t)strtonum
			(m->value, -100, 100, &ep);
		if (ep == NULL)
			st->headers_offs = val + 1;
	} else if (strcmp(m->key, "baseheaderlevel") == 0) {
		val = (ssize_t)strtonum
			(m->value, 1, 100, &ep);
		if (ep == NULL)
			st->headers_offs = val;
	}

	return 1;
}

static int
rndr_doc_header(struct mdown_buf *ob,
	const struct mdown_buf *content,
	const struct mdown_metaq *mq, 
	const struct html *st)
{
	const struct mdown_meta	*m;
	const char			*author = NULL, *title = NULL,
					*affil = NULL, *date = NULL,
					*copy = NULL, *rcsauthor = NULL, 
					*rcsdate = NULL, *css = NULL,
					*script = NULL;

	if (!(st->flags & MDOWN_STANDALONE))
		return 1;

	TAILQ_FOREACH(m, mq, entries)
		if (strcasecmp(m->key, "author") == 0)
			author = m->value;
		else if (strcasecmp(m->key, "copyright") == 0)
			copy = m->value;
		else if (strcasecmp(m->key, "affiliation") == 0)
			affil = m->value;
		else if (strcasecmp(m->key, "date") == 0)
			date = m->value;
		else if (strcasecmp(m->key, "rcsauthor") == 0)
			rcsauthor = rcsauthor2str(m->value);
		else if (strcasecmp(m->key, "rcsdate") == 0)
			rcsdate = rcsdate2str(m->value);
		else if (strcasecmp(m->key, "title") == 0)
			title = m->value;
		else if (strcasecmp(m->key, "css") == 0)
			css = m->value;
		else if (strcasecmp(m->key, "javascript") == 0)
			script = m->value;

	if (!hbuf_putb(ob, content))
		return 0;

	if (!HBUF_PUTSL(ob, 
	    "<head>\n"
	    "<meta charset=\"utf-8\" />\n"
	    "<meta name=\"viewport\""
	    " content=\"width=device-width,initial-scale=1\" />\n"))
		return 0;

	/* Overrides. */

	if (title == NULL)
		title = "Untitled article";
	if (rcsdate != NULL)
		date = rcsdate;
	if (rcsauthor != NULL)
		author = rcsauthor;

	if (!rndr_meta_multi(ob, affil, 0,
	    "<meta name=\"creator\" content=", " />"))
		return 0;

	if (!rndr_meta_multi(ob, author, 0,
	    "<meta name=\"author\" content=", " />"))
		return 0;

	if (!rndr_meta_multi(ob, copy, 0,
	    "<meta name=\"copyright\" content=", " />"))
		return 0;

	/*
	 * FIXME: don't use "scheme" if the date isn't in the
	 * appropriate format, or modify it depending upon the position
	 * of the year?
	 */

	if (date != NULL) {
		if (!hbuf_printf(ob, "<meta name="
		    "\"date\" scheme=\"YYYY-MM-DD\" content=\""))
			return 0;
		if (!hesc_attr(ob, date, strlen(date)))
			return 0;
		if (!HBUF_PUTSL(ob, "\" />\n"))
			return 0;
	}

	if (!rndr_meta_multi(ob, css, 1,
	    "<link rel=\"stylesheet\" href=", " />"))
		return 0;

	if (!rndr_meta_multi(ob, script, 1,
	     "<script src=", "></script>"))
		return 0;

	if (!HBUF_PUTSL(ob, "<title>"))
		return 0;
	if (!hesc_html(ob, title, strlen(title), 
	    st->flags & MDOWN_HTML_OWASP, 0,
	    st->flags & MDOWN_HTML_NUM_ENT))
		return 0;
	if (!HBUF_PUTSL(ob, "</title>\n"))
		return 0;
	return HBUF_PUTSL(ob, "</head>\n<body>\n");
}

static int
rndr(struct mdown_buf *ob,
	struct mdown_metaq *mq, void *ref, 
	const struct mdown_node *n)
{
	const struct mdown_node	*child;
	struct mdown_buf		*tmp;
	int32_t				 ent;
	struct html			*st = ref;
	int				 ret = 1, rc = 1;

	if ((tmp = hbuf_new(64)) == NULL)
		return 0;

	/*
	 * If we're processing metadata, don't escape the content as we
	 * read and parse it.  This prevents double-escaping.  We'll
	 * properly escape things as we inline them (standalone mode) or
	 * when we write body text.
	 */

	if (n->type == MDOWN_META)
		st->noescape = 1;

	TAILQ_FOREACH(child, &n->children, entries)
		if (!rndr(tmp, mq, st, child))
			goto out;

	if (n->chng == MDOWN_CHNG_INSERT && 
	    !HBUF_PUTSL(ob, "<ins>"))
		goto out;
	if (n->chng == MDOWN_CHNG_DELETE && 
	   !HBUF_PUTSL(ob, "<del>"))
		goto out;

	switch (n->type) {
	case MDOWN_ROOT:
		rc = rndr_root(ob, tmp, st);
		break;
	case MDOWN_BLOCKCODE:
		rc = rndr_blockcode(ob, &n->rndr_blockcode, st);
		break;
	case MDOWN_BLOCKQUOTE:
		rc = rndr_blockquote(ob, tmp);
		break;
	case MDOWN_DEFINITION:
		rc = rndr_definition(ob, tmp);
		break;
	case MDOWN_DEFINITION_TITLE:
		rc = rndr_definition_title(ob, tmp);
		break;
	case MDOWN_DEFINITION_DATA:
		rc = rndr_definition_data(ob, tmp);
		break;
	case MDOWN_DOC_HEADER:
		rc = rndr_doc_header(ob, tmp, mq, st);
		break;
	case MDOWN_META:
		st->noescape = 0;
		if (n->chng != MDOWN_CHNG_DELETE)
			rc = rndr_meta(ob, tmp, mq, n, st);
		break;
	case MDOWN_DOC_FOOTER:
		rc = rndr_doc_footer(ob, st);
		break;
	case MDOWN_HEADER:
		rc = rndr_header(ob, tmp, &n->rndr_header, st);
		break;
	case MDOWN_HRULE:
		rc = rndr_hrule(ob);
		break;
	case MDOWN_LIST:
		rc = rndr_list(ob, tmp, &n->rndr_list);
		break;
	case MDOWN_LISTITEM:
		rc = rndr_listitem(ob, tmp, n);
		break;
	case MDOWN_PARAGRAPH:
		rc = rndr_paragraph(ob, tmp, st);
		break;
	case MDOWN_TABLE_BLOCK:
		rc = rndr_table(ob, tmp);
		break;
	case MDOWN_TABLE_HEADER:
		rc = rndr_table_header(ob, tmp);
		break;
	case MDOWN_TABLE_BODY:
		rc = rndr_table_body(ob, tmp);
		break;
	case MDOWN_TABLE_ROW:
		rc = rndr_tablerow(ob, tmp);
		break;
	case MDOWN_TABLE_CELL:
		rc = rndr_tablecell(ob, tmp, &n->rndr_table_cell);
		break;
	case MDOWN_FOOTNOTES_BLOCK:
		rc = rndr_footnotes(ob, tmp);
		break;
	case MDOWN_FOOTNOTE_DEF:
		rc = rndr_footnote_def(ob, tmp, &n->rndr_footnote_def);
		break;
	case MDOWN_BLOCKHTML:
		rc = rndr_raw_block(ob, &n->rndr_blockhtml, st);
		break;
	case MDOWN_LINK_AUTO:
		rc = rndr_autolink(ob, &n->rndr_autolink, st);
		break;
	case MDOWN_CODESPAN:
		rc = rndr_codespan(ob, &n->rndr_codespan, st);
		break;
	case MDOWN_DOUBLE_EMPHASIS:
		rc = rndr_double_emphasis(ob, tmp);
		break;
	case MDOWN_EMPHASIS:
		rc = rndr_emphasis(ob, tmp);
		break;
	case MDOWN_HIGHLIGHT:
		rc = rndr_highlight(ob, tmp);
		break;
	case MDOWN_IMAGE:
		rc = rndr_image(ob, &n->rndr_image, st);
		break;
	case MDOWN_LINEBREAK:
		rc = rndr_linebreak(ob);
		break;
	case MDOWN_LINK:
		rc = rndr_link(ob, tmp, &n->rndr_link, st);
		break;
	case MDOWN_TRIPLE_EMPHASIS:
		rc = rndr_triple_emphasis(ob, tmp);
		break;
	case MDOWN_STRIKETHROUGH:
		rc = rndr_strikethrough(ob, tmp);
		break;
	case MDOWN_SUPERSCRIPT:
		rc = rndr_superscript(ob, tmp);
		break;
	case MDOWN_FOOTNOTE_REF:
		rc = rndr_footnote_ref(ob, &n->rndr_footnote_ref);
		break;
	case MDOWN_MATH_BLOCK:
		rc = rndr_math(ob, &n->rndr_math, st);
		break;
	case MDOWN_RAW_HTML:
		rc = rndr_raw_html(ob, &n->rndr_raw_html, st);
		break;
	case MDOWN_NORMAL_TEXT:
		rc = rndr_normal_text(ob, &n->rndr_normal_text, st);
		break;
	case MDOWN_ENTITY:
		if (!(st->flags & MDOWN_HTML_NUM_ENT)) {
			rc = hbuf_putb(ob, &n->rndr_entity.text);
			break;
		}

		/*
		 * Prefer numeric entities.
		 * This is because we're emitting XML (XHTML5) and it's
		 * not clear whether the processor can handle HTML
		 * entities.
		 */

		ent = entity_find_iso(&n->rndr_entity.text);
		rc = ent > 0 ?
			hbuf_printf(ob, "&#%" PRId32 ";", ent) :
			hbuf_putb(ob, &n->rndr_entity.text);
		break;
	default:
		rc = hbuf_putb(ob, tmp);
		break;
	}
	if (!rc)
		goto out;

	if (n->chng == MDOWN_CHNG_INSERT && 
	    !HBUF_PUTSL(ob, "</ins>"))
		goto out;
	if (n->chng == MDOWN_CHNG_DELETE &&
	    !HBUF_PUTSL(ob, "</del>"))
		goto out;

	ret = 1;
out:
	hbuf_free(tmp);
	return ret;
}

int
mdown_html_rndr(struct mdown_buf *ob,
	void *arg, const struct mdown_node *n)
{
	struct html		*st = arg;
	struct mdown_metaq	 metaq;
	int			 rc;

	TAILQ_INIT(&metaq);
	st->headers_offs = 1;

	rc = rndr(ob, &metaq, st, n);

	mdown_metaq_free(&metaq);
	return rc;
}

void *
mdown_html_new(const struct mdown_opts *opts)
{
	struct html	*p;

	if ((p = calloc(1, sizeof(struct html))) == NULL)
		return NULL;

	TAILQ_INIT(&p->headers_used);
	p->flags = opts == NULL ? 0 : opts->oflags;
	return p;
}

void
mdown_html_free(void *arg)
{
	struct html	*st = arg;
	struct hentry	*hentry;

	if (st == NULL)
		return;

	while ((hentry = TAILQ_FIRST(&st->headers_used)) != NULL) {
		TAILQ_REMOVE(&st->headers_used, hentry, entries);
		free(hentry->str);
		free(hentry);
	}

	free(st);
}
