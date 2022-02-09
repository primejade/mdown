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
 / OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#ifndef MDOWN_H
#define MDOWN_H

/*
 * All of this is documented in mdown.3.
 * If it's not documented, don't use it.
 * Or report it as a bug.
 */

/* We need this for compilation on musl systems. */

#include "config.h"
//#include <cstddef.h>
#include <stdio.h>
#ifndef __BEGIN_DECLS
# ifdef __cplusplus
#  define       __BEGIN_DECLS           extern "C" {
# else
#  define       __BEGIN_DECLS
# endif
#endif
#ifndef __END_DECLS
# ifdef __cplusplus
#  define       __END_DECLS             }
# else
#  define       __END_DECLS
# endif
#endif

enum	mdown_type {
	MDOWN_GEMINI,
	MDOWN_HTML,
	MDOWN_LATEX,
	MDOWN_XELATEX,
	MDOWN_MAN,
	MDOWN_NROFF,
	MDOWN_FODT,
	MDOWN_TERM,
	MDOWN_TREE,
	MDOWN_NULL
};

/*
 * All types of Markdown nodes that mdown understands.
 */
enum	mdown_rndrt {
	MDOWN_ROOT,
	MDOWN_BLOCKCODE,
	MDOWN_BLOCKQUOTE,
	MDOWN_DEFINITION,
	MDOWN_DEFINITION_TITLE,
	MDOWN_DEFINITION_DATA,
	MDOWN_HEADER,
	MDOWN_HRULE,
	MDOWN_LIST,
	MDOWN_LISTITEM,
	MDOWN_PARAGRAPH,
	MDOWN_TABLE_BLOCK,
	MDOWN_TABLE_HEADER,
	MDOWN_TABLE_BODY,
	MDOWN_TABLE_ROW,
	MDOWN_TABLE_CELL,
	MDOWN_FOOTNOTES_BLOCK,
	MDOWN_FOOTNOTE_DEF,
	MDOWN_BLOCKHTML,
	MDOWN_LINK_AUTO,
	MDOWN_CODESPAN,
	MDOWN_DOUBLE_EMPHASIS,
	MDOWN_EMPHASIS,
	MDOWN_HIGHLIGHT,
	MDOWN_IMAGE,
	MDOWN_LINEBREAK,
	MDOWN_LINK,
	MDOWN_TRIPLE_EMPHASIS,
	MDOWN_STRIKETHROUGH,
	MDOWN_SUPERSCRIPT,
	MDOWN_FOOTNOTE_REF,
	MDOWN_MATH_BLOCK,
	MDOWN_RAW_HTML,
	MDOWN_ENTITY,
	MDOWN_NORMAL_TEXT,
	MDOWN_DOC_HEADER,
	MDOWN_META,
	MDOWN_DOC_FOOTER,
	MDOWN__MAX
};

struct	mdown_buf {
	char		*data;	/* actual character data */
	size_t		 size;	/* size of the string */
	size_t		 maxsize; /* allocated size (0 = volatile) */
	size_t		 unit;	/* realloc unit size (0 = read-only) */
	int 		 buffer_free; /* obj should be freed */
};

TAILQ_HEAD(mdown_nodeq, mdown_node);

enum 	htbl_flags {
	HTBL_FL_ALIGN_LEFT = 1,
	HTBL_FL_ALIGN_RIGHT = 2,
	HTBL_FL_ALIGN_CENTER = 3,
	HTBL_FL_ALIGNMASK = 3,
	HTBL_FL_HEADER = 4
};

enum 	halink_type {
	HALINK_NONE, /* used internally when it is not an autolink */
	HALINK_NORMAL, /* normal http/http/ftp/mailto/etc link */
	HALINK_EMAIL /* e-mail link without explit mailto: */
};

enum	hlist_fl {
	HLIST_FL_ORDERED = (1 << 0), /* <ol> list item */
	HLIST_FL_BLOCK = (1 << 1), /* <li> containing block data */
	HLIST_FL_UNORDERED = (1 << 2), /* <ul> list item */
	HLIST_FL_DEF = (1 << 3), /* <dl> list item */
	HLIST_FL_CHECKED = (1 << 4), /* <li> with checked box */
	HLIST_FL_UNCHECKED = (1 << 5), /* <li> with unchecked box */
};

/*
 * Meta-data keys and values.
 * Both of these are non-NULL (but possibly empty).
 */
struct	mdown_meta {
	char		*key;
	char		*value;
	TAILQ_ENTRY(mdown_meta) entries;
};

TAILQ_HEAD(mdown_metaq, mdown_meta);

enum	mdown_chng {
	MDOWN_CHNG_NONE = 0,
	MDOWN_CHNG_INSERT,
	MDOWN_CHNG_DELETE,
};

struct	rndr_meta {
	struct mdown_buf key;
};

struct	rndr_paragraph {
	size_t lines; /* input lines */
	int beoln; /* ends on blank line */
};

struct	rndr_normal_text {
	struct mdown_buf text; /* basic text */
};

struct	rndr_entity {
	struct mdown_buf text; /* entity text */
};

struct	rndr_autolink {
	struct mdown_buf link; /* link address */
	enum halink_type type; /* type of link */
};

struct	rndr_raw_html {
	struct mdown_buf text; /* raw html buffer */
};

struct	rndr_link {
	struct mdown_buf link;
	struct mdown_buf title;
	struct mdown_buf attr_cls;
	struct mdown_buf attr_id;
};

struct	rndr_blockcode {
	struct mdown_buf text; /* raw code buffer */
	struct mdown_buf lang; /* fence language */
};

struct	rndr_definition {
	enum hlist_fl flags;
};

struct	rndr_codespan {
	struct mdown_buf text; /* raw code buffer */
};

struct	rndr_table{
	size_t columns;
};

struct	rndr_table_header {
	enum htbl_flags *flags;
	size_t columns;
};

struct	rndr_table_cell {
	enum htbl_flags flags;
	size_t col;
	size_t columns;
};

struct	rndr_footnote_def {
	size_t num;
	struct mdown_buf key;
};

struct	rndr_footnote_ref {
	size_t num;
	struct mdown_buf def;
	struct mdown_buf key;
};

struct	rndr_blockhtml {
	struct mdown_buf text;
};

struct	rndr_list {
	enum hlist_fl flags;
	size_t start;
};

struct	rndr_listitem {
	enum hlist_fl flags;
	size_t num;
};

struct	rndr_header{
	size_t level; /* hN level */
};

struct	rndr_image {
	struct mdown_buf link;
	struct mdown_buf title;
	struct mdown_buf dims;
	struct mdown_buf alt;
	struct mdown_buf attr_width;
	struct mdown_buf attr_height;
	struct mdown_buf attr_cls;
	struct mdown_buf attr_id;
};

struct rndr_math {
	struct mdown_buf text; /* equation (opaque) */
	int blockmode;
};

/*
 * Node parsed from input document.
 * Each node is part of the parse tree.
 */
struct	mdown_node {
	enum mdown_rndrt	 type;
	enum mdown_chng	 chng; /* change type */
	size_t			 id; /* unique identifier */
	union {
		struct rndr_meta rndr_meta;
		struct rndr_list rndr_list; 
		struct rndr_paragraph rndr_paragraph;
		struct rndr_listitem rndr_listitem; 
		struct rndr_header rndr_header; 
		struct rndr_normal_text rndr_normal_text; 
		struct rndr_entity rndr_entity; 
		struct rndr_autolink rndr_autolink; 
		struct rndr_raw_html rndr_raw_html; 
		struct rndr_link rndr_link; 
		struct rndr_blockcode rndr_blockcode; 
		struct rndr_definition rndr_definition; 
		struct rndr_codespan rndr_codespan; 
		struct rndr_table rndr_table; 
		struct rndr_table_header rndr_table_header; 
		struct rndr_table_cell rndr_table_cell; 
		struct rndr_footnote_def rndr_footnote_def;
		struct rndr_footnote_ref rndr_footnote_ref;
		struct rndr_image rndr_image;
		struct rndr_math rndr_math;
		struct rndr_blockhtml rndr_blockhtml;
	};
	struct mdown_node *parent;
	struct mdown_nodeq children;
	TAILQ_ENTRY(mdown_node) entries;
};

/*
 * These options contain everything needed to parse and render content.
 */
struct	mdown_opts {
	enum mdown_type	  type;
	size_t			  maxdepth; /* max parse tree depth */
	size_t			  cols; /* -Tterm width */
	size_t			  hmargin; /* -Tterm left margin */
	size_t			  vmargin; /* -Tterm top/bot margin */
	unsigned int		  feat;
#define MDOWN_TABLES		  0x01
#define MDOWN_FENCED		  0x02
#define MDOWN_FOOTNOTES	  0x04
#define MDOWN_AUTOLINK	  0x08
#define MDOWN_STRIKE		  0x10
/* Omitted 			  0x20 */
#define MDOWN_HILITE		  0x40
/* Omitted 			  0x80 */
#define MDOWN_SUPER		  0x100
#define MDOWN_MATH		  0x200
#define MDOWN_NOINTEM		  0x400
/* Disabled MDOWN_MATHEXP	  0x1000 */
#define MDOWN_NOCODEIND	  0x2000
#define	MDOWN_METADATA	  0x4000
#define	MDOWN_COMMONMARK	  0x8000
#define	MDOWN_DEFLIST		  0x10000
#define	MDOWN_IMG_EXT	 	  0x20000 /* -> MDOWN_ATTRS */
#define MDOWN_TASKLIST	  0x40000
#define MDOWN_ATTRS		  0x80000
	unsigned int		  oflags;
#define	MDOWN_GEMINI_LINK_END	  0x8000 /* links at end */
#define	MDOWN_GEMINI_LINK_IN	  0x10000 /* links inline */
#define	MDOWN_GEMINI_LINK_NOREF 0x200000 /* for !inline, no names */
#define	MDOWN_GEMINI_LINK_ROMAN 0x400000 /* roman link names */
#define	MDOWN_HTML_NUM_ENT	  0x1000 /* use &#nn; if possible */
#define	MDOWN_HTML_OWASP	  0x800 /* use OWASP escaping */
#define	MDOWN_ODT_SKIP_HTML	  0x2000000 /* skip all HTML */
#define	MDOWN_SMARTY	  	  0x40 /* smart typography */
#define	MDOWN_TERM_NOANSI	  0x1000000 /* no ANSI escapes at all */
#define	MDOWN_TERM_NOCOLOUR	  0x800000 /* no ANSI colours */
#define MDOWN_GEMINI_METADATA	  0x100000 /* show metadata */
#define MDOWN_HTML_ESCAPE	  0x02 /* escape HTML (if not skip) */
#define MDOWN_HTML_HARD_WRAP	  0x04 /* paragraph line breaks */
#define MDOWN_HTML_HEAD_IDS	  0x100 /* <hN id="the_name"> */
#define MDOWN_HTML_SKIP_HTML	  0x01 /* skip all HTML */
#define MDOWN_XELATEX_NUMBERED	  0x4000 /* numbered sections */
#define MDOWN_XELATEX_SKIP_HTML	  0x2000 /* skip all HTML */
#define MDOWN_LATEX_NUMBERED	  0x4000 /* numbered sections */
#define MDOWN_LATEX_SKIP_HTML	  0x2000 /* skip all HTML */
#define MDOWN_NROFF_GROFF	  0x20 /* use groff extensions */
/* Disable MDOWN_NROFF_HARD_WRAP 0x10 */
#define MDOWN_NROFF_NOLINK	  0x80000 /* don't show URLs */
#define MDOWN_NROFF_NUMBERED	  0x80 /* numbered section headers */
#define MDOWN_NROFF_SHORTLINK	  0x40000 /* shorten URLs */
#define MDOWN_NROFF_SKIP_HTML	  0x08 /* skip all HTML */
#define MDOWN_STANDALONE	  0x200 /* emit complete document */
#define MDOWN_TERM_NOLINK	  0x20000 /* don't show URLs */
#define MDOWN_TERM_SHORTLINK	  0x400 /* shorten URLs */
	char			**meta;
	size_t			  metasz;
	char			**metaovr;
	size_t			  metaovrsz;
};

struct mdown_doc;

__BEGIN_DECLS

/*
 * High-level functions.
 * These use the "mdown_opts" to determine how to parse and render
 * content, and extract that content from a buffer, file, or descriptor.
 */
int	 mdown_buf(const struct mdown_opts *, 
		const char *, size_t,
		char **, size_t *, struct mdown_metaq *);
int	 mdown_buf_diff(const struct mdown_opts *, 
		const char *, size_t, const char *, size_t,
		char **, size_t *);
int	 mdown_file(const struct mdown_opts *, 
		FILE *, char **, size_t *, struct mdown_metaq *);
int	 mdown_file_diff(const struct mdown_opts *, FILE *, 
		FILE *, char **, size_t *);

/* 
 * Low-level functions.
 * These actually parse and render the AST from a buffer in various
 * ways.
 */

struct mdown_buf
	*mdown_buf_new(size_t) __attribute__((malloc));
void	 mdown_buf_free(struct mdown_buf *);

struct mdown_doc
	*mdown_doc_new(const struct mdown_opts *);
struct mdown_node
	*mdown_doc_parse(struct mdown_doc *, size_t *,
		const char *, size_t, struct mdown_metaq *);
struct mdown_node
	*mdown_diff(const struct mdown_node *,
		const struct mdown_node *, size_t *);
void	 mdown_doc_free(struct mdown_doc *);
void	 mdown_metaq_free(struct mdown_metaq *);

void 	 mdown_node_free(struct mdown_node *);

void	 mdown_html_free(void *);
void	*mdown_html_new(const struct mdown_opts *);
int 	 mdown_html_rndr(struct mdown_buf *, void *, 
		const struct mdown_node *);

void	 mdown_gemini_free(void *);
void	*mdown_gemini_new(const struct mdown_opts *);
int 	 mdown_gemini_rndr(struct mdown_buf *, void *, 
		const struct mdown_node *);

void	 mdown_term_free(void *);
void	*mdown_term_new(const struct mdown_opts *);
int 	 mdown_term_rndr(struct mdown_buf *, void *, 
		const struct mdown_node *);

void	 mdown_nroff_free(void *);
void	*mdown_nroff_new(const struct mdown_opts *);
int 	 mdown_nroff_rndr(struct mdown_buf *, void *, 
		const struct mdown_node *);

int 	 mdown_tree_rndr(struct mdown_buf *, 
		const struct mdown_node *);

void	 mdown_xelatex_free(void *);
void	*mdown_xelatex_new(const struct mdown_opts *);
int 	 mdown_xelatex_rndr(struct mdown_buf *, void *, 
		const struct mdown_node *);

void	 mdown_latex_free(void *);
void	*mdown_latex_new(const struct mdown_opts *);
int 	 mdown_latex_rndr(struct mdown_buf *, void *, 
		const struct mdown_node *);

void	 mdown_odt_free(void *);
void	*mdown_odt_new(const struct mdown_opts *);
int 	 mdown_odt_rndr(struct mdown_buf *, void *, 
		const struct mdown_node *);

__END_DECLS

#endif /* !MDOWN_H */
