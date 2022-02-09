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
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "mdown.h"
#include "extern.h"

/*
 * Starting size for input and output buffers.
 */
#define HBUF_START_BIG 4096

/*
 * Starting size for metadata buffers.
 */
#define HBUF_START_SMALL 128

/*
 * Merge adjacent text nodes into single text nodes, freeing the
 * duplicates along the way.
 * This is only used when diffing, as it makes the diff algorithm hvae a
 * more reasonable view of text in the tree.
 * Otherwise, it's just a waste of time.
 * Returns FALSE on failure (memory), TRUE on success.
 */
static int
mdown_merge_adjacent_text(struct mdown_node *n)
{
	struct mdown_node 	*nn, *prev, *tmp;

	TAILQ_FOREACH_SAFE(nn, &n->children, entries, tmp) {
		if (nn->type != MDOWN_NORMAL_TEXT) {
			if (!mdown_merge_adjacent_text(nn))
				return 0;
			continue;
		}
		prev = TAILQ_PREV(nn, mdown_nodeq, entries);
		if (prev == NULL ||
		    prev->type != MDOWN_NORMAL_TEXT)
			continue;
		hbuf_putb(&prev->rndr_normal_text.text, 
			&nn->rndr_normal_text.text);
		TAILQ_REMOVE(&n->children, nn, entries);
		mdown_node_free(nn);
	}
	return 1;
}

/*
 * Return FALSE on failure, TRUE on success.
 */
static int
mdown_render(const struct mdown_opts *opts,
	struct mdown_buf *ob, const struct mdown_node *n)
{
	void	*rndr;
	int	 c = 0;

	switch (opts == NULL ? MDOWN_HTML : opts->type) {
	case MDOWN_GEMINI:
		if ((rndr = mdown_gemini_new(opts)) == NULL)
			return 0;
		c = mdown_gemini_rndr(ob, rndr, n);
		mdown_gemini_free(rndr);
		break;
	case MDOWN_HTML:
		if ((rndr = mdown_html_new(opts)) == NULL)
			return 0;
		c = mdown_html_rndr(ob, rndr, n);
		mdown_html_free(rndr);
		break;
	case MDOWN_XELATEX:
		if ((rndr = mdown_xelatex_new(opts)) == NULL)
			return 0;
		c = mdown_xelatex_rndr(ob, rndr, n);
		mdown_xelatex_free(rndr);
		break;
	case MDOWN_LATEX:
		if ((rndr = mdown_latex_new(opts)) == NULL)
			return 0;
		c = mdown_latex_rndr(ob, rndr, n);
		mdown_latex_free(rndr);
		break;
	case MDOWN_MAN:
	case MDOWN_NROFF:
		if ((rndr = mdown_nroff_new(opts)) == NULL)
			return 0;
		c = mdown_nroff_rndr(ob, rndr, n);
		mdown_nroff_free(rndr);
		break;
	case MDOWN_FODT:
		if ((rndr = mdown_odt_new(opts)) == NULL)
			return 0;
		c = mdown_odt_rndr(ob, rndr, n);
		mdown_odt_free(rndr);
		break;
	case MDOWN_TERM:
		if ((rndr = mdown_term_new(opts)) == NULL)
			return 0;
		c = mdown_term_rndr(ob, rndr, n);
		mdown_term_free(rndr);
		break;
	case MDOWN_TREE:
		c = mdown_tree_rndr(ob, n);
		break;
	default:
		abort();
		/* NOTREACHED */
	}

	return c;
}

int
mdown_buf(const struct mdown_opts *opts,
	const char *data, size_t datasz,
	char **res, size_t *rsz,
	struct mdown_metaq *metaq)
{
	struct mdown_buf	*ob = NULL;
	struct mdown_doc	*doc;
	size_t			 maxn;
	enum mdown_type	 t;
	struct mdown_node	*n = NULL;
	int			 rc = 0;

	t = opts == NULL ? MDOWN_HTML : opts->type;

	if ((doc = mdown_doc_new(opts)) == NULL)
		goto err;

	n = mdown_doc_parse(doc, &maxn, data, datasz, metaq);
	if (n == NULL)
		goto err;
	assert(n->type == MDOWN_ROOT);

    	if (opts != NULL && (opts->oflags & MDOWN_SMARTY)) 
		if (!smarty(n, maxn, t))
			goto err;

	if ((ob = mdown_buf_new(HBUF_START_BIG)) == NULL)
		goto err;

	if (!mdown_render(opts, ob, n))
		goto err;

	*res = ob->data;
	*rsz = ob->size;
	ob->data = NULL;
	rc = 1;
err:
	mdown_buf_free(ob);
	mdown_node_free(n);
	mdown_doc_free(doc);
	return rc;
}

int
mdown_buf_diff(const struct mdown_opts *opts,
	const char *new, size_t newsz,
	const char *old, size_t oldsz,
	char **res, size_t *rsz)
{
	struct mdown_buf 	*ob = NULL;
	struct mdown_doc 	*doc = NULL;
	enum mdown_type 	 t;
	struct mdown_node 	*nnew = NULL, *nold = NULL, 
				*ndiff = NULL;
	size_t			 maxn;
	int			 rc = 0;

	t = opts == NULL ? MDOWN_HTML : opts->type;

	if ((doc = mdown_doc_new(opts)) == NULL)
		goto err;

	nnew = mdown_doc_parse(doc, NULL, new, newsz, NULL);
	if (nnew == NULL)
		goto err;
	nold = mdown_doc_parse(doc, NULL, old, oldsz, NULL);
	if (nold == NULL)
		goto err;

	if (!mdown_merge_adjacent_text(nnew))
		goto err;
	if (!mdown_merge_adjacent_text(nold))
		goto err;

	ndiff = mdown_diff(nold, nnew, &maxn);

    	if (opts != NULL && (opts->oflags & MDOWN_SMARTY)) 
		if (!smarty(ndiff, maxn, t))
			goto err;

	if ((ob = mdown_buf_new(HBUF_START_BIG)) == NULL)
		goto err;

	if (!mdown_render(opts, ob, ndiff))
		goto err;

	*res = ob->data;
	*rsz = ob->size;
	ob->data = NULL;
	rc = 1;
err:
	mdown_buf_free(ob);
	mdown_node_free(ndiff);
	mdown_node_free(nnew);
	mdown_node_free(nold);
	mdown_doc_free(doc);
	return rc;
}

int
mdown_file(const struct mdown_opts *opts, FILE *fin,
	char **res, size_t *rsz, struct mdown_metaq *metaq)
{
	struct mdown_buf	*bin = NULL;
	int	 		 rc = 0;

	if ((bin = mdown_buf_new(HBUF_START_BIG)) == NULL)
		goto out;
	if (!hbuf_putf(bin, fin))
		goto out;

	if (!mdown_buf(opts,
	    bin->data, bin->size, res, rsz, metaq))
		goto out;
	rc = 1;
out:
	mdown_buf_free(bin);
	return rc;
}

int
mdown_file_diff(const struct mdown_opts *opts,
	FILE *fnew, FILE *fold, char **res, size_t *rsz)
{
	struct mdown_buf	*bnew = NULL, *bold = NULL;
	int	 		 rc = 0;

	if ((bnew = mdown_buf_new(HBUF_START_BIG)) == NULL)
		goto out;
	if ((bold = mdown_buf_new(HBUF_START_BIG)) == NULL)
		goto out;
	if (!hbuf_putf(bold, fold))
		goto out;
	if (!hbuf_putf(bnew, fnew))
		goto out;

	if (!mdown_buf_diff(opts, 
	    bnew->data, bnew->size, 
	    bold->data, bold->size, 
	    res, rsz))
		goto out;
	rc = 1;
out:
	mdown_buf_free(bnew);
	mdown_buf_free(bold);
	return rc;
}

